#include "controller/DeviceRuntimeController.h"

#include "controller/SigningSettings.h"
#include "device/HarmonyPackageRewriter.h"
#include "signing/HarmonyHapSigner.h"
#include "signing/SigningMaterialInspector.h"

#include <hyle/hap/hap.h>

#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QtConcurrent>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace {

constexpr int kMinScreenRefreshIntervalMs = 500;
constexpr int kMaxScreenRefreshIntervalMs = 10000;
constexpr int kStaleTemporaryFileMaxAgeHours = 24;
constexpr auto kInstalledLaunchMetadataGroup = "DeviceRuntime/InstalledLaunchMetadata";

struct DevicePackageIdentity {
    QString bundleName;
    QString summaryError;
};

struct DeviceLaunchMetadata {
    QString bundleName;
    QString moduleName;
    QString abilityName;
    QString error;
};

struct DeviceInstallResult {
    bool installed = false;
    DeviceInstallStatus status = DeviceInstallStatus::None;
    DeviceInstallError error = DeviceInstallError::None;
    QString commandLog;
    DeviceLaunchMetadata launchMetadata;
};

struct DeviceInstallProbeResult {
    bool installed = false;
    bool needsSigningApproval = false;
    DeviceInstallStatus status = DeviceInstallStatus::None;
    DeviceInstallError error = DeviceInstallError::None;
    CommandResult initialInstall;
    DeviceLaunchMetadata launchMetadata;
};

bool hasLaunchMetadata(const DeviceLaunchMetadata& metadata)
{
    return !metadata.bundleName.trimmed().isEmpty();
}

QString packageFingerprint(const QString& packagePath, const QString& targetId)
{
    const QFileInfo info(packagePath);
    const QString payload = QStringLiteral("%1\n%2\n%3\n%4")
        .arg(targetId.trimmed(),
            info.absoluteFilePath(),
            QString::number(info.size()),
            QString::number(info.lastModified().toMSecsSinceEpoch()));
    return QString::fromLatin1(
        QCryptographicHash::hash(payload.toUtf8(), QCryptographicHash::Sha256).toHex());
}

DeviceLaunchMetadata loadInstalledLaunchMetadata(const QString& packagePath, const QString& targetId)
{
    DeviceLaunchMetadata metadata;
    if (packagePath.trimmed().isEmpty()) {
        return metadata;
    }

    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kInstalledLaunchMetadataGroup));
    settings.beginGroup(packageFingerprint(packagePath, targetId));
    metadata.bundleName = settings.value(QStringLiteral("bundle")).toString().trimmed();
    metadata.moduleName = settings.value(QStringLiteral("module")).toString().trimmed();
    metadata.abilityName = settings.value(QStringLiteral("ability")).toString().trimmed();
    return metadata;
}

void saveInstalledLaunchMetadata(
    const QString& packagePath,
    const QString& targetId,
    const DeviceLaunchMetadata& metadata)
{
    if (packagePath.trimmed().isEmpty() || !hasLaunchMetadata(metadata)) {
        return;
    }

    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kInstalledLaunchMetadataGroup));
    settings.beginGroup(packageFingerprint(packagePath, targetId));
    settings.setValue(QStringLiteral("bundle"), metadata.bundleName.trimmed());
    settings.setValue(QStringLiteral("module"), metadata.moduleName.trimmed());
    settings.setValue(QStringLiteral("ability"), metadata.abilityName.trimmed());
}

std::string toStdString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

QString fromStdString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

bool isDeviceRuntimeTemporaryFile(const QString& fileName)
{
    return (fileName.startsWith(QStringLiteral("reark-device-")) && fileName.endsWith(QStringLiteral(".jpeg")))
        || (fileName.startsWith(QStringLiteral("reark-ui-layout-")) && fileName.endsWith(QStringLiteral(".json")));
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

QString signedHapFileName(const QString& sourcePath)
{
    QString baseName = QFileInfo(sourcePath).completeBaseName().trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("reark-device");
    }
    return baseName + QStringLiteral("-signed.hap");
}

QString rewrittenHapFileName(const QString& sourcePath)
{
    QString baseName = QFileInfo(sourcePath).completeBaseName().trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("reark-device");
    }
    return baseName + QStringLiteral("-rebundle-unsigned.hap");
}

DevicePackageIdentity readPackageIdentity(const QString& hapPath)
{
    DevicePackageIdentity identity;
    auto summary = hyle::hap::summarize_decompiled_package(toStdString(hapPath));
    if (!summary) {
        identity.summaryError = QStringLiteral("Package summary failed: %1")
            .arg(fromStdString(summary.error().message()));
        return identity;
    }
    identity.bundleName = fromStdString(summary->bundle_name).trimmed();
    return identity;
}

QByteArray bytesToByteArray(const std::vector<std::byte>& bytes)
{
    return QByteArray(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()));
}

