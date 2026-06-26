#include "controller/AgentController.h"

#ifdef REARK_HAS_WUWE
#include <wuwe/agent/llm/llm_agent_runner.h>
#include <wuwe/agent/llm/llm_client.h>
#include <wuwe/agent/llm/llm_config.h>
#include <wuwe/agent/llm/llm_provider_factory.h>
#include <wuwe/agent/llm/llm_provider_registry.h>
#include <wuwe/agent/tools/tool.hpp>
#if !defined(REARK_DISABLE_WUWE_EXECUTION) && __has_include(<wuwe/agent/execution/execution.hpp>)
#include <wuwe/agent/execution/execution.hpp>
#define REARK_HAS_WUWE_EXECUTION 1
#endif
#if __has_include(<wuwe/agent/reasoning/reasoning.hpp>)
#include <wuwe/agent/reasoning/reasoning.hpp>
#define REARK_HAS_WUWE_REASONING 1
#endif
#endif

#include "controller/AgentSettings.h"
#include "controller/AgentKnowledgeController.h"
#include "controller/DecompilerController.h"
#include "controller/PythonRuntimeResolver.h"
#include "controller/SigningSettings.h"
#include "device/HdcDeviceBackend.h"
#include "device/HarmonyPackageRewriter.h"
#include "device/InstallablePackageResolver.h"
#include "device/UiAutomationBackend.h"
#include "model/AgentMessageModel.h"
#include "signing/HarmonyHapSigner.h"
#include "signing/SigningMaterialInspector.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QTime>
#include <QTimer>
#include <QVariantMap>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace {

#ifdef REARK_HAS_WUWE

QString toolRoundBudgetExceededMessage()
{
    return AgentController::tr("Agent tool call rounds were exhausted before a final answer was produced.");
}

bool isToolRoundBudgetExceededText(const QString& value)
{
    const QString folded = value.trimmed().toCaseFolded();
    if (folded.isEmpty()) {
        return false;
    }

    return folded.contains(QStringLiteral("tool_round_budget_exceeded"))
        || folded.contains(QStringLiteral("agent_loop_budget_exceeded"))
        || folded.contains(QStringLiteral("tool round budget exceeded"))
        || folded.contains(QStringLiteral("agent tool round budget exceeded"))
        || (folded.contains(QStringLiteral("tool"))
            && folded.contains(QStringLiteral("round"))
            && folded.contains(QStringLiteral("budget")));
}

bool isLegacyToolRoundBudgetError(std::error_code ec)
{
    return ec == std::make_error_code(std::errc::resource_unavailable_try_again);
}

bool isScriptToolName(const std::string& name)
{
    return name == "run_analysis_script";
}

QString toolDisplayName(const std::string& name)
{
    if (name == "summarize_current_target") {
        return AgentController::tr("current target summary");
    }
    if (name == "list_files") {
        return AgentController::tr("file index");
    }
    if (name == "search_loaded_content") {
        return AgentController::tr("loaded source and disassembly");
    }
    if (name == "read_source") {
        return AgentController::tr("source or resource file");
    }
    if (name == "read_disassembly") {
        return AgentController::tr("source-file disassembly");
    }
    if (name == "read_abc_literal") {
        return AgentController::tr("ABC literal");
    }
    if (name == "search_abc_strings") {
        return AgentController::tr("ABC strings");
    }
    if (name == "read_abc_tree") {
        return AgentController::tr("ABC tree");
    }
    if (name == "find_abc_xrefs") {
        return AgentController::tr("ABC cross-references");
    }
    if (name == "find_abc_call_argument_flows") {
        return AgentController::tr("ABC call argument flows");
    }
    if (name == "inspect_entry_points") {
        return AgentController::tr("entry points");
    }
    if (name == "read_signature_summary") {
        return AgentController::tr("signature summary");
    }
    if (name == "list_harmony_devices") {
        return AgentController::tr("HarmonyOS devices");
    }
    if (name == "install_current_hap") {
        return AgentController::tr("HAP install");
    }
    if (name == "start_harmony_app") {
        return AgentController::tr("HarmonyOS app launch");
    }
    if (name == "read_hilog") {
        return AgentController::tr("device hilog");
    }
    if (name == "capture_device_screenshot") {
        return AgentController::tr("device screenshot");
    }
    if (name == "dump_ui_layout") {
        return AgentController::tr("UI layout");
    }
    if (name == "tap_ui") {
        return AgentController::tr("UI tap");
    }
    if (name == "tap_ui_text") {
        return AgentController::tr("UI text tap");
    }
    if (name == "input_ui_text") {
        return AgentController::tr("UI text input");
    }
    if (name == "press_device_key") {
        return AgentController::tr("device key");
    }
    if (name == "swipe_device") {
        return AgentController::tr("UI swipe");
    }
    if (isScriptToolName(name)) {
        return AgentController::tr("local analysis script");
    }
    return QString::fromStdString(name);
}

std::string toStdString(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString fromStringView(std::string_view value)
{
    return QString::fromUtf8(value.data(), qsizetype(value.size()));
}

std::shared_ptr<wuwe::llm_client> createLlmClient(const AgentSettings& settings)
{
    const QString provider = settings.provider.trimmed();
    const std::string providerId = toStdString(provider);
    wuwe::llm_client_config config {
        .base_url = toStdString(settings.baseUrl),
        .api_key = toStdString(settings.apiKey),
        .require_api_key = settings.requireApiKey,
        .model = toStdString(settings.model),
        .timeout = 120000,
        .stream_timeouts = {
            .total_ms = 300000,
            .connect_ms = 15000,
            .first_event_ms = 45000,
            .idle_ms = 45000,
        },
        .referer_url = "https://www.cppmore.com/",
        .app_title = "ReArk"
    };

    auto normalized = wuwe::normalize_llm_client_config(providerId, std::move(config));
    if (!normalized) {
        throw std::invalid_argument("unknown Wuwe LLM provider: " + providerId);
    }

    auto client = wuwe::make_llm_client(providerId, std::move(*normalized));
    if (!client) {
        throw std::invalid_argument("failed to create Wuwe LLM provider: " + providerId);
    }
    return client;
}

#ifdef REARK_HAS_WUWE_EXECUTION
constexpr std::size_t kMaxAnalysisScriptCodeBytes = 32 * 1024;
constexpr std::size_t kMaxAnalysisScriptStdinBytes = 256 * 1024;
constexpr std::size_t kMaxAnalysisScriptArgumentsBytes =
    kMaxAnalysisScriptCodeBytes + kMaxAnalysisScriptStdinBytes + 4096;
constexpr int kMaxAnalysisScriptTimeoutMs = 5000;

wuwe::agent::execution::execution_policy rearkExecutionPolicy(const std::filesystem::path& workdir)
{
    namespace execution = wuwe::agent::execution;

    execution::execution_policy policy;
    policy.allowed_languages = { execution::execution_language::python };
    policy.default_workdir = workdir;
    policy.max_limits = {
        .timeout = std::chrono::milliseconds { kMaxAnalysisScriptTimeoutMs },
        .max_stdout_bytes = 65536,
        .max_stderr_bytes = 65536,
        .max_code_bytes = kMaxAnalysisScriptCodeBytes,
        .max_stdin_bytes = kMaxAnalysisScriptStdinBytes,
        .max_total_input_bytes = kMaxAnalysisScriptCodeBytes + kMaxAnalysisScriptStdinBytes
    };
    policy.allow_network = false;
    policy.allow_file_read = false;
    policy.allow_file_write = false;
    policy.allow_shell = false;
    policy.require_approval_for_network = true;
    policy.require_approval_for_file_write = true;
    policy.require_approval_for_shell = true;
    policy.allowed_env = {};
    return policy;
}

struct ReArkExecutionBackendSelection {
    std::unique_ptr<wuwe::agent::execution::execution_backend> backend;
    QString promptNote;
};

QString sandboxBackendUnavailableText(
    const std::optional<wuwe::agent::sandbox::sandbox_backend_info>& info,
    const QString& fallback)
{
    if (!info.has_value()) {
        return fallback;
    }
    if (!info->unavailable_reason.empty()) {
        return QString::fromStdString(info->unavailable_reason);
    }
    if (!info->available) {
        return QStringLiteral("%1 backend is unavailable.").arg(QString::fromStdString(info->name));
    }
    return fallback;
}

ReArkExecutionBackendSelection makeReArkExecutionBackend(
    const AgentSettings& settings,
    const PythonRuntimeProbe& pythonRuntime,
    const std::filesystem::path& workdir)
{
    namespace execution = wuwe::agent::execution;
    namespace sandbox = wuwe::agent::sandbox;

    execution::controlled_process_backend_config controlledConfig {
        .python_interpreter = PythonRuntimeResolver::toFilesystemPath(pythonRuntime.resolvedPath),
        .fallback_workdir = workdir,
        .use_job_object = true,
        .validate_python_on_start = true,
        .python_startup_timeout = std::chrono::milliseconds { 3000 }
    };

    execution::restricted_process_backend_config restrictedConfig;
    restrictedConfig.python_interpreter = controlledConfig.python_interpreter;
    restrictedConfig.fallback_workdir = workdir;
    restrictedConfig.runtime_staging_root = workdir / "restricted-runtime";
    restrictedConfig.readable_roots = { workdir };
    restrictedConfig.writable_roots = { workdir };
    restrictedConfig.deny_network = true;
    restrictedConfig.use_job_object = true;
    restrictedConfig.inherit_parent_environment = false;
    restrictedConfig.cleanup_runtime_staging = true;
    restrictedConfig.python_startup_timeout = std::chrono::milliseconds { 3000 };

    execution::execution_backend_registry_options registryOptions {
        .controlled_process = std::move(controlledConfig),
        .enable_restricted_process_backend = settings.enableRestrictedPythonBackend,
        .restricted_process = std::move(restrictedConfig)
    };
    auto registry = execution::make_execution_backend_registry(std::move(registryOptions));

    ReArkExecutionBackendSelection selection;
    if (settings.enableRestrictedPythonBackend) {
        execution::execution_backend_requirements requirements;
        requirements.isolation = sandbox::isolation_level::restricted_process;
        requirements.require_shell_disabled = true;
        requirements.require_timeout = true;
        requirements.require_cancellation = true;
        requirements.require_stdout_limit = true;
        requirements.require_stderr_limit = true;
        requirements.require_environment_allowlist = true;
        requirements.require_process_tree_cleanup = true;
        requirements.require_filesystem_read_deny = true;
        requirements.require_filesystem_write_deny = true;
        requirements.require_network_deny = true;

        selection.backend = registry.create_best(requirements);
        if (selection.backend != nullptr) {
            const auto info = selection.backend->info();
            selection.promptNote = QStringLiteral(
                "Local Python analysis uses Wuwe restricted_process. File access is limited to the temporary execution workdir, network access is denied, and backend result metadata reports enforcement status.");
            if (!info.available && !info.unavailable_reason.empty()) {
                selection.promptNote += QStringLiteral(" Backend availability note: %1")
                    .arg(QString::fromStdString(info.unavailable_reason));
            }
            return selection;
        }

        selection.promptNote = QStringLiteral(
            "Local Python analysis is unavailable because Windows restricted_process was requested but no backend satisfying filesystem and network deny requirements is available: %1")
            .arg(sandboxBackendUnavailableText(
                registry.describe("restricted_process"),
                QStringLiteral("restricted_process backend unavailable")));
        return selection;
    }

    selection.backend = registry.create("controlled_process");
    if (selection.backend != nullptr) {
        selection.promptNote = QStringLiteral(
            "Local Python analysis uses Wuwe controlled_process with bounded output, timeout, environment allowlist, and process cleanup where supported. It is not a file or network security sandbox.");
    } else {
        selection.promptNote = QStringLiteral("Local Python analysis is unavailable because controlled_process backend could not be created.");
    }
    return selection;
}

void applyAnalysisScriptSchemaLimits(wuwe::llm_tool& tool)
{
    if (!isScriptToolName(tool.name) || tool.parameters_json_schema.empty()) {
        return;
    }

    try {
        auto schema = nlohmann::json::parse(tool.parameters_json_schema);
        if (!schema.is_object()) {
            return;
        }

        schema["additionalProperties"] = false;
        auto& properties = schema["properties"];
        if (properties.is_object()) {
            if (auto code = properties.find("code"); code != properties.end() && code->is_object()) {
                (*code)["maxLength"] = kMaxAnalysisScriptCodeBytes;
            }
            if (auto stdinText = properties.find("stdin_text");
                stdinText != properties.end() && stdinText->is_object()) {
                (*stdinText)["maxLength"] = kMaxAnalysisScriptStdinBytes;
            }
            if (auto timeoutMs = properties.find("timeout_ms");
                timeoutMs != properties.end() && timeoutMs->is_object()) {
                (*timeoutMs)["minimum"] = 1;
                (*timeoutMs)["maximum"] = kMaxAnalysisScriptTimeoutMs;
            }
        }

        tool.parameters_json_schema = schema.dump();
    } catch (const std::exception&) {
    }
}

std::string analysisScriptArgumentError(const std::string& argumentsJson)
{
    if (argumentsJson.size() > kMaxAnalysisScriptArgumentsBytes) {
        return "run_analysis_script rejected: arguments JSON is "
            + std::to_string(argumentsJson.size())
            + " bytes, exceeding the ReArk host limit of "
            + std::to_string(kMaxAnalysisScriptArgumentsBytes)
            + " bytes.";
    }

    nlohmann::json arguments;
    try {
        arguments = nlohmann::json::parse(argumentsJson);
    } catch (const std::exception& ex) {
        return std::string("Invalid run_analysis_script arguments: ") + ex.what();
    }

    if (!arguments.is_object()) {
        return "Invalid run_analysis_script arguments: expected a JSON object.";
    }

    for (const auto& item : arguments.items()) {
        const std::string& key = item.key();
        if (key != "code" && key != "stdin_text" && key != "timeout_ms") {
            return "Invalid run_analysis_script arguments: unsupported parameter '" + key
                + "'. Only code, stdin_text, and timeout_ms are allowed.";
        }
    }

    return {};
}

class ReArkExecutionToolProvider {
public:
    explicit ReArkExecutionToolProvider(
        std::shared_ptr<wuwe::agent::execution::execution_tool_provider> provider)
        : provider_(std::move(provider))
    {
    }

    [[nodiscard]] std::vector<wuwe::llm_tool> tools() const
    {
        std::vector<wuwe::llm_tool> result = provider_->tools();
        for (wuwe::llm_tool& tool : result) {
            if (!isScriptToolName(tool.name)) {
                continue;
            }
            tool.description += " Host limits: code <= "
                + std::to_string(kMaxAnalysisScriptCodeBytes)
                + " bytes, stdin_text <= "
                + std::to_string(kMaxAnalysisScriptStdinBytes)
                + " bytes, timeout_ms 1-"
                + std::to_string(kMaxAnalysisScriptTimeoutMs)
                + ".";
            applyAnalysisScriptSchemaLimits(tool);
        }
        return result;
    }

    [[nodiscard]] wuwe::llm_tool_result invoke(
        const std::string& name,
        const std::string& argumentsJson,
        std::stop_token stopToken) const
    {
        if (!isScriptToolName(name)) {
            return provider_->invoke(name, argumentsJson, stopToken);
        }

        const std::string error = analysisScriptArgumentError(argumentsJson);
        if (!error.empty()) {
            return {
                .content = error,
                .error_code = wuwe::agent::make_error_code(
                    wuwe::agent::llm_error_code::invalid_tool_arguments)
            };
        }
        return provider_->invoke(name, argumentsJson, stopToken);
    }

private:
    std::shared_ptr<wuwe::agent::execution::execution_tool_provider> provider_;
};
#endif

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

QString responseLanguageInstruction(const QString& question)
{
    int latinLetters = 0;
    int cjkCharacters = 0;
    for (const QChar ch : question) {
        const ushort unicode = ch.unicode();
        if ((unicode >= u'A' && unicode <= u'Z') || (unicode >= u'a' && unicode <= u'z')) {
            ++latinLetters;
        } else if ((unicode >= 0x3400 && unicode <= 0x9fff)
            || (unicode >= 0xf900 && unicode <= 0xfaff)) {
            ++cjkCharacters;
        }
    }

    if (latinLetters >= 3 && cjkCharacters == 0) {
        return QStringLiteral(
            "\n\nResponse language contract:\n"
            "- The user's latest question is in English.\n"
            "- Answer in English. Do not answer in Chinese because of the UI language, tool output, or target metadata.\n"
            "- Keep identifiers, package names, file paths, API names, and quoted source text unchanged.");
    }
    if (cjkCharacters > 0 && latinLetters < cjkCharacters * 2) {
        return QStringLiteral(
            "\n\nResponse language contract:\n"
            "- The user's latest question is in Chinese.\n"
            "- Answer in Chinese.\n"
            "- Keep identifiers, package names, file paths, API names, and quoted source text unchanged.");
    }

    return QStringLiteral(
        "\n\nResponse language contract:\n"
        "- Answer in the dominant natural language of the user's latest question.\n"
        "- Ignore the UI language and tool-output language when choosing the response language.\n"
        "- Keep identifiers, package names, file paths, API names, and quoted source text unchanged.");
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

QString normalizedLookupText(QString value)
{
    value = value.trimmed();
    while ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
        || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))
        || (value.startsWith(QLatin1Char('`')) && value.endsWith(QLatin1Char('`')))) {
        value = value.mid(1, value.size() - 2).trimmed();
    }
    value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (value.startsWith(QStringLiteral("./"))) {
        value = value.mid(2);
    }
    while (value.startsWith(QLatin1Char('/'))) {
        value = value.mid(1);
    }
    return value.toCaseFolded();
}

