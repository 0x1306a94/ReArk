#include "controller/AgentRequestRouter.h"

#include <QStringList>

namespace {

bool containsAnyTerm(const QString& foldedText, const QStringList& terms)
{
    for (const QString& term : terms) {
        if (!term.isEmpty() && foldedText.contains(term.toCaseFolded())) {
            return true;
        }
    }
    return false;
}

bool containsCjkText(const QString& text)
{
    for (const QChar ch : text) {
        const ushort unicode = ch.unicode();
        if ((unicode >= 0x3400 && unicode <= 0x9fff)
            || (unicode >= 0xf900 && unicode <= 0xfaff)) {
            return true;
        }
    }
    return false;
}

bool hasGreetingIntent(const QString& question)
{
    const QString folded = question.trimmed().toCaseFolded();
    if (folded.isEmpty() || folded.size() > 24) {
        return false;
    }

    return folded == QStringLiteral("hi")
        || folded == QStringLiteral("hello")
        || folded == QStringLiteral("hey")
        || folded == QStringLiteral("你好")
        || folded == QStringLiteral("您好")
        || folded == QStringLiteral("在吗")
        || folded == QStringLiteral("在么")
        || folded == QStringLiteral("嗨")
        || folded == QStringLiteral("哈喽");
}

bool hasLightweightChatIntent(const QString& question)
{
    const QString folded = question.trimmed().toCaseFolded();
    if (folded.isEmpty() || folded.size() > 24) {
        return false;
    }

    return hasGreetingIntent(question)
        || folded == QStringLiteral("谢谢")
        || folded == QStringLiteral("多谢")
        || folded == QStringLiteral("thanks")
        || folded == QStringLiteral("thank you");
}

bool asksAboutCurrentLoadedApplication(const QString& question)
{
    const QString folded = question.trimmed().toCaseFolded();
    if (folded.isEmpty()) {
        return false;
    }

    const bool currentTarget = containsAnyTerm(folded, {
        QStringLiteral("当前应用"),
        QStringLiteral("当前app"),
        QStringLiteral("当前 app"),
        QStringLiteral("这个应用"),
        QStringLiteral("此应用"),
        QStringLiteral("这个app"),
        QStringLiteral("这个 app"),
        QStringLiteral("已加载应用"),
        QStringLiteral("加载的应用"),
        QStringLiteral("目标应用"),
        QStringLiteral("当前项目"),
        QStringLiteral("当前工程"),
        QStringLiteral("这个项目"),
        QStringLiteral("此项目"),
        QStringLiteral("已加载项目"),
        QStringLiteral("加载的项目"),
        QStringLiteral("当前包"),
        QStringLiteral("应用包"),
        QStringLiteral("current app"),
        QStringLiteral("current application"),
        QStringLiteral("current project"),
        QStringLiteral("loaded project"),
        QStringLiteral("target project"),
        QStringLiteral("loaded app"),
        QStringLiteral("loaded package"),
        QStringLiteral("target app"),
        QStringLiteral("target package")
    });
    const bool overviewIntent = containsAnyTerm(folded, {
        QStringLiteral("基本信息"),
        QStringLiteral("基础信息"),
        QStringLiteral("分析"),
        QStringLiteral("看一下"),
        QStringLiteral("看看"),
        QStringLiteral("功能"),
        QStringLiteral("用途"),
        QStringLiteral("作用"),
        QStringLiteral("做什么"),
        QStringLiteral("是什么"),
        QStringLiteral("概况"),
        QStringLiteral("概览"),
        QStringLiteral("简介"),
        QStringLiteral("信息"),
        QStringLiteral("basic info"),
        QStringLiteral("basic information"),
        QStringLiteral("analyze"),
        QStringLiteral("analyse"),
        QStringLiteral("features"),
        QStringLiteral("functionality"),
        QStringLiteral("what does"),
        QStringLiteral("what is"),
        QStringLiteral("overview"),
        QStringLiteral("summary")
    });

    return currentTarget && overviewIntent;
}

bool hasGenericPackageAnalysisIntent(const QString& question)
{
    const QString folded = question.trimmed().toCaseFolded();
    if (folded.isEmpty() || folded.size() > 48) {
        return false;
    }
    if (agentHasStaticCtfIntent(question) || agentHasExplicitDeviceRuntimeIntent(question)) {
        return false;
    }
    return containsAnyTerm(folded, {
        QStringLiteral("分析一下"),
        QStringLiteral("分析下"),
        QStringLiteral("帮我分析"),
        QStringLiteral("看一下"),
        QStringLiteral("看看"),
        QStringLiteral("分析这个应用"),
        QStringLiteral("分析当前应用"),
        QStringLiteral("analyze this app"),
        QStringLiteral("analyse this app"),
        QStringLiteral("analyze current app"),
        QStringLiteral("analyse current app")
    });
}

bool hasFocusedStaticAnalysisIntent(const QString& question)
{
    const QString folded = question.trimmed().toCaseFolded();
    if (folded.isEmpty()) {
        return false;
    }

    return containsAnyTerm(folded, {
        QStringLiteral("入口逻辑"),
        QStringLiteral("入口点"),
        QStringLiteral("启动路径"),
        QStringLiteral("页面逻辑"),
        QStringLiteral("主要页面"),
        QStringLiteral("源码"),
        QStringLiteral("反汇编"),
        QStringLiteral("字节码"),
        QStringLiteral("abc"),
        QStringLiteral("abc迹索"),
        QStringLiteral("字符串"),
        QStringLiteral("字面量"),
        QStringLiteral("引用"),
        QStringLiteral("交叉引用"),
        QStringLiteral("调用链"),
        QStringLiteral("调用参数"),
        QStringLiteral("参数流"),
        QStringLiteral("校验逻辑"),
        QStringLiteral("验证逻辑"),
        QStringLiteral("怎么走"),
        QStringLiteral("如何被使用"),
        QStringLiteral("在哪里被使用"),
        QStringLiteral("哪里被使用"),
        QStringLiteral("方法"),
        QStringLiteral("函数"),
        QStringLiteral("entry point"),
        QStringLiteral("entry points"),
        QStringLiteral("startup path"),
        QStringLiteral("source"),
        QStringLiteral("disassembly"),
        QStringLiteral("bytecode"),
        QStringLiteral("abc evidence"),
        QStringLiteral("string"),
        QStringLiteral("literal"),
        QStringLiteral("xrefs"),
        QStringLiteral("cross reference"),
        QStringLiteral("call chain"),
        QStringLiteral("call flow"),
        QStringLiteral("call argument"),
        QStringLiteral("argument flow"),
        QStringLiteral("verifier logic"),
        QStringLiteral("validation logic"),
        QStringLiteral("where is"),
        QStringLiteral("how is")
    });
}

QString noLoadedApplicationResponse(const QString& question)
{
    if (containsCjkText(question)) {
        return QStringLiteral("当前没有发现应用。");
    }
    return QStringLiteral("No application is currently loaded.");
}

bool hasMetaReviewIntent(const QString& question)
{
    const QString folded = question.toCaseFolded();
    if (containsAnyTerm(folded, {
            QStringLiteral("反思"),
            QStringLiteral("复盘"),
            QStringLiteral("怎么回事"),
            QStringLiteral("为什么"),
            QStringLiteral("原因"),
            QStringLiteral("问题在哪"),
            QStringLiteral("哪里存在问题"),
            QStringLiteral("哪里有问题"),
            QStringLiteral("哪里错"),
            QStringLiteral("错在哪"),
            QStringLiteral("流程问题"),
            QStringLiteral("总结问题"),
            QStringLiteral("任务完成得不彻底"),
            QStringLiteral("上下文"),
            QStringLiteral("忘了"),
            QStringLiteral("糊涂"),
            QStringLiteral("越改越弱"),
            QStringLiteral("更蠢"),
            QStringLiteral("不专业"),
            QStringLiteral("不稳定"),
            QStringLiteral("不自然"),
            QStringLiteral("what happened"),
            QStringLiteral("what went wrong"),
            QStringLiteral("where did you go wrong"),
            QStringLiteral("reflect"),
            QStringLiteral("postmortem"),
            QStringLiteral("root cause"),
            QStringLiteral("why"),
            QStringLiteral("what is the issue"),
            QStringLiteral("what's the issue"),
            QStringLiteral("what is the problem"),
            QStringLiteral("what's the problem")
        })) {
        return true;
    }

    const bool correctionOrReferenceAnswer = containsAnyTerm(folded, {
        QStringLiteral("不是这个"),
        QStringLiteral("不是这个口令"),
        QStringLiteral("不对"),
        QStringLiteral("又错"),
        QStringLiteral("还是错"),
        QStringLiteral("正确口令是"),
        QStringLiteral("正确密码是"),
        QStringLiteral("正确 flag 是"),
        QStringLiteral("正确flag是"),
        QStringLiteral("actual password"),
        QStringLiteral("correct password"),
        QStringLiteral("correct flag"),
        QStringLiteral("not this"),
        QStringLiteral("wrong again")
    });
    if (!correctionOrReferenceAnswer) {
        return false;
    }

    const bool explicitFreshSolveRequest = containsAnyTerm(folded, {
        QStringLiteral("重新破解"),
        QStringLiteral("继续破解"),
        QStringLiteral("再破解"),
        QStringLiteral("重算"),
        QStringLiteral("重新计算"),
        QStringLiteral("重新解码"),
        QStringLiteral("继续解码"),
        QStringLiteral("重新分析"),
        QStringLiteral("继续分析"),
        QStringLiteral("solve again"),
        QStringLiteral("crack again"),
        QStringLiteral("decode again"),
        QStringLiteral("recompute"),
        QStringLiteral("reanalyze")
    });
    return !explicitFreshSolveRequest;
}

AgentTaskProfile staticFastPathProfile()
{
    AgentTaskProfile profile;
    profile.mode = AgentTaskMode::StaticFastPath;
    profile.deviceRuntimeToolsEnabled = false;
    profile.maxHistoryMessages = 2;
    profile.maxHistoryCharsPerMessage = 1200;
    profile.maxPackageSummaryChars = 4000;
    profile.maxEntryPointChars = 6000;
    profile.maxFileListChars = 4000;
    return profile;
}

AgentTaskProfile deviceRuntimeProfile()
{
    AgentTaskProfile profile;
    profile.mode = AgentTaskMode::DeviceRuntime;
    profile.deviceRuntimeToolsEnabled = true;
    profile.maxHistoryMessages = 10;
    profile.maxHistoryCharsPerMessage = 3000;
    profile.maxPackageSummaryChars = 10000;
    profile.maxEntryPointChars = 10000;
    profile.maxFileListChars = 10000;
    return profile;
}

AgentTaskProfile packageOverviewProfile()
{
    AgentTaskProfile profile;
    profile.mode = AgentTaskMode::PackageOverview;
    profile.deviceRuntimeToolsEnabled = false;
    profile.maxHistoryMessages = 2;
    profile.maxHistoryCharsPerMessage = 800;
    profile.maxPackageSummaryChars = 0;
    profile.maxEntryPointChars = 0;
    profile.maxFileListChars = 0;
    return profile;
}

AgentTaskProfile focusedStaticAnalysisProfile()
{
    AgentTaskProfile profile;
    profile.mode = AgentTaskMode::FocusedStaticAnalysis;
    profile.deviceRuntimeToolsEnabled = false;
    profile.maxHistoryMessages = 4;
    profile.maxHistoryCharsPerMessage = 1800;
    profile.maxPackageSummaryChars = 7000;
    profile.maxEntryPointChars = 10000;
    profile.maxFileListChars = 9000;
    return profile;
}

AgentTaskProfile classifyAgentTask(const QString& question, bool forceDeviceRuntime)
{
    AgentTaskProfile profile;
    if (forceDeviceRuntime) {
        return deviceRuntimeProfile();
    }
    if (hasLightweightChatIntent(question)) {
        profile.mode = AgentTaskMode::LightweightChat;
        profile.deviceRuntimeToolsEnabled = false;
        profile.maxHistoryMessages = 1;
        profile.maxHistoryCharsPerMessage = 300;
        profile.maxPackageSummaryChars = 0;
        profile.maxEntryPointChars = 0;
        profile.maxFileListChars = 0;
        return profile;
    }
    if (agentHasStaticCtfIntent(question)
        && !agentHasExplicitDeviceRuntimeIntent(question)
        && !hasMetaReviewIntent(question)) {
        return staticFastPathProfile();
    }
    if (agentHasExplicitDeviceRuntimeIntent(question)) {
        return deviceRuntimeProfile();
    }
    if (hasFocusedStaticAnalysisIntent(question) && !hasMetaReviewIntent(question)) {
        return focusedStaticAnalysisProfile();
    }
    return profile;
}

} // namespace