bool stringArrayContains(const QJsonValue& value, const QString& expected)
{
    if (!value.isArray()) {
        return false;
    }

    const QJsonArray array = value.toArray();
    for (const QJsonValue& item : array) {
        if (item.toString().compare(expected, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool abilityLooksLauncher(const QJsonObject& ability)
{
    const QJsonArray skills = ability.value(QStringLiteral("skills")).toArray();
    for (const QJsonValue& skillValue : skills) {
        const QJsonObject skill = skillValue.toObject();
        if (stringArrayContains(skill.value(QStringLiteral("entities")), QStringLiteral("entity.system.home"))
            || stringArrayContains(skill.value(QStringLiteral("actions")), QStringLiteral("action.system.home"))) {
            return true;
        }
    }
    return false;
}

QString firstAbilityName(const QJsonArray& abilities)
{
    for (const QJsonValue& value : abilities) {
        const QString name = value.toObject().value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty()) {
            return name;
        }
    }
    return {};
}

QString launcherAbilityName(const QJsonArray& abilities)
{
    for (const QJsonValue& value : abilities) {
        const QJsonObject ability = value.toObject();
        const QString name = ability.value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty() && abilityLooksLauncher(ability)) {
            return name;
        }
    }
    return firstAbilityName(abilities);
}

DeviceLaunchMetadata readLaunchMetadata(const QString& hapPath)
{
    DeviceLaunchMetadata metadata;
    if (hapPath.trimmed().isEmpty()) {
        return metadata;
    }

    auto session = hyle::hap::open_decompiled_package(toStdString(hapPath));
    if (!session) {
        metadata.error = QStringLiteral("Open package failed: %1").arg(fromStdString(session.error().message()));
        return metadata;
    }

    auto summary = session->summary();
    if (summary) {
        metadata.bundleName = fromStdString(summary->bundle_name).trimmed();
        metadata.moduleName = fromStdString(summary->module_name).trimmed();
    }

    auto moduleJson = session->read_resource(std::string("module.json"), 1024 * 1024);
    if (!moduleJson) {
        if (metadata.bundleName.isEmpty()) {
            metadata.error = QStringLiteral("Read module.json failed: %1")
                .arg(fromStdString(moduleJson.error().message()));
        }
        return metadata;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytesToByteArray(moduleJson->bytes), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (metadata.bundleName.isEmpty()) {
            metadata.error = QStringLiteral("Parse module.json failed.");
        }
        return metadata;
    }

    const QJsonObject root = document.object();
    const QJsonObject app = root.value(QStringLiteral("app")).toObject();
    const QJsonObject module = root.value(QStringLiteral("module")).toObject();
    const QString bundleName = app.value(QStringLiteral("bundleName")).toString().trimmed();
    const QString moduleName = module.value(QStringLiteral("name")).toString().trimmed();
    const QString abilityName = launcherAbilityName(module.value(QStringLiteral("abilities")).toArray()).trimmed();

    if (!bundleName.isEmpty()) {
        metadata.bundleName = bundleName;
    }
    if (!moduleName.isEmpty()) {
        metadata.moduleName = moduleName;
    }
    metadata.abilityName = abilityName;

    return metadata;
}

QString signingSettingsStatusLine(const QString& validationMessage)
{
    return validationMessage.trimmed().isEmpty()
        ? QStringLiteral("# signing_settings: configured")
        : QStringLiteral("# signing_settings: invalid\n# signing_settings_error: %1").arg(validationMessage);
}

QString installAttemptSummary(
    const CommandResult& initialInstall,
    const CommandResult& signResult,
    const CommandResult& signedInstall,
    const HarmonyBundleRewriteResult* rewriteResult,
    const QString& extra)
{
    QString text;
    text += QStringLiteral("# status: %1\n").arg(
        HdcDeviceBackend::installSucceeded(signedInstall) ? QStringLiteral("ok") : QStringLiteral("error"));
    text += QStringLiteral("# operation: install_package\n");
    if (!extra.trimmed().isEmpty()) {
        text += extra.trimmed() + QStringLiteral("\n");
    }
    text += QStringLiteral("\n[initial install]\n%1\n").arg(HdcDeviceBackend::resultSummary(initialInstall));
    if (rewriteResult != nullptr) {
        text += QStringLiteral("\n[bundle rewrite]\n");
        text += QStringLiteral("# status: %1\n").arg(rewriteResult->ok ? QStringLiteral("ok") : QStringLiteral("error"));
        text += QStringLiteral("# input_hap: %1\n").arg(rewriteResult->inputHapPath);
        text += QStringLiteral("# output_hap: %1\n").arg(rewriteResult->outputHapPath);
        if (!rewriteResult->error.trimmed().isEmpty()) {
            text += QStringLiteral("# error: %1\n").arg(rewriteResult->error.trimmed());
        }
        if (!rewriteResult->report.trimmed().isEmpty()) {
            text += QStringLiteral("\n%1\n").arg(rewriteResult->report.trimmed());
        }
        if (rewriteResult->packingResult.started || !rewriteResult->packingResult.program.isEmpty()) {
            text += QStringLiteral("\n[pack]\n%1\n").arg(
                HarmonyHapSigner::resultSummary(rewriteResult->packingResult));
        }
    }
    if (signResult.started || !signResult.program.isEmpty()) {
        text += QStringLiteral("\n[sign]\n%1\n").arg(HarmonyHapSigner::resultSummary(signResult));
    }
    if (signedInstall.started || !signedInstall.program.isEmpty()) {
        text += QStringLiteral("\n[signed install]\n%1").arg(HdcDeviceBackend::resultSummary(signedInstall));
    }
    return text.trimmed();
}

DeviceInstallResult installPackageWithAutoSigning(
    const QString& packagePath,
    const QString& targetId,
    const CommandResult& initialInstall,
    std::stop_token stopToken)
{
    DeviceInstallResult result;
    HdcDeviceBackend backend;

    if (stopToken.stop_requested()) {
        result.error = DeviceInstallError::Cancelled;
        result.commandLog = QStringLiteral("# status: error\n# operation: install_package\n# cancelled: true\n\n%1")
            .arg(HdcDeviceBackend::resultSummary(initialInstall));
        return result;
    }

    const HarmonySigningSettings signingSettings = SigningSettingsStore::loadHarmony();
    const QString validationMessage = SigningSettingsStore::harmonyValidationMessage(signingSettings);
    if (!validationMessage.isEmpty()) {
        result.error = DeviceInstallError::UnsignedWithoutSigning;
        result.commandLog = QStringLiteral("# status: error\n# operation: install_package\n# re_sign: approved_by_user\n# auto_sign: unavailable\n%1\n\n%2")
            .arg(signingSettingsStatusLine(validationMessage), HdcDeviceBackend::resultSummary(initialInstall));
        return result;
    }

    const SigningMaterialStatus materialStatus = SigningMaterialInspector::inspectHarmony(signingSettings);
    const DevicePackageIdentity packageIdentity = readPackageIdentity(packagePath);

    QTemporaryDir signedDir(QDir::temp().filePath(QStringLiteral("ReArk-device-signed-hap-XXXXXX")));
    if (!signedDir.isValid()) {
        result.error = DeviceInstallError::TemporaryDirectoryFailed;
        result.commandLog = QStringLiteral("# status: error\n# operation: install_package\n# re_sign: approved_by_user\n# auto_sign: failed\n\n%1")
            .arg(HdcDeviceBackend::resultSummary(initialInstall));
        return result;
    }

    QString signInputHapPath = packagePath;
    HarmonyBundleRewriteResult rewriteResult;
    bool rewriteUsed = false;
    if (!packageIdentity.bundleName.isEmpty()
        && !materialStatus.profileBundleName.isEmpty()
        && packageIdentity.bundleName != materialStatus.profileBundleName) {
        rewriteUsed = true;
        const QString rewrittenHapPath = signedDir.filePath(rewrittenHapFileName(packagePath));
        rewriteResult = HarmonyPackageRewriter::rewriteBundleIdentity({
            .inputHapPath = packagePath,
            .outputHapPath = rewrittenHapPath,
            .oldBundleName = packageIdentity.bundleName,
            .newBundleName = materialStatus.profileBundleName,
            .javaProgram = HarmonyHapSigner::resolvedJavaProgram(),
            .packingToolPath = HarmonyHapSigner::bundledPackingToolPath(),
            .stopToken = stopToken
        });
        if (stopToken.stop_requested()) {
            result.error = DeviceInstallError::Cancelled;
            result.commandLog = installAttemptSummary(
                initialInstall,
                {},
                {},
                &rewriteResult,
                QStringLiteral("# re_sign: approved_by_user\n# cancelled: true"));
            return result;
        }
        if (!rewriteResult.ok) {
            signedDir.setAutoRemove(false);
            result.error = DeviceInstallError::BundleRewriteFailed;
            const QString extra = QStringLiteral("# re_sign: approved_by_user\n# auto_sign: blocked\n# bundle_rewrite: failed\n# package_bundle: %1\n# signing_profile_bundle: %2\n# retained_dir: %3")
                .arg(packageIdentity.bundleName, materialStatus.profileBundleName, signedDir.path());
            result.commandLog = installAttemptSummary(initialInstall, {}, {}, &rewriteResult, extra);
            return result;
        }
        signInputHapPath = rewrittenHapPath;
    }

    const QString signedHapPath = signedDir.filePath(signedHapFileName(signInputHapPath));
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
        .outputHapPath = signedHapPath
    }), stopToken);
    if (stopToken.stop_requested()) {
        result.error = DeviceInstallError::Cancelled;
        result.commandLog = installAttemptSummary(
            initialInstall,
            signResult,
            {},
            rewriteUsed ? &rewriteResult : nullptr,
            QStringLiteral("# re_sign: approved_by_user\n# cancelled: true"));
        return result;
    }
    if (!signResult.succeeded()) {
        signedDir.setAutoRemove(false);
        result.error = DeviceInstallError::SigningFailed;
        QString extra = QStringLiteral("# re_sign: approved_by_user\n# auto_sign: failed\n# bundle_rewrite: %1\n# signed_hap: %2\n# retained_dir: %3\n%4")
            .arg(rewriteUsed ? QStringLiteral("used") : QStringLiteral("not_needed"),
                signedHapPath,
                signedDir.path(),
                signingSettingsStatusLine(validationMessage));
        if (!packageIdentity.summaryError.isEmpty()) {
            extra += QStringLiteral("\n# package_summary_warning: %1").arg(packageIdentity.summaryError);
        }
        result.commandLog = installAttemptSummary(
            initialInstall,
            signResult,
            {},
            rewriteUsed ? &rewriteResult : nullptr,
            extra);
        return result;
    }

    const CommandResult signedInstall = CommandRunner::runBlocking(
        backend.installRequest(signedHapPath, targetId),
        stopToken);
    if (stopToken.stop_requested()) {
        result.error = DeviceInstallError::Cancelled;
        result.commandLog = installAttemptSummary(
            initialInstall,
            signResult,
            signedInstall,
            rewriteUsed ? &rewriteResult : nullptr,
            QStringLiteral("# re_sign: approved_by_user\n# cancelled: true"));
        return result;
    }
    result.installed = HdcDeviceBackend::installSucceeded(signedInstall);
    if (result.installed) {
        result.status = rewriteUsed
            ? DeviceInstallStatus::SignedRewrittenInstalled
            : DeviceInstallStatus::SignedInstalled;
        result.launchMetadata = readLaunchMetadata(signInputHapPath);
    } else {
        signedDir.setAutoRemove(false);
        result.error = DeviceInstallError::SignedInstallFailed;
    }

    QString extra = QStringLiteral("# re_sign: approved_by_user\n# auto_sign: used\n# bundle_rewrite: %1\n%2")
        .arg(rewriteUsed ? QStringLiteral("used") : QStringLiteral("not_needed"),
            signingSettingsStatusLine(validationMessage));
    if (!packageIdentity.bundleName.isEmpty()) {
        extra += QStringLiteral("\n# package_bundle: %1").arg(packageIdentity.bundleName);
    }
    if (!materialStatus.profileBundleName.isEmpty()) {
        extra += QStringLiteral("\n# signing_profile_bundle: %1").arg(materialStatus.profileBundleName);
    }
    if (!result.installed) {
        extra += QStringLiteral("\n# signed_hap: %1\n# retained_dir: %2").arg(signedHapPath, signedDir.path());
    }
    if (!packageIdentity.summaryError.isEmpty()) {
        extra += QStringLiteral("\n# package_summary_warning: %1").arg(packageIdentity.summaryError);
    }
    result.commandLog = installAttemptSummary(
        initialInstall,
        signResult,
        signedInstall,
        rewriteUsed ? &rewriteResult : nullptr,
        extra);
    return result;
}

