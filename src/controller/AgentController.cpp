#include "controller/AgentController.h"

#ifdef REARK_HAS_WUWE
#include <wuwe/agent/llm/llm_agent_runner.h>
#include <wuwe/agent/llm/llm_error.h>
#include <wuwe/agent/llm/openrouter_llm_client.h>
#include <wuwe/agent/tools/tool.hpp>
#if __has_include(<wuwe/agent/reasoning/reasoning.hpp>)
#include <wuwe/agent/reasoning/reasoning.hpp>
#define REARK_HAS_WUWE_REASONING 1
#endif
#endif

#include "controller/AgentSettings.h"
#include "controller/AgentKnowledgeController.h"
#include "controller/DecompilerController.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QStringList>
#include <QTime>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>

namespace {

#ifdef REARK_HAS_WUWE

std::string toStdString(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString fromStringView(std::string_view value)
{
    return QString::fromUtf8(value.data(), qsizetype(value.size()));
}

struct ReArkToolContext {
    std::shared_ptr<const DecompilerController::AgentSnapshot> snapshot;
    std::stop_token stopToken;
};

int normalizedLimit(int limit, int fallback)
{
    if (limit <= 0) {
        return fallback;
    }
    return std::clamp(limit, 1, 200);
}

QString boundedSnapshotText(const QString& text, int maxChars)
{
    const int limit = std::clamp(maxChars <= 0 ? 12000 : maxChars, 1000, 60000);
    if (text.size() <= limit) {
        return text;
    }
    return text.left(limit)
        + QStringLiteral("\n\n[truncated to %1 characters for the Agent snapshot]").arg(limit);
}

QStringList queryTerms(const QString& query)
{
    return query.trimmed().toCaseFolded().split(
        QRegularExpression(QStringLiteral("\\s+")),
        Qt::SkipEmptyParts);
}

bool matchesQuery(const QString& text, const QString& query)
{
    const QStringList terms = queryTerms(query);
    if (terms.isEmpty()) {
        return true;
    }

    const QString folded = text.toCaseFolded();
    for (const QString& term : terms) {
        if (!folded.contains(term)) {
            return false;
        }
    }
    return true;
}

QString fileSearchText(const DecompilerController::AgentFileSnapshot& file)
{
    return file.name + QLatin1Char('\n')
        + file.path + QLatin1Char('\n')
        + file.kind + QLatin1Char('\n')
        + file.section + QLatin1Char('\n')
        + file.contentMode + QLatin1Char('\n')
        + file.content + QLatin1Char('\n')
        + file.disassembly;
}

QString formatSnapshotFileLine(const DecompilerController::AgentFileSnapshot& file)
{
    QStringList details;
    if (!file.kind.isEmpty()) {
        details.append(file.kind);
    }
    if (!file.section.isEmpty()) {
        details.append(file.section);
    }
    details.append(file.loaded ? QStringLiteral("loaded") : QStringLiteral("not loaded"));
    if (file.hasDisassembly) {
        details.append(file.disassemblyLoaded
            ? QStringLiteral("disassembly loaded")
            : QStringLiteral("disassembly available"));
    }

    return QStringLiteral("%1 [%2]").arg(file.path, details.join(QStringLiteral(", ")));
}

QString listSnapshotFiles(const DecompilerController::AgentSnapshot& snapshot, const QString& query, int limit)
{
    const int maxCount = normalizedLimit(limit, 40);
    QString result;
    int count = 0;
    for (const auto& file : snapshot.files) {
        if (!matchesQuery(fileSearchText(file), query)) {
            continue;
        }
        result += QStringLiteral("- %1\n").arg(formatSnapshotFileLine(file));
        if (++count >= maxCount) {
            break;
        }
    }

    if (result.isEmpty()) {
        return QStringLiteral("No files matched the Agent snapshot query: %1").arg(query);
    }
    return boundedSnapshotText(result, 24000);
}

QString contentSnippet(const QString& text, const QString& query)
{
    const QString foldedText = text.toCaseFolded();
    int position = -1;
    for (const QString& term : queryTerms(query)) {
        position = foldedText.indexOf(term);
        if (position >= 0) {
            break;
        }
    }
    if (position < 0) {
        return {};
    }

    const int start = std::max(0, position - 80);
    const int length = std::min(static_cast<int>(text.size()) - start, 220);
    QString snippet = text.mid(start, length).simplified();
    if (start > 0) {
        snippet.prepend(QStringLiteral("... "));
    }
    if (start + length < text.size()) {
        snippet.append(QStringLiteral(" ..."));
    }
    return snippet;
}

QString searchSnapshotContent(const DecompilerController::AgentSnapshot& snapshot, const QString& query, int limit)
{
    if (query.trimmed().isEmpty()) {
        return QStringLiteral("Search query is empty.");
    }

    const int maxCount = normalizedLimit(limit, 40);
    QString result;
    int count = 0;
    for (const auto& file : snapshot.files) {
        if (file.content.isEmpty() && file.disassembly.isEmpty()) {
            continue;
        }
        if (!matchesQuery(fileSearchText(file), query)) {
            continue;
        }

        QString snippet = contentSnippet(file.content, query);
        if (snippet.isEmpty()) {
            snippet = contentSnippet(file.disassembly, query);
        }
        result += QStringLiteral("- %1").arg(file.path);
        if (!snippet.isEmpty()) {
            result += QStringLiteral(": %1").arg(snippet);
        }
        result += QLatin1Char('\n');
        if (++count >= maxCount) {
            break;
        }
    }

    if (result.isEmpty()) {
        return QStringLiteral("No loaded snapshot content matched: %1").arg(query);
    }
    return boundedSnapshotText(result, 24000);
}

const DecompilerController::AgentFileSnapshot* bestSnapshotFile(
    const DecompilerController::AgentSnapshot& snapshot,
    const QString& query)
{
    const QString foldedQuery = query.trimmed().toCaseFolded();
    const DecompilerController::AgentFileSnapshot* best = nullptr;
    int bestScore = -1;

    for (const auto& file : snapshot.files) {
        if (!matchesQuery(fileSearchText(file), query)) {
            continue;
        }

        const QString foldedPath = file.path.toCaseFolded();
        const QString foldedName = file.name.toCaseFolded();
        int score = 10;
        if (!foldedQuery.isEmpty()) {
            if (foldedPath == foldedQuery || foldedName == foldedQuery) {
                score += 1000;
            } else if (foldedPath.endsWith(QLatin1Char('/') + foldedQuery)) {
                score += 700;
            } else if (foldedName.contains(foldedQuery)) {
                score += 500;
            } else if (foldedPath.contains(foldedQuery)) {
                score += 250;
            }
        }
        if (score > bestScore) {
            best = &file;
            bestScore = score;
        }
    }

    return best;
}

QString readSnapshotSource(const DecompilerController::AgentSnapshot& snapshot, const QString& query, int maxChars)
{
    const auto* file = bestSnapshotFile(snapshot, query);
    if (file == nullptr) {
        return QStringLiteral("No source or resource file matched the Agent snapshot query: %1").arg(query);
    }
    if (!file->loaded || file->content.isEmpty()) {
        return QStringLiteral("Matched file is not loaded in the Agent snapshot: %1").arg(file->path);
    }

    QString text;
    text += QStringLiteral("# file: %1\n").arg(file->path);
    text += QStringLiteral("# kind: %1\n\n").arg(file->kind);
    text += file->content;
    return boundedSnapshotText(text, maxChars);
}

QString readSnapshotDisassembly(const DecompilerController::AgentSnapshot& snapshot, const QString& query, int maxChars)
{
    const auto* file = bestSnapshotFile(snapshot, query);
    if (file == nullptr) {
        return QStringLiteral("No source file matched the Agent snapshot query: %1").arg(query);
    }
    if (!file->hasDisassembly) {
        return QStringLiteral("Matched file does not have source-file disassembly: %1").arg(file->path);
    }
    if (!file->disassemblyLoaded || file->disassembly.isEmpty()) {
        return QStringLiteral("Disassembly is not loaded in the Agent snapshot for: %1").arg(file->path);
    }

    QString text;
    text += QStringLiteral("# disassembly: %1\n\n").arg(file->path);
    text += file->disassembly;
    return boundedSnapshotText(text, maxChars);
}

std::optional<wuwe::llm_tool_result> cancelledToolResult(const ReArkToolContext& context)
{
    if (!context.stopToken.stop_requested()) {
        return std::nullopt;
    }
    return wuwe::llm_tool_result {
        .content = "cancelled",
        .error_code = wuwe::agent::make_error_code(wuwe::agent::llm_error_code::cancelled)
    };
}

struct summarize_package {
    static constexpr std::string_view description =
        "Summarize the currently loaded ReArk analysis target, active tab, status, and important files.";

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        if (!context.snapshot) {
            return { .content = "No active ReArk analysis snapshot." };
        }
        return {
            .content = toStdString(context.snapshot->packageSummary)
        };
    }
};

struct list_files {
    static constexpr std::string_view description =
        "List package files or source files in the currently loaded ReArk target.";

