#include "controller/ReleaseNotesLocalizer.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

struct LocalizedBlock {
    QString locale;
    QString content;
};

QString normalizedLocale(QString locale)
{
    return locale.trimmed().replace(QLatin1Char('_'), QLatin1Char('-')).toLower();
}

QStringList localeCandidates(const QString& locale)
{
    QStringList candidates;
    const QString normalized = normalizedLocale(locale);
    if (normalized.isEmpty()) {
        return candidates;
    }

    candidates.append(normalized);
    const int separator = normalized.indexOf(QLatin1Char('-'));
    if (separator > 0) {
        candidates.append(normalized.left(separator));
    }
    return candidates;
}

void appendUnique(QStringList* values, const QStringList& candidates)
{
    for (const QString& candidate : candidates) {
        if (!candidate.isEmpty() && !values->contains(candidate)) {
            values->append(candidate);
        }
    }
}

QList<LocalizedBlock> parseBlocks(const QString& body)
{
    static const QRegularExpression blockPattern(
        QStringLiteral(R"(<!--\s*reark:lang\s*=\s*([A-Za-z0-9_-]+)\s*-->([\s\S]*?)<!--\s*/reark:lang\s*-->)"),
        QRegularExpression::CaseInsensitiveOption);

    QList<LocalizedBlock> blocks;
    QRegularExpressionMatchIterator it = blockPattern.globalMatch(body);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString locale = normalizedLocale(match.captured(1));
        const QString content = match.captured(2).trimmed();
        if (!locale.isEmpty() && !content.isEmpty()) {
            blocks.append({ locale, content });
        }
    }
    return blocks;
}

} // namespace

namespace ReleaseNotesLocalizer {

QString selectForLocale(const QString& body, const QString& locale)
{
    return selectForLocales(body, { locale });
}

QString selectForLocales(const QString& body, const QStringList& locales)
{
    const QList<LocalizedBlock> blocks = parseBlocks(body);
    if (blocks.isEmpty()) {
        return body;
    }

    QStringList preferredLocales;
    for (const QString& locale : locales) {
        appendUnique(&preferredLocales, localeCandidates(locale));
    }
    appendUnique(&preferredLocales, localeCandidates(QStringLiteral("en_US")));

    for (const QString& preferredLocale : preferredLocales) {
        for (const LocalizedBlock& block : blocks) {
            if (block.locale == preferredLocale) {
                return block.content;
            }
        }
    }

    return blocks.first().content;
}

} // namespace ReleaseNotesLocalizer