DeviceInstallProbeResult probeInstallPackage(
    const QString& packagePath,
    const QString& targetId,
    std::stop_token stopToken)
{
    DeviceInstallProbeResult result;
    HdcDeviceBackend backend;
    result.initialInstall = CommandRunner::runBlocking(
        backend.installRequest(packagePath, targetId),
        stopToken);
    if (stopToken.stop_requested()) {
        result.error = DeviceInstallError::Cancelled;
        return result;
    }
    result.installed = HdcDeviceBackend::installSucceeded(result.initialInstall);
    if (result.installed) {
        result.status = DeviceInstallStatus::Installed;
        result.launchMetadata = readLaunchMetadata(packagePath);
        return result;
    }

    if (isMissingSignatureInstallFailure(result.initialInstall)) {
        result.needsSigningApproval = true;
        result.status = DeviceInstallStatus::RequiresSigning;
        return result;
    }

    result.error = DeviceInstallError::InstallFailed;
    return result;
}

} // namespace

DeviceRuntimeController::DeviceRuntimeController(QObject* parent)
    : QObject(parent)
    , runner_(this)
    , uiBackend_(backend_)
    , status_(tr("Ready"))
{
    cleanupStaleTemporaryLocalFiles();
    screenRefreshStatus_ = tr("Screen refresh stopped.");
    screenRefreshTimer_.setSingleShot(false);
    connect(&screenRefreshTimer_, &QTimer::timeout, this, &DeviceRuntimeController::captureAutoRefreshFrame);
    initialUiSnapshotTimer_.setSingleShot(true);
    initialUiSnapshotTimer_.setInterval(160);
    connect(&initialUiSnapshotTimer_, &QTimer::timeout, this, &DeviceRuntimeController::performInitialUiSnapshot);
}

DeviceRuntimeController::~DeviceRuntimeController()
{
    screenRefreshTimer_.stop();
    cleanupTemporaryLocalFiles();
}

QVariantList DeviceRuntimeController::devices() const
{
    return devices_;
}

QString DeviceRuntimeController::selectedDeviceId() const
{
    return selectedDeviceId_;
}

void DeviceRuntimeController::setSelectedDeviceId(const QString& selectedDeviceId)
{
    const QString trimmed = selectedDeviceId.trimmed();
    if (selectedDeviceId_ == trimmed) {
        return;
    }
    selectedDeviceId_ = trimmed;
    clearDeviceSessionState();
    emit selectedDeviceChanged();
    if (!launchMetadataPackagePath_.isEmpty()) {
        refreshLaunchMetadata(launchMetadataPackagePath_);
    }
    scheduleInitialUiSnapshot();
}

bool DeviceRuntimeController::busy() const
{
    return busy_;
}

QString DeviceRuntimeController::activeOperation() const
{
    return activeOperation_;
}

bool DeviceRuntimeController::screenRefreshBusy() const
{
    return busy_ && activeOperation_ == tr("Refresh screen");
}

QString DeviceRuntimeController::status() const
{
    return status_;
}

QString DeviceRuntimeController::errorMessage() const
{
    return errorMessage_;
}

QString DeviceRuntimeController::commandLog() const
{
    return commandLog_;
}

QString DeviceRuntimeController::hilogText() const
{
    return hilogText_;
}

QString DeviceRuntimeController::screenshotPath() const
{
    return screenshotPath_;
}

QString DeviceRuntimeController::launchBundleName() const
{
    return launchBundleName_;
}

QString DeviceRuntimeController::launchModuleName() const
{
    return launchModuleName_;
}

QString DeviceRuntimeController::launchAbilityName() const
{
    return launchAbilityName_;
}

QVariantList DeviceRuntimeController::uiNodes() const
{
    return uiNodes_;
}

QVariantList DeviceRuntimeController::filteredUiNodes() const
{
    return filteredUiNodes_;
}

QString DeviceRuntimeController::uiNodeSummary() const
{
    return uiNodeSummary_;
}

QString DeviceRuntimeController::uiNodeFilter() const
{
    return uiNodeFilter_;
}