int snapshotFileScore(const DecompilerController::AgentFileSnapshot& file, const QString& query)
{
    const QString foldedQuery = normalizedLookupText(query);
    if (foldedQuery.isEmpty()) {
        return 10;
    }

    const QString foldedPath = normalizedLookupText(file.path);
    const QString foldedName = normalizedLookupText(file.name);
    int score = -1;
    if (foldedPath == foldedQuery || foldedName == foldedQuery) {
        score = 1000;
    } else if (foldedPath.endsWith(QLatin1Char('/') + foldedQuery)) {
        score = 850;
    } else if (foldedPath.endsWith(foldedQuery)) {
        score = 760;
    } else if (foldedName.contains(foldedQuery)) {
        score = 620;
    } else if (foldedPath.contains(foldedQuery)) {
        score = 500;
    } else if (matchesQuery(fileSearchText(file), query)) {
        score = 100;
    }

    if (score < 0) {
        return score;
    }
    if (file.loaded) {
        score += 20;
    }
    if (file.hasDisassembly) {
        score += 12;
    }
    if (file.disassemblyLoaded) {
        score += 12;
    }
    return score;
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
    const DecompilerController::AgentFileSnapshot* best = nullptr;
    int bestScore = -1;

    for (const auto& file : snapshot.files) {
        const int score = snapshotFileScore(file, query);
        if (score > bestScore) {
            best = &file;
            bestScore = score;
        }
    }

    return best;
}

QString snapshotFileCandidates(const DecompilerController::AgentSnapshot& snapshot, const QString& query, int limit = 8)
{
    struct Candidate {
        const DecompilerController::AgentFileSnapshot* file = nullptr;
        int score = -1;
    };

    QVector<Candidate> candidates;
    candidates.reserve(snapshot.files.size());
    for (const auto& file : snapshot.files) {
        const int score = snapshotFileScore(file, query);
        if (score < 0) {
            continue;
        }
        candidates.push_back({ &file, score });
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        return lhs.score > rhs.score;
    });

    QString result;
    const int count = std::min(limit, static_cast<int>(candidates.size()));
    for (int i = 0; i < count; ++i) {
        result += QStringLiteral("- %1\n").arg(formatSnapshotFileLine(*candidates.at(i).file));
    }
    return result;
}

QString readSnapshotSource(
    const DecompilerController::AgentSnapshot& snapshot,
    const QString& query,
    int maxChars,
    std::stop_token stopToken)
{
    const auto* file = bestSnapshotFile(snapshot, query);
    if (file == nullptr) {
        return QStringLiteral(
            "# status: error\n"
            "# code: file_not_found\n"
            "# query: %1\n"
            "# message: No source or resource file matched the query.\n"
            "Try list_files first to inspect available paths.").arg(query);
    }

    if (file->loaded && !file->content.isEmpty()) {
        QString text;
        text += QStringLiteral("# status: ok\n");
        text += QStringLiteral("# file: %1\n").arg(file->path);
        text += QStringLiteral("# kind: %1\n").arg(file->kind);
        text += QStringLiteral("# section: %1\n").arg(file->section);
        text += QStringLiteral("# content_mode: %1\n\n").arg(file->contentMode);
        text += file->content;
        return boundedSnapshotText(text, maxChars);
    }

    if (!snapshot.packageContext) {
        QString message = QStringLiteral(
            "# status: error\n"
            "# code: no_active_session\n"
            "# matched file: %1\n"
            "# reason: no active Hyle session is available for on-demand Agent reads.\n"
            "Closest candidates:\n%2")
            .arg(file->path, snapshotFileCandidates(snapshot, query));
        return boundedSnapshotText(message, maxChars);
    }

    HyleDecompiler::SourceResult result;
    if (file->section == QStringLiteral("resource")) {
        result = HyleDecompiler::readResourceContent(
            snapshot.packageContext,
            -1,
            file->hyleId,
            file->name,
            stopToken,
            file->packageId);
    } else if (file->section == QStringLiteral("signature")) {
        result = HyleDecompiler::readSignatureContent(
            snapshot.packageContext,
            -1,
            file->name,
            file->packageId);
    } else if (file->section == QStringLiteral("summary")) {
        result = HyleDecompiler::readSummaryContent(
            snapshot.packageContext,
            -1,
            file->name,
            stopToken,
            file->packageId);
    } else {
        result = HyleDecompiler::decompileSourceFile(
            snapshot.packageContext,
            -1,
            file->hyleId,
            file->name,
            stopToken,
            file->packageId);
    }

    if (!result.error.isEmpty()) {
        QString message = QStringLiteral(
            "# status: error\n"
            "# code: source_read_failed\n"
            "# matched file: %1\n"
            "# error: %2\n"
            "Closest candidates:\n%3")
            .arg(file->path, result.error, snapshotFileCandidates(snapshot, query));
        return boundedSnapshotText(message, maxChars);
    }

    QString text;
    text += QStringLiteral("# status: ok\n");
    text += QStringLiteral("# file: %1\n").arg(file->path);
    text += QStringLiteral("# kind: %1\n").arg(result.kind.isEmpty() ? file->kind : result.kind);
    text += QStringLiteral("# section: %1\n").arg(file->section);
    text += QStringLiteral("# content_mode: %1\n").arg(result.contentMode.isEmpty() ? file->contentMode : result.contentMode);
    if (!result.binaryContent.isEmpty()) {
        text += QStringLiteral("# binary_size: %1 bytes\n").arg(result.binaryContent.size());
    }
    if (!result.diagnostics.isEmpty()) {
        text += QStringLiteral("# diagnostics:\n%1\n").arg(result.diagnostics);
    }
    text += QStringLiteral("\n");
    if (!result.content.isEmpty()) {
        text += result.content;
    } else {
        text += QStringLiteral(
            "[non-text content is available to ReArk, but this Agent tool returns text. "
            "Use list_files to inspect metadata or ask for resources by path.]");
    }
    return boundedSnapshotText(text, maxChars);
}

QString readSnapshotDisassembly(
    const DecompilerController::AgentSnapshot& snapshot,
    const QString& query,
    int maxChars,
    std::stop_token stopToken)
{
    const auto* file = bestSnapshotFile(snapshot, query);
    if (file == nullptr) {
        return QStringLiteral(
            "# status: error\n"
            "# code: file_not_found\n"
            "# query: %1\n"
            "# message: No source file matched the query.\n"
            "Try list_files first to inspect available source paths.").arg(query);
    }
    if (!file->hasDisassembly) {
        QString text = QStringLiteral(
            "# status: error\n"
            "# code: disassembly_unsupported\n"
            "# matched file: %1\n"
            "# reason: this file has no source-file disassembly in the current ReArk snapshot.\n")
            .arg(file->path);
        if (file->loaded && !file->content.isEmpty()) {
            text += QStringLiteral("\n# loaded source fallback\n\n");
            text += file->content;
        } else {
            text += QStringLiteral("\nClosest candidates:\n%1").arg(snapshotFileCandidates(snapshot, query));
        }
        return boundedSnapshotText(text, maxChars);
    }
    if (file->disassemblyLoaded && !file->disassembly.isEmpty()) {
        QString text;
        text += QStringLiteral("# status: ok\n");
        text += QStringLiteral("# disassembly: %1\n\n").arg(file->path);
        text += file->disassembly;
        return boundedSnapshotText(text, maxChars);
    }

    if (!snapshot.packageContext) {
        QString text = QStringLiteral(
            "# status: error\n"
            "# code: no_active_session\n"
            "# matched file: %1\n"
            "# reason: no active Hyle session is available for on-demand Agent disassembly.\n"
            "Closest candidates:\n%2")
            .arg(file->path, snapshotFileCandidates(snapshot, query));
        return boundedSnapshotText(text, maxChars);
    }

    const auto result = HyleDecompiler::disassembleSourceFileText(
        snapshot.packageContext,
        -1,
        file->hyleId,
        file->name,
        stopToken,
        file->packageId);
    if (!result.error.isEmpty()) {
        QString text = QStringLiteral(
            "# status: error\n"
            "# code: disassembly_read_failed\n"
            "# matched file: %1\n"
            "# error: %2\n")
            .arg(file->path, result.error);
        if (file->loaded && !file->content.isEmpty()) {
            text += QStringLiteral("\n# loaded source fallback\n\n");
            text += file->content;
        } else {
            text += QStringLiteral("\nClosest candidates:\n%1").arg(snapshotFileCandidates(snapshot, query));
        }
        return boundedSnapshotText(text, maxChars);
    }

    QString text;
    text += QStringLiteral("# status: ok\n");
    text += QStringLiteral("# disassembly: %1\n\n").arg(file->path);
    text += result.content;
    return boundedSnapshotText(text, maxChars);
}

HdcDeviceBackend agentDeviceBackend()
{
    return HdcDeviceBackend();
}

UiAutomationBackend agentUiBackend()
{
    return UiAutomationBackend(agentDeviceBackend());
}

std::string deviceCommandToolContent(
    const QString& title,
    const CommandResult& result,
    const QString& extra = {},
    bool success = false,
    bool useExplicitSuccess = false)
{
    const bool ok = useExplicitSuccess ? success : result.succeeded();
    QString text;
    text += QStringLiteral("# status: %1\n").arg(ok ? QStringLiteral("ok") : QStringLiteral("error"));
    text += QStringLiteral("# operation: %1\n").arg(title);
    if (!extra.isEmpty()) {
        text += extra.trimmed() + QStringLiteral("\n");
    }
    text += HdcDeviceBackend::resultSummary(result);
    return toStdString(boundedSnapshotText(text, 24000));
}

bool isMissingSignatureInstallFailure(const CommandResult& result)
{
    const QString text = QStringLiteral("%1\n%2\n%3")
        .arg(result.standardOutput, result.standardError, result.errorMessage)
        .toCaseFolded();
    return text.contains(QStringLiteral("no signature file"))
        || text.contains(QStringLiteral("missing signature"))
        || text.contains(QStringLiteral("signature file not found"))
        || text.contains(QStringLiteral("not signed"))
        || text.contains(QStringLiteral("verify signature"))
        || text.contains(QStringLiteral("signature verify"));
}

bool signatureSummaryLooksUnsigned(const QString& summary)
{
    const QString folded = summary.toCaseFolded();
    static const QRegularExpression signedFalsePattern(
        QStringLiteral("(^|\\n)\\s*signed\\s*:\\s*(false|no|0)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return signedFalsePattern.match(summary).hasMatch()
        || folded.contains(QStringLiteral("status: unsigned"))
        || folded.contains(QStringLiteral("unsigned"))
        || folded.contains(QStringLiteral("not signed"))
        || folded.contains(QStringLiteral("no signature"))
        || folded.contains(QStringLiteral("未签名"));
}

QString signingSettingsStatusLine(const QString& validationMessage)
{
    return validationMessage.trimmed().isEmpty()
        ? QStringLiteral("# signing_settings: configured")
        : QStringLiteral("# signing_settings: invalid\n# signing_settings_error: %1").arg(validationMessage);
}

QString agentSignedHapFileName(const QString& sourcePath)
{
    const QFileInfo info(sourcePath);
    QString baseName = info.completeBaseName().trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("reark-agent");
    }
    return baseName + QStringLiteral("-signed.hap");
}

QString agentRewrittenHapFileName(const QString& sourcePath)
{
    const QFileInfo info(sourcePath);
    QString baseName = info.completeBaseName().trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("reark-agent");
    }
    return baseName + QStringLiteral("-rebundle-unsigned.hap");
}

