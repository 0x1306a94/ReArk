#ifndef REARK_AGENT_CONTROLLER_H
#define REARK_AGENT_CONTROLLER_H

#include <QAbstractItemModel>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>

class AgentMessageModel;
class DecompilerController;
class AgentKnowledgeController;
class QTimer;

class AgentController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString transcript READ transcript NOTIFY transcriptChanged)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(QAbstractItemModel* messageModel READ messageModel CONSTANT)
    Q_PROPERTY(bool hasMessages READ hasMessages NOTIFY messagesChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit AgentController(
        DecompilerController* decompilerController,
        AgentKnowledgeController* knowledgeController,
        QObject* parent = nullptr);
    ~AgentController() override;

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool running() const;
    [[nodiscard]] QString transcript() const;
    [[nodiscard]] QVariantList messages() const;
    [[nodiscard]] QAbstractItemModel* messageModel() const;
    [[nodiscard]] bool hasMessages() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] QString status() const;

    Q_INVOKABLE void ask(const QString& question);
    Q_INVOKABLE void editUserMessage(int row, const QString& text);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void newChat();
    Q_INVOKABLE void copyTextToClipboard(const QString& text) const;

signals:
    void runningChanged();
    void transcriptChanged();
    void messagesChanged();
    void errorMessageChanged();
    void statusChanged();

private:
    struct Runtime;
    enum class RunWaitPhase {
        Idle,
        Model,
        Tool,
        Other
    };

    struct RunUiUpdate {
        QString status;
        QVariantMap activity;
        RunWaitPhase phase = RunWaitPhase::Other;
    };

    void setRunning(bool running);
    void setTranscript(const QString& transcript);
    void clearMessages();
    void appendMessage(const QString& role, const QString& text, const QString& state = {});
    void enqueueAssistantDeltaFromWorker(quint64 runId, const QString& text);
    void enqueueAssistantReasoningDeltaFromWorker(quint64 runId, const QString& text);
    void enqueueRunUiUpdateFromWorker(
        quint64 runId,
        const QString& status,
        const QVariantMap& activity,
        RunWaitPhase phase);
    void drainWorkerAssistantDeltas();
    [[nodiscard]] std::optional<RunUiUpdate> drainWorkerRunUiUpdate();
    void queueAssistantDelta(const QString& text);
    void queueAssistantReasoningDelta(const QString& text);
    void flushPendingAssistantDelta();
    void appendToActiveAssistantMessage(const QString& text);
    void appendReasoningToActiveAssistantMessage(const QString& text);
    void recordActiveAssistantActivity(
        const QString& type,
        const QString& title,
        const QString& detail = {},
        const QString& state = {});
    void finishActiveAssistantMessage(const QString& fallbackText = {}, bool replaceExistingText = false);
    void finishInterruptedAssistantMessage(const QString& notice);
    void failActiveAssistantMessage();
    void rebuildTranscript();
    void appendTranscript(const QString& text);
    void setErrorMessage(const QString& errorMessage);
    void setStatus(const QString& status);
    void startRunWatchdog();
    void stopRunWatchdog();
    void noteRunActivity(RunWaitPhase phase);
    void checkRunWatchdog();
    void resetRun();
    void cancelCurrentRun(bool clearPendingQuestion);
    void startPendingQuestion();
    [[nodiscard]] QString unavailableMessage() const;

    DecompilerController* decompilerController_ = nullptr;
    AgentKnowledgeController* knowledgeController_ = nullptr;
    std::unique_ptr<Runtime> runtime_;
    QString transcript_;
    QVariantList messages_;
    AgentMessageModel* messageModel_ = nullptr;
    QString errorMessage_;
    QString status_;
    std::mutex workerAssistantDeltaMutex_;
    QString workerAssistantDelta_;
    QString workerAssistantReasoningDelta_;
    quint64 workerAssistantDeltaRunId_ = 0;
    bool workerAssistantDeltaFlushQueued_ = false;
    std::mutex workerRunUiMutex_;
    QString workerRunStatus_;
    QVariantMap workerRunActivity_;
    RunWaitPhase workerRunPhase_ = RunWaitPhase::Other;
    quint64 workerRunUiRunId_ = 0;
    bool workerRunUiFlushQueued_ = false;
    QString pendingAssistantDelta_;
    QString pendingAssistantReasoningDelta_;
    QTimer* assistantDeltaTimer_ = nullptr;
    QTimer* runWatchdogTimer_ = nullptr;
    qint64 runStartedAtMs_ = 0;
    qint64 lastRunActivityAtMs_ = 0;
    RunWaitPhase runWaitPhase_ = RunWaitPhase::Idle;
    int activeAssistantMessage_ = -1;
    QString pendingQuestion_;
    quint64 activeRunId_ = 0;
    bool running_ = false;
};

#endif // REARK_AGENT_CONTROLLER_H