    wuwe::field<std::string> query {
        .default_value = std::string {},
        .description = "Path, file name, kind, or text to match. Leave empty to list the most relevant files."
    };
    wuwe::field<int> limit {
        .default_value = 40,
        .description = "Maximum number of file candidates to return."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        if (!context.snapshot) {
            return { .content = "No active ReArk analysis snapshot." };
        }
        return {
            .content = toStdString(listSnapshotFiles(
                *context.snapshot,
                QString::fromStdString(query.value),
                limit.value))
        };
    }
};

struct search_loaded_content {
    static constexpr std::string_view description =
        "Search text that ReArk has already loaded or cached for the current target.";

    wuwe::field<std::string> query {
        .description = "Text, identifier, string, or path fragment to search for."
    };
    wuwe::field<int> limit {
        .default_value = 40,
        .description = "Maximum number of matches to return."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        if (!context.snapshot) {
            return { .content = "No active ReArk analysis snapshot." };
        }
        return {
            .content = toStdString(searchSnapshotContent(
                *context.snapshot,
                QString::fromStdString(query.value),
                limit.value))
        };
    }
};

struct read_source {
    static constexpr std::string_view description =
        "Read a loaded source, resource text, summary, or descriptor file from the current ReArk target.";

    wuwe::field<std::string> path_or_query {
        .description = "Exact path or search query for the file to read."
    };
    wuwe::field<int> max_chars {
        .default_value = 12000,
        .description = "Maximum number of characters to return."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        if (!context.snapshot) {
            return { .content = "No active ReArk analysis snapshot." };
        }
        return {
            .content = toStdString(readSnapshotSource(
                *context.snapshot,
                QString::fromStdString(path_or_query.value),
                max_chars.value))
        };
    }
};

struct read_disassembly {
    static constexpr std::string_view description =
        "Read already loaded source-file disassembly for a source file in the current ReArk target.";