QString packageBundleNameFromSummary(const QString& summary)
{
    static const QRegularExpression bundleNamePattern(
        QStringLiteral("\"bundleName\"\\s*:\\s*\"([^\"]+)\""));
    QRegularExpressionMatch match = bundleNamePattern.match(summary);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    static const QRegularExpression profileBundleNamePattern(
        QStringLiteral("\"bundle-name\"\\s*:\\s*\"([^\"]+)\""));
    match = profileBundleNamePattern.match(summary);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString packageCompatibleVersionFromSummary(const QString& summary)
{
    static const QRegularExpression compatiblePattern(
        QStringLiteral("\"compatible\"\\s*:\\s*(\\d+)"));
    const QRegularExpressionMatch match = compatiblePattern.match(summary);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

std::string installWithAutoSignToolContent(
    const CommandResult& initialInstall,
    const CommandResult& signResult,
    const CommandResult& signedInstall,
    const QString& extra)
{
    const bool signedInstallOk = HdcDeviceBackend::installSucceeded(signedInstall);
    QString text;
    text += QStringLiteral("# status: %1\n").arg(signedInstallOk ? QStringLiteral("ok") : QStringLiteral("error"));
    text += QStringLiteral("# operation: install_current_hap\n");
    if (!extra.trimmed().isEmpty()) {
        text += extra.trimmed() + QStringLiteral("\n");
    }
    text += QStringLiteral("\n[initial install]\n%1\n").arg(HdcDeviceBackend::resultSummary(initialInstall));
    text += QStringLiteral("\n[sign]\n%1\n").arg(HarmonyHapSigner::resultSummary(signResult));
    if (signedInstall.started || !signedInstall.program.isEmpty()) {
        text += QStringLiteral("\n[signed install]\n%1").arg(HdcDeviceBackend::resultSummary(signedInstall));
    }
    return toStdString(boundedSnapshotText(text, 30000));
}

std::string installWithRewriteSignToolContent(
    const CommandResult& initialInstall,
    const HarmonyBundleRewriteResult& rewriteResult,
    const CommandResult& signResult,
    const CommandResult& signedInstall,
    const QString& extra)
{
    const bool signedInstallOk = HdcDeviceBackend::installSucceeded(signedInstall);
    QString text;
    text += QStringLiteral("# status: %1\n").arg(signedInstallOk ? QStringLiteral("ok") : QStringLiteral("error"));
    text += QStringLiteral("# operation: install_current_hap\n");
    if (!extra.trimmed().isEmpty()) {
        text += extra.trimmed() + QStringLiteral("\n");
    }
    text += QStringLiteral("\n[initial install]\n%1\n").arg(HdcDeviceBackend::resultSummary(initialInstall));
    text += QStringLiteral("\n[bundle rewrite]\n");
    text += QStringLiteral("# status: %1\n").arg(rewriteResult.ok ? QStringLiteral("ok") : QStringLiteral("error"));
    text += QStringLiteral("# input_hap: %1\n").arg(rewriteResult.inputHapPath);
    text += QStringLiteral("# output_hap: %1\n").arg(rewriteResult.outputHapPath);
    if (!rewriteResult.unpackedDirectory.trimmed().isEmpty() && !rewriteResult.ok) {
        text += QStringLiteral("# unpacked_dir: %1\n").arg(rewriteResult.unpackedDirectory);
    }
    if (!rewriteResult.error.trimmed().isEmpty()) {
        text += QStringLiteral("# error: %1\n").arg(rewriteResult.error.trimmed());
    }
    if (!rewriteResult.report.trimmed().isEmpty()) {
        text += QStringLiteral("\n%1\n").arg(rewriteResult.report.trimmed());
    }
    if (rewriteResult.packingResult.started || !rewriteResult.packingResult.program.isEmpty()) {
        text += QStringLiteral("\n[pack]\n%1\n").arg(HarmonyHapSigner::resultSummary(rewriteResult.packingResult));
    }
    text += QStringLiteral("\n[sign]\n%1\n").arg(HarmonyHapSigner::resultSummary(signResult));
    if (signedInstall.started || !signedInstall.program.isEmpty()) {
        text += QStringLiteral("\n[signed install]\n%1").arg(HdcDeviceBackend::resultSummary(signedInstall));
    }
    return toStdString(boundedSnapshotText(text, 36000));
}

bool commandOutputContains(const CommandResult& result, const QString& needle)
{
    const QString trimmedNeedle = needle.trimmed();
    if (trimmedNeedle.isEmpty()) {
        return false;
    }
    return (result.standardOutput + QLatin1Char('\n') + result.standardError)
        .toCaseFolded()
        .contains(trimmedNeedle.toCaseFolded());
}

bool startWaitOutputLooksLaunched(const CommandResult& result, const QString& bundleName)
{
    if (!result.succeeded()) {
        return false;
    }

    const QString output = (result.standardOutput + QLatin1Char('\n') + result.standardError).toCaseFolded();
    if (output.contains(QStringLiteral("error"))
        || output.contains(QStringLiteral("failed"))
        || output.contains(QStringLiteral("not found"))) {
        return false;
    }

    const bool hasWaitDetails = output.contains(QStringLiteral("startmode:"))
        || output.contains(QStringLiteral("totaltime:"))
        || output.contains(QStringLiteral("thiswaittime:"))
        || output.contains(QStringLiteral("abilityname:"));
    if (!hasWaitDetails) {
        return false;
    }

    const QString foldedBundle = bundleName.trimmed().toCaseFolded();
    return foldedBundle.isEmpty() || output.contains(foldedBundle);
}

std::string startHarmonyAppToolContent(
    const CommandResult& startResult,
    const CommandResult& missionResult,
    const CommandResult& processResult,
    const QString& bundleName,
    const QString& extra)
{
    const bool commandAccepted = startResult.succeeded();
    const bool startWaitReportedLaunch = startWaitOutputLooksLaunched(startResult, bundleName);
    const QString missionOutput = missionResult.standardOutput + QLatin1Char('\n') + missionResult.standardError;
    const bool hasMissionRecord = HdcDeviceBackend::missionDumpHasBundleRecord(missionOutput, bundleName);
    const bool hasVisibleMission = HdcDeviceBackend::missionDumpShowsVisibleBundle(missionOutput, bundleName);
    const bool hasProcess = commandOutputContains(processResult, bundleName);
    const bool visibleLaunch = commandAccepted && (startWaitReportedLaunch || hasVisibleMission);

    QString text;
    text += QStringLiteral("# status: %1\n").arg(visibleLaunch ? QStringLiteral("ok") : QStringLiteral("error"));
    text += QStringLiteral("# operation: start_harmony_app\n");
    if (!extra.trimmed().isEmpty()) {
        text += extra.trimmed() + QStringLiteral("\n");
    }
    text += QStringLiteral("# command_accepted: %1\n").arg(commandAccepted ? QStringLiteral("true") : QStringLiteral("false"));
    text += QStringLiteral("# start_wait_reported_launch: %1\n").arg(startWaitReportedLaunch ? QStringLiteral("true") : QStringLiteral("false"));
    text += QStringLiteral("# mission_record_found: %1\n").arg(hasMissionRecord ? QStringLiteral("true") : QStringLiteral("false"));
    text += QStringLiteral("# visible_mission_found: %1\n").arg(hasVisibleMission ? QStringLiteral("true") : QStringLiteral("false"));
    text += QStringLiteral("# process_found: %1\n").arg(hasProcess ? QStringLiteral("true") : QStringLiteral("false"));
    if (commandAccepted && !visibleLaunch) {
        text += QStringLiteral("# diagnosis: aa start accepted the request, but ReArk did not observe start -W launch details or a ready attached mission for this bundle.\n");
        if (hasMissionRecord) {
            text += QStringLiteral("# note: a mission record exists, but it is not ready and attached, so it is not treated as proof that the UI opened.\n");
        }
        if (hasProcess) {
            text += QStringLiteral("# note: a process is present, but that alone is not treated as proof that the UI opened.\n");
        }
    }
    text += QStringLiteral("\n[start]\n%1\n").arg(HdcDeviceBackend::resultSummary(startResult));
    text += QStringLiteral("\n[missions]\n%1\n").arg(HdcDeviceBackend::resultSummary(missionResult));
    text += QStringLiteral("\n[processes]\n%1").arg(HdcDeviceBackend::resultSummary(processResult));
    return toStdString(boundedSnapshotText(text, 30000));
}

QList<InstallablePackageCandidate> installablePackageCandidates(
    const DecompilerController::AgentSnapshot& snapshot)
{
    QList<InstallablePackageCandidate> candidates;
    for (const DecompilerController::AgentInstallablePackageSnapshot& item : snapshot.installablePackages) {
        candidates.append({
            .path = item.path,
            .displayName = item.displayName
        });
    }
    return candidates;
}

QString agentScreenshotPath()
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (root.isEmpty()) {
        root = QDir::tempPath();
    }
    QDir dir(root);
    dir.mkpath(QStringLiteral("ReArk"));
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    return dir.filePath(QStringLiteral("ReArk/reark-agent-device-%1.jpeg").arg(timestamp));
}

QString agentLayoutPath()
{
    QDir dir(QDir::tempPath());
    dir.mkpath(QStringLiteral("ReArk"));
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    return dir.filePath(QStringLiteral("ReArk/reark-agent-ui-layout-%1.json").arg(timestamp));
}

struct AgentUiLayoutEvidence {
    CommandResult dump;
    CommandResult receive;
    QList<UiAutomationNode> nodes;
    QByteArray rawLayout;
    QString localPath;

    [[nodiscard]] bool succeeded() const
    {
        return dump.succeeded() && receive.succeeded() && !rawLayout.isEmpty();
    }
};

AgentUiLayoutEvidence dumpUiLayoutForAgent(
    const QString& target,
    const QString& bundleName = {})
{
    UiAutomationBackend backend = agentUiBackend();
    HdcDeviceBackend deviceBackend = agentDeviceBackend();
    const QString remotePath = QStringLiteral("/data/local/tmp/reark-agent-ui-layout.json");
    AgentUiLayoutEvidence evidence;
    evidence.localPath = agentLayoutPath();
    evidence.dump = CommandRunner::runBlocking(
        backend.dumpLayoutRequest(remotePath, target, bundleName));
    if (!evidence.dump.succeeded()) {
        return evidence;
    }

    evidence.receive = CommandRunner::runBlocking(
        deviceBackend.receiveFileRequest(remotePath, evidence.localPath, target));
    (void)CommandRunner::runBlocking(deviceBackend.removeRemoteFileRequest(remotePath, target));
    if (!evidence.receive.succeeded()) {
        return evidence;
    }

    QFile file(evidence.localPath);
    if (file.open(QIODevice::ReadOnly)) {
        evidence.rawLayout = file.readAll();
        evidence.nodes = UiAutomationBackend::parseLayout(evidence.rawLayout);
    }
    return evidence;
}

UiNodeSelector makeUiNodeSelector(
    const std::string& query,
    const std::string& text,
    const std::string& id,
    const std::string& type,
    bool exactText,
    bool exactId,
    bool exactType,
    bool clickableOnly,
    bool enabledOnly,
    bool visibleOnly)
{
    UiNodeSelector selector;
    selector.query = QString::fromStdString(query);
    selector.text = QString::fromStdString(text);
    selector.id = QString::fromStdString(id);
    selector.type = QString::fromStdString(type);
    selector.exactText = exactText;
    selector.exactId = exactId;
    selector.exactType = exactType;
    if (clickableOnly) {
        selector.clickable = true;
    }
    if (enabledOnly) {
        selector.enabled = true;
    }
    if (visibleOnly) {
        selector.visible = true;
    }
    return selector;
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
        "Read a source file, resource text, summary, or descriptor file from the current ReArk target. ReArk may load it on demand from the active Hyle session.";

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
                max_chars.value,
                context.stopToken))
        };
    }
};

struct read_disassembly {
    static constexpr std::string_view description =
        "Read source-file disassembly for a source file in the current ReArk target. ReArk may disassemble it on demand from the active Hyle session.";

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
                max_chars.value,
                context.stopToken))
        };
    }
};

struct read_abc_literal {
    static constexpr std::string_view description =
        "Resolve an ABC literal offset such as literal@0x00005757 to decoded strings, array items, and raw evidence from the current ReArk target.";

    wuwe::field<std::string> offset {
        .description = "ABC literal offset, for example 0x5757 or literal@0x00005757."
    };
    wuwe::field<std::string> path_or_query {
        .default_value = std::string {},
        .description = "Optional ABC file path or query. Leave empty for ReArk's current primary ABC, or pass a value such as modules.abc when a package has multiple ABC files."
    };
    wuwe::field<int> max_chars {
        .default_value = 12000,
        .description = "Maximum number of evidence characters to return."
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
            .content = toStdString(HyleDecompiler::readAbcLiteralEvidence(
                context.snapshot->packageContext,
                context.snapshot->packagePath,
                QString::fromStdString(path_or_query.value),
                QString::fromStdString(offset.value),
                max_chars.value,
                context.stopToken))
        };
    }
};

struct search_abc_strings {
    static constexpr std::string_view description =
        "Search the full decoded ABC string index, including class, bytecode, literal, annotation, and debug strings, with optional regex filtering.";

