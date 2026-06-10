#include "controller/AgentSettings.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QSettings>
#include <QUrl>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <wincrypt.h>
#endif

namespace {

constexpr auto kAgentBaseUrlKey = "Agent/BaseUrl";
constexpr auto kAgentApiKeyKey = "Agent/ApiKey";
constexpr auto kAgentProtectedApiKeyKey = "Agent/ApiKeyProtected";
constexpr auto kAgentModelKey = "Agent/Model";
constexpr auto kAgentRequireApiKeyKey = "Agent/RequireApiKey";
constexpr auto kDefaultBaseUrl = "https://openrouter.ai/api";
constexpr auto kDefaultModel = "openai/gpt-4o-mini";

QString envString(const char* name)
{
    return QString::fromUtf8(qgetenv(name));
}

bool envBool(const char* name, bool fallback)
{
    const QByteArray raw = qgetenv(name).trimmed().toLower();
    if (raw.isEmpty()) {
        return fallback;
    }
    return raw == "1" || raw == "true" || raw == "yes" || raw == "on";
}

bool looksLocalEndpoint(const QString& baseUrl)
{
    return baseUrl.startsWith(QStringLiteral("http://127.0.0.1"))
        || baseUrl.startsWith(QStringLiteral("http://localhost"))
        || baseUrl.startsWith(QStringLiteral("https://localhost"));
}

#ifdef Q_OS_WIN
QByteArray protectSecret(const QString& secret)
{
    const QByteArray plain = secret.toUtf8();
    DATA_BLOB input {
        .cbData = static_cast<DWORD>(plain.size()),
        .pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData()))
    };
    DATA_BLOB output {};

    if (!CryptProtectData(
            &input,
            L"ReArk Agent API Key",
            nullptr,
            nullptr,
            nullptr,
            0,
            &output)) {
        return {};
    }

    QByteArray protectedBytes(
        reinterpret_cast<const char*>(output.pbData),
        static_cast<qsizetype>(output.cbData));
    LocalFree(output.pbData);
    return protectedBytes.toBase64();
}

QString unprotectSecret(const QString& protectedSecret)
{
    const QByteArray protectedBytes = QByteArray::fromBase64(protectedSecret.toUtf8());
    if (protectedBytes.isEmpty()) {
        return {};
    }

    DATA_BLOB input {
        .cbData = static_cast<DWORD>(protectedBytes.size()),
        .pbData = reinterpret_cast<BYTE*>(const_cast<char*>(protectedBytes.constData()))
    };
    DATA_BLOB output {};

    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
        return {};
    }

    const QString secret = QString::fromUtf8(
        reinterpret_cast<const char*>(output.pbData),
        static_cast<qsizetype>(output.cbData));
    LocalFree(output.pbData);
    return secret;
}
#else
QByteArray protectSecret(const QString& secret)
{
    return secret.toUtf8().toBase64();
}

QString unprotectSecret(const QString& protectedSecret)
{
    return QString::fromUtf8(QByteArray::fromBase64(protectedSecret.toUtf8()));
}
#endif

QString loadApiKey(QSettings& settings)
{
    const QString protectedKey = settings.value(QString::fromLatin1(kAgentProtectedApiKeyKey)).toString();
    if (!protectedKey.isEmpty()) {
        return unprotectSecret(protectedKey);
    }

    const QString legacyPlaintextKey = settings.value(QString::fromLatin1(kAgentApiKeyKey)).toString();
    if (!legacyPlaintextKey.isEmpty()) {
        settings.remove(QString::fromLatin1(kAgentApiKeyKey));
        const QByteArray protectedLegacyKey = protectSecret(legacyPlaintextKey);
        if (!protectedLegacyKey.isEmpty()) {
            settings.setValue(QString::fromLatin1(kAgentProtectedApiKeyKey), QString::fromLatin1(protectedLegacyKey));
        }
        return legacyPlaintextKey;
    }

    return AgentSettingsStore::defaultApiKey();
}

} // namespace

