#ifndef REARK_SETTINGS_CONTROLLER_H
#define REARK_SETTINGS_CONTROLLER_H

#include <QObject>
#include <QString>

class SettingsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString agentBaseUrl READ agentBaseUrl WRITE setAgentBaseUrl NOTIFY agentSettingsChanged)
    Q_PROPERTY(QString agentApiKey READ agentApiKey WRITE setAgentApiKey NOTIFY agentSettingsChanged)
    Q_PROPERTY(QString agentModel READ agentModel WRITE setAgentModel NOTIFY agentSettingsChanged)
    Q_PROPERTY(bool agentRequireApiKey READ agentRequireApiKey WRITE setAgentRequireApiKey NOTIFY agentSettingsChanged)
    Q_PROPERTY(QString agentValidationMessage READ agentValidationMessage NOTIFY agentValidationChanged)

public:
    explicit SettingsController(QObject* parent = nullptr);

    [[nodiscard]] QString agentBaseUrl() const;
    void setAgentBaseUrl(const QString& agentBaseUrl);

    [[nodiscard]] QString agentApiKey() const;
    void setAgentApiKey(const QString& agentApiKey);

    [[nodiscard]] QString agentModel() const;
    void setAgentModel(const QString& agentModel);

    [[nodiscard]] bool agentRequireApiKey() const;
    void setAgentRequireApiKey(bool agentRequireApiKey);

    [[nodiscard]] QString agentValidationMessage() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE bool saveAgentSettings(
        const QString& baseUrl,
        const QString& apiKey,
        const QString& model,
        bool requireApiKey);
    Q_INVOKABLE void resetAgentSettings();

signals:
    void agentSettingsChanged();
    void agentValidationChanged();

private:
    void loadAgentSettings();
    void setAgentSettings(const QString& baseUrl, const QString& apiKey, const QString& model, bool requireApiKey);
    void setAgentValidationMessage(const QString& message);

    QString agentBaseUrl_;
    QString agentApiKey_;
    QString agentModel_;
    QString agentValidationMessage_;
    bool agentRequireApiKey_ = true;
};

#endif // REARK_SETTINGS_CONTROLLER_H