    wuwe::field<std::string> path_or_query {
        .default_value = std::string {},
        .description = "Optional ABC file path or query. Leave empty for ReArk's current primary ABC, or pass a value such as modules.abc when a package has multiple ABC files."
    };
    wuwe::field<std::string> pattern {
        .default_value = std::string {},
        .description = "Optional regex pattern, for example [0-9a-f]{64} for SHA-256-like hashes."
    };
    wuwe::field<int> min_len {
        .default_value = 4,
        .description = "Minimum string length."
    };
    wuwe::field<int> max_len {
        .default_value = 0,
        .description = "Maximum string length, or 0 for unlimited."
    };
    wuwe::field<int> limit {
        .default_value = 80,
        .description = "Maximum number of string matches."
    };
    wuwe::field<int> max_chars {
        .default_value = 24000,
        .description = "Maximum number of evidence characters to return."
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
            .content = toStdString(HyleDecompiler::searchAbcStringEvidence(
                context.snapshot->packageContext,
                context.snapshot->packagePath,
                QString::fromStdString(path_or_query.value),
                QString::fromStdString(pattern.value),
                min_len.value,
                max_len.value,
                limit.value,
                max_chars.value,
                context.stopToken))
        };
    }
};

struct read_abc_tree {
    static constexpr std::string_view description =
        "Read the structured ABC class, method, field, code, string, and literal tree from the current ReArk target.";

    wuwe::field<std::string> path_or_query {
        .default_value = std::string {},
        .description = "Optional ABC file path or query. Leave empty for ReArk's current primary ABC."
    };
    wuwe::field<int> limit {
        .default_value = 80,
        .description = "Maximum number of classes to list."
    };
    wuwe::field<int> max_chars {
        .default_value = 24000,
        .description = "Maximum number of evidence characters to return."
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
            .content = toStdString(HyleDecompiler::readAbcTreeEvidence(
                context.snapshot->packageContext,
                context.snapshot->packagePath,
                QString::fromStdString(path_or_query.value),
                limit.value,
                max_chars.value,
                context.stopToken))
        };
    }
};

struct find_abc_xrefs {
    static constexpr std::string_view description =
        "Find structured ABC bytecode cross-references to a string, method, or literal offset.";

    wuwe::field<std::string> query {
        .description = "String/method text to search, or an offset such as 0x5757."
    };
    wuwe::field<std::string> path_or_query {
        .default_value = std::string {},
        .description = "Optional ABC file path or query. Leave empty for ReArk's current primary ABC."
    };
    wuwe::field<std::string> kind {
        .default_value = "any",
        .description = "Reference kind: any, string, method, or literal."
    };
    wuwe::field<int> limit {
        .default_value = 80,
        .description = "Maximum number of xrefs to list."
    };
    wuwe::field<int> max_chars {
        .default_value = 24000,
        .description = "Maximum number of evidence characters to return."
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
            .content = toStdString(HyleDecompiler::findAbcXrefEvidence(
                context.snapshot->packageContext,
                context.snapshot->packagePath,
                QString::fromStdString(path_or_query.value),
                QString::fromStdString(query.value),
                QString::fromStdString(kind.value),
                limit.value,
                max_chars.value,
                context.stopToken))
        };
    }
};

struct find_abc_call_argument_flows {
    static constexpr std::string_view description =
        "Find conservative ABC evidence that a string/literal/method reference flows into a call argument.";

    wuwe::field<std::string> query {
        .description = "String/method text to search, or an offset such as 0x5757."
    };
    wuwe::field<std::string> path_or_query {
        .default_value = std::string {},
        .description = "Optional ABC file path or query. Leave empty for ReArk's current primary ABC."
    };
    wuwe::field<std::string> kind {
        .default_value = "any",
        .description = "Reference kind: any, string, method, or literal."
    };
    wuwe::field<int> limit {
        .default_value = 80,
        .description = "Maximum number of flows to list."
    };
    wuwe::field<int> max_chars {
        .default_value = 24000,
        .description = "Maximum number of evidence characters to return."
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
            .content = toStdString(HyleDecompiler::findAbcCallArgumentFlowEvidence(
                context.snapshot->packageContext,
                context.snapshot->packagePath,
                QString::fromStdString(path_or_query.value),
                QString::fromStdString(query.value),
                QString::fromStdString(kind.value),
                limit.value,
                max_chars.value,
                context.stopToken))
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

struct list_harmony_devices {
    static constexpr std::string_view description =
        "List HarmonyOS devices visible through ReArk's bundled hdc executable. This is a fixed ReArk device operation, not a shell.";

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }

        HdcDeviceBackend backend = agentDeviceBackend();
        const CommandResult result = CommandRunner::runBlocking(backend.listTargetsRequest());
        const QList<HdcDeviceTarget> targets = HdcDeviceBackend::parseTargets(result.standardOutput);

        QString extra = QStringLiteral("# hdc: %1\n# target_count: %2").arg(
            backend.resolvedProgram()).arg(targets.size());
        for (const HdcDeviceTarget& target : targets) {
            extra += QStringLiteral("\n- %1 [%2]").arg(target.id, target.state);
        }

        return {
            .content = deviceCommandToolContent(QStringLiteral("list_harmony_devices"), result, extra),
            .error_code = result.succeeded()
                ? std::error_code {}
                : std::make_error_code(std::errc::io_error)
        };
    }
};

struct install_current_hap {
    static constexpr std::string_view description =
        "Install the currently loaded ReArk package to a HarmonyOS target through hdc. If the active package is an APP container, ReArk installs the resolved inner HAP module. If hdc rejects the HAP because it is unsigned and Harmony signing is configured in Settings, ReArk signs the HAP with the configured local signing material and retries installation automatically. For multi-HAP APP packages, pass module to choose a module from the tool's candidate list.";

    wuwe::field<std::string> target_id {
        .default_value = std::string {},
        .description = "Optional hdc target id. Leave empty to let hdc use its default target."
    };
    wuwe::field<std::string> module {
        .default_value = std::string {},
        .description = "Optional module display name, file name, or path when the active APP contains multiple HAP modules."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        if (!context.snapshot || context.snapshot->packagePath.trimmed().isEmpty()) {
            return { .content = "No active ReArk package is loaded." };
        }

        HdcDeviceBackend backend = agentDeviceBackend();
        InstallablePackageSelection selection = InstallablePackageResolver::select(
            installablePackageCandidates(*context.snapshot),
            context.snapshot->packagePath,
            QString::fromStdString(module.value));
        if (!selection.ok) {
            return {
                .content = toStdString(selection.diagnostic),
                .error_code = wuwe::agent::make_error_code(wuwe::agent::llm_error_code::invalid_tool_arguments)
            };
        }

        const QString targetId = QString::fromStdString(target_id.value);
        const CommandResult result = CommandRunner::runBlocking(
            backend.installRequest(selection.path, targetId));
        const bool installOk = HdcDeviceBackend::installSucceeded(result);
        const QString extra = QStringLiteral("# active_package: %1\n# installable_hap: %2\n# source_module: %3\n# hdc: %4").arg(
            context.snapshot->packagePath,
            selection.path,
            selection.displayName,
            backend.resolvedProgram());
        const bool missingSignatureFailure = isMissingSignatureInstallFailure(result);
        const bool packageAppearsUnsigned = signatureSummaryLooksUnsigned(context.snapshot->signatureSummary);
        if (!installOk && (missingSignatureFailure || packageAppearsUnsigned)) {
            const HarmonySigningSettings signingSettings = SigningSettingsStore::loadHarmony();
            const QString validationMessage = SigningSettingsStore::harmonyValidationMessage(signingSettings);
            if (!validationMessage.isEmpty()) {
                QString text;
                text += QStringLiteral("# status: error\n");
                text += QStringLiteral("# operation: install_current_hap\n");
                text += extra.trimmed() + QStringLiteral("\n");
                text += QStringLiteral("# auto_sign: unavailable\n");
                text += signingSettingsStatusLine(validationMessage) + QStringLiteral("\n\n");
                text += HdcDeviceBackend::resultSummary(result);
                text += QStringLiteral("\n\nInstallation failed because the HAP is unsigned. Configure Harmony signing in Settings, then retry install_current_hap.");
                return {
                    .content = toStdString(boundedSnapshotText(text, 26000)),
                    .error_code = std::make_error_code(std::errc::io_error)
                };
            }

            const SigningMaterialStatus materialStatus =
                SigningMaterialInspector::inspectHarmony(signingSettings);
            const QString packageBundleName =
                packageBundleNameFromSummary(context.snapshot->packageSummary);

            QTemporaryDir signedDir(QDir::temp().filePath(QStringLiteral("ReArk-agent-signed-hap-XXXXXX")));
            if (!signedDir.isValid()) {
                QString text;
                text += QStringLiteral("# status: error\n");
                text += QStringLiteral("# operation: install_current_hap\n");
                text += extra.trimmed() + QStringLiteral("\n");
                text += QStringLiteral("# auto_sign: failed\n");
                text += QStringLiteral("# error: Could not create temporary signed HAP directory.\n\n");
                text += HdcDeviceBackend::resultSummary(result);
                return {
                    .content = toStdString(boundedSnapshotText(text, 26000)),
                    .error_code = std::make_error_code(std::errc::io_error)
                };
            }

            QString signInputHapPath = selection.path;
            HarmonyBundleRewriteResult rewriteResult;
            bool bundleRewriteUsed = false;
            if (!packageBundleName.isEmpty()
                && !materialStatus.profileBundleName.isEmpty()
                && packageBundleName != materialStatus.profileBundleName) {
                bundleRewriteUsed = true;
                const QString rewrittenHapPath =
                    signedDir.filePath(agentRewrittenHapFileName(selection.path));
                rewriteResult = HarmonyPackageRewriter::rewriteBundleIdentity({
                    .inputHapPath = selection.path,
                    .outputHapPath = rewrittenHapPath,
                    .oldBundleName = packageBundleName,
                    .newBundleName = materialStatus.profileBundleName,
                    .javaProgram = HarmonyHapSigner::resolvedJavaProgram(),
                    .packingToolPath = HarmonyHapSigner::bundledPackingToolPath()
                });
                if (!rewriteResult.ok) {
                    signedDir.setAutoRemove(false);
                    QString text;
                    text += QStringLiteral("# status: error\n");
                    text += QStringLiteral("# operation: install_current_hap\n");
                    text += extra.trimmed() + QStringLiteral("\n");
                    text += QStringLiteral("# auto_sign: blocked\n");
                    text += QStringLiteral("# bundle_rewrite: failed\n");
                    text += signingSettingsStatusLine(validationMessage) + QStringLiteral("\n");
                    text += QStringLiteral("# package_bundle: %1\n").arg(packageBundleName);
                    text += QStringLiteral("# signing_profile_bundle: %1\n").arg(materialStatus.profileBundleName);
                    text += QStringLiteral("# rewritten_hap: %1\n\n").arg(rewrittenHapPath);
                    text += HdcDeviceBackend::resultSummary(result);
                    text += QStringLiteral("\n\n[bundle rewrite]\n");
                    text += QStringLiteral("# status: error\n# error: %1\n").arg(rewriteResult.error.trimmed());
                    if (!rewriteResult.report.trimmed().isEmpty()) {
                        text += QStringLiteral("\n%1\n").arg(rewriteResult.report.trimmed());
                    }
                    if (rewriteResult.packingResult.started || !rewriteResult.packingResult.program.isEmpty()) {
                        text += QStringLiteral("\n[pack]\n%1\n").arg(
                            HarmonyHapSigner::resultSummary(rewriteResult.packingResult));
                    }
                    text += QStringLiteral("\nInstallation failed because ReArk could not rewrite the HAP bundle identity to match the configured Harmony profile.");
                    return {
                        .content = toStdString(boundedSnapshotText(text, 32000)),
                        .error_code = std::make_error_code(std::errc::io_error)
                    };
                }
                signInputHapPath = rewrittenHapPath;
            }

            const QString signedHapPath = signedDir.filePath(agentSignedHapFileName(signInputHapPath));
            const CommandResult signResult = CommandRunner::runBlocking(HarmonyHapSigner::signCommand({
                .javaProgram = HarmonyHapSigner::resolvedJavaProgram(),
                .signToolPath = HarmonyHapSigner::bundledSignToolPath(),
                .keystorePath = signingSettings.keystorePath,
                .keystorePassword = signingSettings.keystorePassword,
                .keyAlias = signingSettings.keyAlias,
                .keyPassword = signingSettings.keyPassword,
                .profilePath = signingSettings.profilePath,
                .certificatePath = signingSettings.certificatePath,
                .inputHapPath = signInputHapPath,
                .outputHapPath = signedHapPath,
                .compatibleVersion = packageCompatibleVersionFromSummary(context.snapshot->packageSummary)
            }));
            if (!signResult.succeeded()) {
                signedDir.setAutoRemove(false);
                const QString signedExtra = extra
                    + QStringLiteral("\n# auto_sign: failed")
                    + QStringLiteral("\n# bundle_rewrite: %1").arg(bundleRewriteUsed ? QStringLiteral("used") : QStringLiteral("not_needed"))
                    + (bundleRewriteUsed
                        ? QStringLiteral("\n# package_bundle: %1\n# signing_profile_bundle: %2\n# rewritten_hap: %3")
                            .arg(packageBundleName, materialStatus.profileBundleName, signInputHapPath)
                        : QString())
                    + QStringLiteral("\n%1").arg(signingSettingsStatusLine(validationMessage))
                    + QStringLiteral("\n# signed_hap: %1").arg(signedHapPath);
                return {
                    .content = bundleRewriteUsed
                        ? installWithRewriteSignToolContent(result, rewriteResult, signResult, {}, signedExtra)
                        : installWithAutoSignToolContent(result, signResult, {}, signedExtra),
                    .error_code = std::make_error_code(std::errc::io_error)
                };
            }

            const CommandResult signedInstallResult = CommandRunner::runBlocking(
                backend.installRequest(signedHapPath, targetId));
            const bool signedInstallOk = HdcDeviceBackend::installSucceeded(signedInstallResult);
            if (!signedInstallOk) {
                signedDir.setAutoRemove(false);
            }
            const QString signedExtra = extra
                + QStringLiteral("\n# auto_sign: used")
                + QStringLiteral("\n# bundle_rewrite: %1").arg(bundleRewriteUsed ? QStringLiteral("used") : QStringLiteral("not_needed"))
                + (bundleRewriteUsed
                    ? QStringLiteral("\n# package_bundle: %1\n# signing_profile_bundle: %2\n# installed_bundle: %2")
                        .arg(packageBundleName, materialStatus.profileBundleName)
                    : QString())
                + QStringLiteral("\n%1").arg(signingSettingsStatusLine(validationMessage))
                + (signedInstallOk
                    ? QStringLiteral("\n# signed_hap: temporary")
                    : QStringLiteral("\n# signed_hap: %1").arg(signedHapPath));
            return {
                .content = bundleRewriteUsed
                    ? installWithRewriteSignToolContent(result, rewriteResult, signResult, signedInstallResult, signedExtra)
                    : installWithAutoSignToolContent(result, signResult, signedInstallResult, signedExtra),
                .error_code = signedInstallOk
                    ? std::error_code {}
                    : std::make_error_code(std::errc::io_error)
            };
        }
        return {
            .content = deviceCommandToolContent(QStringLiteral("install_current_hap"), result, extra, installOk, true),
            .error_code = installOk
                ? std::error_code {}
                : std::make_error_code(std::errc::io_error)
        };
    }
};

