#include "signing/HarmonyHapSigner.h"
#include "signing/SigningMaterialInspector.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << "FAIL: " << message << '\n';
    return 1;
}

bool writeFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(content) == content.size();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString signToolPath =
        HarmonyHapSigner::bundledSignToolPathForApplicationDir(QStringLiteral("D:/ReArk/bin"));
    if (signToolPath != QStringLiteral("D:/ReArk/bin/plugin/harmony-tools/lib/hap-sign-tool.jar")) {
        return fail(QStringLiteral("bundled sign tool path should use app-local plugin path"));
    }

    const QString packingToolPath =
        HarmonyHapSigner::bundledPackingToolPathForApplicationDir(QStringLiteral("D:/ReArk/bin"));
    if (packingToolPath != QStringLiteral("D:/ReArk/bin/plugin/harmony-tools/lib/app_packing_tool.jar")) {
        return fail(QStringLiteral("bundled packing tool path should use app-local plugin path"));
    }

    HarmonyHapSigningRequest request;
    request.javaProgram = QStringLiteral("java");
    request.signToolPath = signToolPath;
    request.keystorePath = QStringLiteral("D:/signing/debug.p12");
    request.keystorePassword = QStringLiteral("store-secret");
    request.keyAlias = QStringLiteral("debug");
    request.profilePath = QStringLiteral("D:/signing/profile.p7b");
    request.certificatePath = QStringLiteral("D:/signing/debug.cer");
    request.inputHapPath = QStringLiteral("D:/packages/agent-unsigned.hap");
    request.outputHapPath = QStringLiteral("D:/packages/agent.hap");
    request.compatibleVersion = QStringLiteral("12");

    const CommandRequest command = HarmonyHapSigner::signCommand(request);
    const QStringList expected {
        QStringLiteral("-jar"),
        QStringLiteral("D:/ReArk/bin/plugin/harmony-tools/lib/hap-sign-tool.jar"),
        QStringLiteral("sign-app"),
        QStringLiteral("-mode"),
        QStringLiteral("localSign"),
        QStringLiteral("-keystoreFile"),
        QStringLiteral("D:/signing/debug.p12"),
        QStringLiteral("-keystorePwd"),
        QStringLiteral("store-secret"),
        QStringLiteral("-keyAlias"),
        QStringLiteral("debug"),
        QStringLiteral("-signAlg"),
        QStringLiteral("SHA256withECDSA"),
        QStringLiteral("-profileFile"),
        QStringLiteral("D:/signing/profile.p7b"),
        QStringLiteral("-appCertFile"),
        QStringLiteral("D:/signing/debug.cer"),
        QStringLiteral("-inFile"),
        QStringLiteral("D:/packages/agent-unsigned.hap"),
        QStringLiteral("-outFile"),
        QStringLiteral("D:/packages/agent.hap"),
        QStringLiteral("-compatibleVersion"),
        QStringLiteral("12")
    };

    if (command.program != QStringLiteral("java") || command.arguments != expected) {
        return fail(QStringLiteral("sign-app argv changed unexpectedly"));
    }

    HarmonyHapPackingRequest packingRequest;
    packingRequest.javaProgram = QStringLiteral("java");
    packingRequest.packingToolPath = packingToolPath;
    packingRequest.unpackedDirectory = QStringLiteral("D:/packages/unpacked");
    packingRequest.outputHapPath = QStringLiteral("D:/packages/repacked.hap");
    const CommandRequest packCommand = HarmonyHapSigner::packCommand(packingRequest);
    const QStringList expectedPack {
        QStringLiteral("-jar"),
        QStringLiteral("D:/ReArk/bin/plugin/harmony-tools/lib/app_packing_tool.jar"),
        QStringLiteral("--mode"),
        QStringLiteral("hap"),
        QStringLiteral("--json-path"),
        QStringLiteral("D:/packages/unpacked/module.json"),
        QStringLiteral("--ets-path"),
        QStringLiteral("D:/packages/unpacked/ets"),
        QStringLiteral("--lib-path"),
        QStringLiteral("D:/packages/unpacked/libs"),
        QStringLiteral("--resources-path"),
        QStringLiteral("D:/packages/unpacked/resources"),
        QStringLiteral("--pack-info-path"),
        QStringLiteral("D:/packages/unpacked/pack.info"),
        QStringLiteral("--index-path"),
        QStringLiteral("D:/packages/unpacked/resources.index"),
        QStringLiteral("--out-path"),
        QStringLiteral("D:/packages/repacked.hap"),
        QStringLiteral("--force"),
        QStringLiteral("true")
    };
    if (packCommand.program != QStringLiteral("java") || packCommand.arguments != expectedPack) {
        return fail(QStringLiteral("app packing argv changed unexpectedly"));
    }

    request.keyPassword = QStringLiteral("key-secret");
    const QStringList explicitKeyPasswordArgs = HarmonyHapSigner::signCommand(request).arguments;
    if (!explicitKeyPasswordArgs.contains(QStringLiteral("-keyPwd"))
        || !explicitKeyPasswordArgs.contains(QStringLiteral("key-secret"))) {
        return fail(QStringLiteral("explicit key password should be preserved"));
    }

    CommandResult result;
    result.program = QStringLiteral("java");
    result.arguments = HarmonyHapSigner::signCommand(request).arguments;
    result.standardError = QStringLiteral("bad password");
    const QString summary = HarmonyHapSigner::resultSummary(result);
    if (summary.contains(QStringLiteral("store-secret"))
        || summary.contains(QStringLiteral("key-secret"))
        || !summary.contains(QStringLiteral("<redacted>"))) {
        return fail(QStringLiteral("signing result summary must redact password arguments"));
    }

    QTemporaryDir signingDir;
    if (!signingDir.isValid()) {
        return fail(QStringLiteral("temporary signing dir should be available"));
    }

    const QString keystorePath = signingDir.filePath(QStringLiteral("debug.p12"));
    const QString profilePath = signingDir.filePath(QStringLiteral("debug.p7b"));
    const QString certificatePath = signingDir.filePath(QStringLiteral("debug.cer"));
    const QDateTime inspectionTime =
        QDateTime::fromString(QStringLiteral("2026-06-25T00:00:00Z"), Qt::ISODate);
    const qint64 profileNotBefore =
        QDateTime::fromString(QStringLiteral("2026-06-24T00:00:00Z"), Qt::ISODate)
            .toSecsSinceEpoch();
    const qint64 profileNotAfter =
        QDateTime::fromString(QStringLiteral("2030-06-24T00:00:00Z"), Qt::ISODate)
            .toSecsSinceEpoch();
    const QByteArray profileBytes =
        QByteArrayLiteral("pkcs7-prefix")
        + QByteArrayLiteral("{\"ignored\":true}")
        + QByteArrayLiteral("{\"validity\":{\"not-before\":")
        + QByteArray::number(profileNotBefore)
        + QByteArrayLiteral(",\"not-after\":")
        + QByteArray::number(profileNotAfter)
        + QByteArrayLiteral("},\"type\":\"debug\",\"bundle-info\":{\"bundle-name\":\"com.example.reark\"}}")
        + QByteArrayLiteral("pkcs7-suffix");
    static constexpr auto kCertificatePem =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDMzCCAhugAwIBAgIUSjb8DAhtgVNtDfEyHnJCPOe4m/MwDQYJKoZIhvcNAQEL\n"
        "BQAwKTEnMCUGA1UEAwweUmVBcmsgVGVzdCBTaWduaW5nIENlcnRpZmljYXRlMB4X\n"
        "DTI2MDYyNDEwMjgwMVoXDTM2MDYyMTEwMjgwMVowKTEnMCUGA1UEAwweUmVBcmsg\n"
        "VGVzdCBTaWduaW5nIENlcnRpZmljYXRlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A\n"
        "MIIBCgKCAQEAt6e2dbLU+Axhn8kztUDH5qIAzC2FOwB8ECJAK8AlKs5MjlaRKO2h\n"
        "tRmftF9qC5ClgmIukLkelvjskyB8/HgwcC6Zdxw2mTyBBLz8QjVssDoUtsew8Yf3\n"
        "sqMq555r2Kg6Ctwjqfvjh57wuhervgJ7hHmvwMzvTCk2uu7eouJqe2KskrwKpGlV\n"
        "d3w7xQ72J6+GQoTsQwPmRVV+ojOIv3vqxMk7U8GCFLWJrUWD2qX1rPySjdt0XvA9\n"
        "icJyXahGyupOSXlHrzlXt6Pt6nAW9o4CniHKX43+Wny8WF5piuifKI1m/8T2T/KW\n"
        "8vWZHDwEo/GGjN9RuNfR0Lt+/gTEh/o+PwIDAQABo1MwUTAdBgNVHQ4EFgQUYe30\n"
        "LGug1CoA2MleExljnFTxa/AwHwYDVR0jBBgwFoAUYe30LGug1CoA2MleExljnFTx\n"
        "a/AwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEARBVJbKy0yWvm\n"
        "P9XLHjw2b/cqzdy/im4hCPSbThdgPeH/8oxYnWjyWP53LUonIHuNb/Yspw0qN9hG\n"
        "SruUBBWRGKHxTr2COiGCfKNMVfD8WFKz7IUXvv1AtZEbchW+UYLDdSY5i8kLR9Te\n"
        "3mouWE/geEV4pTKGfYSjuZrdE8pzkid3DBR3ylHlo2OzFe1CJBCKoWrShdVLjHuo\n"
        "LQELIZcWZ6GkE79B0Y6+nLky0bGn9ozWuzJhk69ybhnYrVn+tObyRSO/wpnEnoJ/\n"
        "75ZuWYq0GGsjw0xouG7NnnsEpKxBlBYYcIMhlNvP4s2EK83opr8gQu50dOvn523h\n"
        "5uX6FeOTfA==\n"
        "-----END CERTIFICATE-----\n";
    if (!writeFile(keystorePath, "p12")
        || !writeFile(profilePath, profileBytes)
        || !writeFile(certificatePath, kCertificatePem)) {
        return fail(QStringLiteral("temporary signing material should be writable"));
    }

    const SigningMaterialStatus materialStatus = SigningMaterialInspector::inspectHarmony({
        .keystorePath = keystorePath,
        .keystorePassword = QStringLiteral("store-secret"),
        .keyAlias = QStringLiteral("debug"),
        .keyPassword = {},
        .profilePath = profilePath,
        .certificatePath = certificatePath
    }, inspectionTime);
    if (materialStatus.tone != QStringLiteral("ok")
        || !materialStatus.summary.contains(QStringLiteral("valid until"))
        || !materialStatus.profileNotAfter.isValid()
        || !materialStatus.certificateNotAfter.isValid()
        || materialStatus.effectiveNotAfter != materialStatus.profileNotAfter
        || materialStatus.profileBundleName != QStringLiteral("com.example.reark")) {
        return fail(QStringLiteral("signing material inspector should report profile-bounded validity"));
    }

    const SigningMaterialStatus missingStatus = SigningMaterialInspector::inspectHarmony({
        .keystorePath = keystorePath,
        .keystorePassword = QStringLiteral("store-secret"),
        .keyAlias = QStringLiteral("debug"),
        .keyPassword = {},
        .profilePath = profilePath,
        .certificatePath = signingDir.filePath(QStringLiteral("missing.cer"))
    });
    if (missingStatus.tone != QStringLiteral("error")
        || !missingStatus.summary.contains(QStringLiteral("Certificate file does not exist"))) {
        return fail(QStringLiteral("signing material inspector should report missing certificates"));
    }

    QTextStream(stdout) << "Harmony HAP signer tests passed\n";
    return 0;
}
