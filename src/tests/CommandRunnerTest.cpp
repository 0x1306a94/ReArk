#include "core/CommandRunner.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QThread>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << "FAIL: " << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    if (app.arguments().contains(QStringLiteral("--child-delay"))) {
        QThread::msleep(250);
        return 0;
    }

    CommandRequest request;
    request.program = QCoreApplication::applicationFilePath();
    request.arguments = { QStringLiteral("--child-delay") };
    request.timeoutMs = 5000;

    const CommandResult result = CommandRunner::runBlocking(request);
    if (!result.succeeded()) {
        return fail(QStringLiteral("a normal delayed process should not inherit transient wait timeouts; error=%1 exit=%2 timedOut=%3 elapsed=%4")
            .arg(result.errorMessage)
            .arg(result.exitCode)
            .arg(result.timedOut)
            .arg(result.elapsedMs));
    }

    QTextStream(stdout) << "Command runner tests passed\n";
    return 0;
}