void DeviceRuntimeController::setUiNodeFilter(const QString& uiNodeFilter)
{
    const QString trimmed = uiNodeFilter.trimmed();
    if (uiNodeFilter_ == trimmed) {
        return;
    }
    uiNodeFilter_ = trimmed;
    refreshFilteredUiNodes();
    emit uiNodesChanged();
    emit uiNodeFilterChanged();
}

bool DeviceRuntimeController::screenRefreshRunning() const
{
    return screenRefreshTimer_.isActive();
}

QString DeviceRuntimeController::screenRefreshStatus() const
{
    return screenRefreshStatus_;
}

int DeviceRuntimeController::screenRefreshFrameCount() const
{
    return screenRefreshFrameCount_;
}

double DeviceRuntimeController::screenRefreshFps() const
{
    return screenRefreshFps_;
}

void DeviceRuntimeController::refreshDevices()
{
    runOperation(tr("Refresh devices"), backend_.listTargetsRequest(), [this](const CommandResult& result) {
        if (!result.succeeded()) {
            return;
        }
        const QList<HdcDeviceTarget> targets = HdcDeviceBackend::parseTargets(result.standardOutput);
        setDevices(targets);
        setStatus(targets.isEmpty()
                ? tr("No HDC targets found.")
                : tr("Found %n HDC target(s).", nullptr, targets.size()));
        scheduleInitialUiSnapshot();
    });
}

void DeviceRuntimeController::refreshLaunchMetadata(const QString& packagePath)
{
    const QString trimmed = packagePath.trimmed();
    launchMetadataPackagePath_ = trimmed;
    const int requestId = ++launchMetadataRequestId_;
    if (trimmed.isEmpty() || !QFileInfo::exists(trimmed)) {
        setLaunchMetadata({}, {}, {});
        return;
    }

    auto* watcher = new QFutureWatcher<DeviceLaunchMetadata>(this);
    connect(watcher, &QFutureWatcher<DeviceLaunchMetadata>::finished, this, [this, watcher, requestId]() {
        const DeviceLaunchMetadata metadata = watcher->result();
        watcher->deleteLater();
        if (requestId != launchMetadataRequestId_) {
            return;
        }
        setLaunchMetadata(metadata.bundleName, metadata.moduleName, metadata.abilityName);
    });
    const QString targetId = currentTargetId();
    watcher->setFuture(QtConcurrent::run([trimmed, targetId]() {
        DeviceLaunchMetadata metadata = loadInstalledLaunchMetadata(trimmed, targetId);
        if (hasLaunchMetadata(metadata)) {
            return metadata;
        }
        return readLaunchMetadata(trimmed);
    }));
}

void DeviceRuntimeController::installPackage(const QString& packagePath)
{
    const QString trimmed = packagePath.trimmed();
    if (trimmed.isEmpty()) {
        setErrorMessage(tr("No package is loaded."));
        return;
    }
    if (!QFileInfo::exists(trimmed)) {
        setErrorMessage(tr("Package does not exist: %1").arg(trimmed));
        return;
    }
    if (screenRefreshTimer_.isActive()) {
        stopScreenRefresh();
    }
    if (busy_) {
        setErrorMessage(activeOperation_ == tr("Refresh screen")
                ? tr("Waiting for screen refresh to finish before installing.")
                : tr("Another device operation is still running."));
        return;
    }
    if (!ensureDeviceAvailable()) {
        return;
    }

    hasPendingSigningInstall_ = false;
    pendingSigningPackagePath_.clear();
    pendingSigningTargetId_.clear();
    installStopSource_ = std::stop_source {};
    const std::stop_token stopToken = installStopSource_.get_token();
    const quint64 runId = ++asyncInstallRunId_;
    setBusy(true, tr("Install package"));
    setErrorMessage({});
    auto* probeWatcher = new QFutureWatcher<DeviceInstallProbeResult>(this);
    const QString targetId = currentTargetId();
    connect(probeWatcher, &QFutureWatcher<DeviceInstallProbeResult>::finished, this, [this, probeWatcher, trimmed, targetId, runId]() {
        const DeviceInstallProbeResult result = probeWatcher->result();
        probeWatcher->deleteLater();
        if (runId != asyncInstallRunId_) {
            return;
        }

        if (result.installed) {
            appendCommandLog(result.initialInstall);
            if (hasLaunchMetadata(result.launchMetadata)) {
                saveInstalledLaunchMetadata(trimmed, targetId, result.launchMetadata);
                setLaunchMetadata(
                    result.launchMetadata.bundleName,
                    result.launchMetadata.moduleName,
                    result.launchMetadata.abilityName);
            }
            setStatus(result.status == DeviceInstallStatus::None
                    ? tr("Package installed.")
                    : translatedInstallStatus(result.status));
            setBusy(false);
            return;
        }

        if (!result.needsSigningApproval) {
            appendCommandLog(result.initialInstall);
            setErrorMessage(result.error == DeviceInstallError::None
                    ? tr("Install package failed.")
                    : translatedInstallError(result.error));
            setBusy(false);
            return;
        }

        pendingSigningPackagePath_ = trimmed;
        pendingSigningTargetId_ = targetId;
        pendingSigningInitialInstall_ = result.initialInstall;
        hasPendingSigningInstall_ = true;
        setStatus(tr("Install requires signing confirmation."));
        setBusy(false);
        emit resignInstallConfirmationRequested(
            tr("Re-sign and install package?"),
            tr("The device rejected this package because its signature is missing or invalid. ReArk can use your configured Harmony signing material to re-sign the package and install the signed copy."));
    });
    probeWatcher->setFuture(QtConcurrent::run([trimmed, targetId, stopToken]() {
        return probeInstallPackage(trimmed, targetId, stopToken);
    }));
}

void DeviceRuntimeController::approveResignInstall()
{
    if (!hasPendingSigningInstall_) {
        setErrorMessage(tr("No package is waiting for signing approval."));
        return;
    }
    if (busy_) {
        setErrorMessage(tr("Another device operation is still running."));
        return;
    }

    const QString packagePath = pendingSigningPackagePath_;
    const QString targetId = pendingSigningTargetId_;
    const CommandResult initialInstall = pendingSigningInitialInstall_;
    hasPendingSigningInstall_ = false;
    pendingSigningPackagePath_.clear();
    pendingSigningTargetId_.clear();
    installStopSource_ = std::stop_source {};
    const std::stop_token stopToken = installStopSource_.get_token();
    const quint64 runId = ++asyncInstallRunId_;

    setBusy(true, tr("Re-sign and install package"));
    setErrorMessage({});
    auto* watcher = new QFutureWatcher<DeviceInstallResult>(this);
    connect(watcher, &QFutureWatcher<DeviceInstallResult>::finished, this, [this, watcher, packagePath, targetId, runId]() {
        const DeviceInstallResult result = watcher->result();
        watcher->deleteLater();
        if (runId != asyncInstallRunId_) {
            return;
        }

        if (!result.commandLog.trimmed().isEmpty()) {
            if (!commandLog_.isEmpty()) {
                commandLog_ += QStringLiteral("\n\n");
            }
            commandLog_ += result.commandLog.trimmed();
            emit commandLogChanged();
        }

        if (result.installed) {
            if (hasLaunchMetadata(result.launchMetadata)) {
                saveInstalledLaunchMetadata(packagePath, targetId, result.launchMetadata);
                setLaunchMetadata(
                    result.launchMetadata.bundleName,
                    result.launchMetadata.moduleName,
                    result.launchMetadata.abilityName);
            }
            setStatus(result.status == DeviceInstallStatus::None
                    ? tr("Package installed.")
                    : translatedInstallStatus(result.status));
        } else {
            setErrorMessage(result.error == DeviceInstallError::None
                    ? tr("Install package failed.")
                    : translatedInstallError(result.error));
        }
        setBusy(false);
    });
    watcher->setFuture(QtConcurrent::run([packagePath, targetId, initialInstall, stopToken]() {
        return installPackageWithAutoSigning(packagePath, targetId, initialInstall, stopToken);
    }));
}

