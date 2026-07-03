#include "controller/UpdateController.h"

#include "controller/LanguageController.h"
#include "controller/ReleaseNotesLocalizer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QtGlobal>

#include <algorithm>

namespace {

constexpr auto kLatestReleaseUrl = "https://api.github.com/repos/lkimuk/ReArk/releases/latest";
constexpr auto kProjectReleaseUrl = "https://github.com/lkimuk/ReArk/releases";
constexpr auto kUserAgent = "ReArkUpdater/1.0";
constexpr auto kUpdateSettingsGroup = "Updates";
constexpr auto kLastAutomaticCheckKey = "lastAutomaticCheckUtc";
constexpr qint64 kAutomaticCheckIntervalSeconds = 24 * 60 * 60;

bool previewEnabledByEnvironment()
{
#ifdef QT_DEBUG
    const QString value = qEnvironmentVariable("REARK_UPDATE_PREVIEW").trimmed().toLower();
    return value == QLatin1String("1")
           || value == QLatin1String("true")
           || value == QLatin1String("yes")
           || value == QLatin1String("on");
#else
    return false;
#endif
}

QString normalizeVersionText(const QString& version)
{
    QString normalized = version.trimmed();
    if (normalized.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        normalized.remove(0, 1);
    }
    const QRegularExpression suffixPattern(QStringLiteral(R"([+-].*$)"));
    normalized.remove(suffixPattern);
    return normalized;
}

QList<int> versionParts(const QString& version)
{
    QList<int> parts;
    const QString normalized = normalizeVersionText(version);
    for (const QString& part : normalized.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int number = part.toInt(&ok);
        parts.append(ok ? number : 0);
    }
    while (parts.size() < 3) {
        parts.append(0);
    }
    return parts;
}

} // namespace

UpdateController::UpdateController(LanguageController* languageController, QObject* parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
    , languageController_(languageController)
{
}

bool UpdateController::checking() const
{
    return checking_;
}

QString UpdateController::latestVersion() const
{
    return latestVersion_;
}

QString UpdateController::releaseUrl() const
{
    return releaseUrl_;
}

bool UpdateController::updatePreviewAvailable() const
{
    return previewEnabledByEnvironment();
}

void UpdateController::checkForUpdates(bool silent)
{
    if (checking_) {
        return;
    }

    setChecking(true);
    QNetworkRequest request(QUrl(QString::fromLatin1(kLatestReleaseUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setRawHeader("Accept", "application/vnd.github+json");

    auto* reply = networkManager_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, silent]() {
        handleLatestReleaseReply(reply, silent);
    });
}

void UpdateController::checkForUpdatesIfDue()
{
    if (checking_ || !automaticCheckDue()) {
        return;
    }
    recordAutomaticCheckAttempt();
    checkForUpdates(true);
}

void UpdateController::previewUpdateAvailable()
{
#ifdef QT_DEBUG
    if (!updatePreviewAvailable()) {
        return;
    }

    latestVersion_ = QStringLiteral("0.2.1");
    releaseUrl_ = QString::fromLatin1(kProjectReleaseUrl);
    emit latestReleaseChanged();
    emit updateAvailable(
        latestVersion_,
        QStringLiteral(R"(Include following updates:

**1. ReArk Agent upgrades**
- Added ABC evidence tools for strings, literals, xrefs, call argument flows, ABC tree inspection, source reading, and disassembly reading.
- Added richer package context for the Agent, including package summary, entry points, signature state, and installable HAP candidates.
- Improved Agent execution boundaries and internal tool routing.

**2. HarmonyOS Device Runtime**
- Added a dedicated Device Runtime workspace.
- Added bundled HDC-based device discovery, package install, ability launch, hilog capture, screenshot capture, and UI layout inspection.
- Added UI node overlay, node tapping, coordinate tapping, text input, Back/Home key events, swipe gestures, and screenshot auto-refresh.
- Added explicit user confirmation before re-signing and installing packages that fail due to missing or invalid signatures.

**3. Harmony HAP packing and signing workflow**
- Added Harmony signing settings for `.p12`, key alias, `.p7b` profile, and `.cer` certificate.
- Added protected local storage for signing passwords.
- Added signing material inspection, including profile/certificate validity and profile bundle name detection.
- Bundled Harmony signing tools: `hap-sign-tool.jar` and `app_packing_tool.jar`.
- Added HAP signing, official packing, and bundle identity rewrite support.

**4. Package install and re-sign flow**
- Added APP container installable HAP resolution.
- Improved HDC install result detection when `hdc` exits with code 0 but reports install failure in stdout.
- Added automatic handling for unsigned HAP detection, with user-approved re-signing and signed reinstall.
- Added command log evidence for initial install, signing, bundle rewrite, and signed install steps.

**5. UI and settings improvements**
- Added a cleaner Device Runtime interface independent from the code/tree workspace.
- Refined Settings UI with flatter controls and icon-only secret visibility buttons.
- Added shared `ReArkTextField` to fix floating placeholder/border overlap.
- Reduced UI clutter by hiding empty sections, temporary paths, and duplicate runtime controls.

**6. Infrastructure and tests**
- Added `CommandRunner`, `HdcDeviceBackend`, `UiAutomationBackend`, `HarmonyHapSigner`, and `HarmonyPackageRewriter`.
- Added tests for HDC command handling, UI automation parsing, HAP signing, package rewriting, and ABC evidence.
- Added HarmonyOS sample packages and release screenshots.)"),
        releaseUrl_,
        QStringLiteral("2026-07-02T10:30:00Z"));
#endif
}

void UpdateController::openReleasePage(const QString& releaseUrl) const
{
    const QUrl url(releaseUrl.isEmpty() ? QString::fromLatin1(kProjectReleaseUrl) : releaseUrl);
    QDesktopServices::openUrl(url);
}

void UpdateController::setChecking(bool checking)
{
    if (checking_ == checking) {
        return;
    }
    checking_ = checking;
    emit checkingChanged();
}

void UpdateController::handleLatestReleaseReply(QNetworkReply* reply, bool silent)
{
    const auto cleanup = [this, reply]() {
        reply->deleteLater();
        setChecking(false);
    };

    if (reply->error() != QNetworkReply::NoError) {
        const QString message = networkErrorMessage(reply);
        cleanup();
        if (!silent) {
            emit checkFailed(message);
        }
        return;
    }

    const QByteArray body = reply->readAll();
    cleanup();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (!silent) {
            emit checkFailed(tr("The update response was not valid JSON."));
        }
        return;
    }

    const QJsonObject object = document.object();
    const QString tagName = object.value(QStringLiteral("tag_name")).toString();
    const QString htmlUrl = object.value(QStringLiteral("html_url")).toString(QString::fromLatin1(kProjectReleaseUrl));
    const QString changelog = ReleaseNotesLocalizer::selectForLocale(
        object.value(QStringLiteral("body")).toString(),
        preferredReleaseNotesLocale());
    const QString releaseDate = object.value(QStringLiteral("published_at")).toString();
    if (tagName.isEmpty()) {
        if (!silent) {
            emit checkFailed(tr("The update response did not include a release version."));
        }
        return;
    }

    if (!isNewerVersion(tagName, QCoreApplication::applicationVersion())) {
        resetLatestRelease();
        if (!silent) {
            emit noUpdateAvailable();
        }
        return;
    }

    latestVersion_ = normalizedVersion(tagName);
    releaseUrl_ = htmlUrl;
    emit latestReleaseChanged();
    emit updateAvailable(latestVersion_, changelog, releaseUrl_, releaseDate);
}

void UpdateController::resetLatestRelease()
{
    if (latestVersion_.isEmpty() && releaseUrl_.isEmpty()) {
        return;
    }
    latestVersion_.clear();
    releaseUrl_.clear();
    emit latestReleaseChanged();
}

QString UpdateController::preferredReleaseNotesLocale() const
{
    if (languageController_ != nullptr) {
        return languageController_->currentLanguage();
    }
    return QLocale::system().name();
}

bool UpdateController::automaticCheckDue()
{
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kUpdateSettingsGroup));
    const QDateTime lastCheck = settings.value(QString::fromLatin1(kLastAutomaticCheckKey)).toDateTime();
    settings.endGroup();

    if (!lastCheck.isValid()) {
        return true;
    }

    return lastCheck.toUTC().secsTo(QDateTime::currentDateTimeUtc()) >= kAutomaticCheckIntervalSeconds;
}