    wuwe::field<std::string> path_or_query {
        .description = "Exact source path or search query for the file disassembly to read."
    };
    wuwe::field<int> max_chars {
        .default_value = 20000,
        .description = "Maximum number of disassembly characters to return."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        if (!context.snapshot) {
            return { .content = "No active ReArk analysis snapshot." };
        }
        return {
            .content = toStdString(readSnapshotDisassembly(
                *context.snapshot,
                QString::fromStdString(path_or_query.value),
                max_chars.value))
        };
    }
};

struct inspect_entry_points {
    static constexpr std::string_view description =
        "List likely entry points, descriptors, summary, signature, pages, and important files.";

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        if (!context.snapshot) {
            return { .content = "No active ReArk analysis snapshot." };
        }
        return {
            .content = toStdString(context.snapshot->entryPoints)
        };
    }
};

struct explain_signature {
    static constexpr std::string_view description =
        "Read the package signature summary if it is available in ReArk.";

    wuwe::field<int> max_chars {
        .default_value = 12000,
        .description = "Maximum number of signature characters to return."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        if (!context.snapshot) {
            return { .content = "No active ReArk analysis snapshot." };
        }
        return {
            .content = toStdString(boundedSnapshotText(context.snapshot->signatureSummary, max_chars.value))
        };
    }
};

class ReArkToolProvider {
public:
    explicit ReArkToolProvider(std::shared_ptr<const DecompilerController::AgentSnapshot> snapshot)
        : snapshot_(std::move(snapshot))
    {
        registerTool<summarize_package>();
        registerTool<list_files>();
        registerTool<search_loaded_content>();
        registerTool<read_source>();
        registerTool<read_disassembly>();
        registerTool<inspect_entry_points>();
        registerTool<explain_signature>();
    }

    std::vector<wuwe::llm_tool> tools() const
    {
        return tools_;
    }

    wuwe::llm_tool_result invoke(
        const std::string& name,
        const std::string& argumentsJson,
        std::stop_token stopToken)
    {
        ReArkToolContext context {
            .snapshot = snapshot_,
            .stopToken = stopToken
        };

        const auto invoker = invokers_.find(name);
        if (invoker != invokers_.end()) {
            return invoker->second(argumentsJson, context);
        }

        return {
            .content = "tool not found: " + name,
            .error_code = std::make_error_code(std::errc::function_not_supported)
        };
    }

private:
    template <typename Tool>
    void registerTool()
    {
        auto tool = wuwe::make_llm_tool<Tool>();
        const std::string name = tool.name;
        invokers_.emplace(name, [](const std::string& argumentsJson, const ReArkToolContext& context) {
            return wuwe::invoke_reflected_tool<Tool>(argumentsJson, context);
        });
        tools_.push_back(std::move(tool));
    }

    std::shared_ptr<const DecompilerController::AgentSnapshot> snapshot_;
    std::vector<wuwe::llm_tool> tools_;
    std::unordered_map<std::string, std::function<wuwe::llm_tool_result(
        const std::string&,
        const ReArkToolContext&)>> invokers_;
};

QString agentErrorMessage(std::error_code ec, const QString& message)
{
    if (ec == wuwe::agent::llm_error_code::missing_api_key) {
        return AgentController::tr("Missing API key. Configure Agent settings or set REARK_LLM_API_KEY / OPENROUTER_API_KEY.");
    }
    if (ec == wuwe::agent::llm_error_code::authentication_failed) {
        return AgentController::tr("Authentication failed. Please check the configured API key.");
    }
    if (ec == wuwe::agent::llm_error_code::rate_limited) {
        return AgentController::tr("The model provider is rate limited. Please try again later.");
    }
    if (ec == wuwe::agent::llm_error_code::model_unavailable) {
        return AgentController::tr("The configured model is unavailable.");
    }
    if (ec == wuwe::agent::llm_error_code::cancelled) {
        return AgentController::tr("Analysis cancelled.");
    }
    if (!message.isEmpty()) {
        return message;
    }
    return QString::fromStdString(ec.message());
}

