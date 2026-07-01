#include "device/HarmonyPackageRewriter.h"

#include <hyle/hap/hap.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << "FAIL: " << message << '\n';
    return 1;
}

std::string toStdString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

std::optional<QString> codepointsToString(const QString& encoded)
{
    QString result;
    const QStringList parts = encoded.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& rawPart : parts) {
        bool ok = false;
        const uint value = rawPart.trimmed().toUInt(&ok, 16);
        if (!ok || value > 0x10ffff) {
            return std::nullopt;
        }
        if (value <= 0xffff) {
            result.append(QChar(static_cast<ushort>(value)));
        } else {
            const uint adjusted = value - 0x10000;
            result.append(QChar(static_cast<ushort>(0xd800 + (adjusted >> 10))));
            result.append(QChar(static_cast<ushort>(0xdc00 + (adjusted & 0x3ff))));
        }
    }
    return result;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 3 || argc > 7) {
        return fail(QStringLiteral("usage: reark_harmony_package_rewriter_test <sample.hap> <app_packing_tool.jar> [output.hap] [new.bundle] [old.bundle]\n"
                                   "       reark_harmony_package_rewriter_test <sample.hap> <app_packing_tool.jar> --rewrite-string <output.hap> <old.text> <new.text>\n"
                                   "       reark_harmony_package_rewriter_test <sample.hap> <app_packing_tool.jar> --rewrite-string-codepoints <output.hap> <old.text> <hex[,hex...]>"));
    }

    const QString samplePath = QString::fromLocal8Bit(argv[1]);
    const QString packingToolPath = QString::fromLocal8Bit(argv[2]);
    if (!QFileInfo::exists(samplePath)) {
        return fail(QStringLiteral("sample HAP does not exist: %1").arg(samplePath));
    }
    if (!QFileInfo::exists(packingToolPath)) {
        return fail(QStringLiteral("packing tool does not exist: %1").arg(packingToolPath));
    }

    if (argc == 7 && QString::fromLocal8Bit(argv[3]) == QStringLiteral("--rewrite-string")) {
        const QString outputPath = QString::fromLocal8Bit(argv[4]);
        const QString oldText = QString::fromLocal8Bit(argv[5]);
        const QString newText = QString::fromLocal8Bit(argv[6]);
        QDir().mkpath(QFileInfo(outputPath).absolutePath());

        const HarmonyAbcStringRewriteResult result =
            HarmonyPackageRewriter::rewriteAbcString({
                .inputHapPath = samplePath,
                .outputHapPath = outputPath,
                .oldText = oldText,
                .newText = newText,
                .javaProgram = QStringLiteral("java"),
                .packingToolPath = packingToolPath
            });
        if (!result.ok) {
            return fail(QStringLiteral("ABC string rewrite should succeed: %1\n%2")
                .arg(result.error, result.report));
        }
        auto session = hyle::hap::open_decompiled_package(toStdString(outputPath));
        if (!session) {
            return fail(QStringLiteral("string-rewritten HAP should reopen with Hyle: %1")
                .arg(QString::fromUtf8(session.error().message().data())));
        }
        QTextStream(stdout) << "ABC string rewrite passed\n" << result.report << '\n';
        return 0;
    }

    if (argc == 7 && QString::fromLocal8Bit(argv[3]) == QStringLiteral("--rewrite-string-codepoints")) {
        const QString outputPath = QString::fromLocal8Bit(argv[4]);
        const QString oldText = QString::fromLocal8Bit(argv[5]);
        const std::optional<QString> newText = codepointsToString(QString::fromLatin1(argv[6]));
        if (!newText.has_value()) {
            return fail(QStringLiteral("invalid codepoint list: %1").arg(QString::fromLatin1(argv[6])));
        }
        QDir().mkpath(QFileInfo(outputPath).absolutePath());

        const HarmonyAbcStringRewriteResult result =
            HarmonyPackageRewriter::rewriteAbcString({
                .inputHapPath = samplePath,
                .outputHapPath = outputPath,
                .oldText = oldText,
                .newText = *newText,
                .javaProgram = QStringLiteral("java"),
                .packingToolPath = packingToolPath
            });
        if (!result.ok) {
            return fail(QStringLiteral("ABC string rewrite should succeed: %1\n%2")
                .arg(result.error, result.report));
        }
        auto session = hyle::hap::open_decompiled_package(toStdString(outputPath));
        if (!session) {
            return fail(QStringLiteral("string-rewritten HAP should reopen with Hyle: %1")
                .arg(QString::fromUtf8(session.error().message().data())));
        }
        QTextStream(stdout) << "ABC string codepoint rewrite passed\n" << result.report << '\n';
        return 0;
    }

    QString oldBundleName;
    if (argc >= 6) {
        oldBundleName = QString::fromLocal8Bit(argv[5]);
    } else {
        auto inputSession = hyle::hap::open_decompiled_package(toStdString(samplePath));
        if (!inputSession) {
            return fail(QStringLiteral("input HAP should open with Hyle: %1")
                .arg(QString::fromUtf8(inputSession.error().message().data())));
        }
        auto inputSummary = inputSession->summary();
        if (!inputSummary) {
            return fail(QStringLiteral("input HAP summary should be readable: %1")
                .arg(QString::fromUtf8(inputSummary.error().message().data())));
        }
        oldBundleName = QString::fromUtf8(inputSummary->bundle_name);
    }
    const QString newBundleName = argc >= 5
        ? QString::fromLocal8Bit(argv[4])
        : QStringLiteral("com.example.arks");
    if (oldBundleName.trimmed().isEmpty()) {
        return fail(QStringLiteral("input bundle name is empty"));
    }
    if (newBundleName.trimmed().isEmpty()) {
        return fail(QStringLiteral("new bundle name is empty"));
    }

    QTemporaryDir outputDir;
    QString outputPath;
    if (argc >= 4) {
        outputPath = QString::fromLocal8Bit(argv[3]);
        QDir().mkpath(QFileInfo(outputPath).absolutePath());
    } else {
        if (!outputDir.isValid()) {
            return fail(QStringLiteral("temporary output dir should be available"));
        }
        outputPath = outputDir.filePath(QStringLiteral("Bridge-rebundle.hap"));
    }
    const HarmonyBundleRewriteResult result =
        HarmonyPackageRewriter::rewriteBundleIdentity({
            .inputHapPath = samplePath,
            .outputHapPath = outputPath,
            .oldBundleName = oldBundleName,
            .newBundleName = newBundleName,
            .javaProgram = QStringLiteral("java"),
            .packingToolPath = packingToolPath
        });

    if (!result.ok) {
        return fail(QStringLiteral("bundle rewrite should succeed: %1\n%2")
            .arg(result.error, result.report));
    }
    if (!QFileInfo::exists(outputPath) || QFileInfo(outputPath).size() <= 0) {
        return fail(QStringLiteral("rewritten HAP should be written"));
    }
    if (!result.report.contains(QStringLiteral("replaced_class_names"))
        || !result.report.contains(newBundleName)) {
        return fail(QStringLiteral("rewrite report should describe Hyle patch output"));
    }

    auto session = hyle::hap::open_decompiled_package(toStdString(outputPath));
    if (!session) {
        return fail(QStringLiteral("rewritten HAP should reopen with Hyle: %1")
            .arg(QString::fromUtf8(session.error().message().data())));
    }
    auto summary = session->summary();
    if (!summary) {
        return fail(QStringLiteral("rewritten HAP summary should be readable: %1")
            .arg(QString::fromUtf8(summary.error().message().data())));
    }
    const QString summaryBundleName = QString::fromUtf8(summary->bundle_name);
    if (summaryBundleName != newBundleName) {
        return fail(QStringLiteral("rewritten HAP metadata bundle should be %1, got %2")
            .arg(newBundleName, summaryBundleName));
    }
    const QString formattedSummary =
        QString::fromUtf8(hyle::hap::format_decompiled_package_summary(*summary));
    if (!formattedSummary.contains(newBundleName)) {
        return fail(QStringLiteral("rewritten HAP summary should mention the new bundle name"));
    }

    const QString stringRewriteOutputPath = outputDir.isValid()
        ? outputDir.filePath(QStringLiteral("Bridge-string-rewrite.hap"))
        : QFileInfo(outputPath).absoluteDir().filePath(QStringLiteral("Bridge-string-rewrite.hap"));
    const QString replacementString = QStringLiteral("com.reark.safe.verify");
    const HarmonyAbcStringRewriteResult stringRewriteResult =
        HarmonyPackageRewriter::rewriteAbcString({
            .inputHapPath = samplePath,
            .outputHapPath = stringRewriteOutputPath,
            .oldText = oldBundleName,
            .newText = replacementString,
            .javaProgram = QStringLiteral("java"),
            .packingToolPath = packingToolPath,
            .requireUnique = false
        });
    if (!stringRewriteResult.ok) {
        return fail(QStringLiteral("ABC string rewrite should succeed: %1\n%2")
            .arg(stringRewriteResult.error, stringRewriteResult.report));
    }
    if (stringRewriteResult.replacementCount <= 0 || stringRewriteResult.rewrittenAbcCount <= 0) {
        return fail(QStringLiteral("ABC string rewrite should report replacements"));
    }
    if (!stringRewriteResult.report.contains(QStringLiteral("matched_strings"))
        || !stringRewriteResult.report.contains(replacementString)) {
        return fail(QStringLiteral("ABC string rewrite report should describe replacement output"));
    }
    auto stringRewriteSession = hyle::hap::open_decompiled_package(toStdString(stringRewriteOutputPath));
    if (!stringRewriteSession) {
        return fail(QStringLiteral("string-rewritten HAP should reopen with Hyle: %1")
            .arg(QString::fromUtf8(stringRewriteSession.error().message().data())));
    }

    QTextStream(stdout) << "Harmony package rewriter tests passed\n";
    return 0;
}
