#ifndef REARK_HARMONY_PACKAGE_REWRITER_H
#define REARK_HARMONY_PACKAGE_REWRITER_H

#include "core/CommandRunner.h"

#include <QString>
#include <QStringList>
#include <stop_token>

struct HarmonyBundleRewriteRequest {
    QString inputHapPath;
    QString outputHapPath;
    QString oldBundleName;
    QString newBundleName;
    QStringList strippedRequestPermissions;
    QStringList forcedDeviceTypes;
    int forcedCompatibleApi = 0;
    int forcedTargetApi = 0;
    QString javaProgram;
    QString packingToolPath;
    std::stop_token stopToken;
    int timeoutMs = 300000;
};

struct HarmonyBundleRewriteResult {
    bool ok = false;
    QString inputHapPath;
    QString outputHapPath;
    QString unpackedDirectory;
    QString error;
    QString report;
    CommandResult packingResult;
};

struct HarmonyAbcStringRewriteRequest {
    QString inputHapPath;
    QString outputHapPath;
    QString oldText;
    QString newText;
    QString javaProgram;
    QString packingToolPath;
    std::stop_token stopToken;
    int timeoutMs = 300000;
    bool requireUnique = true;
};

struct HarmonyAbcStringRewriteResult {
    bool ok = false;
    QString inputHapPath;
    QString outputHapPath;
    QString unpackedDirectory;
    QString error;
    QString report;
    int abcCount = 0;
    int rewrittenAbcCount = 0;
    int replacementCount = 0;
    CommandResult packingResult;
};

class HarmonyPackageRewriter {
public:
    [[nodiscard]] static HarmonyBundleRewriteResult rewriteBundleIdentity(
        const HarmonyBundleRewriteRequest& request);
    [[nodiscard]] static HarmonyAbcStringRewriteResult rewriteAbcString(
        const HarmonyAbcStringRewriteRequest& request);
};

#endif // REARK_HARMONY_PACKAGE_REWRITER_H