#ifdef REARK_HAS_WUWE_REASONING
QString reasoningEventStatus(const wuwe::agent::reasoning::reasoning_event& event)
{
    namespace reasoning = wuwe::agent::reasoning;

    switch (event.type) {
    case reasoning::reasoning_event_type::started:
        return AgentController::tr("Thinking...");
    case reasoning::reasoning_event_type::model_started:
        return AgentController::tr("Calling model...");
    case reasoning::reasoning_event_type::tool_started:
        return AgentController::tr("Running tool: %1").arg(QString::fromStdString(event.message));
    case reasoning::reasoning_event_type::tool_completed:
        return event.tool_result != nullptr && event.tool_result->error_code
            ? AgentController::tr("Tool failed: %1").arg(QString::fromStdString(event.tool_call != nullptr
                  ? event.tool_call->name
                  : std::string {}))
            : AgentController::tr("Tool completed");
    case reasoning::reasoning_event_type::reflection_started:
        return AgentController::tr("Reviewing result...");
    case reasoning::reasoning_event_type::reflection_completed:
        return AgentController::tr("Review completed");
    case reasoning::reasoning_event_type::plan_created:
        return AgentController::tr("Plan created");
    case reasoning::reasoning_event_type::plan_step_started:
        return AgentController::tr("Running plan step...");
    case reasoning::reasoning_event_type::plan_step_completed:
        return AgentController::tr("Plan step completed");
    case reasoning::reasoning_event_type::plan_step_failed:
        return AgentController::tr("Plan step failed");
    case reasoning::reasoning_event_type::plan_step_blocked:
        return AgentController::tr("Plan step blocked");
    case reasoning::reasoning_event_type::plan_revised:
        return AgentController::tr("Plan revised");
    case reasoning::reasoning_event_type::completed:
        return AgentController::tr("Ready");
    case reasoning::reasoning_event_type::failed:
        return AgentController::tr("Analysis failed.");
    case reasoning::reasoning_event_type::cancelled:
        return AgentController::tr("Analysis cancelled.");
    case reasoning::reasoning_event_type::content_delta:
        break;
    }
    return {};
}

QString conversationInputForReasoning(const QVariantList& messages)
{
    QStringList lines;
    lines.append(QStringLiteral("Conversation:"));
    for (const QVariant& item : messages) {
        const QVariantMap message = item.toMap();
        const QString role = message.value(QStringLiteral("role")).toString();
        const QString content = message.value(QStringLiteral("text")).toString().trimmed();
        if (content.isEmpty()
            || (role != QStringLiteral("user") && role != QStringLiteral("assistant"))) {
            continue;
        }
        lines.append(QStringLiteral("%1: %2").arg(
            role == QStringLiteral("user") ? QStringLiteral("User") : QStringLiteral("Assistant"),
            content));
    }
    return lines.join(QLatin1Char('\n'));
}

QString dumpReasoningJson(const nlohmann::json& value)
{
    return QString::fromStdString(value.dump(2));
}

QString reasoningErrorToJson(const wuwe::agent::reasoning::reasoning_error& error)
{
    nlohmann::json value {
        { "completed", false },
        { "reasoning_error", wuwe::agent::reasoning::to_string(error.code) },
        { "underlying_error", error.underlying_error ? error.underlying_error.message() : "" },
        { "message", error.message }
    };
    return dumpReasoningJson(value);
}
#endif

#endif

} // namespace

struct AgentController::Runtime {
#ifdef REARK_HAS_WUWE
    std::unique_ptr<wuwe::openrouter_llm_client> client;
    std::shared_ptr<ReArkToolProvider> rearkProvider;
    std::shared_ptr<AgentKnowledgeController::KnowledgeToolProviderHandle> knowledgeProvider;
    std::shared_ptr<wuwe::composite_tool_provider> provider;
    std::unique_ptr<wuwe::llm_agent_runner> runner;
    std::optional<wuwe::llm_agent_run> run;
    std::stop_source stopSource;
#ifdef REARK_HAS_WUWE_REASONING
    std::unique_ptr<wuwe::agent::reasoning::reasoning_runner> reasoningRunner;
    std::optional<wuwe::agent::reasoning::reasoning_run> reasoningRun;
#endif
#endif
};

AgentController::AgentController(
    DecompilerController* decompilerController,
    AgentKnowledgeController* knowledgeController,
    QObject* parent)
    : QObject(parent)
    , decompilerController_(decompilerController)
    , knowledgeController_(knowledgeController)
    , runtime_(std::make_unique<Runtime>())
    , assistantDeltaTimer_(new QTimer(this))
{
    assistantDeltaTimer_->setSingleShot(true);
    assistantDeltaTimer_->setInterval(50);
    connect(assistantDeltaTimer_, &QTimer::timeout, this, &AgentController::flushPendingAssistantDelta);
    setStatus(available() ? tr("Ready") : unavailableMessage());
}

AgentController::~AgentController()
{
    resetRun();
}

bool AgentController::available() const
{
#ifdef REARK_HAS_WUWE
    return true;
#else
    return false;
#endif
}

bool AgentController::running() const
{
    return running_;
}

QString AgentController::transcript() const
{
    return transcript_;
}

QVariantList AgentController::messages() const
{
    return messages_;
}

bool AgentController::hasMessages() const
{
    return !messages_.isEmpty();
}

