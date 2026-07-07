#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QTextStream>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << "FAIL: " << message << '\n';
    return 1;
}

bool containsAll(const QString& text, const QStringList& needles, QString* missing)
{
    for (const QString& needle : needles) {
        if (!text.contains(needle)) {
            if (missing != nullptr) {
                *missing = needle;
            }
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QFile file(QStringLiteral(REARK_AGENT_CONTROLLER_SOURCE));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("could not open AgentController.cpp: %1").arg(file.errorString()));
    }

    QString source = QString::fromUtf8(file.readAll());
    QFile routerFile(QStringLiteral(REARK_SOURCE_ROOT "/src/controller/AgentRequestRouter.cpp"));
    if (!routerFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("could not open AgentRequestRouter.cpp: %1").arg(routerFile.errorString()));
    }
    source += QStringLiteral("\n");
    source += QString::fromUtf8(routerFile.readAll());
    QFile rootCmake(QStringLiteral(REARK_SOURCE_ROOT "/CMakeLists.txt"));
    if (!rootCmake.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("could not open root CMakeLists.txt: %1").arg(rootCmake.errorString()));
    }
    const QString rootCmakeSource = QString::fromUtf8(rootCmake.readAll());

    QFile agentSettings(QStringLiteral(REARK_SOURCE_ROOT "/src/controller/AgentSettings.cpp"));
    if (!agentSettings.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("could not open AgentSettings.cpp: %1").arg(agentSettings.errorString()));
    }
    const QString agentSettingsSource = QString::fromUtf8(agentSettings.readAll());

    QFile pythonResolver(QStringLiteral(REARK_SOURCE_ROOT "/src/controller/PythonRuntimeResolver.cpp"));
    if (!pythonResolver.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("could not open PythonRuntimeResolver.cpp: %1").arg(pythonResolver.errorString()));
    }
    const QString pythonResolverSource = QString::fromUtf8(pythonResolver.readAll());
    QFile smartAnalysisPage(QStringLiteral(REARK_SOURCE_ROOT "/src/ui/qml/ReArk/SmartAnalysisPage.qml"));
    if (!smartAnalysisPage.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("could not open SmartAnalysisPage.qml: %1").arg(smartAnalysisPage.errorString()));
    }
    const QString smartAnalysisPageSource = QString::fromUtf8(smartAnalysisPage.readAll());
    QFile markdownMessage(QStringLiteral(REARK_SOURCE_ROOT "/src/ui/qml/ReArk/MarkdownMessage.qml"));
    if (!markdownMessage.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("could not open MarkdownMessage.qml: %1").arg(markdownMessage.errorString()));
    }
    const QString markdownMessageSource = QString::fromUtf8(markdownMessage.readAll());

    QString missing;
    const QStringList requiredContract = {
        QStringLiteral("sample-independent"),
        QStringLiteral("never rely on a contest name"),
        QStringLiteral("Do not hand-convert hexadecimal constants"),
        QStringLiteral("encode(candidate) == verifier"),
        QStringLiteral("Spot checks or random samples are not enough"),
        QStringLiteral("spot checks"),
        QStringLiteral("complete answer must include the concrete candidate value"),
        QStringLiteral("do not end with only a script for the user to run"),
        QStringLiteral("Local Python analysis is available in this run through run_analysis_script"),
        QStringLiteral("Do not tell the user to run a local Python script"),
        QStringLiteral("do not infer or invent 'no Python environment'"),
        QStringLiteral("没有 Python 执行环境"),
        QStringLiteral("reconcile it with scratchpad and Python-session state"),
        QStringLiteral("Runtime verification is a closed loop"),
        QStringLiteral("Do not narrate planned tool use"),
        QStringLiteral("call run_analysis_script immediately"),
        QStringLiteral("makeStaticFastPathFirstToolAgentComplete"),
        QStringLiteral("static_fast_path_first_tool_required"),
        QStringLiteral("llm_tool_choice_mode::required"),
        QStringLiteral("isToolChoiceUnsupportedResponse"),
        QStringLiteral("firstRequest.tool_choice.reset"),
        QStringLiteral("Host Python preflight for static CTF analysis succeeded"),
        QStringLiteral("static_fast_path_python_preflight_failed"),
        QStringLiteral("answerHasFullStaticVerifierAssertion"),
        QStringLiteral("explicitVerifierEqualityPattern"),
        QStringLiteral("full verifier match"),
        QStringLiteral("重加密验证"),
        QStringLiteral("密文完全一致"),
        QStringLiteral("完整 verifier 断言"),
        QStringLiteral("Static CTF execution contract: do not answer in prose"),
        QStringLiteral("Call run_analysis_script now"),
        QStringLiteral("scriptToolAvailable"),
        QStringLiteral("static_fast_path_python_unavailable"),
        QStringLiteral("当前工具列表没有注册 run_analysis_script"),
        QStringLiteral("Windows App Execution Aliases"),
        QStringLiteral("scriptRequest.tools = scriptTools"),
        QStringLiteral("requires_tools = profile.mode == AgentTaskMode::StaticFastPath"),
        QStringLiteral("clear_hilog before decisive interactions"),
        QStringLiteral("fresh read_hilog output"),
        QStringLiteral("When a concrete candidate has already been decoded and device runtime tools are enabled"),
        QStringLiteral("generated files, or app state"),
        QStringLiteral("A generic Toast mount"),
        QStringLiteral("不等于业务成功"),
        QStringLiteral("inspect_app_files/read_device_file"),
        QStringLiteral("expected file contents from read_device_file"),
        QStringLiteral("Do not assume filesDir"),
        QStringLiteral("If static evidence and runtime behavior disagree"),
        QStringLiteral("A verifier-patched HAP proves the success branch"),
        QStringLiteral("derive the replacement from the same verified transform"),
        QStringLiteral("distinguish success-branch validation from exact delivery"),
        QStringLiteral("Do not hand off full-secret device validation to manual user input"),
        QStringLiteral("install_current_hap_with_abc_string_rewrite"),
        QStringLiteral("十六进制到十进制的换算不一致"),
        QStringLiteral("需要复核"),
        QStringLiteral("finalAnswerQualityNotice"),
        QStringLiteral("deviceRuntimeContinuation"),
        QStringLiteral("finalAnswerRuntimeHandoffNotice"),
        QStringLiteral("agentIsAffirmativeDeviceVerificationFollowUp"),
        QStringLiteral("startIndex = index - 1"),
        QStringLiteral("继续真机"),
        QStringLiteral("install_current_hap_with_abc_string_rewrite"),
        QStringLiteral("dump_ui_layout"),
        QStringLiteral("hasMetaReviewIntent"),
        QStringLiteral("怎么回事"),
        QStringLiteral("哪里存在问题"),
        QStringLiteral("正确口令是"),
        QStringLiteral("重新破解"),
        QStringLiteral("clear_hilog"),
        QStringLiteral("inspect_app_files"),
        QStringLiteral("read_device_file"),
        QStringLiteral("是否继续连接设备检验一遍"),
        QStringLiteral("static verification only"),
        QStringLiteral("device verification pending"),
        QStringLiteral("do not imply device validation"),
        QStringLiteral("routeAgentRequest"),
        QStringLiteral("NoLoadedApplication"),
        QStringLiteral("当前没有发现应用。"),
        QStringLiteral("Do not inspect or summarize the current package for lightweight chat"),
        QStringLiteral("HAP/APP package analysis"),
        QStringLiteral("Device Runtime"),
        QStringLiteral("ABC迹索"),
        QStringLiteral("ABC evidence"),
        QStringLiteral("UI automation"),
        QStringLiteral("install verification"),
        QStringLiteral("LightweightChat"),
        QStringLiteral("PackageOverview"),
        QStringLiteral("FocusedStaticAnalysis"),
        QStringLiteral("focused_static_analysis"),
        QStringLiteral("Focused static analysis is active"),
        QStringLiteral("package_overview"),
        QStringLiteral("agentTaskUsesPlainModelOnly"),
        QStringLiteral("!plainModelOnly && decompilerController_ != nullptr"),
        QStringLiteral("runtime_->provider = !plainModelOnly"),
        QStringLiteral("当前项目"),
        QStringLiteral("Use ReArk tools to inspect the currently loaded target"),
        QStringLiteral("summarize_current_target and inspect_entry_points"),
        QStringLiteral("First sentence must be a direct function-level conclusion"),
        QStringLiteral("When the provider streams a user-visible reasoning summary"),
        QStringLiteral("provider-visible reasoning summaries"),
        QStringLiteral("Any provider-visible reasoning summary, thinking summary, intermediate narration, and final answer must be in Chinese"),
        QStringLiteral("Any provider-visible reasoning summary, thinking summary, intermediate narration, and final answer must be in English"),
        QStringLiteral("Do not fabricate progress"),
        QStringLiteral("result.final_response.content"),
        QStringLiteral("on_reasoning_delta"),
        QStringLiteral("on_reasoning_done"),
        QStringLiteral("queueAssistantReasoningDelta"),
        QStringLiteral("replaceExistingText"),
        QStringLiteral("setErrorMessage({})"),
        QStringLiteral("finishInterruptedAssistantMessage({})")
    };
    if (!containsAll(source, requiredContract, &missing)) {
        return fail(QStringLiteral("Agent prompt contract is missing required generic rule: %1").arg(missing));
    }

    const QString folded = source.toCaseFolded();
    if (folded.contains(QStringLiteral("shctf"))) {
        return fail(QStringLiteral("Agent control logic must not contain sample-specific contest names"));
    }
    if (source.contains(QStringLiteral("ReArkSafe123"))) {
        return fail(QStringLiteral("Agent prompt examples should not hardcode a specific probe password"));
    }
    if (source.contains(QStringLiteral("HAP/HSP/APP"))) {
        return fail(QStringLiteral("Agent capability copy must not claim unsupported HSP package support"));
    }
    if (source.contains(QStringLiteral("welcomeResponse"))
        || source.contains(QStringLiteral("AgentLocalReplyKind::Welcome"))
        || source.contains(QStringLiteral("localReplyKind = AgentLocalReplyKind::Welcome"))) {
        return fail(QStringLiteral("Greetings must use lightweight model chat, not a local canned welcome reply"));
    }
    if (source.contains(QStringLiteral("Host answer guard"))
        || source.contains(QStringLiteral("Answer Needs Verification"))) {
        return fail(QStringLiteral("User-facing answer guards must use natural wording, not internal host diagnostics"));
    }
    if (source.contains(QStringLiteral("Package overview evidence for this request"))
        || source.contains(QStringLiteral("buildPackageOverviewEvidence"))
        || rootCmakeSource.contains(QStringLiteral("AgentEvidenceAssembler"))) {
        return fail(QStringLiteral("Package overview must use Agent tools, not preassembled host evidence"));
    }
    if (source.contains(QStringLiteral("affirmativeContinuation"))) {
        return fail(QStringLiteral("A bare affirmative continuation must not suppress static verifier quality guards"));
    }
    if (source.contains(QStringLiteral("reasoningEventVisibleThought"))
        || source.contains(QStringLiteral("好的，我来分析当前应用"))
        || source.contains(QStringLiteral("我正在进行第 %1 轮模型分析"))
        || source.contains(QStringLiteral("稍后会用整理好的答案替换这些过程内容"))) {
        return fail(QStringLiteral("Agent must not fake model thinking with hardcoded visible-thought narration"));
    }
    if (!markdownMessageSource.contains(QStringLiteral("suppressRawMarkdownFallback"))
        || !markdownMessageSource.contains(QStringLiteral("rawMarkdownSuppressed"))
        || !smartAnalysisPageSource.contains(QStringLiteral("suppressRawMarkdownFallback: !messageDelegate.userMessage"))
        || !smartAnalysisPageSource.contains(QStringLiteral("messageReasoningText"))
        || !smartAnalysisPageSource.contains(QStringLiteral("visibleMessageText"))
        || !smartAnalysisPageSource.contains(QStringLiteral("showReasoningText"))
        || !source.contains(QStringLiteral("reasoning_event_type::reasoning_delta"))
        || !source.contains(QStringLiteral("reasoning_event_type::reasoning_completed"))
        || !smartAnalysisPageSource.contains(QStringLiteral("Generating answer..."))) {
        return fail(QStringLiteral("Streaming assistant output must not expose raw Markdown as fake thinking"));
    }
    if (smartAnalysisPageSource.contains(QStringLiteral("Still analyzing..."))) {
        return fail(QStringLiteral("Streaming answer text must be labelled as answer generation, not hidden thinking"));
    }
    if (source.contains(QStringLiteral("QStringLiteral(\"problem\")"))
        || source.contains(QStringLiteral("QStringLiteral(\"issue\")"))) {
        return fail(QStringLiteral("Meta-review routing must not treat generic English CTF problem/issue wording as critique"));
    }
    if (!rootCmakeSource.contains(QStringLiteral(
            "option(REARK_ENABLE_WUWE_EXECUTION \"Enable Wuwe execution backend integration when the installed Wuwe package supports it\" ON)"))) {
        return fail(QStringLiteral("Wuwe execution integration must default to ON when the installed package supports it"));
    }
    if (source.contains(QStringLiteral("make_execution_backend_registry"))) {
        return fail(QStringLiteral("Agent execution setup must not depend on the unavailable Wuwe registry factory"));
    }
    if (!source.contains(QStringLiteral("make_controlled_process_backend"))) {
        return fail(QStringLiteral("Agent execution setup must create the controlled_process backend directly"));
    }
    if (!agentSettingsSource.contains(QStringLiteral("llm_provider_registry"))
        || !agentSettingsSource.contains(QStringLiteral("list_llm_providers"))
        || !agentSettingsSource.contains(QStringLiteral("make_default_llm_config"))) {
        return fail(QStringLiteral("Agent settings provider metadata must come from Wuwe registry when Wuwe is available"));
    }
    if (source.contains(QStringLiteral("llm_provider_registry"))
        || source.contains(QStringLiteral("normalize_llm_client_config"))) {
        return fail(QStringLiteral("Agent runtime should create Wuwe LLM providers through the provider factory only"));
    }
    if (!containsAll(pythonResolverSource, {
            QStringLiteral("installedPythonCandidates"),
            QStringLiteral("LOCALAPPDATA"),
            QStringLiteral("Programs/Python"),
            QStringLiteral("Python*"),
            QStringLiteral("python.exe")
        }, &missing)) {
        return fail(QStringLiteral("Python resolver is missing installed interpreter discovery contract: %1").arg(missing));
    }

    QTextStream(stdout) << "Agent prompt contract tests passed\n";
    return 0;
}
