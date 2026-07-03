#include "controller/AgentRequestRouter.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QVariantMap>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << "FAIL: " << message << '\n';
    return 1;
}

bool expect(bool condition, const QString& message)
{
    return condition || (fail(message), false);
}

QVariantMap message(const QString& role, const QString& text)
{
    QVariantMap item;
    item.insert(QStringLiteral("role"), role);
    item.insert(QStringLiteral("text"), text);
    return item;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const AgentRequestRoute noPackage = routeAgentRequest(
        QStringLiteral("当前应用的基本信息和功能"),
        false,
        {});
    if (!expect(noPackage.localReplyKind == AgentLocalReplyKind::NoLoadedApplication,
            QStringLiteral("current app overview without package should be a local reply"))) {
        return 1;
    }
    if (!expect(noPackage.localReplyText == QStringLiteral("当前没有发现应用。"),
            QStringLiteral("Chinese no-package app overview reply should be short and direct"))) {
        return 1;
    }
    if (!expect(!noPackage.usesModel(),
            QStringLiteral("no-package app overview should not use the model"))) {
        return 1;
    }
    const AgentRequestRoute noPackageFocusedStatic = routeAgentRequest(
        QStringLiteral("这个字符串如何被使用"),
        false,
        {});
    if (!expect(noPackageFocusedStatic.localReplyKind == AgentLocalReplyKind::NoLoadedApplication,
            QStringLiteral("package-dependent focused static analysis without a package should be a local reply"))) {
        return 1;
    }
    const AgentRequestRoute noPackageGenericAnalysis = routeAgentRequest(
        QStringLiteral("分析一下"),
        false,
        {});
    if (!expect(noPackageGenericAnalysis.localReplyKind == AgentLocalReplyKind::NoLoadedApplication,
            QStringLiteral("generic package analysis without a package should be a local reply"))) {
        return 1;
    }

    const AgentRequestRoute loadedPackage = routeAgentRequest(
        QStringLiteral("当前应用的基本信息和功能"),
        true,
        {});
    if (!expect(loadedPackage.usesModel(),
            QStringLiteral("loaded-package app overview should use the model with package context"))) {
        return 1;
    }
    if (!expect(loadedPackage.taskProfile.mode == AgentTaskMode::PackageOverview,
            QStringLiteral("loaded-package app overview should use the lightweight package overview mode"))) {
        return 1;
    }
    if (!expect(loadedPackage.taskProfile.maxPackageSummaryChars == 0
            && loadedPackage.taskProfile.maxFileListChars == 0
            && !loadedPackage.taskProfile.deviceRuntimeToolsEnabled,
            QStringLiteral("package overview should use tools instead of pre-attached package context"))) {
        return 1;
    }
    const AgentRequestRoute loadedProject = routeAgentRequest(
        QStringLiteral("当前项目的基本信息和功能"),
        true,
        {});
    if (!expect(loadedProject.taskProfile.mode == AgentTaskMode::PackageOverview,
            QStringLiteral("current project overview wording should use package overview mode"))) {
        return 1;
    }
    const AgentRequestRoute genericAnalysis = routeAgentRequest(
        QStringLiteral("分析一下"),
        true,
        {});
    if (!expect(genericAnalysis.taskProfile.mode == AgentTaskMode::PackageOverview,
            QStringLiteral("generic loaded-package analysis should use package overview evidence mode"))) {
        return 1;
    }
    if (!expect(!agentTaskUsesPlainModelOnly(AgentTaskMode::PackageOverview)
            && agentTaskUsesPlainModelOnly(AgentTaskMode::LightweightChat)
            && !agentTaskUsesPlainModelOnly(AgentTaskMode::FocusedStaticAnalysis),
            QStringLiteral("only lightweight chat should bypass tool-capable agent runs"))) {
        return 1;
    }

    const AgentRequestRoute greeting = routeAgentRequest(QStringLiteral("hi"), false, {});
    if (!expect(greeting.usesModel(),
            QStringLiteral("greetings should use the lightweight model path, not a local canned reply"))) {
        return 1;
    }
    if (!expect(greeting.taskProfile.mode == AgentTaskMode::LightweightChat,
            QStringLiteral("greetings should use lightweight chat mode"))) {
        return 1;
    }
    if (!expect(greeting.taskProfile.maxPackageSummaryChars == 0
            && greeting.taskProfile.maxEntryPointChars == 0
            && greeting.taskProfile.maxFileListChars == 0
            && !greeting.taskProfile.deviceRuntimeToolsEnabled,
            QStringLiteral("greetings should not attach package context or device tools"))) {
        return 1;
    }

    const AgentRequestRoute loadedGreeting = routeAgentRequest(QStringLiteral("hi"), true, {});
    if (!expect(loadedGreeting.usesModel()
            && loadedGreeting.taskProfile.mode == AgentTaskMode::LightweightChat,
            QStringLiteral("loaded-package greetings should still use lightweight chat"))) {
        return 1;
    }
    if (!expect(loadedGreeting.taskProfile.maxPackageSummaryChars == 0
            && loadedGreeting.taskProfile.maxEntryPointChars == 0
            && loadedGreeting.taskProfile.maxFileListChars == 0,
            QStringLiteral("loaded-package greetings should not attach current target context"))) {
        return 1;
    }

    const AgentRequestRoute chineseGreeting = routeAgentRequest(QStringLiteral("你好"), false, {});
    if (!expect(chineseGreeting.usesModel(),
            QStringLiteral("Chinese greetings should use the lightweight model path"))) {
        return 1;
    }
    if (!expect(chineseGreeting.taskProfile.mode == AgentTaskMode::LightweightChat,
            QStringLiteral("Chinese greetings should use lightweight chat mode"))) {
        return 1;
    }
    if (!expect(chineseGreeting.taskProfile.maxPackageSummaryChars == 0
            && chineseGreeting.taskProfile.maxEntryPointChars == 0
            && chineseGreeting.taskProfile.maxFileListChars == 0,
            QStringLiteral("Chinese greetings should not attach package context"))) {
        return 1;
    }

    const AgentRequestRoute thanks = routeAgentRequest(QStringLiteral("thanks"), false, {});
    if (!expect(thanks.usesModel(),
            QStringLiteral("thanks should remain a lightweight model response"))) {
        return 1;
    }
    if (!expect(thanks.taskProfile.mode == AgentTaskMode::LightweightChat,
            QStringLiteral("thanks should use lightweight chat mode"))) {
        return 1;
    }
    if (!expect(thanks.taskProfile.maxHistoryMessages == 1
            && thanks.taskProfile.maxPackageSummaryChars == 0,
            QStringLiteral("lightweight chat should minimize history and package context"))) {
        return 1;
    }

    const AgentRequestRoute ctf = routeAgentRequest(
        QStringLiteral("帮我破解这个 secretKey 的 encode 校验"),
        true,
        {});
    if (!expect(ctf.taskProfile.mode == AgentTaskMode::StaticFastPath,
            QStringLiteral("static CTF request should use static fast path"))) {
        return 1;
    }
    if (!expect(!ctf.taskProfile.deviceRuntimeToolsEnabled,
            QStringLiteral("static CTF request should not enable device runtime tools by default"))) {
        return 1;
    }
    const AgentRequestRoute ctfAnalysis = routeAgentRequest(
        QStringLiteral("分析一下这个 secretKey 的 encode 校验"),
        true,
        {});
    if (!expect(ctfAnalysis.taskProfile.mode == AgentTaskMode::StaticFastPath,
            QStringLiteral("specific secretKey analysis should not be downgraded to package overview"))) {
        return 1;
    }

    const AgentRequestRoute focusedString = routeAgentRequest(
        QStringLiteral("这个字符串如何被使用"),
        true,
        {});
    if (!expect(focusedString.taskProfile.mode == AgentTaskMode::FocusedStaticAnalysis,
            QStringLiteral("string usage questions should use focused static analysis"))) {
        return 1;
    }
    if (!expect(!focusedString.taskProfile.deviceRuntimeToolsEnabled
            && focusedString.taskProfile.maxHistoryMessages < 8
            && focusedString.taskProfile.maxFileListChars > 0,
            QStringLiteral("focused static analysis should use bounded context without device tools"))) {
        return 1;
    }

    const AgentRequestRoute focusedEntry = routeAgentRequest(
        QStringLiteral("入口逻辑怎么走"),
        true,
        {});
    if (!expect(focusedEntry.taskProfile.mode == AgentTaskMode::FocusedStaticAnalysis,
            QStringLiteral("entry logic questions should use focused static analysis"))) {
        return 1;
    }

    const AgentRequestRoute device = routeAgentRequest(
        QStringLiteral("安装当前 HAP 到设备并截图"),
        true,
        {});
    if (!expect(device.taskProfile.mode == AgentTaskMode::DeviceRuntime,
            QStringLiteral("device request should use device runtime mode"))) {
        return 1;
    }
    if (!expect(device.taskProfile.deviceRuntimeToolsEnabled,
            QStringLiteral("device request should enable device runtime tools"))) {
        return 1;
    }

    QVariantList history;
    history.append(message(QStringLiteral("assistant"), QStringLiteral("静态验证已完成，是否继续连接设备检验一遍？")));
    history.append(message(QStringLiteral("user"), QStringLiteral("继续")));
    const AgentRequestRoute continuation = routeAgentRequest(
        QStringLiteral("继续"),
        true,
        history);
    if (!expect(continuation.taskProfile.mode == AgentTaskMode::DeviceRuntime,
            QStringLiteral("affirmative continuation after runtime handoff should use device runtime mode"))) {
        return 1;
    }

    QTextStream(stdout) << "Agent request router tests passed\n";
    return 0;
}