QString AgentController::errorMessage() const
{
    return errorMessage_;
}

QString AgentController::status() const
{
    return status_;
}

bool AgentController::hasReasoningDetails() const
{
    return !reasoningResultJson_.isEmpty()
        || !reasoningTraceJson_.isEmpty()
        || !reasoningUsageJson_.isEmpty();
}

QString AgentController::reasoningResultJson() const
{
    return reasoningResultJson_;
}

QString AgentController::reasoningTraceJson() const
{
    return reasoningTraceJson_;
}

QString AgentController::reasoningUsageJson() const
{
    return reasoningUsageJson_;
}

void AgentController::ask(const QString& question)
{
    const QString trimmed = question.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    setErrorMessage({});
    clearReasoningDetails();
    if (!available()) {
        appendMessage(QStringLiteral("user"), trimmed);
        appendMessage(QStringLiteral("assistant"), unavailableMessage(), QStringLiteral("error"));
        setErrorMessage(unavailableMessage());
        setStatus(unavailableMessage());
        return;
    }

#ifdef REARK_HAS_WUWE
    if (running_) {
        cancel();
        return;
    }

    resetRun();

    const AgentSettings settings = AgentSettingsStore::load();
    const QString validationMessage = AgentSettingsStore::validationMessage(settings);
    if (!validationMessage.isEmpty()) {
        appendMessage(QStringLiteral("user"), trimmed);
        appendMessage(QStringLiteral("assistant"), validationMessage, QStringLiteral("error"));
        setErrorMessage(validationMessage);
        setStatus(validationMessage);
        return;
    }

    auto snapshot = std::make_shared<DecompilerController::AgentSnapshot>(
        decompilerController_ != nullptr
            ? decompilerController_->agentSnapshot()
            : DecompilerController::AgentSnapshot {});

    runtime_->client = std::make_unique<wuwe::openrouter_llm_client>(wuwe::llm_client_config {
        .base_url = toStdString(settings.baseUrl),
        .api_key = toStdString(settings.apiKey),
        .require_api_key = settings.requireApiKey,
        .model = toStdString(settings.model),
        .timeout = 30000,
        .referer_url = "https://www.cppmore.com/",
        .app_title = "ReArk"
    });
    runtime_->rearkProvider = std::make_shared<ReArkToolProvider>(snapshot);
    runtime_->knowledgeProvider = knowledgeController_ != nullptr
        ? knowledgeController_->createKnowledgeToolProvider()
        : nullptr;
    runtime_->provider = runtime_->knowledgeProvider != nullptr && runtime_->knowledgeProvider->provider != nullptr
        ? wuwe::compose_tool_providers(runtime_->rearkProvider, runtime_->knowledgeProvider->provider)
        : wuwe::compose_tool_providers(runtime_->rearkProvider);
    runtime_->stopSource = std::stop_source {};

    appendMessage(QStringLiteral("user"), trimmed);
    appendMessage(QStringLiteral("assistant"), {}, QStringLiteral("streaming"));
    setStatus(tr("Thinking..."));
    setRunning(true);

    QString systemPrompt =
        QStringLiteral("You are an expert HarmonyOS NEXT application reverse engineering assistant embedded in ReArk. "
            "Use ReArk tools whenever you need package, source, disassembly, resource, signature, or entry-point data. "
            "When user-provided reference documents are attached, use search_knowledge for external "
            "HarmonyOS, reverse engineering, security, or app analysis knowledge before giving detailed conclusions. "
            "Be concise, evidence-based, and mention when requested data is not loaded yet.");
    if (knowledgeController_ != nullptr && knowledgeController_->hasReadyReferences()) {
        systemPrompt += QStringLiteral(
            "\n\nAttached reference documents for this chat:\n%1"
            "\nWhen calling search_knowledge for these documents, always include filters "
            "{\"reark_session_id\":\"%2\"}.")
            .arg(knowledgeController_->referenceSummaryForPrompt(),
                 knowledgeController_->referenceSessionId());
    }
    systemPrompt += QStringLiteral("\n\nCurrent ReArk snapshot:\n%1")
        .arg(snapshot->packageSummary.isEmpty() ? QStringLiteral("<none>") : snapshot->packageSummary);

    QPointer<AgentController> self(this);

#ifdef REARK_HAS_WUWE_REASONING
    namespace reasoning = wuwe::agent::reasoning;

    auto onEvent = [self](const reasoning::reasoning_event& event) {
        if (!self) {
            return;
        }

        const QString status = reasoningEventStatus(event);
        if (!status.isEmpty()) {
            QMetaObject::invokeMethod(self.data(), [self, status] {
                if (!self) {
                    return;
                }
                self->setStatus(status);
            }, Qt::QueuedConnection);
        }
    };

    const std::stop_token reasoningStopToken = runtime_->stopSource.get_token();
    runtime_->reasoningRunner = std::make_unique<reasoning::reasoning_runner>(
        reasoning::make_default_agentic_runner(
            *runtime_->client,
            runtime_->provider,
            reasoning::default_agentic_runner_options {
                .model = toStdString(settings.model),
                .observer = std::move(onEvent),
                .should_cancel = [reasoningStopToken] {
                    return reasoningStopToken.stop_requested();
                }
            }));

    reasoning::reasoning_request request;
    request.input = toStdString(conversationInputForReasoning(messages_));
    request.system_prompt = toStdString(systemPrompt);
    request.model = toStdString(settings.model);
    request.temperature = 0.2;
    request.policy = reasoning::select_policy(reasoning::reasoning_task_description {
        .input = request.input,
        .has_tools = true,
        .requires_tools = true
    });
    request.metadata.emplace("host", "ReArk");
    request.metadata.emplace("target_summary", toStdString(boundedSnapshotText(snapshot->packageSummary, 2000)));

    reasoning::reasoning_run_options options;
    options.stop_token = runtime_->stopSource.get_token();
    options.callbacks.on_delta = [self](std::string_view delta) {
        if (!self) {
            return;
        }
        const QString chunk = fromStringView(delta);
        QMetaObject::invokeMethod(self.data(), [self, chunk] {
            if (self) {
                self->queueAssistantDelta(chunk);
            }
        }, Qt::QueuedConnection);
    };
    options.callbacks.on_done = [self](const reasoning::reasoning_result& result) {
        if (!self) {
            return;
        }
        const QString finalText = QString::fromStdString(result.content);
        const QString resultJson = dumpReasoningJson(reasoning::reasoning_result_to_json(result));
        const QString traceJson = dumpReasoningJson(reasoning::reasoning_trace_to_json(result.trace));
        const QString usageJson = dumpReasoningJson(reasoning::reasoning_usage_to_json(result.usage));
        QMetaObject::invokeMethod(self.data(), [self, finalText, resultJson, traceJson, usageJson] {
            if (!self) {
                return;
            }
            self->setReasoningDetails(resultJson, traceJson, usageJson);
            self->setRunning(false);
            self->finishActiveAssistantMessage(finalText.isEmpty()
                ? AgentController::tr("No response.")
                : finalText);
            self->setStatus(AgentController::tr("Ready"));
            self->resetRun();
        }, Qt::QueuedConnection);
    };
    options.callbacks.on_error = [self](const reasoning::reasoning_error& error) {
        if (!self) {
            return;
        }
        QString message = error.underlying_error
            ? agentErrorMessage(error.underlying_error, QString::fromStdString(error.message))
            : QString::fromStdString(error.message);
        if (message.isEmpty()) {
            message = AgentController::tr("Analysis failed.");
        }
        const QString resultJson = reasoningErrorToJson(error);
        QMetaObject::invokeMethod(self.data(), [self, message, resultJson] {
            if (!self) {
                return;
            }
            self->setReasoningDetails(resultJson, {}, {});
            self->setErrorMessage(message);
            self->flushPendingAssistantDelta();
            self->appendToActiveAssistantMessage(message);
            self->finishActiveAssistantMessage();
            self->setRunning(false);
            self->setStatus(message);
            self->resetRun();
        }, Qt::QueuedConnection);
    };
    options.callbacks.on_cancelled = [self](const reasoning::reasoning_result& result) {
        if (!self) {
            return;
        }
        const QString resultJson = dumpReasoningJson(reasoning::reasoning_result_to_json(result));
        const QString traceJson = dumpReasoningJson(reasoning::reasoning_trace_to_json(result.trace));
        const QString usageJson = dumpReasoningJson(reasoning::reasoning_usage_to_json(result.usage));
        QMetaObject::invokeMethod(self.data(), [self, resultJson, traceJson, usageJson] {
            if (!self) {
                return;
            }
            self->setReasoningDetails(resultJson, traceJson, usageJson);
            self->setStatus(AgentController::tr("Analysis cancelled."));
            self->finishActiveAssistantMessage(AgentController::tr("Analysis cancelled."));
            self->setRunning(false);
            self->resetRun();
        }, Qt::QueuedConnection);
    };

    runtime_->reasoningRun = runtime_->reasoningRunner->run_async(
        std::move(request),
        std::move(options));
    return;
#else
    runtime_->runner = std::make_unique<wuwe::llm_agent_runner>(
        *runtime_->client,
        runtime_->provider);

    wuwe::llm_request request;
    request.model = toStdString(settings.model);
    request.temperature = 0.2;
    request.messages.push_back({
        .role = "system",
        .content = toStdString(systemPrompt)
    });
    for (const QVariant& item : messages_) {
        const QVariantMap message = item.toMap();
        const QString role = message.value(QStringLiteral("role")).toString();
        const QString content = message.value(QStringLiteral("text")).toString();
        if (content.trimmed().isEmpty()
            || (role != QStringLiteral("user") && role != QStringLiteral("assistant"))) {
            continue;
        }
        request.messages.push_back({
            .role = role == QStringLiteral("user") ? "user" : "assistant",
            .content = toStdString(content)
        });
    }

    wuwe::llm_agent_run_options options;
    options.stop_token = runtime_->stopSource.get_token();
    options.callbacks.on_delta = [self](std::string_view text) {
        if (!self) {
            return;
        }
        const QString chunk = fromStringView(text);
        QMetaObject::invokeMethod(self.data(), [self, chunk] {
            if (self) {
                self->queueAssistantDelta(chunk);
            }
        }, Qt::QueuedConnection);
    };
    options.callbacks.on_tool_start = [self](const wuwe::llm_tool_call& call) {
        if (!self) {
            return;
        }
        const QString toolName = QString::fromStdString(call.name);
        QMetaObject::invokeMethod(self.data(), [self, toolName] {
            if (self) {
                self->setStatus(AgentController::tr("Running tool: %1").arg(toolName));
            }
        }, Qt::QueuedConnection);
    };
    options.callbacks.on_tool_result =
        [self](const wuwe::llm_tool_call& call, const wuwe::llm_tool_result& result) {
            if (!self) {
                return;
            }
            const QString toolName = QString::fromStdString(call.name);
            const bool ok = !result.error_code;
            QMetaObject::invokeMethod(self.data(), [self, toolName, ok] {
                if (self) {
                    self->setStatus(ok
                        ? AgentController::tr("Finished tool: %1").arg(toolName)
                        : AgentController::tr("Tool failed: %1").arg(toolName));
                }
            }, Qt::QueuedConnection);
        };
    options.callbacks.on_done = [self](const wuwe::llm_response&) {
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self] {
            if (self) {
                self->setRunning(false);
                self->finishActiveAssistantMessage(AgentController::tr("No response."));
                self->setStatus(AgentController::tr("Ready"));
                self->resetRun();
            }
        }, Qt::QueuedConnection);
    };
    options.callbacks.on_error =
        [self](std::error_code ec, std::string_view message) {
            if (!self) {
                return;
            }
            const QString msg = agentErrorMessage(ec, fromStringView(message));
            QMetaObject::invokeMethod(self.data(), [self, msg] {
                if (self) {
                    self->setErrorMessage(msg);
                    self->flushPendingAssistantDelta();
                    self->appendToActiveAssistantMessage(msg);
                    self->finishActiveAssistantMessage();
                    self->setStatus(msg);
                    self->setRunning(false);
                    self->resetRun();
                }
            }, Qt::QueuedConnection);
        };
    options.callbacks.on_cancelled = [self] {
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self] {
            if (self) {
                self->setStatus(AgentController::tr("Analysis cancelled."));
                self->finishActiveAssistantMessage(AgentController::tr("Analysis cancelled."));
                self->setRunning(false);
                self->resetRun();
            }
        }, Qt::QueuedConnection);
    };

    runtime_->run = runtime_->runner->run_async(std::move(request), std::move(options));
