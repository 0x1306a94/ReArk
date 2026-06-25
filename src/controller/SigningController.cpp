#include "controller/SigningController.h"

#include "signing/SigningMaterialInspector.h"

SigningController::SigningController(QObject* parent)
    : QObject(parent)
{
    loadHarmonySigningSettings();
}

QString SigningController::harmonySigningKeystorePath() const
{
    return harmonySigningSettings_.keystorePath;
}

void SigningController::setHarmonySigningKeystorePath(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (harmonySigningSettings_.keystorePath == trimmed) {
        return;
    }
    harmonySigningSettings_.keystorePath = trimmed;
    emit signingSettingsChanged();
}

QString SigningController::harmonySigningKeystorePassword() const
{
    return harmonySigningSettings_.keystorePassword;
}

void SigningController::setHarmonySigningKeystorePassword(const QString& password)
{
    if (harmonySigningSettings_.keystorePassword == password) {
        return;
    }
    harmonySigningSettings_.keystorePassword = password;
    emit signingSettingsChanged();
}

QString SigningController::harmonySigningKeyAlias() const
{
    return harmonySigningSettings_.keyAlias;
}

void SigningController::setHarmonySigningKeyAlias(const QString& alias)
{
    const QString trimmed = alias.trimmed();
    if (harmonySigningSettings_.keyAlias == trimmed) {
        return;
    }
    harmonySigningSettings_.keyAlias = trimmed;
    emit signingSettingsChanged();
}

QString SigningController::harmonySigningKeyPassword() const
{
    return harmonySigningSettings_.keyPassword;
}

void SigningController::setHarmonySigningKeyPassword(const QString& password)
{
    if (harmonySigningSettings_.keyPassword == password) {
        return;
    }
    harmonySigningSettings_.keyPassword = password;
    emit signingSettingsChanged();
}

QString SigningController::harmonySigningProfilePath() const
{
    return harmonySigningSettings_.profilePath;
}

void SigningController::setHarmonySigningProfilePath(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (harmonySigningSettings_.profilePath == trimmed) {
        return;
    }
    harmonySigningSettings_.profilePath = trimmed;
    emit signingSettingsChanged();
}

QString SigningController::harmonySigningCertificatePath() const
{
    return harmonySigningSettings_.certificatePath;
}

void SigningController::setHarmonySigningCertificatePath(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (harmonySigningSettings_.certificatePath == trimmed) {
        return;
    }
    harmonySigningSettings_.certificatePath = trimmed;
    emit signingSettingsChanged();
}

QString SigningController::harmonySigningValidationMessage() const
{
    return harmonySigningValidationMessage_;
}

void SigningController::reload()
{
    const HarmonySigningSettings previousHarmonySigningSettings = harmonySigningSettings_;
    loadHarmonySigningSettings();
    if (harmonySigningSettings_.keystorePath != previousHarmonySigningSettings.keystorePath
        || harmonySigningSettings_.keystorePassword != previousHarmonySigningSettings.keystorePassword
        || harmonySigningSettings_.keyAlias != previousHarmonySigningSettings.keyAlias
        || harmonySigningSettings_.keyPassword != previousHarmonySigningSettings.keyPassword
        || harmonySigningSettings_.profilePath != previousHarmonySigningSettings.profilePath
        || harmonySigningSettings_.certificatePath != previousHarmonySigningSettings.certificatePath) {
        emit signingSettingsChanged();
    }
}

bool SigningController::saveHarmonySigningSettings(
    const QString& keystorePath,
    const QString& keystorePassword,
    const QString& keyAlias,
    const QString& keyPassword,
    const QString& profilePath,
    const QString& certificatePath)
{
    HarmonySigningSettings settings {
        .keystorePath = keystorePath.trimmed(),
        .keystorePassword = keystorePassword,
        .keyAlias = keyAlias.trimmed(),
        .keyPassword = keyPassword,
        .profilePath = profilePath.trimmed(),
        .certificatePath = certificatePath.trimmed()
    };

    const QString validationMessage = SigningSettingsStore::harmonyValidationMessage(settings);
    if (!validationMessage.isEmpty()) {
        setHarmonySigningValidationMessage(validationMessage);
        return false;
    }
    if (!SigningSettingsStore::saveHarmony(settings)) {
        setHarmonySigningValidationMessage(tr("Failed to protect and save Harmony signing passwords."));
        return false;
    }

    setHarmonySigningSettings(settings);
    setHarmonySigningValidationMessage({});
    return true;
}

void SigningController::resetHarmonySigningSettings()
{
    SigningSettingsStore::resetHarmony();
    loadHarmonySigningSettings();
    emit signingSettingsChanged();
}

QVariantMap SigningController::inspectHarmonySigningSettings(
    const QString& keystorePath,
    const QString& keystorePassword,
    const QString& keyAlias,
    const QString& profilePath,
    const QString& certificatePath) const
{
    const SigningMaterialStatus status = SigningMaterialInspector::inspectHarmony({
        .keystorePath = keystorePath.trimmed(),
        .keystorePassword = keystorePassword,
        .keyAlias = keyAlias.trimmed(),
        .keyPassword = {},
        .profilePath = profilePath.trimmed(),
        .certificatePath = certificatePath.trimmed()
    });

    QVariantMap result;
    result.insert(QStringLiteral("tone"), status.tone);
    result.insert(QStringLiteral("summary"), status.summary);
    result.insert(
        QStringLiteral("profileNotAfter"),
        status.profileNotAfter.isValid()
            ? status.profileNotAfter.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QString());
    result.insert(
        QStringLiteral("certificateNotAfter"),
        status.certificateNotAfter.isValid()
            ? status.certificateNotAfter.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QString());
    result.insert(
        QStringLiteral("effectiveNotAfter"),
        status.effectiveNotAfter.isValid()
            ? status.effectiveNotAfter.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QString());
    result.insert(QStringLiteral("profileBundleName"), status.profileBundleName);
    return result;
}

void SigningController::loadHarmonySigningSettings()
{
    harmonySigningSettings_ = SigningSettingsStore::loadHarmony();
    setHarmonySigningValidationMessage({});
}

void SigningController::setHarmonySigningSettings(const HarmonySigningSettings& settings)
{
    const bool changed = harmonySigningSettings_.keystorePath != settings.keystorePath
        || harmonySigningSettings_.keystorePassword != settings.keystorePassword
        || harmonySigningSettings_.keyAlias != settings.keyAlias
        || harmonySigningSettings_.keyPassword != settings.keyPassword
        || harmonySigningSettings_.profilePath != settings.profilePath
        || harmonySigningSettings_.certificatePath != settings.certificatePath;

    harmonySigningSettings_ = settings;
    if (changed) {
        emit signingSettingsChanged();
    }
}

void SigningController::setHarmonySigningValidationMessage(const QString& message)
{
    if (harmonySigningValidationMessage_ == message) {
        return;
    }

    harmonySigningValidationMessage_ = message;
    emit signingValidationChanged();
}
