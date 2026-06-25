#ifndef REARK_SIGNING_MATERIAL_INSPECTOR_H
#define REARK_SIGNING_MATERIAL_INSPECTOR_H

#include "controller/SigningSettings.h"

#include <QDateTime>
#include <QString>

struct SigningMaterialStatus {
    QString tone = QStringLiteral("error");
    QString summary;
    QDateTime profileNotAfter;
    QDateTime certificateNotAfter;
    QDateTime effectiveNotAfter;
    QString profileBundleName;
};

class SigningMaterialInspector {
public:
    [[nodiscard]] static SigningMaterialStatus inspectHarmony(
        const HarmonySigningSettings& settings,
        const QDateTime& now = QDateTime::currentDateTimeUtc());
};

#endif // REARK_SIGNING_MATERIAL_INSPECTOR_H
