#ifndef REARK_BUILD_INFO_PROVIDER_H
#define REARK_BUILD_INFO_PROVIDER_H

#include <QObject>
#include <QString>

class LanguageController;

class BuildInfoProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString buildTimestamp READ buildTimestamp NOTIFY buildTimestampChanged)

public:
    explicit BuildInfoProvider(LanguageController* languageController, QObject* parent = nullptr);

    QString buildTimestamp() const;

signals:
    void buildTimestampChanged();

private:
    LanguageController* languageController_ = nullptr;
};

#endif // REARK_BUILD_INFO_PROVIDER_H
