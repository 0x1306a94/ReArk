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

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 3 || argc > 6) {
        return fail(QStringLiteral("usage: reark_harmony_package_rewriter_test <sample.hap> <app_packing_tool.jar> [output.hap] [new.bundle] [old.bundle]"));
    }

    const QString samplePath = QString::fromLocal8Bit(argv[1]);
    const QString packingToolPath = QString::fromLocal8Bit(argv[2]);
    if (!QFileInfo::exists(samplePath)) {
        return fail(QStringLiteral("sample HAP does not exist: %1").arg(samplePath));
    }
    if (!QFileInfo::exists(packingToolPath)) {
        return fail(QStringLiteral("packing tool does not exist: %1").arg(packingToolPath));
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

    QTextStream(stdout) << "Harmony package rewriter tests passed\n";
    return 0;
}