QString agentTaskModeName(AgentTaskMode mode)
{
    switch (mode) {
    case AgentTaskMode::LightweightChat:
        return QStringLiteral("lightweight_chat");
    case AgentTaskMode::PackageOverview:
        return QStringLiteral("package_overview");
    case AgentTaskMode::FocusedStaticAnalysis:
        return QStringLiteral("focused_static_analysis");
    case AgentTaskMode::StaticFastPath:
        return QStringLiteral("static_fast_path");
    case AgentTaskMode::DeviceRuntime:
        return QStringLiteral("device_runtime");
    case AgentTaskMode::GeneralStatic:
        return QStringLiteral("general_static");
    }
    return QStringLiteral("general_static");
}

bool agentTaskUsesPlainModelOnly(AgentTaskMode mode)
{
    return mode == AgentTaskMode::LightweightChat;
}

AgentRequestRoute routeAgentRequest(
    const QString& question,
    bool hasLoadedPackage,
    const QVariantList& messages)
{
    AgentRequestRoute route;
    if (!hasLoadedPackage
        && (asksAboutCurrentLoadedApplication(question)
            || hasFocusedStaticAnalysisIntent(question)
            || hasGenericPackageAnalysisIntent(question))) {
        route.localReplyKind = AgentLocalReplyKind::NoLoadedApplication;
        route.localReplyText = noLoadedApplicationResponse(question);
        return route;
    }
    if (hasLoadedPackage && asksAboutCurrentLoadedApplication(question)) {
        route.taskProfile = packageOverviewProfile();
        return route;
    }
    if (hasLoadedPackage && hasGenericPackageAnalysisIntent(question)) {
        route.taskProfile = packageOverviewProfile();
        return route;
    }

    route.taskProfile = classifyAgentTask(
        question,
        agentIsAffirmativeDeviceVerificationFollowUp(question, messages));
    return route;
}

