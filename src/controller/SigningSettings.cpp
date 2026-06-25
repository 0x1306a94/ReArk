#include "controller/SigningSettings.h"

#include "controller/ProtectedSettingsSecret.h"

#include <QFileInfo>
#include <QObject>
#include <QSettings>

namespace {

constexpr auto kLegacyHarmonyNameKey = "Signing/Harmony/Name";
constexpr auto kHarmonyKeystorePathKey = "Signing/Harmony/KeystorePath";
constexpr auto kHarmonyKeystorePasswordKey = "Signing/Harmony/KeystorePassword";
constexpr auto kHarmonyProtectedKeystorePasswordKey = "Signing/Harmony/KeystorePasswordProtected";
constexpr auto kHarmonyKeyAliasKey = "Signing/Harmony/KeyAlias";
constexpr auto kHarmonyKeyPasswordKey = "Signing/Harmony/KeyPassword";
constexpr auto kHarmonyProtectedKeyPasswordKey = "Signing/Harmony/KeyPasswordProtected";
constexpr auto kHarmonyProfilePathKey = "Signing/Harmony/ProfilePath";
constexpr auto kHarmonyCertificatePathKey = "Signing/Harmony/CertificatePath";

bool hasSuffix(const QString& path, const QString& suffix)
{
    return path.endsWith(suffix, Qt::CaseInsensitive);
}

} // namespace

HarmonySigningSettings SigningSettingsStore::loadHarmony()
{
    QSettings settings;
    HarmonySigningSettings result;
    result.keystorePath = settings.value(QString::fromLatin1(kHarmonyKeystorePathKey)).toString().trimmed();
    result.keystorePassword = ProtectedSettingsSecret::load(
        settings,
        kHarmonyProtectedKeystorePasswordKey,
        kHarmonyKeystorePasswordKey);
    result.keyAlias = settings.value(QString::fromLatin1(kHarmonyKeyAliasKey)).toString().trimmed();
    result.keyPassword = ProtectedSettingsSecret::load(
        settings,
        kHarmonyProtectedKeyPasswordKey,
        kHarmonyKeyPasswordKey);
    result.profilePath = settings.value(QString::fromLatin1(kHarmonyProfilePathKey)).toString().trimmed();
    result.certificatePath = settings.value(QString::fromLatin1(kHarmonyCertificatePathKey)).toString().trimmed();
    return result;
}

bool SigningSettingsStore::saveHarmony(const HarmonySigningSettings& signingSettings)
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(kHarmonyKeystorePathKey), signingSettings.keystorePath.trimmed());
    settings.setValue(QString::fromLatin1(kHarmonyKeyAliasKey), signingSettings.keyAlias.trimmed());
    settings.setValue(QString::fromLatin1(kHarmonyProfilePathKey), signingSettings.profilePath.trimmed());
    settings.setValue(QString::fromLatin1(kHarmonyCertificatePathKey), signingSettings.certificatePath.trimmed());
    return ProtectedSettingsSecret::save(
               settings,
               kHarmonyProtectedKeystorePasswordKey,
               kHarmonyKeystorePasswordKey,
               signingSettings.keystorePassword)
        && ProtectedSettingsSecret::save(
               settings,
               kHarmonyProtectedKeyPasswordKey,
               kHarmonyKeyPasswordKey,
               signingSettings.keyPassword);
}

void SigningSettingsStore::resetHarmony()
{
    QSettings settings;
    settings.remove(QString::fromLatin1(kLegacyHarmonyNameKey));
    settings.remove(QString::fromLatin1(kHarmonyKeystorePathKey));
    settings.remove(QString::fromLatin1(kHarmonyKeystorePasswordKey));
    settings.remove(QString::fromLatin1(kHarmonyProtectedKeystorePasswordKey));
    settings.remove(QString::fromLatin1(kHarmonyKeyAliasKey));
    settings.remove(QString::fromLatin1(kHarmonyKeyPasswordKey));
    settings.remove(QString::fromLatin1(kHarmonyProtectedKeyPasswordKey));
    settings.remove(QString::fromLatin1(kHarmonyProfilePathKey));
    settings.remove(QString::fromLatin1(kHarmonyCertificatePathKey));
}

QString SigningSettingsStore::harmonyValidationMessage(const HarmonySigningSettings& settings)
{
    if (settings.keystorePath.trimmed().isEmpty()) {
        return QObject::tr("Harmony keystore path is required.");
    }
    if (!QFileInfo::exists(settings.keystorePath.trimmed())) {
        return QObject::tr("Harmony keystore file does not exist.");
    }
    if (!hasSuffix(settings.keystorePath.trimmed(), QStringLiteral(".p12"))) {
        return QObject::tr("Harmony keystore must be a .p12 file.");
    }
    if (settings.keystorePassword.isEmpty()) {
        return QObject::tr("Harmony keystore password is required.");
    }
    if (settings.keyAlias.trimmed().isEmpty()) {
        return QObject::tr("Harmony key alias is required.");
    }
    if (settings.profilePath.trimmed().isEmpty()) {
        return QObject::tr("Harmony profile path is required.");
    }
    if (!QFileInfo::exists(settings.profilePath.trimmed())) {
        return QObject::tr("Harmony profile file does not exist.");
    }
    if (!hasSuffix(settings.profilePath.trimmed(), QStringLiteral(".p7b"))) {
        return QObject::tr("Harmony profile must be a .p7b file.");
    }
    if (settings.certificatePath.trimmed().isEmpty()) {
        return QObject::tr("Harmony certificate path is required.");
    }
    if (!QFileInfo::exists(settings.certificatePath.trimmed())) {
        return QObject::tr("Harmony certificate file does not exist.");
    }
    if (!hasSuffix(settings.certificatePath.trimmed(), QStringLiteral(".cer"))) {
        return QObject::tr("Harmony certificate must be a .cer file.");
    }
    return {};
}
