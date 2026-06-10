#include "controller/SettingsController.h"

#include "controller/AgentSettings.h"

SettingsController::SettingsController(QObject* parent)
    : QObject(parent)
{
    loadAgentSettings();
}

QString SettingsController::agentBaseUrl() const
{
    return agentBaseUrl_;
}

void SettingsController::setAgentBaseUrl(const QString& agentBaseUrl)
{
    const QString trimmed = agentBaseUrl.trimmed();
    if (agentBaseUrl_ == trimmed) {
        return;
    }

    agentBaseUrl_ = trimmed;
    emit agentSettingsChanged();
}

QString SettingsController::agentApiKey() const
{
    return agentApiKey_;
}

void SettingsController::setAgentApiKey(const QString& agentApiKey)
{
    if (agentApiKey_ == agentApiKey) {
        return;
    }

    agentApiKey_ = agentApiKey;
    emit agentSettingsChanged();
}

QString SettingsController::agentModel() const
{
    return agentModel_;
}

void SettingsController::setAgentModel(const QString& agentModel)
{
    const QString trimmed = agentModel.trimmed();
    if (agentModel_ == trimmed) {
        return;
    }

    agentModel_ = trimmed;
    emit agentSettingsChanged();
}

bool SettingsController::agentRequireApiKey() const
{
    return agentRequireApiKey_;
}

void SettingsController::setAgentRequireApiKey(bool agentRequireApiKey)
{
    if (agentRequireApiKey_ == agentRequireApiKey) {
        return;
    }

    agentRequireApiKey_ = agentRequireApiKey;
    emit agentSettingsChanged();
}

QString SettingsController::agentValidationMessage() const
{
    return agentValidationMessage_;
}

void SettingsController::reload()
{
    const QString previousBaseUrl = agentBaseUrl_;
    const QString previousApiKey = agentApiKey_;
    const QString previousModel = agentModel_;
    const bool previousRequireApiKey = agentRequireApiKey_;

    loadAgentSettings();

    if (agentBaseUrl_ != previousBaseUrl
        || agentApiKey_ != previousApiKey
        || agentModel_ != previousModel
        || agentRequireApiKey_ != previousRequireApiKey) {
        emit agentSettingsChanged();
    }
}

bool SettingsController::saveAgentSettings(
    const QString& baseUrl,
    const QString& apiKey,
    const QString& model,
    bool requireApiKey)
{
    AgentSettings settings {
        .baseUrl = baseUrl.trimmed(),
        .apiKey = apiKey,
        .model = model.trimmed(),
        .requireApiKey = requireApiKey
    };

    const QString validationMessage = AgentSettingsStore::validationMessage(settings);
    if (!validationMessage.isEmpty()) {
        setAgentValidationMessage(validationMessage);
        return false;
    }

    if (!AgentSettingsStore::save(settings)) {
        setAgentValidationMessage(tr("Failed to protect and save the API key."));
        return false;
    }

    setAgentSettings(settings.baseUrl, settings.apiKey, settings.model, settings.requireApiKey);
    setAgentValidationMessage({});
    return true;
}

void SettingsController::resetAgentSettings()
{
    AgentSettingsStore::reset();
    reload();
}

void SettingsController::loadAgentSettings()
{
    const AgentSettings settings = AgentSettingsStore::load();
    agentBaseUrl_ = settings.baseUrl;
    agentApiKey_ = settings.apiKey;
    agentModel_ = settings.model;
    agentRequireApiKey_ = settings.requireApiKey;
    setAgentValidationMessage({});
}

void SettingsController::setAgentSettings(
    const QString& baseUrl,
    const QString& apiKey,
    const QString& model,
    bool requireApiKey)
{
    const bool changed = agentBaseUrl_ != baseUrl
        || agentApiKey_ != apiKey
        || agentModel_ != model
        || agentRequireApiKey_ != requireApiKey;

    agentBaseUrl_ = baseUrl;
    agentApiKey_ = apiKey;
    agentModel_ = model;
    agentRequireApiKey_ = requireApiKey;

    if (changed) {
        emit agentSettingsChanged();
    }
}

void SettingsController::setAgentValidationMessage(const QString& message)
{
    if (agentValidationMessage_ == message) {
        return;
    }

    agentValidationMessage_ = message;
    emit agentValidationChanged();
}
