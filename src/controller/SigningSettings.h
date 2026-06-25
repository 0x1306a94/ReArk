#ifndef REARK_SIGNING_SETTINGS_H
#define REARK_SIGNING_SETTINGS_H

#include <QString>

struct HarmonySigningSettings {
    QString keystorePath;
    QString keystorePassword;
    QString keyAlias;
    QString keyPassword;
    QString profilePath;
    QString certificatePath;
};

class SigningSettingsStore {
public:
    [[nodiscard]] static HarmonySigningSettings loadHarmony();
    [[nodiscard]] static bool saveHarmony(const HarmonySigningSettings& settings);
    static void resetHarmony();
    [[nodiscard]] static QString harmonyValidationMessage(const HarmonySigningSettings& settings);
};

#endif // REARK_SIGNING_SETTINGS_H
