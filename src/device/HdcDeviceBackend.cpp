#include "device/HdcDeviceBackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace {

QString hdcExecutableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("hdc.exe");
#else
    return QStringLiteral("hdc");
#endif
}

QStringList splitNonEmptyLines(const QString& text)
{
    QStringList lines;
    for (QString line : text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts)) {
        line = line.trimmed();
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }
    return lines;
}

QString briefText(QString value, int maxChars = 1200)
{
    value = value.trimmed();
    if (value.size() <= maxChars) {
        return value;
    }
    return value.left(maxChars) + QStringLiteral("\n[truncated]");
}

QString hostFileArgument(const QString& path)
{
    return QDir::toNativeSeparators(QFileInfo(path.trimmed()).absoluteFilePath());
}

QString commandOutputText(const CommandResult& result)
{
    return QStringLiteral("%1\n%2\n%3")
        .arg(result.standardOutput, result.standardError, result.errorMessage);
}

QStringList missionRecordBlocks(const QString& output)
{
    return output
        .toCaseFolded()
        .split(QRegularExpression(QStringLiteral("(?=abilityrecord\\s+id\\s+#)")), Qt::SkipEmptyParts);
}

bool missionBlockContainsBundle(const QString& block, const QString& foldedBundle)
{
    return block.contains(QStringLiteral("bundle name [%1]").arg(foldedBundle))
        || block.contains(QStringLiteral("app name [%1]").arg(foldedBundle))
        || block.contains(QStringLiteral("#%1:").arg(foldedBundle));
}

} // namespace

QVariantMap HdcDeviceTarget::toVariantMap() const
{
    QVariantMap item;
    item.insert(QStringLiteral("id"), id);
    item.insert(QStringLiteral("state"), state);
    item.insert(QStringLiteral("rawLine"), rawLine);
    item.insert(QStringLiteral("display"), state.isEmpty() ? id : QStringLiteral("%1  %2").arg(id, state));
    return item;
}

QString HdcDeviceBackend::resolvedProgram() const
{
    return bundledHdcPath();
}

CommandRequest HdcDeviceBackend::listTargetsRequest(int timeoutMs) const
{
    return {
        .program = resolvedProgram(),
        .arguments = { QStringLiteral("list"), QStringLiteral("targets") },
        .timeoutMs = timeoutMs
    };
}

