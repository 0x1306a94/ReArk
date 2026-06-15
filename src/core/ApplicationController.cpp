#include "core/ApplicationController.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent)
{
}

bool ApplicationController::openNewWindow()
{
    const QString executablePath = QCoreApplication::applicationFilePath();
    const QString workingDirectory = QFileInfo(executablePath).absolutePath();
    return QProcess::startDetached(executablePath, {}, workingDirectory);
}

void ApplicationController::copyTextToClipboard(const QString& text) const
{
    if (auto* clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}

QString ApplicationController::licenseText() const
{
    QFile file(QStringLiteral(":/legal/LICENSE"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    return QString::fromUtf8(file.readAll());
}