bool agentHasExplicitDeviceRuntimeIntent(const QString& question)
{
    const QString folded = question.toCaseFolded();
    if (containsAnyTerm(folded, {
        QStringLiteral("device"),
        QStringLiteral("hdc"),
        QStringLiteral("hilog"),
        QStringLiteral("screenshot"),
        QStringLiteral("ui layout"),
        QStringLiteral("install"),
        QStringLiteral("launch"),
        QStringLiteral("start app"),
        QStringLiteral("run on device"),
        QStringLiteral("connected"),
        QStringLiteral("resign"),
        QStringLiteral("re-sign"),
        QStringLiteral("sign and install"),
        QStringLiteral("runtime"),
        QStringLiteral("真机"),
        QStringLiteral("设备"),
        QStringLiteral("手机"),
        QStringLiteral("安装"),
        QStringLiteral("启动"),
        QStringLiteral("打开应用"),
        QStringLiteral("运行应用"),
        QStringLiteral("运行态"),
        QStringLiteral("动态验证"),
        QStringLiteral("跑通"),
        QStringLiteral("重签"),
        QStringLiteral("重签名"),
        QStringLiteral("签名安装"),
        QStringLiteral("截图"),
        QStringLiteral("日志"),
        QStringLiteral("崩溃"),
        QStringLiteral("点击"),
        QStringLiteral("ui input"),
        QStringLiteral("uiinput"),
        QStringLiteral("输入事件"),
        QStringLiteral("模拟输入"),
        QStringLiteral("投屏")
    })) {
        return true;
    }

    const bool asksToVerify = containsAnyTerm(folded, {
        QStringLiteral("verify"),
        QStringLiteral("verification"),
        QStringLiteral("validation"),
        QStringLiteral("test"),
        QStringLiteral("测试"),
        QStringLiteral("验证"),
        QStringLiteral("检验"),
        QStringLiteral("校验")
    });
    if (!asksToVerify) {
        return false;
    }

    return containsAnyTerm(folded, {
        QStringLiteral("device"),
        QStringLiteral("hdc"),
        QStringLiteral("runtime"),
        QStringLiteral("install"),
        QStringLiteral("launch"),
        QStringLiteral("app"),
        QStringLiteral("application"),
        QStringLiteral("ui"),
        QStringLiteral("input"),
        QStringLiteral("toast"),
        QStringLiteral("artifact"),
        QStringLiteral("file"),
        QStringLiteral("真机"),
        QStringLiteral("设备"),
        QStringLiteral("手机"),
        QStringLiteral("运行态"),
        QStringLiteral("动态"),
        QStringLiteral("安装"),
        QStringLiteral("启动"),
        QStringLiteral("应用"),
        QStringLiteral("界面"),
        QStringLiteral("输入"),
        QStringLiteral("点击"),
        QStringLiteral("文件"),
        QStringLiteral("日志"),
        QStringLiteral("截图"),
        QStringLiteral("生成")
    });
}