CommandRequest HdcDeviceBackend::installRequest(
    const QString& packagePath,
    const QString& targetId,
    int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("install")
              << QStringLiteral("-r")
              << hostFileArgument(packagePath);
    return {
        .program = resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest HdcDeviceBackend::startAbilityRequest(
    const QString& bundleName,
    const QString& abilityName,
    const QString& moduleName,
    const QString& targetId,
    int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("aa")
              << QStringLiteral("start")
              << QStringLiteral("-b")
              << bundleName;
    if (!abilityName.trimmed().isEmpty()) {
        arguments << QStringLiteral("-a") << abilityName.trimmed();
    }
    if (!moduleName.trimmed().isEmpty()) {
        arguments << QStringLiteral("-m") << moduleName.trimmed();
    }
    arguments << QStringLiteral("-W");
    return {
        .program = resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest HdcDeviceBackend::missionListRequest(const QString& targetId, int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("aa")
              << QStringLiteral("dump")
              << QStringLiteral("-l");
    return {
        .program = resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest HdcDeviceBackend::processListRequest(const QString& targetId, int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("ps")
              << QStringLiteral("-ef");
    return {
        .program = resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest HdcDeviceBackend::hilogRequest(const QString& targetId, int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("hilog")
              << QStringLiteral("-x");
    return {
        .program = resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest HdcDeviceBackend::screenshotCaptureRequest(
    const QString& remotePath,
    const QString& targetId,
    int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("snapshot_display")
              << QStringLiteral("-f")
              << remotePath;
    return {
        .program = resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest HdcDeviceBackend::receiveFileRequest(
    const QString& remotePath,
    const QString& localPath,
    const QString& targetId,
    int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("file")
              << QStringLiteral("recv")
              << remotePath
              << QDir::toNativeSeparators(localPath);
    return {
        .program = resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest HdcDeviceBackend::removeRemoteFileRequest(
    const QString& remotePath,
    const QString& targetId,
    int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("rm")
              << QStringLiteral("-f")
              << remotePath;
    return {
        .program = resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest HdcDeviceBackend::forwardTcpRequest(
    quint16 localPort,
    quint16 remotePort,
    const QString& targetId,
    int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("fport")
              << QStringLiteral("tcp:%1").arg(localPort)
              << QStringLiteral("tcp:%1").arg(remotePort);
    return {
        .program = resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

QString HdcDeviceBackend::bundledHdcPath()
{
    return bundledHdcPathForApplicationDir(QCoreApplication::applicationDirPath());
}

QString HdcDeviceBackend::bundledHdcPathForApplicationDir(const QString& applicationDir)
{
    QDir appDir(applicationDir.isEmpty() ? QDir::currentPath() : applicationDir);
    const QString executable = hdcExecutableName();

    return QDir::cleanPath(appDir.filePath(QStringLiteral("plugin/hdc/%1").arg(executable)));
}

QList<HdcDeviceTarget> HdcDeviceBackend::parseTargets(const QString& output)
{
    QList<HdcDeviceTarget> targets;
    for (const QString& line : splitNonEmptyLines(output)) {
        const QString folded = line.toCaseFolded();
        if (folded == QStringLiteral("empty")
            || folded.contains(QStringLiteral("no targets"))
            || folded.contains(QStringLiteral("not found"))) {
            continue;
        }

        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        HdcDeviceTarget target;
        target.id = parts.first();
        target.state = parts.size() > 1 ? parts.at(1) : QStringLiteral("connected");
        target.rawLine = line;
        if (!target.id.isEmpty()) {
            targets.append(target);
        }
    }
    return targets;
}

QVariantList HdcDeviceBackend::targetsToVariantList(const QList<HdcDeviceTarget>& targets)
{
    QVariantList result;
    for (const HdcDeviceTarget& target : targets) {
        result.append(target.toVariantMap());
    }
    return result;
}

QString HdcDeviceBackend::filterHilog(const QString& output, const QString& filter, int maxLines)
{
    const QString trimmedFilter = filter.trimmed();
    QStringList lines = splitNonEmptyLines(output);
    if (!trimmedFilter.isEmpty()) {
        const QString foldedFilter = trimmedFilter.toCaseFolded();
        lines.erase(std::remove_if(lines.begin(), lines.end(), [&foldedFilter](const QString& line) {
            return !line.toCaseFolded().contains(foldedFilter);
        }), lines.end());
    }

    const int boundedMaxLines = std::clamp(maxLines <= 0 ? 500 : maxLines, 1, 5000);
    if (lines.size() > boundedMaxLines) {
        lines = lines.mid(lines.size() - boundedMaxLines);
    }
    return lines.join(QLatin1Char('\n'));
}

bool HdcDeviceBackend::missionDumpHasBundleRecord(const QString& output, const QString& bundleName)
{
    const QString foldedBundle = bundleName.trimmed().toCaseFolded();
    if (foldedBundle.isEmpty()) {
        return false;
    }

    const QString foldedOutput = output.toCaseFolded();
    return foldedOutput.contains(QStringLiteral("bundle name [%1]").arg(foldedBundle))
        || foldedOutput.contains(QStringLiteral("app name [%1]").arg(foldedBundle))
        || foldedOutput.contains(QStringLiteral("#%1:").arg(foldedBundle));
}

bool HdcDeviceBackend::missionDumpShowsVisibleBundle(const QString& output, const QString& bundleName)
{
    const QString foldedBundle = bundleName.trimmed().toCaseFolded();
    if (foldedBundle.isEmpty()) {
        return false;
    }

    for (const QString& block : missionRecordBlocks(output)) {
        if (!missionBlockContainsBundle(block, foldedBundle)) {
            continue;
        }

        const bool ready = block.contains(QStringLiteral("ready #1"));
        const bool windowAttached = block.contains(QStringLiteral("window attached #1"));
        const bool stillStarting = block.contains(QStringLiteral("state #initial"))
            || block.contains(QStringLiteral("app state #begin"));
        if (ready && windowAttached && !stillStarting) {
            return true;
        }
    }
    return false;
}

bool HdcDeviceBackend::installSucceeded(const CommandResult& result)
{
    return result.succeeded() && !installOutputReportsFailure(result);
}

bool HdcDeviceBackend::installOutputReportsFailure(const CommandResult& result)
{
    const QString folded = commandOutputText(result).toCaseFolded();
    return folded.contains(QStringLiteral("failed to install bundle"))
        || folded.contains(QStringLiteral("install failed"))
        || folded.contains(QStringLiteral("error: failed to install"))
        || folded.contains(QStringLiteral("no signature file"))
        || folded.contains(QStringLiteral("verify signature failed"))
        || folded.contains(QStringLiteral("signature verify failed"));
}

QString HdcDeviceBackend::resultSummary(const CommandResult& result)
{
    QString text;
    text += QStringLiteral("$ %1\n").arg(result.commandLine());
    text += QStringLiteral("# exit_code: %1\n").arg(result.exitCode);
    text += QStringLiteral("# elapsed_ms: %1\n").arg(result.elapsedMs);
    if (installOutputReportsFailure(result)) {
        text += QStringLiteral("# hdc_reported_failure: true\n");
    }
    if (result.timedOut) {
        text += QStringLiteral("# timed_out: true\n");
    }
    if (!result.errorMessage.trimmed().isEmpty()) {
        text += QStringLiteral("# error: %1\n").arg(result.errorMessage.trimmed());
    }
    if (!result.standardOutput.trimmed().isEmpty()) {
        text += QStringLiteral("\n[stdout]\n%1\n").arg(briefText(result.standardOutput));
    }
    if (!result.standardError.trimmed().isEmpty()) {
        text += QStringLiteral("\n[stderr]\n%1\n").arg(briefText(result.standardError));
    }
    return text.trimmed();
}

QStringList HdcDeviceBackend::targetArguments(const QString& targetId) const
{
    const QString trimmed = targetId.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    return { QStringLiteral("-t"), trimmed };
}
