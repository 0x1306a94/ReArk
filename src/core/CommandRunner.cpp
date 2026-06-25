#include "core/CommandRunner.h"

#include <QElapsedTimer>
#include <QTimer>

#include <algorithm>

namespace {

QString quoteArgument(const QString& argument)
{
    if (argument.isEmpty()) {
        return QStringLiteral("\"\"");
    }
    const bool needsQuotes = std::any_of(argument.cbegin(), argument.cend(), [](QChar ch) {
        return ch.isSpace() || ch == QLatin1Char('"');
    });
    if (!needsQuotes) {
        return argument;
    }

    QString escaped = argument;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

} // namespace

bool CommandResult::succeeded() const
{
    return started
        && !timedOut
        && processError == QProcess::UnknownError
        && exitStatus == QProcess::NormalExit
        && exitCode == 0;
}

QString CommandResult::commandLine() const
{
    QStringList parts;
    parts.reserve(arguments.size() + 1);
    parts.append(quoteArgument(program));
    for (const QString& argument : arguments) {
        parts.append(quoteArgument(argument));
    }
    return parts.join(QLatin1Char(' '));
}

struct CommandRunner::ActiveCommand {
    QProcess* process = nullptr;
    QTimer* timer = nullptr;
    QElapsedTimer elapsed;
    CommandResult result;
    Callback callback;
    bool finished = false;
};

CommandRunner::CommandRunner(QObject* parent)
    : QObject(parent)
{
}

CommandRunner::~CommandRunner()
{
    const QList<int> commandIds = activeCommands_.keys();
    for (int commandId : commandIds) {
        ActiveCommand* active = activeCommands_.take(commandId);
        if (active == nullptr) {
            continue;
        }
        if (active->process != nullptr && active->process->state() != QProcess::NotRunning) {
            active->process->kill();
            active->process->waitForFinished(1000);
        }
        delete active->process;
        delete active->timer;
        delete active;
    }
}

int CommandRunner::run(const CommandRequest& request, Callback callback)
{
    const int commandId = nextCommandId_++;
    auto* active = new ActiveCommand;
    active->process = new QProcess(this);
    active->timer = new QTimer(this);
    active->timer->setSingleShot(true);
    active->result.program = request.program;
    active->result.arguments = request.arguments;
    active->callback = std::move(callback);
    active->elapsed.start();

    if (!request.workingDirectory.trimmed().isEmpty()) {
        active->process->setWorkingDirectory(request.workingDirectory);
    }

    activeCommands_.insert(commandId, active);

    connect(active->timer, &QTimer::timeout, this, [this, commandId]() {
        ActiveCommand* active = activeCommands_.value(commandId, nullptr);
        if (active == nullptr || active->finished) {
            return;
        }
        active->result.timedOut = true;
        active->result.errorMessage = tr("Command timed out.");
        active->process->kill();
    });

    connect(active->process, &QProcess::started, this, [this, commandId]() {
        if (ActiveCommand* active = activeCommands_.value(commandId, nullptr)) {
            active->result.started = true;
        }
    });

    connect(active->process, &QProcess::errorOccurred, this, [this, commandId](QProcess::ProcessError error) {
        ActiveCommand* active = activeCommands_.value(commandId, nullptr);
        if (active == nullptr || active->finished) {
            return;
        }
        active->result.processError = error;
        if (active->result.errorMessage.isEmpty()) {
            active->result.errorMessage = active->process->errorString();
        }
        if (error == QProcess::FailedToStart) {
            finish(commandId);
        }
    });

    connect(active->process, &QProcess::finished, this, [this, commandId](int exitCode, QProcess::ExitStatus exitStatus) {
        ActiveCommand* active = activeCommands_.value(commandId, nullptr);
        if (active == nullptr || active->finished) {
            return;
        }
        active->result.exitCode = exitCode;
        active->result.exitStatus = exitStatus;
        finish(commandId);
    });

    active->timer->start(std::max(1, request.timeoutMs));
    active->process->start(request.program, request.arguments);
    return commandId;
}

void CommandRunner::cancel(int commandId)
{
    ActiveCommand* active = activeCommands_.value(commandId, nullptr);
    if (active == nullptr || active->finished) {
        return;
    }
    active->result.errorMessage = tr("Command cancelled.");
    active->process->kill();
}

void CommandRunner::cancelAll()
{
    const QList<int> commandIds = activeCommands_.keys();
    for (int commandId : commandIds) {
        cancel(commandId);
    }
}

CommandResult CommandRunner::runBlocking(const CommandRequest& request)
{
    QElapsedTimer elapsed;
    elapsed.start();

    CommandResult result;
    result.program = request.program;
    result.arguments = request.arguments;

    QProcess process;
    if (!request.workingDirectory.trimmed().isEmpty()) {
        process.setWorkingDirectory(request.workingDirectory);
    }
    process.start(request.program, request.arguments);
    if (!process.waitForStarted(std::min(std::max(1, request.timeoutMs), 5000))) {
        result.processError = process.error();
        result.errorMessage = process.errorString();
        result.elapsedMs = int(elapsed.elapsed());
        result.standardOutput = QString::fromUtf8(process.readAllStandardOutput());
        result.standardError = QString::fromUtf8(process.readAllStandardError());
        return result;
    }

    result.started = true;
    if (!process.waitForFinished(std::max(1, request.timeoutMs))) {
        result.timedOut = true;
        result.errorMessage = QObject::tr("Command timed out.");
        process.kill();
        process.waitForFinished(1000);
    }

    result.elapsedMs = int(elapsed.elapsed());
    result.exitCode = process.exitCode();
    result.exitStatus = process.exitStatus();
    result.processError = process.error();
    result.standardOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.standardError = QString::fromUtf8(process.readAllStandardError());
    if (result.errorMessage.isEmpty() && result.processError != QProcess::UnknownError) {
        result.errorMessage = process.errorString();
    }
    return result;
}

void CommandRunner::finish(int commandId)
{
    ActiveCommand* active = activeCommands_.take(commandId);
    if (active == nullptr || active->finished) {
        return;
    }

    active->finished = true;
    active->timer->stop();
    active->result.elapsedMs = int(active->elapsed.elapsed());
    active->result.standardOutput = QString::fromUtf8(active->process->readAllStandardOutput());
    active->result.standardError = QString::fromUtf8(active->process->readAllStandardError());
    if (active->result.errorMessage.isEmpty()
        && active->result.processError != QProcess::UnknownError) {
        active->result.errorMessage = active->process->errorString();
    }

    Callback callback = std::move(active->callback);
    CommandResult result = active->result;
    active->process->deleteLater();
    active->timer->deleteLater();
    delete active;

    if (callback) {
        callback(result);
    }
}
