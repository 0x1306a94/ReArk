#include "controller/AgentSettings.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << "FAIL: " << message << '\n';
    return 1;
}

bool expectEqual(const QString& actual, const QString& expected, const QString& label)
{
    if (actual == expected) {
        return true;
    }
    QTextStream(stderr) << "FAIL: " << label << "\nExpected: "
                        << expected << "\nActual: " << actual << '\n';
    return false;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    if (!expectEqual(
            AgentSettingsStore::normalizedBaseUrl(
                QStringLiteral("OpenRouter"),
                QStringLiteral("https://openrouter.ai/api")),
            QStringLiteral("https://openrouter.ai/api/v1"),
            QStringLiteral("OpenRouter legacy API URL should migrate to /api/v1"))) {
        return 1;
    }

    if (!expectEqual(
            AgentSettingsStore::normalizedBaseUrl(
                QStringLiteral("OpenRouter"),
                QStringLiteral("https://openrouter.ai/api/")),
            QStringLiteral("https://openrouter.ai/api/v1"),
            QStringLiteral("OpenRouter legacy API URL with trailing slash should migrate to /api/v1"))) {
        return 1;
    }

    if (!expectEqual(
            AgentSettingsStore::normalizedBaseUrl(
                QStringLiteral("OpenRouter"),
                QStringLiteral("https://openrouter.ai/api/v1")),
            QStringLiteral("https://openrouter.ai/api/v1"),
            QStringLiteral("OpenRouter canonical API URL should remain unchanged"))) {
        return 1;
    }

    if (!expectEqual(
            AgentSettingsStore::normalizedBaseUrl(
                QStringLiteral("OpenAICompatible"),
                QStringLiteral("https://openrouter.ai/api")),
            QStringLiteral("https://openrouter.ai/api"),
            QStringLiteral("Custom compatible endpoints should not be rewritten"))) {
        return 1;
    }

    const QVariantMap openRouterDefaults = AgentSettingsStore::providerDefaults(QStringLiteral("OpenRouter"));
    if (openRouterDefaults.value(QStringLiteral("baseUrl")).toString() != QStringLiteral("https://openrouter.ai/api/v1")) {
        return fail(QStringLiteral("OpenRouter provider defaults should expose the canonical /api/v1 URL."));
    }

    QTextStream(stdout) << "Agent settings tests passed\n";
    return 0;
}
