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

    const QString source = QString::fromUtf8(file.readAll());
    QFile rootCmake(QStringLiteral(REARK_SOURCE_ROOT "/CMakeLists.txt"));
    if (!rootCmake.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("could not open root CMakeLists.txt: %1").arg(rootCmake.errorString()));
    }
    const QString rootCmakeSource = QString::fromUtf8(rootCmake.readAll());

    QFile pythonResolver(QStringLiteral(REARK_SOURCE_ROOT "/src/controller/PythonRuntimeResolver.cpp"));
    if (!pythonResolver.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("could not open PythonRuntimeResolver.cpp: %1").arg(pythonResolver.errorString()));
    }
    const QString pythonResolverSource = QString::fromUtf8(pythonResolver.readAll());

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
        QStringLiteral("isAffirmativeDeviceVerificationFollowUp"),
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
        QStringLiteral("do not imply device validation")
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
    if (source.contains(QStringLiteral("Host answer guard"))
        || source.contains(QStringLiteral("Answer Needs Verification"))) {
        return fail(QStringLiteral("User-facing answer guards must use natural wording, not internal host diagnostics"));
    }
    if (source.contains(QStringLiteral("affirmativeContinuation"))) {
        return fail(QStringLiteral("A bare affirmative continuation must not suppress static verifier quality guards"));
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
