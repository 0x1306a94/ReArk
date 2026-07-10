#include "signing/HarmonyHapSigner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace {

QString javaExecutableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("java.exe");
#else
    return QStringLiteral("java");
#endif
}

QString cleanFilePath(const QString& path)
{
    return QDir::cleanPath(path.trimmed());
}

QString firstExistingFile(const QStringList& candidates)
{
    for (const QString& candidate : candidates) {
        if (candidate.trimmed().isEmpty()) {
            continue;
        }
        const QString cleaned = cleanFilePath(candidate);
        if (QFileInfo::exists(cleaned)) {
            return cleaned;
        }
    }
    return {};
}

QString quoteArgument(const QString& argument)
{
    if (argument.isEmpty()) {
        return QStringLiteral("\"\"");
    }
    const bool needsQuotes = std::any_of(argument.cbegin(), argument.cend(), [](QChar ch) {
        return ch.isSpace() || ch == QLatin1Char('"');
    });
    if (!needsQuotes) {
        return argument;
    }

    QString escaped = argument;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QStringList redactedArguments(const QStringList& arguments)
{
    QStringList redacted = arguments;
    for (int i = 0; i + 1 < redacted.size(); ++i) {
        const QString flag = redacted.at(i);
        if (flag == QStringLiteral("-keystorePwd") || flag == QStringLiteral("-keyPwd")) {
            redacted[i + 1] = QStringLiteral("<redacted>");
            ++i;
        }
    }
    return redacted;
}

QString redactedCommandLine(const CommandResult& result)
{
    QStringList parts;
    parts.reserve(result.arguments.size() + 1);
    parts.append(quoteArgument(result.program));
    for (const QString& argument : redactedArguments(result.arguments)) {
        parts.append(quoteArgument(argument));
    }
    return parts.join(QLatin1Char(' '));
}

QString briefText(QString value, int maxChars = 1200)
{
    value = value.trimmed();
    if (value.size() <= maxChars) {
        return value;
    }
    return value.left(maxChars) + QStringLiteral("\n[truncated]");
}

} // namespace

CommandRequest HarmonyHapSigner::signCommand(const HarmonyHapSigningRequest& request)
{
    const QString signAlgorithm = request.signAlgorithm.trimmed().isEmpty()
        ? QStringLiteral("SHA256withECDSA")
        : request.signAlgorithm.trimmed();

    QStringList arguments {
        QStringLiteral("-jar"),
        cleanFilePath(request.signToolPath),
        QStringLiteral("sign-app"),
        QStringLiteral("-mode"),
        QStringLiteral("localSign"),
        QStringLiteral("-keystoreFile"),
        cleanFilePath(request.keystorePath),
        QStringLiteral("-keystorePwd"),
        request.keystorePassword,
        QStringLiteral("-keyAlias"),
        request.keyAlias.trimmed()
    };
    if (!request.keyPassword.isEmpty()) {
        arguments << QStringLiteral("-keyPwd") << request.keyPassword;
    }
    arguments << QStringLiteral("-signAlg")
              << signAlgorithm
              << QStringLiteral("-profileFile")
              << cleanFilePath(request.profilePath)
              << QStringLiteral("-appCertFile")
              << cleanFilePath(request.certificatePath)
              << QStringLiteral("-inFile")
              << cleanFilePath(request.inputHapPath)
              << QStringLiteral("-outFile")
              << cleanFilePath(request.outputHapPath);
    if (!request.compatibleVersion.trimmed().isEmpty()) {
        arguments << QStringLiteral("-compatibleVersion") << request.compatibleVersion.trimmed();
    }

    return {
        .program = request.javaProgram.trimmed().isEmpty()
            ? resolvedJavaProgram()
            : request.javaProgram.trimmed(),
        .arguments = arguments,
        .timeoutMs = request.timeoutMs
    };
}

