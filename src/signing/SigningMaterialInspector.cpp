#include "signing/SigningMaterialInspector.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QObject>
#include <QSslCertificate>

#include <limits>
#include <optional>

namespace {

struct ProfileValidity {
    QDateTime notBefore;
    QDateTime notAfter;
    QString bundleName;
    QString error;
};

bool hasSuffix(const QString& path, const QString& suffix)
{
    return path.endsWith(suffix, Qt::CaseInsensitive);
}

QString localDateTimeText(const QDateTime& value)
{
    return value.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QList<QSslCertificate> readCertificates(const QString& path)
{
    QList<QSslCertificate> certificates = QSslCertificate::fromPath(path, QSsl::Pem);
    if (!certificates.isEmpty()) {
        return certificates;
    }
    return QSslCertificate::fromPath(path, QSsl::Der);
}

QByteArray extractJsonObjectAt(const QByteArray& bytes, int start)
{
    if (start < 0 || start >= bytes.size() || bytes.at(start) != '{') {
        return {};
    }

    bool inString = false;
    bool escaped = false;
    int depth = 0;
    for (int i = start; i < bytes.size(); ++i) {
        const char ch = bytes.at(i);
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }
        if (ch == '{') {
            ++depth;
            continue;
        }
        if (ch == '}') {
            --depth;
            if (depth == 0) {
                return bytes.mid(start, i - start + 1);
            }
        }
    }
    return {};
}

std::optional<qint64> integerSeconds(const QJsonValue& value)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double seconds = value.toDouble();
    if (seconds < 0 || seconds > double(std::numeric_limits<qint64>::max())) {
        return std::nullopt;
    }
    return static_cast<qint64>(seconds);
}

std::optional<ProfileValidity> parseProfileValidityCandidate(const QByteArray& jsonBytes)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }

    const QJsonObject validity = document.object().value(QStringLiteral("validity")).toObject();
    const std::optional<qint64> notBeforeSeconds =
        integerSeconds(validity.value(QStringLiteral("not-before")));
    const std::optional<qint64> notAfterSeconds =
        integerSeconds(validity.value(QStringLiteral("not-after")));
    if (!notBeforeSeconds || !notAfterSeconds) {
        return std::nullopt;
    }

    return ProfileValidity {
        QDateTime::fromSecsSinceEpoch(*notBeforeSeconds, Qt::UTC),
        QDateTime::fromSecsSinceEpoch(*notAfterSeconds, Qt::UTC),
        document.object()
            .value(QStringLiteral("bundle-info"))
            .toObject()
            .value(QStringLiteral("bundle-name"))
            .toString()
            .trimmed(),
        {}
    };
}

ProfileValidity readProfileValidity(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return { {}, {}, QObject::tr("Profile file could not be read.") };
    }

    const QByteArray bytes = file.readAll();
    int start = -1;
    while ((start = bytes.indexOf('{', start + 1)) >= 0) {
        const QByteArray jsonBytes = extractJsonObjectAt(bytes, start);
        if (jsonBytes.isEmpty()) {
            continue;
        }
        const std::optional<ProfileValidity> validity = parseProfileValidityCandidate(jsonBytes);
        if (validity) {
            return *validity;
        }
    }

    return { {}, {}, QObject::tr("Profile validity could not be found.") };
}

QDateTime earlierValidDate(const QDateTime& left, const QDateTime& right)
{
    if (!left.isValid()) {
        return right;
    }
    if (!right.isValid()) {
        return left;
    }
    return left <= right ? left : right;
}

} // namespace

SigningMaterialStatus SigningMaterialInspector::inspectHarmony(
    const HarmonySigningSettings& settings,
    const QDateTime& now)
{
    const QString keystorePath = settings.keystorePath.trimmed();
    const QString profilePath = settings.profilePath.trimmed();
    const QString certificatePath = settings.certificatePath.trimmed();

    SigningMaterialStatus status;

    if (keystorePath.isEmpty()
        || settings.keystorePassword.isEmpty()
        || settings.keyAlias.trimmed().isEmpty()
        || profilePath.isEmpty()
        || certificatePath.isEmpty()) {
        status.summary = QObject::tr("Incomplete signing configuration.");
        return status;
    }
    if (!QFileInfo::exists(keystorePath)) {
        status.summary = QObject::tr("Keystore file does not exist.");
        return status;
    }
    if (!hasSuffix(keystorePath, QStringLiteral(".p12"))) {
        status.summary = QObject::tr("Keystore must be a .p12 file.");
        return status;
    }
    if (!QFileInfo::exists(profilePath)) {
        status.summary = QObject::tr("Profile file does not exist.");
        return status;
    }
    if (!hasSuffix(profilePath, QStringLiteral(".p7b"))) {
        status.summary = QObject::tr("Profile must be a .p7b file.");
        return status;
    }
    if (!QFileInfo::exists(certificatePath)) {
        status.summary = QObject::tr("Certificate file does not exist.");
        return status;
    }
    if (!hasSuffix(certificatePath, QStringLiteral(".cer"))) {
        status.summary = QObject::tr("Certificate must be a .cer file.");
        return status;
    }

    const ProfileValidity profileValidity = readProfileValidity(profilePath);
    if (!profileValidity.error.isEmpty()) {
        status.summary = profileValidity.error;
        return status;
    }
    status.profileNotAfter = profileValidity.notAfter;
    status.profileBundleName = profileValidity.bundleName;

    const QList<QSslCertificate> certificates = readCertificates(certificatePath);
    if (certificates.isEmpty()) {
        status.summary = QObject::tr("Certificate could not be parsed.");
        return status;
    }

    const QSslCertificate certificate = certificates.first();
    const QDateTime certificateNotBefore = certificate.effectiveDate().toUTC();
    const QDateTime certificateNotAfter = certificate.expiryDate().toUTC();
    const QDateTime normalizedNow = now.toUTC();
    status.certificateNotAfter = certificateNotAfter;
    status.effectiveNotAfter = earlierValidDate(profileValidity.notAfter, certificateNotAfter);

    if (!certificateNotBefore.isValid() || !certificateNotAfter.isValid()) {
        status.summary = QObject::tr("Certificate validity period is unavailable.");
        return status;
    }
    if (!profileValidity.notBefore.isValid() || !profileValidity.notAfter.isValid()) {
        status.summary = QObject::tr("Profile validity period is unavailable.");
        return status;
    }
    if (normalizedNow < profileValidity.notBefore) {
        status.tone = QStringLiteral("warning");
        status.summary = QObject::tr("Profile is not valid until %1.")
            .arg(localDateTimeText(profileValidity.notBefore));
        return status;
    }
    if (normalizedNow < certificateNotBefore) {
        status.tone = QStringLiteral("warning");
        status.summary = QObject::tr("Certificate is not valid until %1.")
            .arg(localDateTimeText(certificateNotBefore));
        return status;
    }
    if (normalizedNow > profileValidity.notAfter) {
        status.summary = QObject::tr("Profile expired at %1.")
            .arg(localDateTimeText(profileValidity.notAfter));
        return status;
    }
    if (normalizedNow > certificateNotAfter) {
        status.summary = QObject::tr("Certificate expired at %1.")
            .arg(localDateTimeText(certificateNotAfter));
        return status;
    }

    status.tone = QStringLiteral("ok");
    status.summary = QObject::tr("Signing material valid until %1.")
        .arg(localDateTimeText(status.effectiveNotAfter));
    return status;
}
