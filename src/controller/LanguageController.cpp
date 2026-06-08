#include "controller/LanguageController.h"

#include "core/Translator.h"

#include <QLatin1String>
#include <QLocale>
#include <QSettings>
#include <QStringList>

namespace {
constexpr auto kSettingsGroup = "Language";
constexpr auto kLocaleKey = "Locale";
constexpr auto kEnglishLocale = "en_US";
constexpr auto kChineseLocale = "zh_CN";
}

LanguageController::LanguageController(QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , translator_(new Translator(engine, this))
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    const QString savedLocale = settings.value(QLatin1String(kLocaleKey)).toString();
    settings.endGroup();

    hasUserLanguage_ = !savedLocale.isEmpty();
    applyLanguage(hasUserLanguage_ ? savedLocale : systemLanguage());
}

QString LanguageController::currentLanguage() const
{
    return currentLanguage_;
}

bool LanguageController::hasUserLanguage() const
{
    return hasUserLanguage_;
}

bool LanguageController::switchLanguage(const QString& locale)
{
    const QString normalizedLocale = normalizedSupportedLocale(locale);
    if (!applyLanguage(normalizedLocale)) {
        return false;
    }

    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.setValue(QLatin1String(kLocaleKey), normalizedLocale);
    settings.endGroup();

    if (!hasUserLanguage_) {
        hasUserLanguage_ = true;
        emit languageChanged();
    }
    return true;
}

void LanguageController::resetLanguage()
{
    clearLanguageOverride();
}

void LanguageController::clearLanguageOverride()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.remove(QLatin1String(kLocaleKey));
    settings.endGroup();

    const bool hadUserLanguage = hasUserLanguage_;
    hasUserLanguage_ = false;
    applyLanguage(systemLanguage());
    if (hadUserLanguage) {
        emit languageChanged();
    }
}

bool LanguageController::applyLanguage(const QString& locale)
{
    const QString normalizedLocale = normalizedSupportedLocale(locale);
    const bool changed = currentLanguage_ != normalizedLocale;

    if (normalizedLocale == QLatin1String(kEnglishLocale)) {
        translator_->resetLanguage();
    } else if (!translator_->switchLanguage(normalizedLocale)) {
        translator_->resetLanguage();
        if (currentLanguage_ == QLatin1String(kEnglishLocale)) {
            return false;
        }
        currentLanguage_ = QLatin1String(kEnglishLocale);
        emit languageChanged();
        return false;
    }

    currentLanguage_ = normalizedLocale;
    if (changed) {
        emit languageChanged();
    }
    return true;
}

QString LanguageController::normalizedSupportedLocale(const QString& locale)
{
    const QLocale parsedLocale(locale);
    if (parsedLocale.language() == QLocale::Chinese) {
        return QLatin1String(kChineseLocale);
    }
    return QLatin1String(kEnglishLocale);
}

QString LanguageController::systemLanguage()
{
    const QLocale systemLocale = QLocale::system();
    const QStringList uiLanguages = systemLocale.uiLanguages();
    for (const QString& uiLanguage : uiLanguages) {
        const QLocale parsedLocale(uiLanguage);
        if (parsedLocale.language() == QLocale::Chinese) {
            return QLatin1String(kChineseLocale);
        }
        if (parsedLocale.language() == QLocale::English) {
            return QLatin1String(kEnglishLocale);
        }
    }

    return normalizedSupportedLocale(systemLocale.name());
}