void DeviceRuntimeController::rejectResignInstall()
{
    if (!hasPendingSigningInstall_) {
        return;
    }

    if (!commandLog_.isEmpty()) {
        commandLog_ += QStringLiteral("\n\n");
    }
    commandLog_ += QStringLiteral("# status: error\n# operation: install_package\n# re_sign: rejected_by_user\n\n%1")
        .arg(HdcDeviceBackend::resultSummary(pendingSigningInitialInstall_));
    emit commandLogChanged();

    hasPendingSigningInstall_ = false;
    pendingSigningPackagePath_.clear();
    pendingSigningTargetId_.clear();
    setErrorMessage(tr("Install cancelled. Package signing was not approved."));
}

void DeviceRuntimeController::startAbility(const QString& bundleName, const QString& abilityName)
{
    if (!ensureDeviceAvailable()) {
        return;
    }

    const QString bundle = bundleName.trimmed();
    if (bundle.isEmpty()) {
        setErrorMessage(tr("Bundle name is required."));
        return;
    }

    const QString module = launchModuleName_.isEmpty() ? QStringLiteral("entry") : launchModuleName_;
    runOperation(
        tr("Start ability"),
        backend_.startAbilityRequest(bundle, abilityName, module, currentTargetId()),
        [this, bundle, ability = abilityName.trimmed(), module](const CommandResult& result) {
            if (HdcDeviceBackend::startSucceeded(result)) {
                saveInstalledLaunchMetadata(
                    launchMetadataPackagePath_,
                    currentTargetId(),
                    {
                        .bundleName = bundle,
                        .moduleName = module,
                        .abilityName = ability
                    });
                setStatus(tr("Start command accepted."));
            } else {
                setErrorMessage(tr("Start ability failed."));
            }
        });
}

void DeviceRuntimeController::readHilog(const QString& filter, const QString& level, int maxLines)
{
    if (!ensureDeviceAvailable()) {
        return;
    }

    runOperation(
        tr("Read hilog"),
        backend_.hilogRequest(currentTargetId(), level),
        [this, filter, maxLines](const CommandResult& result) {
            const QString combinedOutput = result.standardOutput + result.standardError;
            if (!result.succeeded() && combinedOutput.trimmed().isEmpty()) {
                return;
            }
            hilogText_ = HdcDeviceBackend::filterHilog(
                combinedOutput,
                filter,
                maxLines);
            if (hilogText_.isEmpty()) {
                hilogText_ = tr("[No hilog lines matched]");
            }
            emit hilogTextChanged();
            setStatus(result.succeeded() ? tr("Hilog captured.") : tr("Hilog captured before timeout."));
        });
}

void DeviceRuntimeController::captureScreenshot()
{
    captureScreenshot(ScreenshotRequestKind::Manual);
}

void DeviceRuntimeController::scheduleInitialUiSnapshot()
{
    const QString targetId = currentTargetId();
    if (targetId.isEmpty()
        || targetId == initialUiSnapshotDeviceId_
        || screenRefreshTimer_.isActive()
        || initialUiSnapshotTimer_.isActive()) {
        return;
    }
    if (!screenshotPath_.isEmpty() && !uiNodes_.isEmpty()) {
        return;
    }
    initialUiSnapshotTimer_.start();
}

void DeviceRuntimeController::performInitialUiSnapshot()
{
    const QString targetId = currentTargetId();
    if (targetId.isEmpty()
        || targetId == initialUiSnapshotDeviceId_
        || screenRefreshTimer_.isActive()) {
        return;
    }
    if (!screenshotPath_.isEmpty() && !uiNodes_.isEmpty()) {
        return;
    }
    if (busy_) {
        initialUiSnapshotTimer_.start();
        return;
    }

    initialUiSnapshotDeviceId_ = targetId;
    captureUiSnapshot();
}

void DeviceRuntimeController::captureScreenshot(ScreenshotRequestKind kind)
{
    if (currentTargetId().isEmpty()) {
        const QString message = tr("No device is connected.");
        if (kind == ScreenshotRequestKind::AutoRefresh) {
            stopScreenRefresh();
            setScreenRefreshStatus(message);
        }
        setErrorMessage(message);
        return;
    }

    if (busy_) {
        if (kind == ScreenshotRequestKind::AutoRefresh) {
            setScreenRefreshStatus(tr("Waiting for current device operation..."));
        } else {
            setErrorMessage(tr("Another device operation is still running."));
        }
        return;
    }

    const QString remotePath = QStringLiteral("/data/local/tmp/reark-screenshot.jpeg");
    const QString localPath = makeLocalScreenshotPath();
    const bool autoRefresh = kind == ScreenshotRequestKind::AutoRefresh;
    setBusy(true, autoRefresh ? tr("Refresh screen") : tr("Capture screenshot"));
    setErrorMessage({});

    activeCommandId_ = runner_.run(
        backend_.screenshotCaptureRequest(remotePath, currentTargetId()),
        [this, remotePath, localPath, autoRefresh](const CommandResult& captureResult) {
            appendCommandLog(captureResult);
            if (!captureResult.succeeded()) {
                activeCommandId_ = 0;
                const QString message = tr("Screenshot capture failed.");
                if (autoRefresh && screenRefreshTimer_.isActive()) {
                    setScreenRefreshStatus(message);
                }
                setErrorMessage(message);
                setBusy(false);
                return;
            }

            activeCommandId_ = runner_.run(
                backend_.receiveFileRequest(remotePath, localPath, currentTargetId()),
                [this, remotePath, localPath, autoRefresh](const CommandResult& receiveResult) {
                    appendCommandLog(receiveResult);
                    runner_.run(backend_.removeRemoteFileRequest(remotePath, currentTargetId()), [](const CommandResult&) {});
                    activeCommandId_ = 0;
                    if (receiveResult.succeeded() && QFileInfo::exists(localPath)) {
                        rememberTemporaryLocalFile(localPath);
                        screenshotPath_ = localPath;
                        emit screenshotPathChanged();
                        cleanupTemporaryLocalFiles(screenshotPath_);
                        if (autoRefresh && screenRefreshTimer_.isActive()) {
                            ++screenRefreshFrameCount_;
                            const qint64 elapsedMs = screenRefreshElapsed_.isValid()
                                ? screenRefreshElapsed_.elapsed()
                                : 0;
                            screenRefreshFps_ = elapsedMs > 0
                                ? (screenRefreshFrameCount_ * 1000.0 / double(elapsedMs))
                                : 0.0;
                            emit screenRefreshFrameChanged();
                            setScreenRefreshStatus(tr("Auto refresh: %1 frame(s), %2 FPS.")
                                    .arg(screenRefreshFrameCount_)
                                    .arg(screenRefreshFps_, 0, 'f', 1));
                        } else {
                            setStatus(tr("Screenshot updated."));
                        }
                    } else {
                        QFile::remove(localPath);
                        const QString message = tr("Screenshot download failed: %1").arg(localPath);
                        if (autoRefresh && screenRefreshTimer_.isActive()) {
                            setScreenRefreshStatus(message);
                        }
                        setErrorMessage(message);
                    }
                    setBusy(false);
                });
        });
}