#endif
#endif
}

void AgentController::cancel()
{
    if (!available()) {
        setRunning(false);
        setStatus(unavailableMessage());
        return;
    }

#ifdef REARK_HAS_WUWE
#ifdef REARK_HAS_WUWE_REASONING
    if (runtime_->reasoningRun.has_value()) {
        runtime_->stopSource.request_stop();
        if (runtime_->reasoningRun->valid()) {
            runtime_->reasoningRun->request_stop();
        }
        setStatus(tr("Cancelling..."));
        return;
    }
#endif
    if (runtime_->run.has_value()) {
        runtime_->stopSource.request_stop();
        if (runtime_->run->valid()) {
            runtime_->run->request_stop();
        }
        setStatus(tr("Cancelling..."));
        return;
    }
#endif

    setRunning(false);
    setStatus(tr("Analysis cancelled."));
}

void AgentController::newChat()
{
    if (running_) {
        cancel();
        return;
    }
    resetRun();
    setRunning(false);
    clearMessages();
    clearReasoningDetails();
    if (knowledgeController_ != nullptr) {
        knowledgeController_->clearSessionReferences();
    }
    setErrorMessage({});
    setStatus(available() ? tr("Ready") : unavailableMessage());
}

void AgentController::copyTextToClipboard(const QString& text) const
{
    if (auto* clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}

void AgentController::setRunning(bool running)
{
    if (running_ == running) {
        return;
    }
    running_ = running;
    emit runningChanged();
}

void AgentController::setTranscript(const QString& transcript)
{
    if (transcript_ == transcript) {
        return;
    }
    transcript_ = transcript;
    emit transcriptChanged();
}

void AgentController::clearMessages()
{
    if (messages_.isEmpty() && transcript_.isEmpty() && activeAssistantMessage_ < 0) {
        return;
    }
    if (assistantDeltaTimer_ != nullptr) {
        assistantDeltaTimer_->stop();
    }
    pendingAssistantDelta_.clear();
    messages_.clear();
    activeAssistantMessage_ = -1;
    emit messagesChanged();
    setTranscript({});
}

void AgentController::clearReasoningDetails()
{
    setReasoningDetails({}, {}, {});
}

void AgentController::appendMessage(const QString& role, const QString& text, const QString& state)
{
    QVariantMap message;
    message.insert(QStringLiteral("role"), role);
    message.insert(QStringLiteral("text"), text);
    message.insert(QStringLiteral("state"), state);
    message.insert(QStringLiteral("time"), QTime::currentTime().toString(QStringLiteral("h:mm AP")));
    messages_.append(message);
    activeAssistantMessage_ = role == QStringLiteral("assistant")
        ? messages_.size() - 1
        : -1;
    emit messagesChanged();
    rebuildTranscript();
}

void AgentController::queueAssistantDelta(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }

    pendingAssistantDelta_ += text;
    if (pendingAssistantDelta_.size() >= 512) {
        flushPendingAssistantDelta();
        return;
    }
    if (assistantDeltaTimer_ != nullptr && !assistantDeltaTimer_->isActive()) {
        assistantDeltaTimer_->start();
    }
}

