#include "core/CommandRunner.h"
#include "controller/SigningSettings.h"
#include "device/HarmonyPackageRewriter.h"
#include "device/HdcDeviceBackend.h"
#include "signing/HarmonyHapSigner.h"
#include "signing/SigningMaterialInspector.h"

#include <hyle/hap/hap.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>

#include <cstddef>
#include <exception>

namespace {

struct Options {
    QString samplesDir;
    QString samplesFile;
    QString targetId;
    QString keystorePath;
    QString keystorePasswordEnv;
    QString keyAlias;
    QString keyPasswordEnv;
    QString profilePath;
    QString certificatePath;
    QString outputPath;
    QStringList stripRequestPermissions;
    QStringList forceDeviceTypes;
    int forceCompatibleApi = 0;
    int forceTargetApi = 0;
    int limit = 0;
    bool useRearkSettings = false;
};

struct PackageIdentity {
    QString bundleName;
    QString error;
};

QString fromStdString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string toStdString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

QString commandText(const CommandResult& result)
{
    return QStringLiteral("%1\n%2\n%3")
        .arg(result.standardOutput, result.standardError, result.errorMessage)
        .trimmed();
}

QString failureKindName(HdcInstallFailureKind kind)
{
    switch (kind) {
    case HdcInstallFailureKind::None:
        return QStringLiteral("none");
    case HdcInstallFailureKind::SignatureRejected:
        return QStringLiteral("signature_rejected");
    case HdcInstallFailureKind::SigningProfileUnauthorized:
        return QStringLiteral("profile_unauthorized");
    case HdcInstallFailureKind::VersionDowngrade:
        return QStringLiteral("version_downgrade");
    case HdcInstallFailureKind::Other:
        return QStringLiteral("other");
    }
    return QStringLiteral("unknown");
}

PackageIdentity readPackageIdentity(const QString& hapPath)
{
    PackageIdentity identity;
    try {
        auto summary = hyle::hap::summarize_decompiled_package(toStdString(hapPath));
        if (!summary) {
            identity.error = QStringLiteral("Package summary failed: %1")
                .arg(fromStdString(summary.error().message()));
            return identity;
        }
        identity.bundleName = fromStdString(summary->bundle_name).trimmed();
    } catch (const std::exception& ex) {
        identity.error = QStringLiteral("Package summary threw exception: %1")
            .arg(QString::fromUtf8(ex.what()));
    } catch (...) {
        identity.error = QStringLiteral("Package summary threw unknown exception.");
    }
    return identity;
}

QString signedHapFileName(const QString& sourcePath)
{
    QString baseName = QFileInfo(sourcePath).completeBaseName().trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("reark-smoke");
    }
    return baseName + QStringLiteral("-signed.hap");
}

QString rewrittenHapFileName(const QString& sourcePath)
{
    QString baseName = QFileInfo(sourcePath).completeBaseName().trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("reark-smoke");
    }
    return baseName + QStringLiteral("-rebundle-unsigned.hap");
}

bool hdcUninstallSucceeded(const CommandResult& result)
{
    const QString folded = commandText(result).toCaseFolded();
    return result.succeeded()
        && !folded.contains(QStringLiteral("error:"))
        && !folded.contains(QStringLiteral("failed to uninstall"))
        && !folded.contains(QStringLiteral("fail to uninstall"));
}

bool installEntryAlreadyExists(const CommandResult& result)
{
    return commandText(result)
        .toCaseFolded()
        .contains(QStringLiteral("install entry already exist"));
}

void addCommandSummary(QJsonObject* object, const QString& key, const CommandResult& result)
{
    if (object == nullptr || (result.program.isEmpty() && !result.started)) {
        return;
    }

    QJsonObject summary;
    summary.insert(QStringLiteral("exit_code"), result.exitCode);
    summary.insert(QStringLiteral("elapsed_ms"), result.elapsedMs);
    summary.insert(QStringLiteral("succeeded"), result.succeeded());
    summary.insert(QStringLiteral("text"), commandText(result).left(3000));
    object->insert(key, summary);
}