void DeviceRuntimeController::captureUiSnapshot(const QString& bundleName)
{
    if (!ensureDeviceAvailable()) {
        return;
    }
    if (busy_) {
        setErrorMessage(tr("Another device operation is still running."));
        return;
    }

    const QString screenshotRemotePath = QStringLiteral("/data/local/tmp/reark-screenshot.jpeg");
    const QString layoutRemotePath = QStringLiteral("/data/local/tmp/reark-ui-layout.json");
    const QString screenshotLocalPath = makeLocalScreenshotPath();
    const QString layoutLocalPath = makeLocalLayoutPath();
    const QString targetId = currentTargetId();
    setBusy(true, tr("Capture UI snapshot"));
    setErrorMessage({});

    activeCommandId_ = runner_.run(
        backend_.screenshotCaptureRequest(screenshotRemotePath, targetId),
        [this, screenshotRemotePath, layoutRemotePath, screenshotLocalPath, layoutLocalPath, targetId, bundleName](const CommandResult& captureResult) {
            appendCommandLog(captureResult);
            if (!captureResult.succeeded()) {
                activeCommandId_ = 0;
                setErrorMessage(tr("Screenshot capture failed."));
                setBusy(false);
                return;
            }

            activeCommandId_ = runner_.run(
                backend_.receiveFileRequest(screenshotRemotePath, screenshotLocalPath, targetId),
                [this, screenshotRemotePath, layoutRemotePath, screenshotLocalPath, layoutLocalPath, targetId, bundleName](const CommandResult& screenshotReceiveResult) {
                    appendCommandLog(screenshotReceiveResult);
                    runner_.run(backend_.removeRemoteFileRequest(screenshotRemotePath, targetId), [](const CommandResult&) {});
                    if (!screenshotReceiveResult.succeeded() || !QFileInfo::exists(screenshotLocalPath)) {
                        QFile::remove(screenshotLocalPath);
                        activeCommandId_ = 0;
                        setErrorMessage(tr("Screenshot download failed: %1").arg(screenshotLocalPath));
                        setBusy(false);
                        return;
                    }

                    rememberTemporaryLocalFile(screenshotLocalPath);
                    screenshotPath_ = screenshotLocalPath;
                    emit screenshotPathChanged();
                    cleanupTemporaryLocalFiles(screenshotPath_);

                    activeCommandId_ = runner_.run(
                        uiBackend_.dumpLayoutRequest(layoutRemotePath, targetId, bundleName),
                        [this, layoutRemotePath, layoutLocalPath, targetId](const CommandResult& dumpResult) {
                            appendCommandLog(dumpResult);
                            if (!dumpResult.succeeded()) {
                                activeCommandId_ = 0;
                                setErrorMessage(tr("UI layout dump failed."));
                                setBusy(false);
                                return;
                            }

                            activeCommandId_ = runner_.run(
                                backend_.receiveFileRequest(layoutRemotePath, layoutLocalPath, targetId),
                                [this, layoutRemotePath, layoutLocalPath, targetId](const CommandResult& layoutReceiveResult) {
                                    appendCommandLog(layoutReceiveResult);
                                    runner_.run(backend_.removeRemoteFileRequest(layoutRemotePath, targetId), [](const CommandResult&) {});
                                    activeCommandId_ = 0;
                                    if (!layoutReceiveResult.succeeded() || !QFileInfo::exists(layoutLocalPath)) {
                                        setErrorMessage(tr("UI layout download failed."));
                                        setBusy(false);
                                        return;
                                    }

                                    QFile file(layoutLocalPath);
                                    if (!file.open(QIODevice::ReadOnly)) {
                                        QFile::remove(layoutLocalPath);
                                        setErrorMessage(tr("Could not read UI layout: %1").arg(layoutLocalPath));
                                        setBusy(false);
                                        return;
                                    }

                                    const QByteArray data = file.readAll();
                                    file.close();
                                    QFile::remove(layoutLocalPath);
                                    setUiNodes(UiAutomationBackend::parseLayout(data));
                                    setStatus(tr("UI snapshot captured: %n node(s).", nullptr, parsedUiNodes_.size()));
                                    setBusy(false);
                                });
                        });
                });
        });
}

void DeviceRuntimeController::refreshUiLayout(const QString& bundleName)
{
    if (!ensureDeviceAvailable()) {
        return;
    }
    if (busy_) {
        setErrorMessage(tr("Another device operation is still running."));
        return;
    }

    const QString remotePath = QStringLiteral("/data/local/tmp/reark-ui-layout.json");
    const QString localPath = makeLocalLayoutPath();
    setBusy(true, tr("Refresh UI layout"));
    setErrorMessage({});

    activeCommandId_ = runner_.run(
        uiBackend_.dumpLayoutRequest(remotePath, currentTargetId(), bundleName),
        [this, remotePath, localPath](const CommandResult& dumpResult) {
            appendCommandLog(dumpResult);
            if (!dumpResult.succeeded()) {
                activeCommandId_ = 0;
                setErrorMessage(tr("UI layout dump failed."));
                setBusy(false);
                return;
            }

            activeCommandId_ = runner_.run(
                backend_.receiveFileRequest(remotePath, localPath, currentTargetId()),
                [this, remotePath, localPath](const CommandResult& receiveResult) {
                    appendCommandLog(receiveResult);
                    runner_.run(backend_.removeRemoteFileRequest(remotePath, currentTargetId()), [](const CommandResult&) {});
                    activeCommandId_ = 0;
                    if (!receiveResult.succeeded() || !QFileInfo::exists(localPath)) {
                        QFile::remove(localPath);
                        setErrorMessage(tr("UI layout download failed."));
                        setBusy(false);
                        return;
                    }

                    QFile file(localPath);
                    if (!file.open(QIODevice::ReadOnly)) {
                        QFile::remove(localPath);
                        setErrorMessage(tr("Could not read UI layout: %1").arg(localPath));
                        setBusy(false);
                        return;
                    }

                    const QByteArray data = file.readAll();
                    file.close();
                    QFile::remove(localPath);
                    setUiNodes(UiAutomationBackend::parseLayout(data));
                    setStatus(tr("UI layout captured: %n node(s).", nullptr, parsedUiNodes_.size()));
                    setBusy(false);
                });
        });
}

void DeviceRuntimeController::tapUi(int x, int y)
{
    if (!ensureDeviceAvailable()) {
        return;
    }
    runOperation(
        tr("Tap UI"),
        uiBackend_.tapRequest(x, y, currentTargetId()),
        [this, x, y](const CommandResult& result) {
            if (result.succeeded()) {
                setStatus(tr("Tapped (%1, %2).").arg(x).arg(y));
            }
        });
}