CommandRequest HarmonyHapSigner::packCommand(const HarmonyHapPackingRequest& request)
{
    const QDir root(cleanFilePath(request.unpackedDirectory));
    QStringList arguments {
        QStringLiteral("-jar"),
        cleanFilePath(request.packingToolPath),
        QStringLiteral("--mode"),
        QStringLiteral("hap"),
        QStringLiteral("--json-path"),
        cleanFilePath(root.filePath(QStringLiteral("module.json"))),
        QStringLiteral("--ets-path"),
        cleanFilePath(root.filePath(QStringLiteral("ets"))),
        QStringLiteral("--lib-path"),
        cleanFilePath(root.filePath(QStringLiteral("libs"))),
        QStringLiteral("--resources-path"),
        cleanFilePath(root.filePath(QStringLiteral("resources"))),
        QStringLiteral("--pack-info-path"),
        cleanFilePath(root.filePath(QStringLiteral("pack.info"))),
        QStringLiteral("--index-path"),
        cleanFilePath(root.filePath(QStringLiteral("resources.index"))),
        QStringLiteral("--out-path"),
        cleanFilePath(request.outputHapPath),
        QStringLiteral("--force"),
        QStringLiteral("true")
    };

    const QString pkgContextPath = cleanFilePath(root.filePath(QStringLiteral("pkgContextInfo.json")));
    if (QFileInfo::exists(pkgContextPath)) {
        arguments << QStringLiteral("--pkg-context-path") << pkgContextPath;
    }
    const QString hnpPath = cleanFilePath(root.filePath(QStringLiteral("hnp")));
    if (QFileInfo::exists(hnpPath)) {
        arguments << QStringLiteral("--hnp-path") << hnpPath;
    }

    return {
        .program = request.javaProgram.trimmed().isEmpty()
            ? resolvedJavaProgram()
            : request.javaProgram.trimmed(),
        .arguments = arguments,
        .timeoutMs = request.timeoutMs
    };
}

QString HarmonyHapSigner::bundledSignToolPath()
{
    return bundledSignToolPathForApplicationDir(QCoreApplication::applicationDirPath());
}

QString HarmonyHapSigner::bundledSignToolPathForApplicationDir(const QString& applicationDir)
{
    QDir appDir(applicationDir.isEmpty() ? QDir::currentPath() : applicationDir);
    return QDir::cleanPath(appDir.filePath(QStringLiteral("plugin/harmony-tools/lib/hap-sign-tool.jar")));
}

QString HarmonyHapSigner::bundledPackingToolPath()
{
    return bundledPackingToolPathForApplicationDir(QCoreApplication::applicationDirPath());
}

QString HarmonyHapSigner::bundledPackingToolPathForApplicationDir(const QString& applicationDir)
{
    QDir appDir(applicationDir.isEmpty() ? QDir::currentPath() : applicationDir);
    return QDir::cleanPath(appDir.filePath(QStringLiteral("plugin/harmony-tools/lib/app_packing_tool.jar")));
}

QString HarmonyHapSigner::resolvedJavaProgram()
{
    return resolvedJavaProgramForApplicationDir(QCoreApplication::applicationDirPath());
}

QString HarmonyHapSigner::resolvedJavaProgramForApplicationDir(const QString& applicationDir)
{
    QDir appDir(applicationDir.isEmpty() ? QDir::currentPath() : applicationDir);
    const QString executable = javaExecutableName();
    const QString javaHome = QString::fromUtf8(qgetenv("JAVA_HOME")).trimmed();
    const QString configuredJava = QString::fromUtf8(qgetenv("REARK_JAVA")).trimmed();

    const QString bundled = firstExistingFile({
        appDir.filePath(QStringLiteral("plugin/java/bin/%1").arg(executable)),
        appDir.filePath(QStringLiteral("plugin/jbr/bin/%1").arg(executable)),
        appDir.filePath(QStringLiteral("plugin/harmony-tools/jbr/bin/%1").arg(executable)),
        configuredJava,
        javaHome.isEmpty() ? QString() : QDir(javaHome).filePath(QStringLiteral("bin/%1").arg(executable))
    });
    return bundled.isEmpty() ? QStringLiteral("java") : bundled;
}

QString HarmonyHapSigner::resultSummary(const CommandResult& result)
{
    QString text;
    text += QStringLiteral("$ %1\n").arg(redactedCommandLine(result));
    text += QStringLiteral("# exit_code: %1\n").arg(result.exitCode);
    text += QStringLiteral("# elapsed_ms: %1\n").arg(result.elapsedMs);
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