bool parseOptions(const QStringList& args, Options* options, QString* error)
{
    if (options == nullptr) {
        return false;
    }

    for (int i = 1; i < args.size(); ++i) {
        const QString name = args.at(i);
        auto takeValue = [&](QString* target) {
            if (i + 1 >= args.size()) {
                if (error != nullptr) {
                    *error = QStringLiteral("Missing value for %1").arg(name);
                }
                return false;
            }
            *target = args.at(++i);
            return true;
        };

        if (name == QStringLiteral("--samples-dir")) {
            if (!takeValue(&options->samplesDir)) {
                return false;
            }
        } else if (name == QStringLiteral("--samples-file")) {
            if (!takeValue(&options->samplesFile)) {
                return false;
            }
        } else if (name == QStringLiteral("--target")) {
            if (!takeValue(&options->targetId)) {
                return false;
            }
        } else if (name == QStringLiteral("--keystore")) {
            if (!takeValue(&options->keystorePath)) {
                return false;
            }
        } else if (name == QStringLiteral("--keystore-password-env")) {
            if (!takeValue(&options->keystorePasswordEnv)) {
                return false;
            }
        } else if (name == QStringLiteral("--key-alias")) {
            if (!takeValue(&options->keyAlias)) {
                return false;
            }
        } else if (name == QStringLiteral("--key-password-env")) {
            if (!takeValue(&options->keyPasswordEnv)) {
                return false;
            }
        } else if (name == QStringLiteral("--profile")) {
            if (!takeValue(&options->profilePath)) {
                return false;
            }
        } else if (name == QStringLiteral("--certificate")) {
            if (!takeValue(&options->certificatePath)) {
                return false;
            }
        } else if (name == QStringLiteral("--output")) {
            if (!takeValue(&options->outputPath)) {
                return false;
            }
        } else if (name == QStringLiteral("--strip-request-permission")) {
            QString permission;
            if (!takeValue(&permission)) {
                return false;
            }
            if (!permission.trimmed().isEmpty()) {
                options->stripRequestPermissions.append(permission.trimmed());
            }
        } else if (name == QStringLiteral("--force-device-type")) {
            QString deviceType;
            if (!takeValue(&deviceType)) {
                return false;
            }
            if (!deviceType.trimmed().isEmpty()) {
                options->forceDeviceTypes.append(deviceType.trimmed());
            }
        } else if (name == QStringLiteral("--force-compatible-api")) {
            QString value;
            if (!takeValue(&value)) {
                return false;
            }
            bool ok = false;
            options->forceCompatibleApi = value.toInt(&ok);
            if (!ok || options->forceCompatibleApi < 0) {
                if (error != nullptr) {
                    *error = QStringLiteral("Invalid --force-compatible-api value.");
                }
                return false;
            }
        } else if (name == QStringLiteral("--force-target-api")) {
            QString value;
            if (!takeValue(&value)) {
                return false;
            }
            bool ok = false;
            options->forceTargetApi = value.toInt(&ok);
            if (!ok || options->forceTargetApi < 0) {
                if (error != nullptr) {
                    *error = QStringLiteral("Invalid --force-target-api value.");
                }
                return false;
            }
        } else if (name == QStringLiteral("--use-reark-settings")) {
            options->useRearkSettings = true;
        } else if (name == QStringLiteral("--limit")) {
            QString value;
            if (!takeValue(&value)) {
                return false;
            }
            bool ok = false;
            options->limit = value.toInt(&ok);
            if (!ok || options->limit < 0) {
                if (error != nullptr) {
                    *error = QStringLiteral("Invalid --limit value.");
                }
                return false;
            }
        } else {
            if (error != nullptr) {
                *error = QStringLiteral("Unknown argument: %1").arg(name);
            }
            return false;
        }
    }

    if ((options->samplesDir.trimmed().isEmpty() && options->samplesFile.trimmed().isEmpty())
        || options->targetId.trimmed().isEmpty()
        || options->outputPath.trimmed().isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("Required arguments: --samples-dir or --samples-file, plus --target --output");
        }
        return false;
    }
    if (!options->useRearkSettings
        && (options->keystorePath.trimmed().isEmpty()
            || options->keystorePasswordEnv.trimmed().isEmpty()
            || options->keyAlias.trimmed().isEmpty()
            || options->profilePath.trimmed().isEmpty()
            || options->certificatePath.trimmed().isEmpty())) {
        if (error != nullptr) {
            *error = QStringLiteral("Signing arguments are required unless --use-reark-settings is set: --keystore --keystore-password-env --key-alias --profile --certificate");
        }
        return false;
    }

    return true;
}