void DeviceRuntimeController::tapUiNode(int nodeIndex)
{
    if (nodeIndex < 0 || nodeIndex >= parsedUiNodes_.size()) {
        setErrorMessage(tr("UI node is not available."));
        return;
    }
    const QPoint point = parsedUiNodes_.at(nodeIndex).center();
    tapUi(point.x(), point.y());
}

void DeviceRuntimeController::inputUiTextAt(int x, int y, const QString& text)
{
    if (!ensureDeviceAvailable()) {
        return;
    }
    if (text.isEmpty()) {
        setErrorMessage(tr("Text is required."));
        return;
    }
    runOperation(
        tr("Input UI text"),
        uiBackend_.inputTextAtRequest(x, y, text, currentTargetId()));
}

void DeviceRuntimeController::inputFocusedUiText(const QString& text)
{
    if (!ensureDeviceAvailable()) {
        return;
    }
    if (text.isEmpty()) {
        setErrorMessage(tr("Text is required."));
        return;
    }
    runOperation(
        tr("Input focused text"),
        uiBackend_.inputFocusedTextRequest(text, currentTargetId()));
}

void DeviceRuntimeController::pressDeviceKey(const QString& key)
{
    if (!ensureDeviceAvailable()) {
        return;
    }
    const QString trimmed = key.trimmed();
    if (trimmed.isEmpty()) {
        setErrorMessage(tr("Key is required."));
        return;
    }
    runOperation(
        tr("Press key"),
        uiBackend_.keyEventRequest(trimmed, currentTargetId()));
}

void DeviceRuntimeController::swipeDevice(int fromX, int fromY, int toX, int toY, int velocity)
{
    if (!ensureDeviceAvailable()) {
        return;
    }
    runOperation(
        tr("Swipe UI"),
        uiBackend_.swipeRequest(fromX, fromY, toX, toY, currentTargetId(), velocity));
}

void DeviceRuntimeController::startScreenRefresh(int intervalMs)
{
    if (!ensureDeviceAvailable()) {
        return;
    }
    if (screenRefreshTimer_.isActive()) {
        setScreenRefreshStatus(tr("Screen auto refresh is already running."));
        return;
    }

    const int boundedIntervalMs = std::clamp(
        intervalMs,
        kMinScreenRefreshIntervalMs,
        kMaxScreenRefreshIntervalMs);
    screenRefreshFrameCount_ = 0;
    screenRefreshFps_ = 0.0;
    screenRefreshElapsed_.restart();
    screenRefreshTimer_.start(boundedIntervalMs);
    setScreenRefreshStatus(tr("Screen auto refresh started."));
    emit screenRefreshStateChanged();
    emit screenRefreshFrameChanged();
    captureAutoRefreshFrame();
}

void DeviceRuntimeController::stopScreenRefresh()
{
    if (!screenRefreshTimer_.isActive()) {
        setScreenRefreshStatus(tr("Screen refresh stopped."));
        return;
    }

    screenRefreshTimer_.stop();
    setScreenRefreshStatus(tr("Screen refresh stopped."));
    emit screenRefreshStateChanged();
}

void DeviceRuntimeController::captureAutoRefreshFrame()
{
    if (!screenRefreshTimer_.isActive()) {
        return;
    }
    captureScreenshot(ScreenshotRequestKind::AutoRefresh);
}

void DeviceRuntimeController::cancel()
{
    installStopSource_.request_stop();
    if (hasPendingSigningInstall_) {
        hasPendingSigningInstall_ = false;
        pendingSigningPackagePath_.clear();
        pendingSigningTargetId_.clear();
        setStatus(tr("Install cancelled."));
    }
    if (activeCommandId_ != 0) {
        runner_.cancel(activeCommandId_);
    }
    if (busy_ && (activeOperation_ == tr("Install package")
            || activeOperation_ == tr("Re-sign and install package"))) {
        ++asyncInstallRunId_;
        setErrorMessage({});
        setStatus(tr("Install cancelled."));
        setBusy(false);
    }
}

void DeviceRuntimeController::clearOutput()
{
    if (!commandLog_.isEmpty()) {
        commandLog_.clear();
        emit commandLogChanged();
    }
    if (!hilogText_.isEmpty()) {
        hilogText_.clear();
        emit hilogTextChanged();
    }
    if (!uiNodes_.isEmpty() || !uiNodeSummary_.isEmpty() || !parsedUiNodes_.isEmpty()) {
        uiNodes_.clear();
        filteredUiNodes_.clear();
        parsedUiNodes_.clear();
        uiNodeSummary_.clear();
        emit uiNodesChanged();
    }
    setErrorMessage({});
}

void DeviceRuntimeController::runOperation(
    const QString& operation,
    const CommandRequest& request,
    std::function<void(const CommandResult&)> onFinished)
{
    if (busy_) {
        setErrorMessage(tr("Another device operation is still running."));
        return;
    }

    setBusy(true, operation);
    setErrorMessage({});
    activeCommandId_ = runner_.run(request, [this, operation, onFinished = std::move(onFinished)](const CommandResult& result) {
        activeCommandId_ = 0;
        appendCommandLog(result);
        if (result.succeeded()) {
            setStatus(tr("%1 completed.").arg(operation));
        } else {
            setErrorMessage(tr("%1 failed.").arg(operation));
        }
        if (onFinished) {
            onFinished(result);
        }
        setBusy(false);
    });
}

void DeviceRuntimeController::appendCommandLog(const CommandResult& result)
{
    if (!commandLog_.isEmpty()) {
        commandLog_ += QStringLiteral("\n\n");
    }
    commandLog_ += HdcDeviceBackend::resultSummary(result);
    emit commandLogChanged();
}

void DeviceRuntimeController::setBusy(bool busy, const QString& operation)
{
    if (busy_ == busy && activeOperation_ == operation) {
        return;
    }
    busy_ = busy;
    activeOperation_ = busy ? operation : QString();
    emit busyChanged();
}

void DeviceRuntimeController::setStatus(const QString& status)
{
    if (status_ == status) {
        return;
    }
    status_ = status;
    emit statusChanged();
}

void DeviceRuntimeController::setErrorMessage(const QString& errorMessage)
{
    if (errorMessage_ == errorMessage) {
        return;
    }
    errorMessage_ = errorMessage;
    emit errorMessageChanged();
    if (!errorMessage_.isEmpty()) {
        setStatus(errorMessage_);
    }
}

void DeviceRuntimeController::setScreenRefreshStatus(const QString& status)
{
    if (screenRefreshStatus_ == status) {
        return;
    }
    screenRefreshStatus_ = status;
    emit screenRefreshStatusChanged();
}

void DeviceRuntimeController::setDevices(const QList<HdcDeviceTarget>& targets)
{
    const QString previousSelectedDeviceId = selectedDeviceId_;
    devices_.clear();
    for (const HdcDeviceTarget& target : targets) {
        QVariantMap item = target.toVariantMap();
        const QString stateDisplay = translatedDeviceState(target.state);
        item.insert(QStringLiteral("stateDisplay"), stateDisplay);
        item.insert(
            QStringLiteral("display"),
            stateDisplay.isEmpty() ? target.id : QStringLiteral("%1  %2").arg(target.id, stateDisplay));
        devices_.append(item);
    }
    if (!targets.isEmpty()
        && (selectedDeviceId_.isEmpty()
            || std::none_of(targets.cbegin(), targets.cend(), [this](const HdcDeviceTarget& target) {
                return target.id == selectedDeviceId_;
            }))) {
        selectedDeviceId_ = targets.first().id;
    } else if (targets.isEmpty() && !selectedDeviceId_.isEmpty()) {
        selectedDeviceId_.clear();
    }
    if (selectedDeviceId_ != previousSelectedDeviceId) {
        clearDeviceSessionState();
        emit selectedDeviceChanged();
        if (!launchMetadataPackagePath_.isEmpty()) {
            refreshLaunchMetadata(launchMetadataPackagePath_);
        }
    }
    emit devicesChanged();
}

