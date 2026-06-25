#ifndef REARK_COMMAND_RUNNER_H
#define REARK_COMMAND_RUNNER_H

#include <QObject>
#include <QHash>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <functional>

struct CommandRequest {
    QString program;
    QStringList arguments;
    QString workingDirectory;
    int timeoutMs = 10000;
};

struct CommandResult {
    QString program;
    QStringList arguments;
    QString standardOutput;
    QString standardError;
    QString errorMessage;
    int exitCode = -1;
    int elapsedMs = 0;
    bool started = false;
    bool timedOut = false;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
    QProcess::ProcessError processError = QProcess::UnknownError;

    [[nodiscard]] bool succeeded() const;
    [[nodiscard]] QString commandLine() const;
};

class CommandRunner : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(const CommandResult&)>;

    explicit CommandRunner(QObject* parent = nullptr);
    ~CommandRunner() override;

    int run(const CommandRequest& request, Callback callback);
    Q_INVOKABLE void cancel(int commandId);
    Q_INVOKABLE void cancelAll();

    [[nodiscard]] static CommandResult runBlocking(const CommandRequest& request);

private:
    struct ActiveCommand;

    void finish(int commandId);

    int nextCommandId_ = 1;
    QHash<int, ActiveCommand*> activeCommands_;
};

#endif // REARK_COMMAND_RUNNER_H
