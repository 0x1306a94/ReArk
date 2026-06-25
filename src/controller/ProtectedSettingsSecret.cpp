#include "controller/ProtectedSettingsSecret.h"

#include <QSettings>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <wincrypt.h>
#endif

namespace ProtectedSettingsSecret {

#ifdef Q_OS_WIN
QByteArray protect(const QString& secret)
{
    const QByteArray plain = secret.toUtf8();
    DATA_BLOB input {
        .cbData = static_cast<DWORD>(plain.size()),
        .pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData()))
    };
    DATA_BLOB output {};

    if (!CryptProtectData(
            &input,
            L"ReArk Local Secret",
            nullptr,
            nullptr,
            nullptr,
            0,
            &output)) {
        return {};
    }

    QByteArray protectedBytes(
        reinterpret_cast<const char*>(output.pbData),
        static_cast<qsizetype>(output.cbData));
    LocalFree(output.pbData);
    return protectedBytes.toBase64();
}

QString unprotect(const QString& protectedSecret)
{
    const QByteArray protectedBytes = QByteArray::fromBase64(protectedSecret.toUtf8());
    if (protectedBytes.isEmpty()) {
        return {};
    }

    DATA_BLOB input {
        .cbData = static_cast<DWORD>(protectedBytes.size()),
        .pbData = reinterpret_cast<BYTE*>(const_cast<char*>(protectedBytes.constData()))
    };
    DATA_BLOB output {};

    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
        return {};
    }

    const QString secret = QString::fromUtf8(
        reinterpret_cast<const char*>(output.pbData),
        static_cast<qsizetype>(output.cbData));
    LocalFree(output.pbData);
    return secret;
}
#else
QByteArray protect(const QString& secret)
{
    return secret.toUtf8().toBase64();
}

QString unprotect(const QString& protectedSecret)
{
    return QString::fromUtf8(QByteArray::fromBase64(protectedSecret.toUtf8()));
}
#endif

QString load(
    QSettings& settings,
    const QString& protectedKeyName,
    const QString& legacyKeyName,
    const QString& fallback)
{
    const QString protectedValue = settings.value(protectedKeyName).toString();
    if (!protectedValue.isEmpty()) {
        return unprotect(protectedValue);
    }

    const QString legacyPlaintextValue = settings.value(legacyKeyName).toString();
    if (!legacyPlaintextValue.isEmpty()) {
        settings.remove(legacyKeyName);
        const QByteArray protectedLegacyValue = protect(legacyPlaintextValue);
        if (!protectedLegacyValue.isEmpty()) {
            settings.setValue(protectedKeyName, QString::fromLatin1(protectedLegacyValue));
        }
        return legacyPlaintextValue;
    }

    return fallback;
}

QString load(
    QSettings& settings,
    const char* protectedKeyName,
    const char* legacyKeyName,
    const QString& fallback)
{
    return load(
        settings,
        QString::fromLatin1(protectedKeyName),
        QString::fromLatin1(legacyKeyName),
        fallback);
}

bool save(
    QSettings& settings,
    const QString& protectedKeyName,
    const QString& legacyKeyName,
    const QString& value)
{
    settings.remove(legacyKeyName);
    settings.remove(protectedKeyName);
    if (value.isEmpty()) {
        return true;
    }

    const QByteArray protectedValue = protect(value);
    if (protectedValue.isEmpty()) {
        return false;
    }

    settings.setValue(protectedKeyName, QString::fromLatin1(protectedValue));
    return true;
}

bool save(
    QSettings& settings,
    const char* protectedKeyName,
    const char* legacyKeyName,
    const QString& value)
{
    return save(
        settings,
        QString::fromLatin1(protectedKeyName),
        QString::fromLatin1(legacyKeyName),
        value);
}

} // namespace ProtectedSettingsSecret
