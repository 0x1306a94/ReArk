#include "controller/DeviceRuntimeController.h"
#include "device/HarmonyPackageRewriter.h"

#include <hyle/hap/hap.h>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include <exception>

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

QString packageBundleName(const QString& packagePath)
{
    auto session = hyle::hap::open_decompiled_package(toStdString(packagePath));
    if (!session) {
        return {};
    }
    auto summary = session->summary();
    if (!summary) {
        return {};
    }
    return QString::fromUtf8(summary->bundle_name).trimmed();
}

bool waitForLaunchMetadata(DeviceRuntimeController& controller, const QString& packagePath)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&controller, &DeviceRuntimeController::launchMetadataChanged, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    controller.refreshLaunchMetadata(packagePath);
    timeout.start(5000);
    loop.exec();
    return timeout.isActive();
}

int verifyScreenResolutionParser()
{
#ifndef REARK_DEVICE_RUNTIME_TESTING
    return fail(QStringLiteral("screen resolution parser test hook is disabled"));
#else
    const auto expectResolution = [](const QString& output, const QString& expected) -> int {
        const QString actual = rearkTestScreenResolutionFromOutput(output);
        if (actual != expected) {
            return fail(QStringLiteral("screen resolution parser expected %1, got %2 for output: %3")
                .arg(expected, actual, output));
        }
        return 0;
    };

    if (const int result = expectResolution(
            QStringLiteral("activeMode: 1260x2720\nrender resolution: 1260x2720\n"),
            QStringLiteral("2720 x 1260"));
        result != 0) {
        return result;
    }
    if (const int result = expectResolution(
            QStringLiteral("screenWidth: 1080\nscreenHeight: 2340\n"),
            QStringLiteral("2340 x 1080"));
        result != 0) {
        return result;
    }
    if (const int result = expectResolution(
            QStringLiteral("Physical size: 1440x3120\nOverride size: 1080x2340\n"),
            QStringLiteral("3120 x 1440"));
        result != 0) {
        return result;
    }
    if (const int result = expectResolution(
            QStringLiteral("noise\nsmall 120x80\n"),
            QString());
        result != 0) {
        return result;
    }
    return 0;
#endif
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        QCoreApplication app(argc, argv);
        QCoreApplication::setOrganizationName(QStringLiteral("ReArkTests"));
        QCoreApplication::setApplicationName(QStringLiteral("DeviceRuntimeControllerTest"));

        if (const int result = verifyScreenResolutionParser(); result != 0) {
            return result;
        }

        if (argc != 3) {
            return fail(QStringLiteral("usage: reark_device_runtime_controller_test <sample.hap> <app_packing_tool.jar>"));
        }

        const QString samplePath = QString::fromLocal8Bit(argv[1]);
        const QString packingToolPath = QString::fromLocal8Bit(argv[2]);
        if (!QFileInfo::exists(samplePath)) {
            return fail(QStringLiteral("sample HAP does not exist: %1").arg(samplePath));
        }
        if (!QFileInfo::exists(packingToolPath)) {
            return fail(QStringLiteral("packing tool does not exist: %1").arg(packingToolPath));
        }

        const QString originalBundle = packageBundleName(samplePath);
        if (originalBundle.isEmpty()) {
            return fail(QStringLiteral("sample bundle name should be readable"));
        }

        DeviceRuntimeController controller;
        if (!waitForLaunchMetadata(controller, samplePath)) {
            return fail(QStringLiteral("launch metadata refresh timed out for original HAP"));
        }
        if (controller.launchBundleName() != originalBundle) {
            return fail(QStringLiteral("loading an uninstalled package should use package bundle %1, got %2")
                .arg(originalBundle, controller.launchBundleName()));
        }

        QTemporaryDir outputDir;
        if (!outputDir.isValid()) {
            return fail(QStringLiteral("temporary output dir should be available"));
        }

        const QString newBundle = originalBundle == QStringLiteral("com.example.rearktest")
            ? QStringLiteral("com.example.rearktest2")
            : QStringLiteral("com.example.rearktest");
        const QString rewrittenPath = outputDir.filePath(QStringLiteral("rewritten.hap"));
        const HarmonyBundleRewriteResult rewriteResult =
            HarmonyPackageRewriter::rewriteBundleIdentity({
                .inputHapPath = samplePath,
                .outputHapPath = rewrittenPath,
                .oldBundleName = originalBundle,
                .newBundleName = newBundle,
                .javaProgram = QStringLiteral("java"),
                .packingToolPath = packingToolPath
            });
        if (!rewriteResult.ok) {
            return fail(QStringLiteral("bundle rewrite should succeed: %1\n%2")
                .arg(rewriteResult.error, rewriteResult.report));
        }

        if (!waitForLaunchMetadata(controller, rewrittenPath)) {
            return fail(QStringLiteral("launch metadata refresh timed out for rewritten HAP"));
        }
        if (controller.launchBundleName() != newBundle) {
            return fail(QStringLiteral("launch metadata should follow the rewritten installable package identity %1, got %2")
                .arg(newBundle, controller.launchBundleName()));
        }

        QTextStream(stdout) << "Device runtime controller tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        return fail(QStringLiteral("unexpected exception: %1").arg(QString::fromUtf8(ex.what())));
    } catch (...) {
        return fail(QStringLiteral("unexpected unknown exception"));
    }
}
