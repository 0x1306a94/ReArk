#ifndef REARK_APPLICATION_CONTROLLER_H
#define REARK_APPLICATION_CONTROLLER_H

#include <QObject>

class ApplicationController : public QObject {
    Q_OBJECT

public:
    explicit ApplicationController(QObject* parent = nullptr);

    Q_INVOKABLE bool openNewWindow();
    Q_INVOKABLE void copyTextToClipboard(const QString& text) const;
    Q_INVOKABLE QString licenseText() const;
};

#endif // REARK_APPLICATION_CONTROLLER_H