void AgentController::flushPendingAssistantDelta()
{
    if (pendingAssistantDelta_.isEmpty()) {
        return;
    }

    QString delta;
    std::swap(delta, pendingAssistantDelta_);
    appendToActiveAssistantMessage(delta);
}

void AgentController::appendToActiveAssistantMessage(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }
    if (activeAssistantMessage_ < 0 || activeAssistantMessage_ >= messages_.size()) {
        appendMessage(QStringLiteral("assistant"), text);
        return;
    }

    QVariantMap message = messages_.at(activeAssistantMessage_).toMap();
    message.insert(
        QStringLiteral("text"),
        message.value(QStringLiteral("text")).toString() + text);
    messages_[activeAssistantMessage_] = message;
    emit messagesChanged();
}

void AgentController::finishActiveAssistantMessage(const QString& fallbackText)
{
    if (assistantDeltaTimer_ != nullptr) {
        assistantDeltaTimer_->stop();
    }
    flushPendingAssistantDelta();

    if (activeAssistantMessage_ < 0 || activeAssistantMessage_ >= messages_.size()) {
        return;
    }

    QVariantMap message = messages_.at(activeAssistantMessage_).toMap();
    if (message.value(QStringLiteral("state")).toString() != QStringLiteral("streaming")) {
        activeAssistantMessage_ = -1;
        return;
    }
    if (!fallbackText.isEmpty()
        && message.value(QStringLiteral("text")).toString().trimmed().isEmpty()) {
        message.insert(QStringLiteral("text"), fallbackText);
    }
    message.insert(QStringLiteral("state"), QStringLiteral("done"));
    messages_[activeAssistantMessage_] = message;
    activeAssistantMessage_ = -1;
    emit messagesChanged();
    rebuildTranscript();
}

