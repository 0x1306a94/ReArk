#ifndef REARK_HARMONY_HAP_SIGNER_H
#define REARK_HARMONY_HAP_SIGNER_H

#include "core/CommandRunner.h"

#include <QString>

struct HarmonyHapSigningRequest {
    QString javaProgram;
    QString signToolPath;
    QString keystorePath;
    QString keystorePassword;
    QString keyAlias;
    QString keyPassword;
    QString profilePath;
    QString certificatePath;
    QString inputHapPath;
    QString outputHapPath;
    QString signAlgorithm = QStringLiteral("SHA256withECDSA");
    QString compatibleVersion;
    int timeoutMs = 120000;
};

struct HarmonyHapPackingRequest {
    QString javaProgram;
    QString packingToolPath;
    QString unpackedDirectory;
    QString outputHapPath;
    int timeoutMs = 120000;
};

class HarmonyHapSigner {
public:
    [[nodiscard]] static CommandRequest packCommand(const HarmonyHapPackingRequest& request);
    [[nodiscard]] static CommandRequest signCommand(const HarmonyHapSigningRequest& request);
    [[nodiscard]] static QString bundledSignToolPath();
    [[nodiscard]] static QString bundledSignToolPathForApplicationDir(const QString& applicationDir);
    [[nodiscard]] static QString bundledPackingToolPath();
    [[nodiscard]] static QString bundledPackingToolPathForApplicationDir(const QString& applicationDir);
    [[nodiscard]] static QString resolvedJavaProgram();
    [[nodiscard]] static QString resolvedJavaProgramForApplicationDir(const QString& applicationDir);
    [[nodiscard]] static QString resultSummary(const CommandResult& result);
};

#endif // REARK_HARMONY_HAP_SIGNER_H
