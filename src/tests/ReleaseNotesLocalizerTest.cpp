#include "controller/ReleaseNotesLocalizer.h"

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
    QTextStream(stderr) << "FAIL: " << label << "\nExpected:\n"
                        << expected << "\nActual:\n"
                        << actual << '\n';
    return false;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString localizedBody = QStringLiteral(R"(Intro that should be ignored when localized blocks exist.

<!-- reark:lang=en -->
English changelog.
<!-- /reark:lang -->

<!-- reark:lang=zh-CN -->
中文更新内容。
<!-- /reark:lang -->
)");

    if (!expectEqual(
            ReleaseNotesLocalizer::selectForLocale(localizedBody, QStringLiteral("zh_CN")),
            QStringLiteral("中文更新内容。"),
            QStringLiteral("zh_CN should select the Chinese release notes"))) {
        return 1;
    }

    if (!expectEqual(
            ReleaseNotesLocalizer::selectForLocale(localizedBody, QStringLiteral("en-US")),
            QStringLiteral("English changelog."),
            QStringLiteral("en-US should select the English release notes"))) {
        return 1;
    }

    if (!expectEqual(
            ReleaseNotesLocalizer::selectForLocale(localizedBody, QStringLiteral("fr_FR")),
            QStringLiteral("English changelog."),
            QStringLiteral("unsupported locales should fall back to English"))) {
        return 1;
    }

    const QString legacyBody = QStringLiteral("Legacy release notes without language blocks.");
    if (!expectEqual(
            ReleaseNotesLocalizer::selectForLocale(legacyBody, QStringLiteral("zh_CN")),
            legacyBody,
            QStringLiteral("legacy release notes should remain unchanged"))) {
        return 1;
    }

    const QString chineseOnlyBody = QStringLiteral(R"(<!-- reark:lang=zh_CN -->
只有中文。
<!-- /reark:lang -->)");
    if (!expectEqual(
            ReleaseNotesLocalizer::selectForLocale(chineseOnlyBody, QStringLiteral("ja_JP")),
            QStringLiteral("只有中文。"),
            QStringLiteral("localized releases without English should fall back to the first block"))) {
        return 1;
    }

    QTextStream(stdout) << "Release notes localizer tests passed\n";
    return 0;
}
