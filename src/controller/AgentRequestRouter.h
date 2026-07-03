#ifndef REARK_AGENT_REQUEST_ROUTER_H
#define REARK_AGENT_REQUEST_ROUTER_H

#include <QString>
#include <QVariantList>

enum class AgentTaskMode {
    LightweightChat,
    PackageOverview,
    FocusedStaticAnalysis,
    StaticFastPath,
    DeviceRuntime,
    GeneralStatic
};

struct AgentTaskProfile {
    AgentTaskMode mode = AgentTaskMode::GeneralStatic;
    bool deviceRuntimeToolsEnabled = false;
    int maxHistoryMessages = 8;
    int maxHistoryCharsPerMessage = 3000;
    int maxPackageSummaryChars = 8000;
    int maxEntryPointChars = 10000;
    int maxFileListChars = 10000;
};

enum class AgentLocalReplyKind {
    None,
    NoLoadedApplication
};

struct AgentRequestRoute {
    AgentTaskProfile taskProfile;
    AgentLocalReplyKind localReplyKind = AgentLocalReplyKind::None;
    QString localReplyText;

    [[nodiscard]] bool usesModel() const
    {
        return localReplyKind == AgentLocalReplyKind::None;
    }
};

[[nodiscard]] QString agentTaskModeName(AgentTaskMode mode);
[[nodiscard]] bool agentTaskUsesPlainModelOnly(AgentTaskMode mode);
[[nodiscard]] AgentRequestRoute routeAgentRequest(
    const QString& question,
    bool hasLoadedPackage,
    const QVariantList& messages);
[[nodiscard]] bool agentHasExplicitDeviceRuntimeIntent(const QString& question);
[[nodiscard]] bool agentHasStaticCtfIntent(const QString& question);
[[nodiscard]] bool agentIsAffirmativeDeviceVerificationFollowUp(
    const QString& question,
    const QVariantList& messages);

#endif // REARK_AGENT_REQUEST_ROUTER_H
