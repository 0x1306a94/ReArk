#ifndef REARK_HDC_DEVICE_BACKEND_H
#define REARK_HDC_DEVICE_BACKEND_H

#include "core/CommandRunner.h"

#include <QVariantList>

struct HdcDeviceTarget {
    QString id;
    QString state;
    QString rawLine;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

class HdcDeviceBackend {
public:
    HdcDeviceBackend() = default;

    [[nodiscard]] QString resolvedProgram() const;
    [[nodiscard]] CommandRequest listTargetsRequest(int timeoutMs = 5000) const;
    [[nodiscard]] CommandRequest installRequest(
        const QString& packagePath,
        const QString& targetId,
        int timeoutMs = 60000) const;
    [[nodiscard]] CommandRequest startAbilityRequest(
        const QString& bundleName,
        const QString& abilityName,
        const QString& moduleName,
        const QString& targetId,
        int timeoutMs = 10000) const;
    [[nodiscard]] CommandRequest missionListRequest(
        const QString& targetId,
        int timeoutMs = 5000) const;
    [[nodiscard]] CommandRequest processListRequest(
        const QString& targetId,
        int timeoutMs = 5000) const;
    [[nodiscard]] CommandRequest hilogRequest(
        const QString& targetId,
        const QString& level = {},
        int timeoutMs = 10000) const;
    [[nodiscard]] CommandRequest clearHilogRequest(
        const QString& targetId,
        int timeoutMs = 5000) const;
    [[nodiscard]] CommandRequest shellCommandRequest(
        const QString& script,
        const QString& targetId,
        int timeoutMs = 10000) const;
    [[nodiscard]] CommandRequest screenshotCaptureRequest(
        const QString& remotePath,
        const QString& targetId,
        int timeoutMs = 15000) const;
    [[nodiscard]] CommandRequest receiveFileRequest(
        const QString& remotePath,
        const QString& localPath,
        const QString& targetId,
        int timeoutMs = 15000) const;
    [[nodiscard]] CommandRequest removeRemoteFileRequest(
        const QString& remotePath,
        const QString& targetId,
        int timeoutMs = 5000) const;
    [[nodiscard]] CommandRequest forwardTcpRequest(
        quint16 localPort,
        quint16 remotePort,
        const QString& targetId,
        int timeoutMs = 10000) const;

    [[nodiscard]] static QString bundledHdcPath();
    [[nodiscard]] static QString bundledHdcPathForApplicationDir(const QString& applicationDir);
    [[nodiscard]] static QList<HdcDeviceTarget> parseTargets(const QString& output);
    [[nodiscard]] static QVariantList targetsToVariantList(const QList<HdcDeviceTarget>& targets);
    [[nodiscard]] static QString filterHilog(const QString& output, const QString& filter, int maxLines);
    [[nodiscard]] static bool missionDumpHasBundleRecord(const QString& output, const QString& bundleName);
    [[nodiscard]] static bool missionDumpShowsVisibleBundle(const QString& output, const QString& bundleName);
    [[nodiscard]] static bool installSucceeded(const CommandResult& result);
    [[nodiscard]] static bool installOutputReportsFailure(const CommandResult& result);
    [[nodiscard]] static bool startSucceeded(const CommandResult& result);
    [[nodiscard]] static bool startOutputReportsFailure(const CommandResult& result);
    [[nodiscard]] static QString resultSummary(const CommandResult& result);

    [[nodiscard]] QStringList targetArguments(const QString& targetId) const;
};

#endif // REARK_HDC_DEVICE_BACKEND_H