void UpdateController::recordAutomaticCheckAttempt()
{
    QSettings settings;
    settings.beginGroup(QString::fromLatin1(kUpdateSettingsGroup));
    settings.setValue(QString::fromLatin1(kLastAutomaticCheckKey), QDateTime::currentDateTimeUtc());
    settings.endGroup();
}

bool UpdateController::isNewerVersion(const QString& candidate, const QString& current)
{
    const QList<int> candidateParts = versionParts(candidate);
    const QList<int> currentParts = versionParts(current);
    const int count = std::max(candidateParts.size(), currentParts.size());
    for (int i = 0; i < count; ++i) {
        const int candidatePart = i < candidateParts.size() ? candidateParts.at(i) : 0;
        const int currentPart = i < currentParts.size() ? currentParts.at(i) : 0;
        if (candidatePart != currentPart) {
            return candidatePart > currentPart;
        }
    }
    return false;
}

QString UpdateController::normalizedVersion(const QString& version)
{
    return normalizeVersionText(version);
}

QString UpdateController::networkErrorMessage(QNetworkReply* reply)
{
    const QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (statusCode.isValid()) {
        const int code = statusCode.toInt();
        if (code == 403) {
            return tr("GitHub refused the update request. Please try again later.");
        }
        if (code == 404) {
            return tr("The update information could not be found.");
        }
        if (code >= 400) {
            return tr("The update server returned HTTP %1.").arg(code);
        }
    }

    switch (reply->error()) {
    case QNetworkReply::HostNotFoundError:
        return tr("The update server could not be found.");
    case QNetworkReply::ConnectionRefusedError:
        return tr("The update connection was refused.");
    case QNetworkReply::TimeoutError:
        return tr("The update request timed out.");
    case QNetworkReply::SslHandshakeFailedError:
        return tr("The secure update connection failed.");
    default:
        return reply->errorString();
    }
}
