#ifndef REARK_SIGNING_CONTROLLER_H
#define REARK_SIGNING_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "controller/SigningSettings.h"

class SigningController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString harmonySigningKeystorePath READ harmonySigningKeystorePath WRITE setHarmonySigningKeystorePath NOTIFY signingSettingsChanged)
    Q_PROPERTY(QString harmonySigningKeystorePassword READ harmonySigningKeystorePassword WRITE setHarmonySigningKeystorePassword NOTIFY signingSettingsChanged)
    Q_PROPERTY(QString harmonySigningKeyAlias READ harmonySigningKeyAlias WRITE setHarmonySigningKeyAlias NOTIFY signingSettingsChanged)
    Q_PROPERTY(QString harmonySigningKeyPassword READ harmonySigningKeyPassword WRITE setHarmonySigningKeyPassword NOTIFY signingSettingsChanged)
    Q_PROPERTY(QString harmonySigningProfilePath READ harmonySigningProfilePath WRITE setHarmonySigningProfilePath NOTIFY signingSettingsChanged)
    Q_PROPERTY(QString harmonySigningCertificatePath READ harmonySigningCertificatePath WRITE setHarmonySigningCertificatePath NOTIFY signingSettingsChanged)
    Q_PROPERTY(QString harmonySigningValidationMessage READ harmonySigningValidationMessage NOTIFY signingValidationChanged)

public:
    explicit SigningController(QObject* parent = nullptr);

    [[nodiscard]] QString harmonySigningKeystorePath() const;
    void setHarmonySigningKeystorePath(const QString& path);
    [[nodiscard]] QString harmonySigningKeystorePassword() const;
    void setHarmonySigningKeystorePassword(const QString& password);
    [[nodiscard]] QString harmonySigningKeyAlias() const;
    void setHarmonySigningKeyAlias(const QString& alias);
    [[nodiscard]] QString harmonySigningKeyPassword() const;
    void setHarmonySigningKeyPassword(const QString& password);
    [[nodiscard]] QString harmonySigningProfilePath() const;
    void setHarmonySigningProfilePath(const QString& path);
    [[nodiscard]] QString harmonySigningCertificatePath() const;
    void setHarmonySigningCertificatePath(const QString& path);
    [[nodiscard]] QString harmonySigningValidationMessage() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE bool saveHarmonySigningSettings(
        const QString& keystorePath,
        const QString& keystorePassword,
        const QString& keyAlias,
        const QString& keyPassword,
        const QString& profilePath,
        const QString& certificatePath);
    Q_INVOKABLE void resetHarmonySigningSettings();
    Q_INVOKABLE QVariantMap inspectHarmonySigningSettings(
        const QString& keystorePath,
        const QString& keystorePassword,
        const QString& keyAlias,
        const QString& profilePath,
        const QString& certificatePath) const;

signals:
    void signingSettingsChanged();
    void signingValidationChanged();

private:
    void loadHarmonySigningSettings();
    void setHarmonySigningSettings(const HarmonySigningSettings& settings);
    void setHarmonySigningValidationMessage(const QString& message);

    QString harmonySigningValidationMessage_;
    HarmonySigningSettings harmonySigningSettings_;
};

#endif // REARK_SIGNING_CONTROLLER_H
