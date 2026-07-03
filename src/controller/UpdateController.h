#ifndef REARK_UPDATE_CONTROLLER_H
#define REARK_UPDATE_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QUrl>

class LanguageController;
class QNetworkAccessManager;
class QNetworkReply;

class UpdateController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestReleaseChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY latestReleaseChanged)
    Q_PROPERTY(bool updatePreviewAvailable READ updatePreviewAvailable CONSTANT)

public:
    explicit UpdateController(LanguageController* languageController = nullptr, QObject* parent = nullptr);

    [[nodiscard]] bool checking() const;
    [[nodiscard]] QString latestVersion() const;
    [[nodiscard]] QString releaseUrl() const;
    [[nodiscard]] bool updatePreviewAvailable() const;

    Q_INVOKABLE void checkForUpdates(bool silent = false);
    Q_INVOKABLE void checkForUpdatesIfDue();
    Q_INVOKABLE void previewUpdateAvailable();
    Q_INVOKABLE void openReleasePage(const QString& releaseUrl) const;

signals:
    void checkingChanged();
    void latestReleaseChanged();
    void updateAvailable(const QString& version,
                         const QString& changelog,
                         const QString& releaseUrl,
                         const QString& releaseDate);
    void noUpdateAvailable();
    void checkFailed(const QString& message);

private:
    void setChecking(bool checking);
    void handleLatestReleaseReply(QNetworkReply* reply, bool silent);
    void resetLatestRelease();
    [[nodiscard]] QString preferredReleaseNotesLocale() const;
    [[nodiscard]] static bool automaticCheckDue();
    static void recordAutomaticCheckAttempt();
    [[nodiscard]] static bool isNewerVersion(const QString& candidate, const QString& current);
    [[nodiscard]] static QString normalizedVersion(const QString& version);
    [[nodiscard]] static QString networkErrorMessage(QNetworkReply* reply);

    QNetworkAccessManager* networkManager_ = nullptr;
    LanguageController* languageController_ = nullptr;
    bool checking_ = false;
    QString latestVersion_;
    QString releaseUrl_;
};

#endif // REARK_UPDATE_CONTROLLER_H