QFileInfoList readSamplesFile(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error != nullptr) {
            *error = QStringLiteral("Cannot read samples file: %1").arg(path);
        }
        return {};
    }

    QFileInfoList samples;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('"')) && line.endsWith(QLatin1Char('"')) && line.size() >= 2) {
            line = line.mid(1, line.size() - 2);
        }
        const QFileInfo info(line);
        if (!info.exists() || !info.isFile()) {
            if (error != nullptr) {
                *error = QStringLiteral("Sample listed in %1 is not available on disk: %2")
                    .arg(path, line);
            }
            return {};
        }
        samples.append(info);
    }
    return samples;
}

QJsonObject installOne(
    const QFileInfo& sample,
    const QString& targetId,
    const HarmonySigningSettings& signingSettings,
    const SigningMaterialStatus& materialStatus,
    const QStringList& stripRequestPermissions,
    const QStringList& forceDeviceTypes,
    int forceCompatibleApi,
    int forceTargetApi)
{
    QJsonObject row;
    row.insert(QStringLiteral("name"), sample.fileName());
    row.insert(QStringLiteral("path"), sample.absoluteFilePath());
    row.insert(QStringLiteral("bytes"), sample.size());

    HdcDeviceBackend backend;
    const QString packagePath = sample.absoluteFilePath();
    const PackageIdentity identity = readPackageIdentity(packagePath);
    row.insert(QStringLiteral("package_bundle"), identity.bundleName);
    if (!identity.error.isEmpty()) {
        row.insert(QStringLiteral("package_summary_error"), identity.error);
    }

    const CommandResult initialInstall =
        CommandRunner::runBlocking(backend.installRequest(packagePath, targetId));
    addCommandSummary(&row, QStringLiteral("initial_install"), initialInstall);

    if (HdcDeviceBackend::installSucceeded(initialInstall)) {
        row.insert(QStringLiteral("status"), QStringLiteral("direct_ok"));
        return row;
    }

    const HdcInstallFailureKind initialFailure = HdcDeviceBackend::classifyInstallFailure(initialInstall);
    row.insert(QStringLiteral("initial_failure"), failureKindName(initialFailure));

    QTemporaryDir workDir(QDir::temp().filePath(QStringLiteral("ReArk-install-smoke-XXXXXX")));
    if (!workDir.isValid()) {
        row.insert(QStringLiteral("status"), QStringLiteral("failed"));
        row.insert(QStringLiteral("error"), QStringLiteral("Could not create temporary work directory."));
        return row;
    }

    const bool directDowngrade = initialFailure == HdcInstallFailureKind::VersionDowngrade;
    const bool needsSigning = initialFailure == HdcInstallFailureKind::SignatureRejected
        || initialFailure == HdcInstallFailureKind::SigningProfileUnauthorized;

    QString installPath = packagePath;
    QString uninstallBundle = identity.bundleName;
    bool rewriteUsed = false;
    QJsonObject rewriteObject;
    QJsonObject signObject;

    if (needsSigning) {
        if (!identity.bundleName.isEmpty()
            && !materialStatus.profileBundleName.isEmpty()
            && identity.bundleName != materialStatus.profileBundleName) {
            rewriteUsed = true;
            uninstallBundle = materialStatus.profileBundleName;
            const QString rewrittenPath = workDir.filePath(rewrittenHapFileName(packagePath));
            const HarmonyBundleRewriteResult rewriteResult =
                HarmonyPackageRewriter::rewriteBundleIdentity({
                    .inputHapPath = packagePath,
                    .outputHapPath = rewrittenPath,
                    .oldBundleName = identity.bundleName,
                    .newBundleName = materialStatus.profileBundleName,
                    .strippedRequestPermissions = stripRequestPermissions,
                    .forcedDeviceTypes = forceDeviceTypes,
                    .forcedCompatibleApi = forceCompatibleApi,
                    .forcedTargetApi = forceTargetApi,
                    .javaProgram = HarmonyHapSigner::resolvedJavaProgram(),
                    .packingToolPath = HarmonyHapSigner::bundledPackingToolPath()
                });
            rewriteObject.insert(QStringLiteral("ok"), rewriteResult.ok);
            rewriteObject.insert(QStringLiteral("output_hap"), rewriteResult.outputHapPath);
            rewriteObject.insert(QStringLiteral("error"), rewriteResult.error);
            rewriteObject.insert(QStringLiteral("report"), rewriteResult.report.left(3000));
            addCommandSummary(&rewriteObject, QStringLiteral("pack"), rewriteResult.packingResult);
            row.insert(QStringLiteral("bundle_rewrite"), rewriteObject);
            if (!rewriteResult.ok) {
                workDir.setAutoRemove(false);
                row.insert(QStringLiteral("status"), QStringLiteral("rewrite_failed"));
                row.insert(QStringLiteral("retained_dir"), workDir.path());
                return row;
            }
            installPath = rewrittenPath;
        }

        const QString signedPath = workDir.filePath(signedHapFileName(installPath));
        const CommandResult signResult =
            CommandRunner::runBlocking(HarmonyHapSigner::signCommand({
                .javaProgram = HarmonyHapSigner::resolvedJavaProgram(),
                .signToolPath = HarmonyHapSigner::bundledSignToolPath(),
                .keystorePath = signingSettings.keystorePath,
                .keystorePassword = signingSettings.keystorePassword,
                .keyAlias = signingSettings.keyAlias,
                .keyPassword = signingSettings.keyPassword,
                .profilePath = signingSettings.profilePath,
                .certificatePath = signingSettings.certificatePath,
                .inputHapPath = installPath,
                .outputHapPath = signedPath
            }));
        addCommandSummary(&signObject, QStringLiteral("command"), signResult);
        signObject.insert(QStringLiteral("signed_hap"), signedPath);
        row.insert(QStringLiteral("sign"), signObject);
        if (!signResult.succeeded()) {
            workDir.setAutoRemove(false);
            row.insert(QStringLiteral("status"), QStringLiteral("sign_failed"));
            row.insert(QStringLiteral("retained_dir"), workDir.path());
            return row;
        }
        installPath = signedPath;
    } else if (!directDowngrade) {
        row.insert(QStringLiteral("status"), QStringLiteral("initial_failed"));
        return row;
    }

    CommandResult signedInstall;
    if (needsSigning) {
        signedInstall = CommandRunner::runBlocking(backend.installRequest(installPath, targetId));
        addCommandSummary(&row, QStringLiteral("signed_install"), signedInstall);
        if (HdcDeviceBackend::installSucceeded(signedInstall)) {
            row.insert(QStringLiteral("status"), rewriteUsed ? QStringLiteral("signed_rewritten_ok") : QStringLiteral("signed_ok"));
            return row;
        }
    }

    const HdcInstallFailureKind retryFailure = needsSigning
        ? HdcDeviceBackend::classifyInstallFailure(signedInstall)
        : initialFailure;
    const bool shouldRetryOverwrite = retryFailure == HdcInstallFailureKind::VersionDowngrade
        || (needsSigning && installEntryAlreadyExists(signedInstall));
    if (!shouldRetryOverwrite) {
        row.insert(QStringLiteral("status"), needsSigning ? QStringLiteral("signed_install_failed") : QStringLiteral("initial_failed"));
        row.insert(QStringLiteral("retry_failure"), failureKindName(retryFailure));
        return row;
    }

    if (uninstallBundle.trimmed().isEmpty()) {
        row.insert(QStringLiteral("status"), QStringLiteral("downgrade_uninstall_bundle_missing"));
        return row;
    }

    const CommandResult uninstallResult =
        CommandRunner::runBlocking(backend.uninstallRequest(uninstallBundle, targetId));
    addCommandSummary(&row, QStringLiteral("downgrade_uninstall"), uninstallResult);
    if (!hdcUninstallSucceeded(uninstallResult)) {
        row.insert(QStringLiteral("status"), QStringLiteral("downgrade_uninstall_failed"));
        return row;
    }

    const CommandResult overwriteInstall =
        CommandRunner::runBlocking(backend.installRequest(installPath, targetId));
    addCommandSummary(&row, QStringLiteral("overwrite_install"), overwriteInstall);
    if (HdcDeviceBackend::installSucceeded(overwriteInstall)) {
        row.insert(QStringLiteral("status"), needsSigning ? QStringLiteral("signed_overwrite_ok") : QStringLiteral("direct_overwrite_ok"));
    } else {
        row.insert(QStringLiteral("status"), QStringLiteral("overwrite_install_failed"));
        row.insert(QStringLiteral("overwrite_failure"), failureKindName(HdcDeviceBackend::classifyInstallFailure(overwriteInstall)));
    }
    return row;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ReArk"));
    QCoreApplication::setOrganizationName(QStringLiteral("ReArk"));

    Options options;
    QString parseError;
    if (!parseOptions(QCoreApplication::arguments(), &options, &parseError)) {
        QTextStream(stderr) << "FAIL: " << parseError << '\n';
        return 2;
    }

    HarmonySigningSettings signingSettings;
    if (options.useRearkSettings) {
        signingSettings = SigningSettingsStore::loadHarmony();
    } else {
        const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        const QString keystorePassword = environment.value(options.keystorePasswordEnv);
        const QString keyPassword = options.keyPasswordEnv.trimmed().isEmpty()
            ? keystorePassword
            : environment.value(options.keyPasswordEnv);

        signingSettings.keystorePath = options.keystorePath;
        signingSettings.keystorePassword = keystorePassword;
        signingSettings.keyAlias = options.keyAlias;
        signingSettings.keyPassword = keyPassword;
        signingSettings.profilePath = options.profilePath;
        signingSettings.certificatePath = options.certificatePath;
    }

    const QString validationMessage = SigningSettingsStore::harmonyValidationMessage(signingSettings);
    if (!validationMessage.isEmpty()) {
        QTextStream(stderr) << "FAIL: " << validationMessage << '\n';
        if (options.useRearkSettings) {
            QSettings settings;
            QTextStream(stderr) << "QSettings fileName: " << settings.fileName() << '\n';
            QTextStream(stderr) << "QSettings keys: " << settings.allKeys().join(QStringLiteral(", ")) << '\n';
        }
        return 3;
    }

    const SigningMaterialStatus materialStatus = SigningMaterialInspector::inspectHarmony(signingSettings);
    if (materialStatus.tone == QStringLiteral("error")) {
        QTextStream(stderr) << "FAIL: " << materialStatus.summary << '\n';
        return 4;
    }

    QString sampleListError;
    const QDir samplesDir(options.samplesDir);
    QFileInfoList samples = options.samplesFile.trimmed().isEmpty()
        ? samplesDir.entryInfoList(
            { QStringLiteral("*.hap") },
            QDir::Files | QDir::Readable,
            QDir::Name)
        : readSamplesFile(options.samplesFile, &sampleListError);
    if (!sampleListError.isEmpty()) {
        QTextStream(stderr) << "FAIL: " << sampleListError << '\n';
        return 6;
    }
    if (options.limit > 0 && samples.size() > options.limit) {
        samples = samples.mid(0, options.limit);
    }

    QJsonObject report;
    report.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    report.insert(QStringLiteral("samples_dir"), options.samplesDir.trimmed().isEmpty() ? QString() : samplesDir.absolutePath());
    report.insert(QStringLiteral("samples_file"), options.samplesFile.trimmed());
    report.insert(QStringLiteral("target"), options.targetId);
    report.insert(QStringLiteral("count"), samples.size());
    report.insert(QStringLiteral("profile_bundle"), materialStatus.profileBundleName);
    report.insert(QStringLiteral("signing_status"), materialStatus.summary);
    report.insert(QStringLiteral("stripped_request_permissions"), QJsonArray::fromStringList(options.stripRequestPermissions));
    report.insert(QStringLiteral("forced_device_types"), QJsonArray::fromStringList(options.forceDeviceTypes));
    report.insert(QStringLiteral("forced_compatible_api"), options.forceCompatibleApi);
    report.insert(QStringLiteral("forced_target_api"), options.forceTargetApi);

    QJsonArray rows;
    QTextStream(stdout) << "Testing " << samples.size() << " HAP sample(s)\n";
    for (int i = 0; i < samples.size(); ++i) {
        const QFileInfo sample = samples.at(i);
        QTextStream(stdout) << "[" << (i + 1) << "/" << samples.size() << "] " << sample.fileName() << "\n";
        rows.append(installOne(
            sample,
            options.targetId,
            signingSettings,
            materialStatus,
            options.stripRequestPermissions,
            options.forceDeviceTypes,
            options.forceCompatibleApi,
            options.forceTargetApi));
    }
    report.insert(QStringLiteral("results"), rows);

    QJsonObject counts;
    for (const QJsonValue& value : rows) {
        const QString status = value.toObject().value(QStringLiteral("status")).toString(QStringLiteral("unknown"));
        counts.insert(status, counts.value(status).toInt() + 1);
    }
    report.insert(QStringLiteral("status_counts"), counts);

    QDir().mkpath(QFileInfo(options.outputPath).absolutePath());
    QFile output(options.outputPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream(stderr) << "FAIL: Cannot write report: " << options.outputPath << '\n';
        return 5;
    }
    output.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    QTextStream(stdout) << "Report: " << QFileInfo(output).absoluteFilePath() << "\n";
    return 0;
}