struct start_harmony_app {
    static constexpr std::string_view description =
        "Start a HarmonyOS bundle/ability through hdc aa start. Requires explicit bundle name and optional ability name.";

    wuwe::field<std::string> bundle_name {
        .description = "HarmonyOS bundle name, for example com.example.app."
    };
    wuwe::field<std::string> ability_name {
        .default_value = std::string {},
        .description = "Optional ability name, for example EntryAbility."
    };
    wuwe::field<std::string> module_name {
        .default_value = "entry",
        .description = "Optional module name. Defaults to entry for standard entry HAP modules."
    };
    wuwe::field<std::string> target_id {
        .default_value = std::string {},
        .description = "Optional hdc target id."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        const QString bundle = QString::fromStdString(bundle_name.value).trimmed();
        if (bundle.isEmpty()) {
            return {
                .content = "bundle_name is required.",
                .error_code = wuwe::agent::make_error_code(wuwe::agent::llm_error_code::invalid_tool_arguments)
            };
        }

        HdcDeviceBackend backend = agentDeviceBackend();
        const QString target = QString::fromStdString(target_id.value);
        const QString ability = QString::fromStdString(ability_name.value).trimmed();
        const QString module = QString::fromStdString(module_name.value).trimmed();
        const CommandResult result = CommandRunner::runBlocking(
            backend.startAbilityRequest(
                bundle,
                ability,
                module,
                target));
        const CommandResult missionResult = CommandRunner::runBlocking(
            backend.missionListRequest(target));
        const CommandResult processResult = CommandRunner::runBlocking(
            backend.processListRequest(target));
        const QString missionOutput = missionResult.standardOutput + QLatin1Char('\n') + missionResult.standardError;
        const bool visibleLaunch = result.succeeded()
            && (startWaitOutputLooksLaunched(result, bundle)
                || HdcDeviceBackend::missionDumpShowsVisibleBundle(missionOutput, bundle));
        const QString extra = QStringLiteral("# bundle: %1\n# ability: %2\n# module: %3\n# hdc: %4").arg(
            bundle,
            ability,
            module,
            backend.resolvedProgram());
        return {
            .content = startHarmonyAppToolContent(result, missionResult, processResult, bundle, extra),
            .error_code = visibleLaunch
                ? std::error_code {}
                : std::make_error_code(std::errc::io_error)
        };
    }
};

struct read_hilog {
    static constexpr std::string_view description =
        "Read bounded HarmonyOS hilog output through hdc and optionally filter by log level and line substring.";

    wuwe::field<std::string> filter {
        .default_value = std::string {},
        .description = "Optional substring filter applied by ReArk after capture."
    };
    wuwe::field<std::string> level {
        .default_value = std::string {},
        .description = "Optional hilog level: D, I, W, E, F, or empty for all levels."
    };
    wuwe::field<int> max_lines {
        .default_value = 500,
        .description = "Maximum number of final lines to return."
    };
    wuwe::field<std::string> target_id {
        .default_value = std::string {},
        .description = "Optional hdc target id."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }

        HdcDeviceBackend backend = agentDeviceBackend();
        const CommandResult result = CommandRunner::runBlocking(
            backend.hilogRequest(
                QString::fromStdString(target_id.value),
                QString::fromStdString(level.value)));
        const QString filtered = HdcDeviceBackend::filterHilog(
            result.standardOutput + result.standardError,
            QString::fromStdString(filter.value),
            max_lines.value);
        QString extra = QStringLiteral("# hdc: %1\n# level: %2\n# filter: %3\n# lines_returned: %4")
            .arg(backend.resolvedProgram())
            .arg(QString::fromStdString(level.value).trimmed().isEmpty()
                    ? QStringLiteral("All")
                    : QString::fromStdString(level.value))
            .arg(QString::fromStdString(filter.value))
            .arg(filtered.isEmpty() ? 0 : filtered.count(QLatin1Char('\n')) + 1);
        if (!filtered.isEmpty()) {
            extra += QStringLiteral("\n\n[filtered_hilog]\n%1").arg(filtered);
        }

        return {
            .content = deviceCommandToolContent(QStringLiteral("read_hilog"), result, extra),
            .error_code = result.succeeded()
                ? std::error_code {}
                : std::make_error_code(std::errc::io_error)
        };
    }
};

struct capture_device_screenshot {
    static constexpr std::string_view description =
        "Capture a HarmonyOS device screenshot through hdc snapshot_display and download it to the local ReArk screenshots folder.";

    wuwe::field<std::string> target_id {
        .default_value = std::string {},
        .description = "Optional hdc target id."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }

        HdcDeviceBackend backend = agentDeviceBackend();
        const QString target = QString::fromStdString(target_id.value);
        const QString remotePath = QStringLiteral("/data/local/tmp/reark-agent-screenshot.jpeg");
        const QString localPath = agentScreenshotPath();
        const CommandResult capture = CommandRunner::runBlocking(
            backend.screenshotCaptureRequest(remotePath, target));
        if (!capture.succeeded()) {
            return {
                .content = deviceCommandToolContent(QStringLiteral("capture_device_screenshot"), capture),
                .error_code = std::make_error_code(std::errc::io_error)
            };
        }

        const CommandResult receive = CommandRunner::runBlocking(
            backend.receiveFileRequest(remotePath, localPath, target));
        (void)CommandRunner::runBlocking(backend.removeRemoteFileRequest(remotePath, target));

        QString text;
        text += QStringLiteral("# status: %1\n").arg(receive.succeeded() ? QStringLiteral("ok") : QStringLiteral("error"));
        text += QStringLiteral("# operation: capture_device_screenshot\n");
        text += QStringLiteral("# local_path: %1\n\n").arg(localPath);
        text += QStringLiteral("[capture]\n%1\n\n[receive]\n%2").arg(
            HdcDeviceBackend::resultSummary(capture),
            HdcDeviceBackend::resultSummary(receive));

        return {
            .content = toStdString(boundedSnapshotText(text, 24000)),
            .error_code = receive.succeeded()
                ? std::error_code {}
                : std::make_error_code(std::errc::io_error)
        };
    }
};

struct dump_ui_layout {
    static constexpr std::string_view description =
        "Dump the current HarmonyOS UI layout through uitest, parse it into stable node evidence, and optionally filter nodes by structured selector fields.";

    wuwe::field<std::string> query {
        .default_value = std::string {},
        .description = "Optional case-insensitive query matched against node text, id, key, type, description, bundle, or ability."
    };
    wuwe::field<std::string> text {
        .default_value = std::string {},
        .description = "Optional node text/originalText selector."
    };
    wuwe::field<std::string> id {
        .default_value = std::string {},
        .description = "Optional node id/key selector."
    };
    wuwe::field<std::string> type {
        .default_value = std::string {},
        .description = "Optional node type selector."
    };
    wuwe::field<bool> exact_text {
        .default_value = false,
        .description = "Use exact text matching instead of contains."
    };
    wuwe::field<bool> exact_id {
        .default_value = false,
        .description = "Use exact id/key matching instead of contains."
    };
    wuwe::field<bool> exact_type {
        .default_value = false,
        .description = "Use exact type matching instead of contains."
    };
    wuwe::field<bool> clickable_only {
        .default_value = false,
        .description = "Only return clickable nodes."
    };
    wuwe::field<bool> enabled_only {
        .default_value = false,
        .description = "Only return enabled nodes."
    };
    wuwe::field<bool> visible_only {
        .default_value = false,
        .description = "Only return visible nodes."
    };
    wuwe::field<std::string> bundle_name {
        .default_value = std::string {},
        .description = "Optional bundle name used to narrow the target window."
    };
    wuwe::field<int> max_nodes {
        .default_value = 80,
        .description = "Maximum parsed nodes to include in the textual evidence."
    };
    wuwe::field<std::string> target_id {
        .default_value = std::string {},
        .description = "Optional hdc target id."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }

        const AgentUiLayoutEvidence evidence = dumpUiLayoutForAgent(
            QString::fromStdString(target_id.value),
            QString::fromStdString(bundle_name.value));
        QList<UiAutomationNode> nodes = evidence.nodes;
        const UiNodeSelector selector = makeUiNodeSelector(
            query.value,
            text.value,
            id.value,
            type.value,
            exact_text.value,
            exact_id.value,
            exact_type.value,
            clickable_only.value,
            enabled_only.value,
            visible_only.value);
        nodes = UiAutomationBackend::findNodes(nodes, selector, max_nodes.value);

        QString text;
        text += QStringLiteral("# status: %1\n").arg(evidence.succeeded() ? QStringLiteral("ok") : QStringLiteral("error"));
        text += QStringLiteral("# operation: dump_ui_layout\n");
        text += QStringLiteral("# node_count: %1\n").arg(evidence.nodes.size());
        text += QStringLiteral("# returned_nodes: %1\n").arg(nodes.size());
        text += QStringLiteral("# local_path: %1\n\n").arg(evidence.localPath);
        text += QStringLiteral("[dump]\n%1\n\n[receive]\n%2\n\n[nodes]\n%3").arg(
            HdcDeviceBackend::resultSummary(evidence.dump),
            HdcDeviceBackend::resultSummary(evidence.receive),
            UiAutomationBackend::nodesSummary(nodes, max_nodes.value));

        return {
            .content = toStdString(boundedSnapshotText(text, 24000)),
            .error_code = evidence.succeeded()
                ? std::error_code {}
                : std::make_error_code(std::errc::io_error)
        };
    }
};

struct tap_ui {
    static constexpr std::string_view description =
        "Tap fixed screen coordinates through HarmonyOS uitest uiInput. Prefer tap_ui_text when a stable UI node can be selected.";

    wuwe::field<int> x {
        .description = "Screen x coordinate."
    };
    wuwe::field<int> y {
        .description = "Screen y coordinate."
    };
    wuwe::field<std::string> target_id {
        .default_value = std::string {},
        .description = "Optional hdc target id."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }

        UiAutomationBackend backend = agentUiBackend();
        const CommandResult result = CommandRunner::runBlocking(
            backend.tapRequest(x.value, y.value, QString::fromStdString(target_id.value)));
        const QString extra = QStringLiteral("# x: %1\n# y: %2").arg(x.value).arg(y.value);
        return {
            .content = deviceCommandToolContent(QStringLiteral("tap_ui"), result, extra),
            .error_code = result.succeeded()
                ? std::error_code {}
                : std::make_error_code(std::errc::io_error)
        };
    }
};

struct tap_ui_text {
    static constexpr std::string_view description =
        "Dump the UI layout, find a node by structured selector, and tap the matched node center. This avoids coordinate guessing.";

    wuwe::field<std::string> query {
        .default_value = std::string {},
        .description = "Optional broad query matched against node text, id, key, type, description, bundle, or ability."
    };
    wuwe::field<std::string> text {
        .default_value = std::string {},
        .description = "Optional node text/originalText selector."
    };
    wuwe::field<std::string> id {
        .default_value = std::string {},
        .description = "Optional node id/key selector."
    };
    wuwe::field<std::string> type {
        .default_value = std::string {},
        .description = "Optional node type selector."
    };
    wuwe::field<bool> exact_text {
        .default_value = false,
        .description = "Use exact text matching instead of contains."
    };
    wuwe::field<bool> exact_id {
        .default_value = false,
        .description = "Use exact id/key matching instead of contains."
    };
    wuwe::field<bool> exact_type {
        .default_value = false,
        .description = "Use exact type matching instead of contains."
    };
    wuwe::field<bool> clickable_only {
        .default_value = false,
        .description = "Only match clickable nodes."
    };
    wuwe::field<bool> enabled_only {
        .default_value = true,
        .description = "Only match enabled nodes."
    };
    wuwe::field<bool> visible_only {
        .default_value = true,
        .description = "Only match visible nodes."
    };
    wuwe::field<std::string> bundle_name {
        .default_value = std::string {},
        .description = "Optional bundle name used to narrow the target window."
    };
    wuwe::field<std::string> target_id {
        .default_value = std::string {},
        .description = "Optional hdc target id."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        const bool hasSelector = !QString::fromStdString(query.value).trimmed().isEmpty()
            || !QString::fromStdString(text.value).trimmed().isEmpty()
            || !QString::fromStdString(id.value).trimmed().isEmpty()
            || !QString::fromStdString(type.value).trimmed().isEmpty();
        if (!hasSelector) {
            return {
                .content = "At least one selector field is required.",
                .error_code = wuwe::agent::make_error_code(wuwe::agent::llm_error_code::invalid_tool_arguments)
            };
        }

        const QString target = QString::fromStdString(target_id.value);
        const AgentUiLayoutEvidence evidence = dumpUiLayoutForAgent(
            target,
            QString::fromStdString(bundle_name.value));
        if (!evidence.succeeded()) {
            QString text;
            text += QStringLiteral("# status: error\n# operation: tap_ui_text\n\n[dump]\n%1\n\n[receive]\n%2").arg(
                HdcDeviceBackend::resultSummary(evidence.dump),
                HdcDeviceBackend::resultSummary(evidence.receive));
            return {
                .content = toStdString(boundedSnapshotText(text, 24000)),
                .error_code = std::make_error_code(std::errc::io_error)
            };
        }

        const UiNodeSelector selector = makeUiNodeSelector(
            query.value,
            text.value,
            id.value,
            type.value,
            exact_text.value,
            exact_id.value,
            exact_type.value,
            clickable_only.value,
            enabled_only.value,
            visible_only.value);
        QList<UiAutomationNode> matches = UiAutomationBackend::findNodes(evidence.nodes, selector, 25);
        if (matches.isEmpty()) {
            QString output;
            output += QStringLiteral("# status: error\n# operation: tap_ui_text\n# query: %1\n# text: %2\n# id: %3\n# type: %4\n# node_count: %5\n\n[nodes]\n%6")
                .arg(QString::fromStdString(query.value))
                .arg(QString::fromStdString(text.value))
                .arg(QString::fromStdString(id.value))
                .arg(QString::fromStdString(type.value))
                .arg(evidence.nodes.size())
                .arg(UiAutomationBackend::nodesSummary(evidence.nodes, 80));
            return {
                .content = toStdString(boundedSnapshotText(output, 24000)),
                .error_code = std::make_error_code(std::errc::no_such_file_or_directory)
            };
        }

        auto selected = std::find_if(matches.cbegin(), matches.cend(), [](const UiAutomationNode& node) {
            return node.clickable && node.enabled && node.visible;
        });
        if (selected == matches.cend()) {
            selected = matches.cbegin();
        }

        UiAutomationBackend backend = agentUiBackend();
        const QPoint point = selected->center();
        const CommandResult tap = CommandRunner::runBlocking(
            backend.tapRequest(point.x(), point.y(), target));

        QString output;
        output += QStringLiteral("# status: %1\n").arg(tap.succeeded() ? QStringLiteral("ok") : QStringLiteral("error"));
        output += QStringLiteral("# operation: tap_ui_text\n# query: %1\n# text: %2\n# id: %3\n# type: %4\n# selected_node: %5\n# tap: (%6,%7)\n\n")
            .arg(QString::fromStdString(query.value))
            .arg(QString::fromStdString(text.value))
            .arg(QString::fromStdString(id.value))
            .arg(QString::fromStdString(type.value))
            .arg(selected->index)
            .arg(point.x())
            .arg(point.y());
        output += QStringLiteral("[matches]\n%1\n\n[tap]\n%2").arg(
            UiAutomationBackend::nodesSummary(matches, 25),
            HdcDeviceBackend::resultSummary(tap));

        return {
            .content = toStdString(boundedSnapshotText(output, 24000)),
            .error_code = tap.succeeded()
                ? std::error_code {}
                : std::make_error_code(std::errc::io_error)
        };
    }
};

