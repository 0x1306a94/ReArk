#ifndef REARK_DEVICE_RUNTIME_CONTROLLER_H
#define REARK_DEVICE_RUNTIME_CONTROLLER_H

#include "core/CommandRunner.h"
#include "device/HdcDeviceBackend.h"
#include "device/UiAutomationBackend.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

class DeviceRuntimeController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString selectedDeviceId READ selectedDeviceId WRITE setSelectedDeviceId NOTIFY selectedDeviceChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString activeOperation READ activeOperation NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString commandLog READ commandLog NOTIFY commandLogChanged)
    Q_PROPERTY(QString hilogText READ hilogText NOTIFY hilogTextChanged)
    Q_PROPERTY(QString screenshotPath READ screenshotPath NOTIFY screenshotPathChanged)
    Q_PROPERTY(QVariantList uiNodes READ uiNodes NOTIFY uiNodesChanged)
    Q_PROPERTY(QVariantList filteredUiNodes READ filteredUiNodes NOTIFY uiNodesChanged)
    Q_PROPERTY(QString uiNodeSummary READ uiNodeSummary NOTIFY uiNodesChanged)
    Q_PROPERTY(QString uiNodeFilter READ uiNodeFilter WRITE setUiNodeFilter NOTIFY uiNodeFilterChanged)
    Q_PROPERTY(bool screenRefreshRunning READ screenRefreshRunning NOTIFY screenRefreshStateChanged)
    Q_PROPERTY(QString screenRefreshStatus READ screenRefreshStatus NOTIFY screenRefreshStatusChanged)
    Q_PROPERTY(int screenRefreshFrameCount READ screenRefreshFrameCount NOTIFY screenRefreshFrameChanged)
    Q_PROPERTY(double screenRefreshFps READ screenRefreshFps NOTIFY screenRefreshFrameChanged)

public:
    explicit DeviceRuntimeController(QObject* parent = nullptr);

    [[nodiscard]] QVariantList devices() const;
    [[nodiscard]] QString selectedDeviceId() const;
    void setSelectedDeviceId(const QString& selectedDeviceId);

    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString activeOperation() const;
    [[nodiscard]] QString status() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] QString commandLog() const;
    [[nodiscard]] QString hilogText() const;
    [[nodiscard]] QString screenshotPath() const;
    [[nodiscard]] QVariantList uiNodes() const;
    [[nodiscard]] QVariantList filteredUiNodes() const;
    [[nodiscard]] QString uiNodeSummary() const;
    [[nodiscard]] QString uiNodeFilter() const;
    void setUiNodeFilter(const QString& uiNodeFilter);
    [[nodiscard]] bool screenRefreshRunning() const;
    [[nodiscard]] QString screenRefreshStatus() const;
    [[nodiscard]] int screenRefreshFrameCount() const;
    [[nodiscard]] double screenRefreshFps() const;

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void installPackage(const QString& packagePath);
    Q_INVOKABLE void startAbility(const QString& bundleName, const QString& abilityName);
    Q_INVOKABLE void readHilog(const QString& filter = QString(), int maxLines = 500);
    Q_INVOKABLE void captureScreenshot();
    Q_INVOKABLE void captureUiSnapshot(const QString& bundleName = QString());
    Q_INVOKABLE void refreshUiLayout(const QString& bundleName = QString());
    Q_INVOKABLE void tapUi(int x, int y);
    Q_INVOKABLE void tapUiNode(int nodeIndex);
    Q_INVOKABLE void inputUiTextAt(int x, int y, const QString& text);
    Q_INVOKABLE void inputFocusedUiText(const QString& text);
    Q_INVOKABLE void pressDeviceKey(const QString& key);
    Q_INVOKABLE void swipeDevice(int fromX, int fromY, int toX, int toY, int velocity = 600);
    Q_INVOKABLE void startScreenRefresh(int intervalMs = 1500);
    Q_INVOKABLE void stopScreenRefresh();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearOutput();

signals:
    void devicesChanged();
    void selectedDeviceChanged();
    void busyChanged();
    void statusChanged();
    void errorMessageChanged();
    void commandLogChanged();
    void hilogTextChanged();
    void screenshotPathChanged();
    void uiNodesChanged();
    void uiNodeFilterChanged();
    void screenRefreshStateChanged();
    void screenRefreshStatusChanged();
    void screenRefreshFrameChanged();

private:
    enum class ScreenshotRequestKind {
        Manual,
        AutoRefresh
    };

    void runOperation(
        const QString& operation,
        const CommandRequest& request,
        std::function<void(const CommandResult&)> onFinished = {});
    void captureScreenshot(ScreenshotRequestKind kind);
    void captureAutoRefreshFrame();
    void appendCommandLog(const CommandResult& result);
    void setBusy(bool busy, const QString& operation = {});
    void setStatus(const QString& status);
    void setErrorMessage(const QString& errorMessage);
    void setScreenRefreshStatus(const QString& status);
    void setDevices(const QList<HdcDeviceTarget>& targets);
    void setUiNodes(const QList<UiAutomationNode>& nodes);
    void refreshFilteredUiNodes();
    [[nodiscard]] QString currentTargetId() const;
    [[nodiscard]] QString makeLocalScreenshotPath() const;
    [[nodiscard]] QString makeLocalLayoutPath() const;

    CommandRunner runner_;
    HdcDeviceBackend backend_;
    UiAutomationBackend uiBackend_;
    QTimer screenRefreshTimer_;
    QElapsedTimer screenRefreshElapsed_;
    QVariantList devices_;
    QVariantList uiNodes_;
    QVariantList filteredUiNodes_;
    QList<UiAutomationNode> parsedUiNodes_;
    QString selectedDeviceId_;
    QString activeOperation_;
    QString status_;
    QString errorMessage_;
    QString commandLog_;
    QString hilogText_;
    QString screenshotPath_;
    QString uiNodeSummary_;
    QString uiNodeFilter_;
    QString screenRefreshStatus_;
    int screenRefreshFrameCount_ = 0;
    double screenRefreshFps_ = 0.0;
    int activeCommandId_ = 0;
    bool busy_ = false;
};

#endif // REARK_DEVICE_RUNTIME_CONTROLLER_H