void AgentController::rebuildTranscript()
{
    QString text;
    for (const QVariant& item : messages_) {
        const QVariantMap message = item.toMap();
        const QString role = message.value(QStringLiteral("role")).toString();
        const QString content = message.value(QStringLiteral("text")).toString();
        if (!text.isEmpty()) {
            text += QStringLiteral("\n\n");
        }
        text += role == QStringLiteral("user") ? QStringLiteral("You:\n") : QStringLiteral("Assistant:\n");
        text += content;
    }
    setTranscript(text);
}

void AgentController::appendTranscript(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }
    transcript_ += text;
    emit transcriptChanged();
}

void AgentController::setErrorMessage(const QString& errorMessage)
{
    if (errorMessage_ == errorMessage) {
        return;
    }
    errorMessage_ = errorMessage;
    emit errorMessageChanged();
}

void AgentController::setStatus(const QString& status)
{
    if (status_ == status) {
        return;
    }
    status_ = status;
    emit statusChanged();
}

void AgentController::setReasoningDetails(
    const QString& resultJson,
    const QString& traceJson,
    const QString& usageJson)
{
    if (reasoningResultJson_ == resultJson
        && reasoningTraceJson_ == traceJson
        && reasoningUsageJson_ == usageJson) {
        return;
    }

    reasoningResultJson_ = resultJson;
    reasoningTraceJson_ = traceJson;
    reasoningUsageJson_ = usageJson;
    emit reasoningDetailsChanged();
}

void AgentController::resetRun()
{
    if (assistantDeltaTimer_ != nullptr) {
        assistantDeltaTimer_->stop();
    }
    pendingAssistantDelta_.clear();

#ifdef REARK_HAS_WUWE
#ifdef REARK_HAS_WUWE_REASONING
    if (runtime_->reasoningRun.has_value()) {
        runtime_->stopSource.request_stop();
        if (runtime_->reasoningRun->valid()) {
            runtime_->reasoningRun->request_stop();
            runtime_->reasoningRun->wait();
        }
        runtime_->reasoningRun.reset();
    }
    runtime_->reasoningRunner.reset();
#endif
    if (runtime_->run.has_value()) {
        runtime_->stopSource.request_stop();
        if (runtime_->run->valid()) {
            runtime_->run->request_stop();
        }
        runtime_->run.reset();
    }
    runtime_->runner.reset();
    runtime_->provider.reset();
    runtime_->knowledgeProvider.reset();
    runtime_->rearkProvider.reset();
    runtime_->client.reset();
    runtime_->stopSource = std::stop_source {};
#endif
}

QString AgentController::unavailableMessage() const
{
    return tr("Smart analysis is not available in this ReArk build.");
}