struct input_ui_text {
    static constexpr std::string_view description =
        "Input text through HarmonyOS uitest. Use x/y to focus a coordinate first, or omit them to type into the currently focused field.";

    wuwe::field<std::string> text {
        .description = "Text to input."
    };
    wuwe::field<int> x {
        .default_value = -1,
        .description = "Optional screen x coordinate. Use -1 to type into the focused field."
    };
    wuwe::field<int> y {
        .default_value = -1,
        .description = "Optional screen y coordinate. Use -1 to type into the focused field."
    };
    wuwe::field<std::string> target_id {
        .default_value = std::string {},
        .description = "Optional hdc target id."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        if (text.value.empty()) {
            return {
                .content = "text is required.",
                .error_code = wuwe::agent::make_error_code(wuwe::agent::llm_error_code::invalid_tool_arguments)
            };
        }

        UiAutomationBackend backend = agentUiBackend();
        const QString target = QString::fromStdString(target_id.value);
        const QString value = QString::fromStdString(text.value);
        const bool hasPoint = x.value >= 0 && y.value >= 0;
        const CommandResult result = CommandRunner::runBlocking(
            hasPoint
                ? backend.inputTextAtRequest(x.value, y.value, value, target)
                : backend.inputFocusedTextRequest(value, target));
        const QString extra = QStringLiteral("# mode: %1\n# x: %2\n# y: %3")
            .arg(hasPoint ? QStringLiteral("coordinate") : QStringLiteral("focused"))
            .arg(x.value)
            .arg(y.value);
        return {
            .content = deviceCommandToolContent(QStringLiteral("input_ui_text"), result, extra),
            .error_code = result.succeeded()
                ? std::error_code {}
                : std::make_error_code(std::errc::io_error)
        };
    }
};

struct press_device_key {
    static constexpr std::string_view description =
        "Send a fixed HarmonyOS uitest key event such as Back, Home, Power, or a numeric key id.";

    wuwe::field<std::string> key {
        .description = "Key name or id, for example Back, Home, Power, or 2."
    };
    wuwe::field<std::string> target_id {
        .default_value = std::string {},
        .description = "Optional hdc target id."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }
        const QString value = QString::fromStdString(key.value).trimmed();
        if (value.isEmpty()) {
            return {
                .content = "key is required.",
                .error_code = wuwe::agent::make_error_code(wuwe::agent::llm_error_code::invalid_tool_arguments)
            };
        }

        UiAutomationBackend backend = agentUiBackend();
        const CommandResult result = CommandRunner::runBlocking(
            backend.keyEventRequest(value, QString::fromStdString(target_id.value)));
        const QString extra = QStringLiteral("# key: %1").arg(value);
        return {
            .content = deviceCommandToolContent(QStringLiteral("press_device_key"), result, extra),
            .error_code = result.succeeded()
                ? std::error_code {}
                : std::make_error_code(std::errc::io_error)
        };
    }
};

struct swipe_device {
    static constexpr std::string_view description =
        "Swipe between two screen coordinates through HarmonyOS uitest uiInput.";

    wuwe::field<int> from_x { .description = "Start x coordinate." };
    wuwe::field<int> from_y { .description = "Start y coordinate." };
    wuwe::field<int> to_x { .description = "End x coordinate." };
    wuwe::field<int> to_y { .description = "End y coordinate." };
    wuwe::field<int> velocity {
        .default_value = 600,
        .description = "Swipe velocity from 200 to 40000."
    };
    wuwe::field<std::string> target_id {
        .default_value = std::string {},
        .description = "Optional hdc target id."
    };