bool agentHasStaticCtfIntent(const QString& question)
{
    const QString folded = question.toCaseFolded();
    return containsAnyTerm(folded, {
        QStringLiteral("ctf"),
        QStringLiteral("flag"),
        QStringLiteral("password"),
        QStringLiteral("secretkey"),
        QStringLiteral("secret key"),
        QStringLiteral("crack"),
        QStringLiteral("decode"),
        QStringLiteral("decrypt"),
        QStringLiteral("encode"),
        QStringLiteral("hash"),
        QStringLiteral("maze"),
        QStringLiteral("口令"),
        QStringLiteral("破解"),
        QStringLiteral("解码"),
        QStringLiteral("解密"),
        QStringLiteral("算法"),
        QStringLiteral("找 flag"),
        QStringLiteral("找flag"),
        QStringLiteral("密码"),
        QStringLiteral("密钥")
    });
}

bool agentIsAffirmativeDeviceVerificationFollowUp(
    const QString& question,
    const QVariantList& messages)
{
    const QString foldedQuestion = question.trimmed().toCaseFolded();
    const bool affirmative = foldedQuestion == QStringLiteral("继续")
        || foldedQuestion == QStringLiteral("继续吧")
        || foldedQuestion == QStringLiteral("接着")
        || foldedQuestion == QStringLiteral("接着吧")
        || foldedQuestion == QStringLiteral("好")
        || foldedQuestion == QStringLiteral("好的")
        || foldedQuestion == QStringLiteral("可以")
        || foldedQuestion == QStringLiteral("行")
        || foldedQuestion == QStringLiteral("做吧")
        || foldedQuestion == QStringLiteral("跑吧")
        || foldedQuestion.contains(QStringLiteral("继续"))
        || foldedQuestion.contains(QStringLiteral("接着"));
    if (!affirmative) {
        return false;
    }

    int startIndex = messages.size() - 1;
    for (int index = messages.size() - 1; index >= 0; --index) {
        const QVariantMap item = messages.at(index).toMap();
        if (item.value(QStringLiteral("role")).toString() == QStringLiteral("user")
            && item.value(QStringLiteral("text")).toString().trimmed() == question.trimmed()) {
            startIndex = index - 1;
            break;
        }
    }

    int inspected = 0;
    for (int index = startIndex; index >= 0; --index) {
        const QVariantMap item = messages.at(index).toMap();
        const QString role = item.value(QStringLiteral("role")).toString();
        if (role != QStringLiteral("assistant") && role != QStringLiteral("user")) {
            continue;
        }
        const QString foldedMessage = item.value(QStringLiteral("text")).toString().toCaseFolded();
        if (containsAnyTerm(foldedMessage, {
                QStringLiteral("device verification pending"),
                QStringLiteral("是否继续连接设备检验一遍"),
                QStringLiteral("尚未包含设备运行态证据"),
                QStringLiteral("运行态未验证"),
                QStringLiteral("尚未设备验证"),
                QStringLiteral("真机验证"),
                QStringLiteral("继续真机"),
                QStringLiteral("设备验证"),
                QStringLiteral("安装验证"),
                QStringLiteral("运行态验证"),
                QStringLiteral("hdc"),
                QStringLiteral("install_current_hap"),
                QStringLiteral("install_current_hap_with_abc_string_rewrite"),
                QStringLiteral("start_harmony_app"),
                QStringLiteral("dump_ui_layout"),
                QStringLiteral("input_ui_text"),
                QStringLiteral("tap_ui"),
                QStringLiteral("read_hilog"),
                QStringLiteral("clear_hilog")
            })) {
            return true;
        }
        ++inspected;
        if (inspected >= 8) {
            break;
        }
    }

    return false;
}