AgentSettings AgentSettingsStore::load()
{
    QSettings settings;
    AgentSettings result;
    result.baseUrl = settings.value(QString::fromLatin1(kAgentBaseUrlKey), defaultBaseUrl()).toString().trimmed();
    result.apiKey = loadApiKey(settings);
    result.model = settings.value(QString::fromLatin1(kAgentModelKey), defaultModel()).toString().trimmed();
    result.requireApiKey = settings.value(
        QString::fromLatin1(kAgentRequireApiKeyKey),
        envBool("REARK_LLM_REQUIRE_API_KEY", defaultRequireApiKey(result.baseUrl))).toBool();
    return result;
}

bool AgentSettingsStore::save(const AgentSettings& settings)
{
    QByteArray protectedKey;
    if (!settings.apiKey.isEmpty()) {
        protectedKey = protectSecret(settings.apiKey);
        if (protectedKey.isEmpty()) {
            return false;
        }
    }

    QSettings qsettings;
    qsettings.setValue(QString::fromLatin1(kAgentBaseUrlKey), settings.baseUrl.trimmed());
    qsettings.setValue(QString::fromLatin1(kAgentModelKey), settings.model.trimmed());
    qsettings.setValue(QString::fromLatin1(kAgentRequireApiKeyKey), settings.requireApiKey);
    qsettings.remove(QString::fromLatin1(kAgentApiKeyKey));
    qsettings.remove(QString::fromLatin1(kAgentProtectedApiKeyKey));

    if (settings.apiKey.isEmpty()) {
        return true;
    }

    qsettings.setValue(QString::fromLatin1(kAgentProtectedApiKeyKey), QString::fromLatin1(protectedKey));
    return true;
}

void AgentSettingsStore::reset()
{
    QSettings settings;
    settings.remove(QString::fromLatin1(kAgentBaseUrlKey));
    settings.remove(QString::fromLatin1(kAgentApiKeyKey));
    settings.remove(QString::fromLatin1(kAgentProtectedApiKeyKey));
    settings.remove(QString::fromLatin1(kAgentModelKey));
    settings.remove(QString::fromLatin1(kAgentRequireApiKeyKey));
}

QString AgentSettingsStore::validationMessage(const AgentSettings& settings)
{
    const QString baseUrl = settings.baseUrl.trimmed();
    if (baseUrl.isEmpty()) {
        return QCoreApplication::translate("AgentSettings", "Base URL is required.");
    }

    const QUrl url(baseUrl);
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()
        || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))) {
        return QCoreApplication::translate("AgentSettings", "Base URL must be a valid HTTP or HTTPS endpoint.");
    }

    if (settings.model.trimmed().isEmpty()) {
        return QCoreApplication::translate("AgentSettings", "Model is required.");
    }

    if (settings.requireApiKey && settings.apiKey.isEmpty()) {
        return QCoreApplication::translate("AgentSettings", "API key is required for this endpoint.");
    }

    return {};
}

QString AgentSettingsStore::defaultBaseUrl()
{
    const QString configured = envString("REARK_LLM_BASE_URL");
    return configured.isEmpty() ? QString::fromLatin1(kDefaultBaseUrl) : configured;
}

QString AgentSettingsStore::defaultApiKey()
{
    const QString configured = envString("REARK_LLM_API_KEY");
    return configured.isEmpty() ? envString("OPENROUTER_API_KEY") : configured;
}

QString AgentSettingsStore::defaultModel()
{
    const QString configured = envString("REARK_LLM_MODEL");
    return configured.isEmpty() ? QString::fromLatin1(kDefaultModel) : configured;
}

bool AgentSettingsStore::defaultRequireApiKey(const QString& baseUrl)
{
    return !looksLocalEndpoint(baseUrl.isEmpty() ? defaultBaseUrl() : baseUrl.trimmed());
}