    wuwe::llm_tool_result invoke(const ReArkToolContext& context) const
    {
        if (auto cancelled = cancelledToolResult(context)) {
            return *cancelled;
        }

        UiAutomationBackend backend = agentUiBackend();
        const CommandResult result = CommandRunner::runBlocking(
            backend.swipeRequest(
                from_x.value,
                from_y.value,
                to_x.value,
                to_y.value,
                QString::fromStdString(target_id.value),
                velocity.value));
        const QString extra = QStringLiteral("# from: (%1,%2)\n# to: (%3,%4)\n# velocity: %5")
            .arg(from_x.value)
            .arg(from_y.value)
            .arg(to_x.value)
            .arg(to_y.value)
            .arg(velocity.value);
        return {
            .content = deviceCommandToolContent(QStringLiteral("swipe_device"), result, extra),
            .error_code = result.succeeded()
                ? std::error_code {}
                : std::make_error_code(std::errc::io_error)
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
        registerTool<read_abc_literal>();
        registerTool<search_abc_strings>();
        registerTool<read_abc_tree>();
        registerTool<find_abc_xrefs>();
        registerTool<find_abc_call_argument_flows>();
        registerTool<inspect_entry_points>();
        registerTool<explain_signature>();
        registerTool<list_harmony_devices>();
        registerTool<install_current_hap>();
        registerTool<start_harmony_app>();
        registerTool<read_hilog>();
        registerTool<capture_device_screenshot>();
        registerTool<dump_ui_layout>();
        registerTool<tap_ui>();
        registerTool<tap_ui_text>();
        registerTool<input_ui_text>();
        registerTool<press_device_key>();
        registerTool<swipe_device>();
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
    if (isToolRoundBudgetExceededText(message)
        || isToolRoundBudgetExceededText(QString::fromStdString(ec.message()))
        || isLegacyToolRoundBudgetError(ec)) {
        return toolRoundBudgetExceededMessage();
    }
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
    if (ec == wuwe::agent::llm_error_code::timeout) {
        return AgentController::tr("Analysis timed out before a final answer was produced.");
    }
    if (!message.isEmpty()) {
        return message;
    }
    return QString::fromStdString(ec.message());
}

#ifdef REARK_HAS_WUWE_REASONING
QString reasoningEventStatus(
    const wuwe::agent::reasoning::reasoning_event& event,
    int modelCallCount,
    int toolCallCount)
{
    namespace reasoning = wuwe::agent::reasoning;

    switch (event.type) {
    case reasoning::reasoning_event_type::started:
        return AgentController::tr("Preparing analysis context...");
    case reasoning::reasoning_event_type::model_started:
        return AgentController::tr("Model analysis round %1: deciding the next step...")
            .arg(modelCallCount);
    case reasoning::reasoning_event_type::model_first_event:
        return AgentController::tr("Model analysis round %1: receiving response...")
            .arg(modelCallCount);
    case reasoning::reasoning_event_type::tool_call_building:
        return AgentController::tr("Preparing the next evidence request...");
    case reasoning::reasoning_event_type::tool_call_ready:
        return event.tool_call != nullptr && isScriptToolName(event.tool_call->name)
            ? AgentController::tr("Local analysis script is ready to run...")
            : AgentController::tr("Evidence request is ready...");
    case reasoning::reasoning_event_type::tool_started:
        return event.tool_call != nullptr
            ? AgentController::tr("Step %1: %2...")
                .arg(toolCallCount)
                .arg(isScriptToolName(event.tool_call->name)
                        ? AgentController::tr("running local analysis script")
                        : AgentController::tr("reading %1").arg(toolDisplayName(event.tool_call->name)))
            : AgentController::tr("Step %1: reading analysis data...").arg(toolCallCount);
    case reasoning::reasoning_event_type::tool_completed:
        if (event.tool_call != nullptr && isScriptToolName(event.tool_call->name)) {
            return event.tool_result != nullptr && event.tool_result->error_code
                ? AgentController::tr("Local analysis script failed.")
                : AgentController::tr("Local analysis script completed.");
        }
        return event.tool_result != nullptr && event.tool_result->error_code
            ? AgentController::tr("Evidence read failed.")
            : AgentController::tr("Evidence collected.");
    case reasoning::reasoning_event_type::model_completed:
        return AgentController::tr("Model round %1 completed.").arg(modelCallCount);
    case reasoning::reasoning_event_type::reflection_started:
        return AgentController::tr("Reviewing the answer...");
    case reasoning::reasoning_event_type::reflection_completed:
        return AgentController::tr("Review completed.");
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
        if (event.result != nullptr
            && event.result->reasoning_error == reasoning::reasoning_error_code::timeout) {
            return AgentController::tr("Analysis timed out.");
        }
        return AgentController::tr("Analysis failed.");
    case reasoning::reasoning_event_type::cancelled:
        return AgentController::tr("Analysis cancelled.");
    case reasoning::reasoning_event_type::content_delta:
        return AgentController::tr("Writing the answer...");
    }
    return {};
}

QVariantMap reasoningEventActivity(const wuwe::agent::reasoning::reasoning_event& event)
{
    namespace reasoning = wuwe::agent::reasoning;

    auto activity = [](const QString& type,
                       const QString& title,
                       const QString& detail,
                       const QString& state) {
        QVariantMap item;
        item.insert(QStringLiteral("type"), type);
        item.insert(QStringLiteral("title"), title);
        item.insert(QStringLiteral("detail"), detail);
        item.insert(QStringLiteral("state"), state);
        return item;
    };

    switch (event.type) {
    case reasoning::reasoning_event_type::started:
        return activity(
            QStringLiteral("run"),
            AgentController::tr("Analysis started"),
            AgentController::tr("Preparing context and available tools."),
            QStringLiteral("active"));
    case reasoning::reasoning_event_type::model_started:
        return activity(
            QStringLiteral("model"),
            AgentController::tr("Calling model"),
            AgentController::tr("Waiting for the first model event."),
            QStringLiteral("active"));
    case reasoning::reasoning_event_type::model_first_event:
        return activity(
            QStringLiteral("model"),
            AgentController::tr("Model stream started"),
            AgentController::tr("Receiving structured model events."),
            QStringLiteral("active"));
    case reasoning::reasoning_event_type::tool_call_building:
        return activity(
            QStringLiteral("prepare"),
            AgentController::tr("Preparing analysis step"),
            AgentController::tr("The model is selecting data or tools."),
            QStringLiteral("active"));
    case reasoning::reasoning_event_type::tool_call_ready:
        return activity(
            QStringLiteral("prepare"),
            event.tool_call != nullptr && isScriptToolName(event.tool_call->name)
                ? AgentController::tr("Analysis script prepared")
                : AgentController::tr("Data request prepared"),
            AgentController::tr("The next analysis step is ready to run."),
            QStringLiteral("done"));
    case reasoning::reasoning_event_type::tool_started:
        return activity(
            event.tool_call != nullptr && isScriptToolName(event.tool_call->name)
                ? QStringLiteral("script")
                : QStringLiteral("data"),
            event.tool_call != nullptr && isScriptToolName(event.tool_call->name)
                ? AgentController::tr("Running analysis script")
                : AgentController::tr("Reading analysis data"),
            event.tool_call != nullptr && isScriptToolName(event.tool_call->name)
                ? AgentController::tr("Executing bounded local analysis.")
                : AgentController::tr("Collecting evidence from the current package."),
            QStringLiteral("active"));
    case reasoning::reasoning_event_type::tool_completed: {
        const bool failed = event.tool_result != nullptr && event.tool_result->error_code;
        const bool script = event.tool_call != nullptr && isScriptToolName(event.tool_call->name);
        return activity(
            script ? QStringLiteral("script") : QStringLiteral("data"),
            failed
                ? (script ? AgentController::tr("Analysis script failed")
                          : AgentController::tr("Analysis data read failed"))
                : (script ? AgentController::tr("Analysis script completed")
                          : AgentController::tr("Analysis data ready")),
            failed
                ? AgentController::tr("The step returned an error.")
                : AgentController::tr("Evidence is available for the next model call."),
            failed ? QStringLiteral("failed") : QStringLiteral("done"));
    }
    case reasoning::reasoning_event_type::model_completed:
        return activity(
            QStringLiteral("model"),
            AgentController::tr("Model response received"),
            AgentController::tr("The model call completed."),
            QStringLiteral("done"));
    case reasoning::reasoning_event_type::reflection_started:
        return activity(
            QStringLiteral("review"),
            AgentController::tr("Reviewing result"),
            AgentController::tr("Checking the answer before returning it."),
            QStringLiteral("active"));
    case reasoning::reasoning_event_type::reflection_completed:
        return activity(
            QStringLiteral("review"),
            AgentController::tr("Review completed"),
            AgentController::tr("The answer passed the review step."),
            QStringLiteral("done"));
    case reasoning::reasoning_event_type::content_delta:
        return activity(
            QStringLiteral("answer"),
            AgentController::tr("Writing answer"),
            AgentController::tr("Streaming the final response."),
            QStringLiteral("active"));
    case reasoning::reasoning_event_type::completed:
        return activity(
            QStringLiteral("run"),
            AgentController::tr("Analysis complete"),
            AgentController::tr("Ready for the next question."),
            QStringLiteral("done"));
    case reasoning::reasoning_event_type::failed:
        return activity(
            QStringLiteral("run"),
            AgentController::tr("Analysis failed"),
            AgentController::tr("The run stopped before a complete answer was produced."),
            QStringLiteral("failed"));
    case reasoning::reasoning_event_type::cancelled:
        return activity(
            QStringLiteral("run"),
            AgentController::tr("Analysis cancelled"),
            AgentController::tr("The run was stopped."),
            QStringLiteral("failed"));
    case reasoning::reasoning_event_type::plan_created:
    case reasoning::reasoning_event_type::plan_step_started:
    case reasoning::reasoning_event_type::plan_step_completed:
    case reasoning::reasoning_event_type::plan_step_failed:
    case reasoning::reasoning_event_type::plan_step_blocked:
    case reasoning::reasoning_event_type::plan_revised:
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

QString reasoningErrorMessage(const wuwe::agent::reasoning::reasoning_error& error)
{
    namespace reasoning = wuwe::agent::reasoning;

    const QString code = QString::fromUtf8(wuwe::agent::reasoning::to_string(error.code));
    const QString message = QString::fromStdString(error.message);
    const QString underlying = error.underlying_error
        ? QString::fromStdString(error.underlying_error.message())
        : QString();

    if (isToolRoundBudgetExceededText(code)
        || isToolRoundBudgetExceededText(message)
        || isToolRoundBudgetExceededText(underlying)
        || (error.underlying_error && isLegacyToolRoundBudgetError(error.underlying_error))) {
        return toolRoundBudgetExceededMessage();
    }

    switch (error.code) {
    case reasoning::reasoning_error_code::timeout:
        return AgentController::tr("Analysis timed out before a final answer was produced.");
    case reasoning::reasoning_error_code::model_call_budget_exceeded:
        return AgentController::tr("Analysis stopped because the model call budget was exhausted.");
    case reasoning::reasoning_error_code::tool_call_budget_exceeded:
        return AgentController::tr("Analysis stopped because the tool call budget was exhausted.");
    case reasoning::reasoning_error_code::tool_round_budget_exceeded:
        return toolRoundBudgetExceededMessage();
    case reasoning::reasoning_error_code::planning_budget_exceeded:
        return AgentController::tr("Analysis stopped because the planning budget was exhausted.");
    case reasoning::reasoning_error_code::reflection_budget_exceeded:
        return AgentController::tr("Analysis stopped because the review budget was exhausted.");
    case reasoning::reasoning_error_code::cancelled:
        return AgentController::tr("Analysis cancelled.");
    default:
        break;
    }

    return error.underlying_error
        ? agentErrorMessage(error.underlying_error, message)
        : message;
}

bool isReasoningBudgetExceeded(wuwe::agent::reasoning::reasoning_error_code code)
{
    namespace reasoning = wuwe::agent::reasoning;

    switch (code) {
    case reasoning::reasoning_error_code::model_call_budget_exceeded:
    case reasoning::reasoning_error_code::tool_call_budget_exceeded:
    case reasoning::reasoning_error_code::tool_round_budget_exceeded:
    case reasoning::reasoning_error_code::planning_budget_exceeded:
    case reasoning::reasoning_error_code::reflection_budget_exceeded:
        return true;
    default:
        return false;
    }
}

QString reasoningCancelledMessage(const wuwe::agent::reasoning::reasoning_result& result)
{
    namespace reasoning = wuwe::agent::reasoning;

    if (result.reasoning_error == reasoning::reasoning_error_code::timeout) {
        return AgentController::tr("Analysis timed out before a final answer was produced.");
    }
    if (result.reasoning_error != reasoning::reasoning_error_code::cancelled
        && result.reasoning_error != reasoning::reasoning_error_code::none) {
        return reasoningErrorMessage({
            .code = result.reasoning_error,
            .underlying_error = result.underlying_error,
            .message = result.error
        });
    }
    return AgentController::tr("Analysis cancelled.");
}

wuwe::agent::reasoning::reasoning_policy rearkReasoningPolicy(const std::string& input)
{
    namespace reasoning = wuwe::agent::reasoning;

    auto policy = reasoning::select_policy(reasoning::reasoning_task_description {
        .input = input,
        .has_tools = true,
        .requires_tools = false
    });
    policy.budget.max_model_calls = 144;
    policy.budget.max_tool_calls = 360;
    policy.budget.max_tool_rounds = 96;
    policy.budget.max_steps = 192;
    policy.budget.timeout = std::chrono::milliseconds { 2700000 };
    return policy;
}
#endif

#endif

} // namespace

struct AgentController::Runtime {
#ifdef REARK_HAS_WUWE
    std::shared_ptr<wuwe::llm_client> client;
    std::shared_ptr<ReArkToolProvider> rearkProvider;
#ifdef REARK_HAS_WUWE_EXECUTION
    std::unique_ptr<QTemporaryDir> executionWorkdir;
    wuwe::agent::audit::in_memory_audit_sink executionAuditSink;
    wuwe::agent::approval::deny_all_approval_service executionApprovalService;
    std::unique_ptr<wuwe::agent::execution::execution_runtime> executionRuntime;
    std::shared_ptr<wuwe::agent::execution::execution_tool_provider> executionProvider;
    std::shared_ptr<ReArkExecutionToolProvider> guardedExecutionProvider;
    QString executionPromptNote;
#endif
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
    , messageModel_(new AgentMessageModel(this))
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

QAbstractItemModel* AgentController::messageModel() const
{
    return messageModel_;
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

void AgentController::ask(const QString& question)
{
    const QString trimmed = question.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    setErrorMessage({});
    if (!available()) {
        appendMessage(QStringLiteral("user"), trimmed);
        appendMessage(QStringLiteral("assistant"), unavailableMessage(), QStringLiteral("error"));
        setErrorMessage(unavailableMessage());
        setStatus(unavailableMessage());
        return;
    }

#ifdef REARK_HAS_WUWE
    if (running_) {
        pendingQuestion_ = trimmed;
        cancelCurrentRun(false);
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

    try {
        runtime_->client = createLlmClient(settings);
    } catch (const std::exception& ex) {
        const QString message = tr("Failed to create Wuwe LLM provider %1: %2")
            .arg(settings.provider, QString::fromUtf8(ex.what()));
        appendMessage(QStringLiteral("user"), trimmed);
        appendMessage(QStringLiteral("assistant"), message, QStringLiteral("error"));
        setErrorMessage(message);
        setStatus(message);
        resetRun();
        return;
    }
    runtime_->rearkProvider = std::make_shared<ReArkToolProvider>(snapshot);
#ifdef REARK_HAS_WUWE_EXECUTION
    runtime_->executionWorkdir = std::make_unique<QTemporaryDir>(
        QDir::temp().filePath(QStringLiteral("ReArk-agent-analysis-XXXXXX")));
    runtime_->executionPromptNote.clear();
    const PythonRuntimeProbe pythonRuntime = PythonRuntimeResolver::resolve(settings.pythonInterpreterPath);
    if (runtime_->executionWorkdir->isValid()
        && pythonRuntime.status == PythonRuntimeProbe::Status::Ok) {
        namespace execution = wuwe::agent::execution;
        const auto workdir = PythonRuntimeResolver::toFilesystemPath(runtime_->executionWorkdir->path());
        ReArkExecutionBackendSelection executionBackend =
            makeReArkExecutionBackend(settings, pythonRuntime, workdir);
        runtime_->executionPromptNote = executionBackend.promptNote;
        if (executionBackend.backend != nullptr) {
            runtime_->executionRuntime = std::make_unique<execution::execution_runtime>(
                std::move(executionBackend.backend),
                rearkExecutionPolicy(workdir),
                &runtime_->executionAuditSink,
                &runtime_->executionApprovalService);
            runtime_->executionProvider = std::make_shared<execution::execution_tool_provider>(
                *runtime_->executionRuntime,
                execution::execution_tool_options {
                    .tool_name = "run_analysis_script",
                    .description = settings.enableRestrictedPythonBackend
                        ? "Run a short Python analysis script in Wuwe restricted_process with bounded output and timeout."
                        : "Run a short Python analysis script with bounded output and timeout in a local controlled process. This is not a file or network security sandbox."
                });
            runtime_->guardedExecutionProvider =
                std::make_shared<ReArkExecutionToolProvider>(runtime_->executionProvider);
        }
    } else if (!runtime_->executionWorkdir->isValid()) {
        runtime_->executionPromptNote = tr("Local Python analysis is unavailable because the temporary execution workdir could not be created.");
    } else {
        runtime_->executionPromptNote = tr("Local Python analysis is unavailable because Python runtime validation failed: %1")
            .arg(pythonRuntime.detail.isEmpty()
                    ? PythonRuntimeResolver::statusLabel(pythonRuntime)
                    : pythonRuntime.detail);
    }
#endif
    runtime_->knowledgeProvider = knowledgeController_ != nullptr
        ? knowledgeController_->createKnowledgeToolProvider()
        : nullptr;
    runtime_->provider = wuwe::compose_tool_providers(runtime_->rearkProvider);
#ifdef REARK_HAS_WUWE_EXECUTION
    if (runtime_->guardedExecutionProvider != nullptr) {
        runtime_->provider->add(runtime_->guardedExecutionProvider);
    }
#endif
    if (runtime_->knowledgeProvider != nullptr && runtime_->knowledgeProvider->provider != nullptr) {
        runtime_->provider->add(runtime_->knowledgeProvider->provider);
    }
    runtime_->stopSource = std::stop_source {};

    appendMessage(QStringLiteral("user"), trimmed);
    appendMessage(QStringLiteral("assistant"), {}, QStringLiteral("streaming"));
    setStatus(tr("Preparing analysis context..."));
    setRunning(true);

    QString systemPrompt =
        QStringLiteral("You are an expert HarmonyOS NEXT application reverse engineering assistant embedded in ReArk. "
            "Use ReArk tools when you need package, source, disassembly, resource, signature, or entry-point data, "
            "When ABC disassembly references literal@0x... values, resolve them with ABC literal evidence instead of guessing from text. "
            "For hardcoded credentials, hashes, crypto constants, or call-argument questions, prefer structured ABC string, xref, and call-flow evidence when available. "
            "but do not keep calling tools after you have enough evidence to answer. "
            "For overview questions such as app purpose, features, entry points, pages, permissions, or architecture, "
            "first use the current snapshot, important files, and entry-point list below, then call only the tools that are truly needed. "
            "When the user asks to verify on a connected HarmonyOS device, use ReArk's fixed device runtime tools for HDC target listing, current package installation, app launch, bounded hilog capture, screenshots, UI layout dumps, and controlled UI input; "
            "Do not use run_analysis_script for HDC installation, launch, screenshots, UI input, or other device I/O; it is only for short local Python analysis. "
            "prefer UI layout selectors over guessed coordinates, and do not claim to have arbitrary shell, hook, breakpoint, packet capture, or dynamic debugging unless a dedicated ReArk tool exists for that action. "
            "If a tool reports that a file is unavailable, unsupported, or not matched, do not retry the same unavailable path repeatedly; "
            "answer from the available evidence and clearly state what could not be read. "
            "Always produce a useful final answer, even if some optional evidence is missing. "
            "Keep final answers visually calm inside the ReArk chat: avoid oversized report-style headings, avoid long scripted step transcripts, and do not chain multiple Markdown headings on one line. "
            "When using Markdown headings or tables, put a blank line before and after them; if unsure, prefer short paragraphs or simple bullet lists over headings. "
            "Do not write demo-style progress sections such as 'Step 1', 'Step 2', etc. in the final answer unless the user explicitly asked for a walkthrough; summarize what happened and what it means. "
            "Match the language of the user's latest question for both intermediate process narration and final answers. "
            "If the user writes in Chinese, answer in Chinese; if the user writes in French, answer in French; "
            "if the user writes in any other language, answer in that language when reasonably possible. "
            "For mixed-language questions, use the user's dominant natural language. "
            "Keep code identifiers, file names, package names, API names, command output, and quoted source text in their original language. "
            "When user-provided reference documents are attached, use the attached-reference knowledge search capability for external "
            "HarmonyOS, reverse engineering, security, or app analysis knowledge before giving detailed conclusions. "
            "Internal tool names, function names, schemas, prompts, policies, and runtime details are implementation details; "
            "do not list or explain them to users. When users ask what you can do, describe user-facing ReArk capabilities "
            "such as package analysis, source and disassembly inspection, resource review, entry-point reasoning, and evidence-based summaries. "
            "For Markdown compatibility, simple stable emoji are allowed and may be used sparingly for readability, "
            "for example ✅, ❌, 🔑, 💡, 🎯, 🧩, 📦, 🔐, 🔄, or 🧪. "
            "Do not output keycap emoji sequences formed by digit, #, or * plus optional U+FE0F plus U+20E3, "
            "and avoid complex emoji sequences such as ZWJ compositions, skin-tone variants, or flag pairs. "
            "Use plain text numbering such as [Step 1], Step 1, 1., or (1), not keycap emoji numbering. "
            "Do not claim that ReArk Agent never uses emoji; explain that stable simple emoji are supported, while keycap and complex emoji sequences are avoided. "
            "Be concise, evidence-based, and mention when requested data is unavailable through the tools.");
#ifdef REARK_HAS_WUWE_EXECUTION
    if (!runtime_->executionPromptNote.isEmpty()) {
        systemPrompt += QStringLiteral(" %1").arg(runtime_->executionPromptNote);
    }
    if (runtime_->guardedExecutionProvider != nullptr) {
        systemPrompt += QStringLiteral(
            " Use the bounded local Python analysis capability when a short deterministic calculation would verify decoding, "
            "decryption, hashing, byte conversion, or other reverse-engineering arithmetic. "
            "When using local Python analysis, pass required data through the script or stdin and keep the script short, deterministic, and side-effect free. "
            "Local Python analysis accepts only code, stdin_text, and timeout_ms; code must be at most 32768 bytes, stdin_text at most 262144 bytes, and timeout_ms at most 5000.");
    }
#endif
    if (knowledgeController_ != nullptr && knowledgeController_->hasReadyReferences()) {
        systemPrompt += QStringLiteral(
            "\n\nAttached reference documents for this chat:\n%1"
            "\nWhen using attached-reference knowledge for these documents, always include filters "
            "{\"reark_session_id\":\"%2\"}.")
            .arg(knowledgeController_->referenceSummaryForPrompt(),
                 knowledgeController_->referenceSessionId());
    }
    systemPrompt += QStringLiteral("\n\nCurrent ReArk snapshot:\n%1")
        .arg(snapshot->packageSummary.isEmpty() ? QStringLiteral("<none>") : snapshot->packageSummary);
    systemPrompt += QStringLiteral("\n\nCurrent important entry points:\n%1")
        .arg(snapshot->entryPoints.isEmpty() ? QStringLiteral("<none>") : snapshot->entryPoints);
    systemPrompt += QStringLiteral("\n\nCurrent file index excerpt:\n%1")
        .arg(snapshot->fileList.isEmpty()
                ? QStringLiteral("<none>")
                : boundedSnapshotText(snapshot->fileList, 12000));
    systemPrompt += responseLanguageInstruction(trimmed);

    QPointer<AgentController> self(this);

#ifdef REARK_HAS_WUWE_REASONING
    namespace reasoning = wuwe::agent::reasoning;

    struct RunProgress {
        std::atomic<int> modelCalls { 0 };
        std::atomic<int> toolCalls { 0 };
        std::atomic<bool> answerStarted { false };
    };
    auto progress = std::make_shared<RunProgress>();

    auto onEvent = [self, progress](const reasoning::reasoning_event& event) {
        if (!self) {
            return;
        }

        if (event.type == reasoning::reasoning_event_type::model_started) {
            progress->modelCalls.fetch_add(1, std::memory_order_relaxed);
        }
        if (event.type == reasoning::reasoning_event_type::tool_started) {
            progress->toolCalls.fetch_add(1, std::memory_order_relaxed);
        }
        if (event.type == reasoning::reasoning_event_type::content_delta
            && progress->answerStarted.exchange(true, std::memory_order_relaxed)) {
            return;
        }

        const int modelCallCount = std::max(
            1,
            progress->modelCalls.load(std::memory_order_relaxed));
        const int toolCallCount = std::max(
            1,
            progress->toolCalls.load(std::memory_order_relaxed));

        const QString status = reasoningEventStatus(event, modelCallCount, toolCallCount);
        const QVariantMap activity = reasoningEventActivity(event);
        if (!status.isEmpty() || !activity.isEmpty()) {
            QMetaObject::invokeMethod(self.data(), [self, status, activity] {
                if (!self) {
                    return;
                }
                if (!activity.isEmpty()) {
                    self->recordActiveAssistantActivity(
                        activity.value(QStringLiteral("type")).toString(),
                        activity.value(QStringLiteral("title")).toString(),
                        activity.value(QStringLiteral("detail")).toString(),
                        activity.value(QStringLiteral("state")).toString());
                }
                if (!status.isEmpty()) {
                    self->setStatus(status);
                }
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
    request.policy = rearkReasoningPolicy(request.input);
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
        QMetaObject::invokeMethod(self.data(), [self, finalText] {
            if (!self) {
                return;
            }
            self->setRunning(false);
            self->finishActiveAssistantMessage(finalText.isEmpty()
                ? AgentController::tr("No response.")
                : finalText);
            self->setStatus(AgentController::tr("Ready"));
            self->resetRun();
            self->startPendingQuestion();
        }, Qt::QueuedConnection);
    };
    options.callbacks.on_error = [self](const reasoning::reasoning_error& error) {
        if (!self) {
            return;
        }
        const bool timedOut = error.code == reasoning::reasoning_error_code::timeout;
        const bool budgetExceeded = isReasoningBudgetExceeded(error.code);
        QString message = reasoningErrorMessage(error);
        if (message.isEmpty()) {
            message = AgentController::tr("Analysis failed.");
        }
        QMetaObject::invokeMethod(self.data(), [self, message, timedOut, budgetExceeded] {
            if (!self) {
                return;
            }
            if (timedOut || budgetExceeded) {
                self->setErrorMessage({});
                self->finishInterruptedAssistantMessage(
                    timedOut
                        ? AgentController::tr("Analysis timed out before the model returned a final answer. Partial output was preserved; you can ask ReArk Agent to continue.")
                        : AgentController::tr("Analysis stopped after reaching the reasoning budget. Partial output was preserved; you can ask ReArk Agent to continue from here."));
                self->setRunning(false);
                self->setStatus(message);
                self->resetRun();
                self->startPendingQuestion();
                return;
            }
            self->setErrorMessage(message);
            self->failActiveAssistantMessage();
            self->setRunning(false);
            self->setStatus(message);
            self->resetRun();
            self->startPendingQuestion();
        }, Qt::QueuedConnection);
    };
    options.callbacks.on_cancelled = [self](const reasoning::reasoning_result& result) {
        if (!self) {
            return;
        }
        const QString message = reasoningCancelledMessage(result);
        QMetaObject::invokeMethod(self.data(), [self, message] {
            if (!self) {
                return;
            }
            self->setStatus(message);
            self->finishActiveAssistantMessage(message);
            self->setRunning(false);
            self->resetRun();
            self->startPendingQuestion();
        }, Qt::QueuedConnection);
    };

    runtime_->reasoningRun = runtime_->reasoningRunner->run_async(
        std::move(request),
        std::move(options));
    return;
#else
    runtime_->runner = std::make_unique<wuwe::llm_agent_runner>(
        *runtime_->client,
        runtime_->provider,
        10);

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
        const QString status = call.name == "run_analysis_script"
            ? AgentController::tr("Running analysis script...")
            : AgentController::tr("Reading analysis data...");
        QMetaObject::invokeMethod(self.data(), [self, status] {
            if (self) {
                self->setStatus(status);
            }
        }, Qt::QueuedConnection);
    };
    options.callbacks.on_tool_result =
        [self](const wuwe::llm_tool_call& call, const wuwe::llm_tool_result& result) {
            if (!self) {
                return;
            }
            const bool ok = !result.error_code;
            const bool analysisScript = call.name == "run_analysis_script";
            QMetaObject::invokeMethod(self.data(), [self, ok, analysisScript] {
                if (self) {
                    if (analysisScript) {
                        self->setStatus(ok
                            ? AgentController::tr("Analysis script completed.")
                            : AgentController::tr("Analysis script failed."));
                    } else {
                        self->setStatus(ok
                            ? AgentController::tr("Analysis data ready.")
                            : AgentController::tr("Analysis data read failed."));
                    }
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
                self->startPendingQuestion();
            }
        }, Qt::QueuedConnection);
    };
    options.callbacks.on_error =
        [self](std::error_code ec, std::string_view message) {
            if (!self) {
                return;
            }
            const QString msg = agentErrorMessage(ec, fromStringView(message));
            const bool timedOut = ec == wuwe::agent::llm_error_code::timeout;
            QMetaObject::invokeMethod(self.data(), [self, msg, timedOut] {
                if (self) {
                    if (timedOut) {
                        self->setErrorMessage({});
                        self->finishInterruptedAssistantMessage(
                            AgentController::tr("Analysis timed out before the model returned a final answer. Partial output was preserved; you can ask ReArk Agent to continue."));
                        self->setStatus(msg);
                        self->setRunning(false);
                        self->resetRun();
                        self->startPendingQuestion();
                        return;
                    }
                    self->setErrorMessage(msg);
                    self->failActiveAssistantMessage();
                    self->setStatus(msg);
                    self->setRunning(false);
                    self->resetRun();
                    self->startPendingQuestion();
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
                self->startPendingQuestion();
            }
        }, Qt::QueuedConnection);
    };

    runtime_->run = runtime_->runner->run_async(std::move(request), std::move(options));
#endif
#endif
}

void AgentController::cancel()
{
    cancelCurrentRun(true);
}

void AgentController::cancelCurrentRun(bool clearPendingQuestion)
{
    if (!available()) {
        if (clearPendingQuestion) {
            pendingQuestion_.clear();
        }
        setRunning(false);
        setStatus(unavailableMessage());
        return;
    }

#ifdef REARK_HAS_WUWE
    if (clearPendingQuestion) {
        pendingQuestion_.clear();
    }
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

void AgentController::startPendingQuestion()
{
    const QString next = std::exchange(pendingQuestion_, {});
    if (next.trimmed().isEmpty()) {
        return;
    }

    QMetaObject::invokeMethod(this, [this, next] {
        ask(next);
    }, Qt::QueuedConnection);
}

void AgentController::newChat()
{
    if (running_) {
        cancel();
        return;
    }
    pendingQuestion_.clear();
    resetRun();
    setRunning(false);
    clearMessages();
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
    if (messageModel_ != nullptr) {
        messageModel_->clear();
    }
    activeAssistantMessage_ = -1;
    emit messagesChanged();
    setTranscript({});
}

void AgentController::appendMessage(const QString& role, const QString& text, const QString& state)
{
    const QString time = QTime::currentTime().toString(QStringLiteral("h:mm AP"));
    QVariantMap message;
    message.insert(QStringLiteral("role"), role);
    message.insert(QStringLiteral("text"), text);
    message.insert(QStringLiteral("state"), state);
    message.insert(QStringLiteral("time"), time);
    messages_.append(message);
    if (messageModel_ != nullptr) {
        messageModel_->appendMessage(role, text, state, time);
    }
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
    if (messageModel_ != nullptr) {
        messageModel_->appendText(activeAssistantMessage_, text);
    }
}

void AgentController::recordActiveAssistantActivity(
    const QString& type,
    const QString& title,
    const QString& detail,
    const QString& state)
{
    if (type.isEmpty() || title.isEmpty()
        || activeAssistantMessage_ < 0 || activeAssistantMessage_ >= messages_.size()) {
        return;
    }

    QVariantMap message = messages_.at(activeAssistantMessage_).toMap();
    if (message.value(QStringLiteral("role")).toString() != QStringLiteral("assistant")) {
        return;
    }

    QVariantList activities = message.value(QStringLiteral("activities")).toList();
    const QString effectiveState = state.isEmpty() ? QStringLiteral("active") : state;
    const QString time = QTime::currentTime().toString(QStringLiteral("h:mm AP"));

    QVariantMap item;
    item.insert(QStringLiteral("type"), type);
    item.insert(QStringLiteral("title"), title);
    item.insert(QStringLiteral("detail"), detail);
    item.insert(QStringLiteral("state"), effectiveState);
    item.insert(QStringLiteral("time"), time);

    if (!activities.isEmpty()) {
        QVariantMap last = activities.last().toMap();
        if (last.value(QStringLiteral("type")).toString() == type) {
            last.insert(QStringLiteral("title"), title);
            last.insert(QStringLiteral("detail"), detail);
            last.insert(QStringLiteral("state"), effectiveState);
            last.insert(QStringLiteral("time"), time);
            activities[activities.size() - 1] = last;
        } else {
            if (last.value(QStringLiteral("state")).toString() == QStringLiteral("active")
                && effectiveState == QStringLiteral("active")) {
                last.insert(QStringLiteral("state"), QStringLiteral("done"));
                activities[activities.size() - 1] = last;
            }
            activities.append(item);
        }
    } else {
        activities.append(item);
    }

    message.insert(QStringLiteral("activities"), activities);
    messages_[activeAssistantMessage_] = message;
    if (messageModel_ != nullptr) {
        messageModel_->setActivities(activeAssistantMessage_, activities);
    }
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
    if (messageModel_ != nullptr) {
        messageModel_->finishStreaming(activeAssistantMessage_, fallbackText);
    }
    activeAssistantMessage_ = -1;
    rebuildTranscript();
}

void AgentController::finishInterruptedAssistantMessage(const QString& notice)
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

    const QString existingText = message.value(QStringLiteral("text")).toString();
    const QString trimmedNotice = notice.trimmed();
    const bool emptyAssistantText = existingText.trimmed().isEmpty();
    const QString finalText = emptyAssistantText || trimmedNotice.isEmpty()
        ? (emptyAssistantText ? trimmedNotice : existingText)
        : existingText + QStringLiteral("\n\n") + trimmedNotice;

    message.insert(QStringLiteral("text"), finalText);
    message.insert(QStringLiteral("state"), QStringLiteral("done"));
    messages_[activeAssistantMessage_] = message;
    if (messageModel_ != nullptr) {
        if (emptyAssistantText) {
            messageModel_->finishStreaming(activeAssistantMessage_, trimmedNotice);
        } else {
            if (!trimmedNotice.isEmpty()) {
                messageModel_->appendText(
                    activeAssistantMessage_,
                    QStringLiteral("\n\n") + trimmedNotice);
            }
            messageModel_->finishStreaming(activeAssistantMessage_, {});
        }
    }
    activeAssistantMessage_ = -1;
    emit messagesChanged();
    rebuildTranscript();
}

void AgentController::failActiveAssistantMessage()
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

    const bool emptyAssistantText = message.value(QStringLiteral("text")).toString().trimmed().isEmpty();
    if (emptyAssistantText) {
        messages_.removeAt(activeAssistantMessage_);
        if (messageModel_ != nullptr) {
            messageModel_->removeMessage(activeAssistantMessage_);
        }
        activeAssistantMessage_ = -1;
        emit messagesChanged();
        rebuildTranscript();
        return;
    }

    message.insert(QStringLiteral("state"), QStringLiteral("error"));
    messages_[activeAssistantMessage_] = message;
    if (messageModel_ != nullptr) {
        messageModel_->failStreaming(activeAssistantMessage_);
    }
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
        const QString state = message.value(QStringLiteral("state")).toString();
        if (role == QStringLiteral("assistant") && state != QStringLiteral("done")) {
            continue;
        }
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
#ifdef REARK_HAS_WUWE_EXECUTION
    runtime_->guardedExecutionProvider.reset();
    runtime_->executionProvider.reset();
    runtime_->executionRuntime.reset();
    runtime_->executionWorkdir.reset();
    runtime_->executionPromptNote.clear();
    runtime_->executionAuditSink.clear();
#endif
    runtime_->rearkProvider.reset();
    runtime_->client.reset();
    runtime_->stopSource = std::stop_source {};
#endif
}

QString AgentController::unavailableMessage() const
{
    return tr("Smart analysis is not available in this ReArk build.");
}
