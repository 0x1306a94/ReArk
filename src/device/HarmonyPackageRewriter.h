#ifndef REARK_HARMONY_PACKAGE_REWRITER_H
#define REARK_HARMONY_PACKAGE_REWRITER_H

#include "core/CommandRunner.h"

#include <QString>

struct HarmonyBundleRewriteRequest {
    QString inputHapPath;
    QString outputHapPath;
    QString oldBundleName;
    QString newBundleName;
    QString javaProgram;
    QString packingToolPath;
    int timeoutMs = 120000;
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

class HarmonyPackageRewriter {
public:
    [[nodiscard]] static HarmonyBundleRewriteResult rewriteBundleIdentity(
        const HarmonyBundleRewriteRequest& request);
};

#endif // REARK_HARMONY_PACKAGE_REWRITER_H
