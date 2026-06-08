#include "core/BuildInfoProvider.h"

#include "BuildInfo.h"
#include "controller/LanguageController.h"

#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QTime>
#include <QTimeZone>

namespace {

QDateTime buildDateTimeInBeijing()
{
    const QDateTime utcBuildTime(
        QDate(REARK_BUILD_YEAR, REARK_BUILD_MONTH, REARK_BUILD_DAY),
        QTime(REARK_BUILD_HOUR, REARK_BUILD_MINUTE, REARK_BUILD_SECOND),
        QTimeZone::UTC);
    return utcBuildTime.toTimeZone(QTimeZone("Asia/Shanghai"));
}

} // namespace

BuildInfoProvider::BuildInfoProvider(LanguageController* languageController, QObject* parent)
    : QObject(parent)
    , languageController_(languageController)
{
    if (languageController_) {
        connect(languageController_, &LanguageController::languageChanged,
                this, &BuildInfoProvider::buildTimestampChanged);
    }
}

QString BuildInfoProvider::buildTimestamp() const
{
    const QDateTime buildTime = buildDateTimeInBeijing();
    if (languageController_ && languageController_->currentLanguage() == QStringLiteral("zh_CN")) {
        return QLocale(QLocale::Chinese, QLocale::China)
            .toString(buildTime, QStringLiteral("yyyy年M月d日 HH:mm:ss"));
    }

    return QLocale(QLocale::English, QLocale::UnitedStates)
        .toString(buildTime, QStringLiteral("MMM d yyyy 'at' HH:mm:ss"));
}
