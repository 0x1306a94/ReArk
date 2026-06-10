#ifndef REARK_AGENT_CONTROLLER_H
#define REARK_AGENT_CONTROLLER_H

#include <QObject>
#include <QString>

#include <memory>
#include <optional>
#include <stop_token>

class DecompilerController;

class AgentController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString transcript READ transcript NOTIFY transcriptChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit AgentController(DecompilerController* decompilerController, QObject* parent = nullptr);
    ~AgentController() override;

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool running() const;
    [[nodiscard]] QString transcript() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] QString status() const;

    Q_INVOKABLE void ask(const QString& question);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void newChat();

signals:
    void runningChanged();
    void transcriptChanged();
    void errorMessageChanged();
    void statusChanged();

private:
    struct Runtime;

    void setRunning(bool running);
    void setTranscript(const QString& transcript);
    void appendTranscript(const QString& text);
    void setErrorMessage(const QString& errorMessage);
    void setStatus(const QString& status);
    void resetRun();
    [[nodiscard]] QString unavailableMessage() const;

    DecompilerController* decompilerController_ = nullptr;
    std::unique_ptr<Runtime> runtime_;
    QString transcript_;
    QString errorMessage_;
    QString status_;
    bool running_ = false;
};

#endif // REARK_AGENT_CONTROLLER_H