void DeviceRuntimeController::clearDeviceSessionState()
{
    if (screenRefreshTimer_.isActive()) {
        screenRefreshTimer_.stop();
        emit screenRefreshStateChanged();
    }
    initialUiSnapshotTimer_.stop();
    initialUiSnapshotDeviceId_.clear();
    screenRefreshFrameCount_ = 0;
    screenRefreshFps_ = 0.0;
    emit screenRefreshFrameChanged();
    setScreenRefreshStatus(tr("Screen refresh stopped."));

    if (!screenshotPath_.isEmpty()) {
        cleanupTemporaryLocalFiles();
        screenshotPath_.clear();
        emit screenshotPathChanged();
    }
    if (!hilogText_.isEmpty()) {
        hilogText_.clear();
        emit hilogTextChanged();
    }
    if (!uiNodes_.isEmpty() || !filteredUiNodes_.isEmpty() || !uiNodeSummary_.isEmpty() || !parsedUiNodes_.isEmpty()) {
        uiNodes_.clear();
        filteredUiNodes_.clear();
        parsedUiNodes_.clear();
        uiNodeSummary_.clear();
        emit uiNodesChanged();
    }
}

bool DeviceRuntimeController::ensureDeviceAvailable()
{
    if (!currentTargetId().isEmpty()) {
        return true;
    }
    setErrorMessage(tr("No device is connected."));
    return false;
}

QString DeviceRuntimeController::translatedInstallStatus(DeviceInstallStatus status) const
{
    switch (status) {
    case DeviceInstallStatus::Installed:
        return tr("Package installed.");
    case DeviceInstallStatus::SignedInstalled:
        return tr("Package signed and installed.");
    case DeviceInstallStatus::SignedRewrittenInstalled:
        return tr("Package signed, rewritten, and installed.");
    case DeviceInstallStatus::RequiresSigning:
        return tr("Install requires package signing.");
    case DeviceInstallStatus::None:
        break;
    }
    return {};
}

QString DeviceRuntimeController::translatedInstallError(DeviceInstallError error) const
{
    switch (error) {
    case DeviceInstallError::InstallFailed:
        return tr("Install package failed.");
    case DeviceInstallError::UnsignedWithoutSigning:
        return tr("Install failed because the package is unsigned. Configure Harmony signing in Settings.");
    case DeviceInstallError::TemporaryDirectoryFailed:
        return tr("Could not create temporary signed HAP directory.");
    case DeviceInstallError::BundleRewriteFailed:
        return tr("Install failed because bundle identity rewrite failed.");
    case DeviceInstallError::SigningFailed:
        return tr("Install failed because signing failed.");
    case DeviceInstallError::SignedInstallFailed:
        return tr("Signed package install failed.");
    case DeviceInstallError::Cancelled:
        return tr("Install cancelled.");
    case DeviceInstallError::None:
        break;
    }
    return {};
}

QString DeviceRuntimeController::translatedDeviceState(const QString& state) const
{
    const QString trimmed = state.trimmed();
    if (trimmed.compare(QStringLiteral("connected"), Qt::CaseInsensitive) == 0) {
        return tr("connected");
    }
    if (trimmed.compare(QStringLiteral("offline"), Qt::CaseInsensitive) == 0) {
        return tr("offline");
    }
    return trimmed;
}

void DeviceRuntimeController::setUiNodes(const QList<UiAutomationNode>& nodes)
{
    parsedUiNodes_ = nodes;
    uiNodes_ = UiAutomationBackend::nodesToVariantList(parsedUiNodes_);
    uiNodeSummary_ = UiAutomationBackend::nodesSummary(parsedUiNodes_);
    refreshFilteredUiNodes();
    emit uiNodesChanged();
}

void DeviceRuntimeController::refreshFilteredUiNodes()
{
    const QString filter = uiNodeFilter_.trimmed();
    if (filter.isEmpty()) {
        filteredUiNodes_ = uiNodes_;
        return;
    }

    const QList<UiAutomationNode> matches = UiAutomationBackend::findNodes(parsedUiNodes_, filter, 500);
    filteredUiNodes_ = UiAutomationBackend::nodesToVariantList(matches);
}

QString DeviceRuntimeController::currentTargetId() const
{
    return selectedDeviceId_.trimmed();
}

QString DeviceRuntimeController::makeLocalScreenshotPath() const
{
    QDir dir(QDir::tempPath());
    dir.mkpath(QStringLiteral("ReArk"));
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    return dir.filePath(QStringLiteral("ReArk/reark-device-%1.jpeg").arg(timestamp));
}

QString DeviceRuntimeController::makeLocalLayoutPath() const
{
    QDir dir(QDir::tempPath());
    dir.mkpath(QStringLiteral("ReArk"));
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    return dir.filePath(QStringLiteral("ReArk/reark-ui-layout-%1.json").arg(timestamp));
}

void DeviceRuntimeController::rememberTemporaryLocalFile(const QString& path)
{
    if (path.isEmpty() || temporaryLocalFiles_.contains(path)) {
        return;
    }
    temporaryLocalFiles_.append(path);
}

void DeviceRuntimeController::cleanupTemporaryLocalFiles(const QString& keepPath)
{
    const QString keep = keepPath.isEmpty() ? QString() : QFileInfo(keepPath).absoluteFilePath();
    QStringList kept;
    for (const QString& path : std::as_const(temporaryLocalFiles_)) {
        const QString absolutePath = QFileInfo(path).absoluteFilePath();
        if (!keep.isEmpty() && absolutePath == keep) {
            kept.append(path);
            continue;
        }
        QFile::remove(path);
    }
    temporaryLocalFiles_ = kept;
}

void DeviceRuntimeController::cleanupStaleTemporaryLocalFiles() const
{
    QDir dir(QDir::tempPath());
    if (!dir.cd(QStringLiteral("ReArk"))) {
        return;
    }

    const QDateTime cutoff = QDateTime::currentDateTime().addSecs(-kStaleTemporaryFileMaxAgeHours * 60 * 60);
    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& file : files) {
        if (isDeviceRuntimeTemporaryFile(file.fileName()) && file.lastModified() < cutoff) {
            QFile::remove(file.absoluteFilePath());
        }
    }
}

void DeviceRuntimeController::setLaunchMetadata(
    const QString& bundleName,
    const QString& moduleName,
    const QString& abilityName)
{
    const QString bundle = bundleName.trimmed();
    const QString module = moduleName.trimmed();
    const QString ability = abilityName.trimmed();
    if (launchBundleName_ == bundle
        && launchModuleName_ == module
        && launchAbilityName_ == ability) {
        return;
    }

    launchBundleName_ = bundle;
    launchModuleName_ = module;
    launchAbilityName_ = ability;
    emit launchMetadataChanged();
}
