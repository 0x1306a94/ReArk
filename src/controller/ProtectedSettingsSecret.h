#ifndef REARK_PROTECTED_SETTINGS_SECRET_H
#define REARK_PROTECTED_SETTINGS_SECRET_H

#include <QByteArray>
#include <QString>

class QSettings;

namespace ProtectedSettingsSecret {

[[nodiscard]] QByteArray protect(const QString& secret);
[[nodiscard]] QString unprotect(const QString& protectedSecret);
[[nodiscard]] QString load(
    QSettings& settings,
    const QString& protectedKeyName,
    const QString& legacyKeyName,
    const QString& fallback = {});
[[nodiscard]] QString load(
    QSettings& settings,
    const char* protectedKeyName,
    const char* legacyKeyName,
    const QString& fallback = {});
[[nodiscard]] bool save(
    QSettings& settings,
    const QString& protectedKeyName,
    const QString& legacyKeyName,
    const QString& value);
[[nodiscard]] bool save(
    QSettings& settings,
    const char* protectedKeyName,
    const char* legacyKeyName,
    const QString& value);

} // namespace ProtectedSettingsSecret

#endif // REARK_PROTECTED_SETTINGS_SECRET_H
