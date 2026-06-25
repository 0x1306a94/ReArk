#include "controller/DeviceRuntimeController.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace {

constexpr int kMinScreenRefreshIntervalMs = 500;
constexpr int kMaxScreenRefreshIntervalMs = 10000;

} // namespace

DeviceRuntimeController::DeviceRuntimeController(QObject* parent)
    : QObject(parent)
    , runner_(this)
    , uiBackend_(backend_)
    , status_(tr("Ready"))
{
    screenRefreshStatus_ = tr("Screen refresh stopped.");
    screenRefreshTimer_.setSingleShot(false);
    connect(&screenRefreshTimer_, &QTimer::timeout, this, &DeviceRuntimeController::captureAutoRefreshFrame);
}

QVariantList DeviceRuntimeController::devices() const
{
    return devices_;
}

QString DeviceRuntimeController::selectedDeviceId() const
{
    return selectedDeviceId_;
}

void DeviceRuntimeController::setSelectedDeviceId(const QString& selectedDeviceId)
{
    const QString trimmed = selectedDeviceId.trimmed();
    if (selectedDeviceId_ == trimmed) {
        return;
    }
    selectedDeviceId_ = trimmed;
    emit selectedDeviceChanged();
}

bool DeviceRuntimeController::busy() const
{
    return busy_;
}

QString DeviceRuntimeController::activeOperation() const
{
    return activeOperation_;
}

QString DeviceRuntimeController::status() const
{
    return status_;
}

QString DeviceRuntimeController::errorMessage() const
{
    return errorMessage_;
}

QString DeviceRuntimeController::commandLog() const
{
    return commandLog_;
}

QString DeviceRuntimeController::hilogText() const
{
    return hilogText_;
}

QString DeviceRuntimeController::screenshotPath() const
{
    return screenshotPath_;
}

QVariantList DeviceRuntimeController::uiNodes() const
{
    return uiNodes_;
}

QVariantList DeviceRuntimeController::filteredUiNodes() const
{
    return filteredUiNodes_;
}

QString DeviceRuntimeController::uiNodeSummary() const
{
    return uiNodeSummary_;
}

QString DeviceRuntimeController::uiNodeFilter() const
{
    return uiNodeFilter_;
}

void DeviceRuntimeController::setUiNodeFilter(const QString& uiNodeFilter)
{
    const QString trimmed = uiNodeFilter.trimmed();
    if (uiNodeFilter_ == trimmed) {
        return;
    }
    uiNodeFilter_ = trimmed;
    refreshFilteredUiNodes();
    emit uiNodesChanged();
    emit uiNodeFilterChanged();
}

bool DeviceRuntimeController::screenRefreshRunning() const
{
    return screenRefreshTimer_.isActive();
}

QString DeviceRuntimeController::screenRefreshStatus() const
{
    return screenRefreshStatus_;
}

int DeviceRuntimeController::screenRefreshFrameCount() const
{
    return screenRefreshFrameCount_;
}

double DeviceRuntimeController::screenRefreshFps() const
{
    return screenRefreshFps_;
}

void DeviceRuntimeController::refreshDevices()
{
    runOperation(tr("Refresh devices"), backend_.listTargetsRequest(), [this](const CommandResult& result) {
        if (!result.succeeded()) {
            return;
        }
        const QList<HdcDeviceTarget> targets = HdcDeviceBackend::parseTargets(result.standardOutput);
        setDevices(targets);
        setStatus(targets.isEmpty()
                ? tr("No HDC targets found.")
                : tr("Found %n HDC target(s).", nullptr, targets.size()));
    });
}

void DeviceRuntimeController::installPackage(const QString& packagePath)
{
    const QString trimmed = packagePath.trimmed();
    if (trimmed.isEmpty()) {
        setErrorMessage(tr("No package is loaded."));
        return;
    }
    if (!QFileInfo::exists(trimmed)) {
        setErrorMessage(tr("Package does not exist: %1").arg(trimmed));
        return;
    }

    runOperation(
        tr("Install package"),
        backend_.installRequest(trimmed, currentTargetId()),
        [this](const CommandResult& result) {
            if (HdcDeviceBackend::installSucceeded(result)) {
                setStatus(tr("Package installed."));
            } else {
                setErrorMessage(tr("Install package failed."));
            }
        });
}

void DeviceRuntimeController::startAbility(const QString& bundleName, const QString& abilityName)
{
    const QString bundle = bundleName.trimmed();
    if (bundle.isEmpty()) {
        setErrorMessage(tr("Bundle name is required."));
        return;
    }

    runOperation(
        tr("Start ability"),
        backend_.startAbilityRequest(bundle, abilityName, currentTargetId()),
        [this](const CommandResult& result) {
            if (result.succeeded()) {
                setStatus(tr("Start command completed."));
            }
        });
}

void DeviceRuntimeController::readHilog(const QString& filter, int maxLines)
{
    runOperation(
        tr("Read hilog"),
        backend_.hilogRequest(currentTargetId()),
        [this, filter, maxLines](const CommandResult& result) {
            const QString combinedOutput = result.standardOutput + result.standardError;
            if (!result.succeeded() && combinedOutput.trimmed().isEmpty()) {
                return;
            }
            hilogText_ = HdcDeviceBackend::filterHilog(
                combinedOutput,
                filter,
                maxLines);
            if (hilogText_.isEmpty()) {
                hilogText_ = tr("[No hilog lines matched]");
            }
            emit hilogTextChanged();
            setStatus(result.succeeded() ? tr("Hilog captured.") : tr("Hilog captured before timeout."));
        });
}

void DeviceRuntimeController::captureScreenshot()
{
    captureScreenshot(ScreenshotRequestKind::Manual);
}

void DeviceRuntimeController::captureScreenshot(ScreenshotRequestKind kind)
{
    if (busy_) {
        if (kind == ScreenshotRequestKind::AutoRefresh) {
            setScreenRefreshStatus(tr("Waiting for current device operation..."));
        } else {
            setErrorMessage(tr("Another device operation is still running."));
        }
        return;
    }

    const QString remotePath = QStringLiteral("/data/local/tmp/reark-screenshot.jpeg");
    const QString localPath = makeLocalScreenshotPath();
    const bool autoRefresh = kind == ScreenshotRequestKind::AutoRefresh;
    setBusy(true, autoRefresh ? tr("Refresh screen") : tr("Capture screenshot"));
    setErrorMessage({});

    activeCommandId_ = runner_.run(
        backend_.screenshotCaptureRequest(remotePath, currentTargetId()),
        [this, remotePath, localPath, autoRefresh](const CommandResult& captureResult) {
            appendCommandLog(captureResult);
            if (!captureResult.succeeded()) {
                activeCommandId_ = 0;
                const QString message = tr("Screenshot capture failed.");
                if (autoRefresh && screenRefreshTimer_.isActive()) {
                    setScreenRefreshStatus(message);
                }
                setErrorMessage(message);
                setBusy(false);
                return;
            }

            activeCommandId_ = runner_.run(
                backend_.receiveFileRequest(remotePath, localPath, currentTargetId()),
                [this, remotePath, localPath, autoRefresh](const CommandResult& receiveResult) {
                    appendCommandLog(receiveResult);
                    runner_.run(backend_.removeRemoteFileRequest(remotePath, currentTargetId()), [](const CommandResult&) {});
                    activeCommandId_ = 0;
                    if (receiveResult.succeeded() && QFileInfo::exists(localPath)) {
                        screenshotPath_ = localPath;
                        emit screenshotPathChanged();
                        if (autoRefresh && screenRefreshTimer_.isActive()) {
                            ++screenRefreshFrameCount_;
                            const qint64 elapsedMs = screenRefreshElapsed_.isValid()
                                ? screenRefreshElapsed_.elapsed()
                                : 0;
                            screenRefreshFps_ = elapsedMs > 0
                                ? (screenRefreshFrameCount_ * 1000.0 / double(elapsedMs))
                                : 0.0;
                            emit screenRefreshFrameChanged();
                            setScreenRefreshStatus(tr("Auto refresh: %1 frame(s), %2 FPS.")
                                    .arg(screenRefreshFrameCount_)
                                    .arg(screenRefreshFps_, 0, 'f', 1));
                        } else {
                            setStatus(tr("Screenshot saved: %1").arg(localPath));
                        }
                    } else {
                        const QString message = tr("Screenshot download failed: %1").arg(localPath);
                        if (autoRefresh && screenRefreshTimer_.isActive()) {
                            setScreenRefreshStatus(message);
                        }
                        setErrorMessage(message);
                    }
                    setBusy(false);
                });
        });
}

void DeviceRuntimeController::captureUiSnapshot(const QString& bundleName)
{
    if (busy_) {
        setErrorMessage(tr("Another device operation is still running."));
        return;
    }

    const QString screenshotRemotePath = QStringLiteral("/data/local/tmp/reark-screenshot.jpeg");
    const QString layoutRemotePath = QStringLiteral("/data/local/tmp/reark-ui-layout.json");
    const QString screenshotLocalPath = makeLocalScreenshotPath();
    const QString layoutLocalPath = makeLocalLayoutPath();
    const QString targetId = currentTargetId();
    setBusy(true, tr("Capture UI snapshot"));
    setErrorMessage({});

    activeCommandId_ = runner_.run(
        backend_.screenshotCaptureRequest(screenshotRemotePath, targetId),
        [this, screenshotRemotePath, layoutRemotePath, screenshotLocalPath, layoutLocalPath, targetId, bundleName](const CommandResult& captureResult) {
            appendCommandLog(captureResult);
            if (!captureResult.succeeded()) {
                activeCommandId_ = 0;
                setErrorMessage(tr("Screenshot capture failed."));
                setBusy(false);
                return;
            }

            activeCommandId_ = runner_.run(
                backend_.receiveFileRequest(screenshotRemotePath, screenshotLocalPath, targetId),
                [this, screenshotRemotePath, layoutRemotePath, screenshotLocalPath, layoutLocalPath, targetId, bundleName](const CommandResult& screenshotReceiveResult) {
                    appendCommandLog(screenshotReceiveResult);
                    runner_.run(backend_.removeRemoteFileRequest(screenshotRemotePath, targetId), [](const CommandResult&) {});
                    if (!screenshotReceiveResult.succeeded() || !QFileInfo::exists(screenshotLocalPath)) {
                        activeCommandId_ = 0;
                        setErrorMessage(tr("Screenshot download failed: %1").arg(screenshotLocalPath));
                        setBusy(false);
                        return;
                    }

                    screenshotPath_ = screenshotLocalPath;
                    emit screenshotPathChanged();

                    activeCommandId_ = runner_.run(
                        uiBackend_.dumpLayoutRequest(layoutRemotePath, targetId, bundleName),
                        [this, layoutRemotePath, layoutLocalPath, targetId](const CommandResult& dumpResult) {
                            appendCommandLog(dumpResult);
                            if (!dumpResult.succeeded()) {
                                activeCommandId_ = 0;
                                setErrorMessage(tr("UI layout dump failed."));
                                setBusy(false);
                                return;
                            }

                            activeCommandId_ = runner_.run(
                                backend_.receiveFileRequest(layoutRemotePath, layoutLocalPath, targetId),
                                [this, layoutRemotePath, layoutLocalPath, targetId](const CommandResult& layoutReceiveResult) {
                                    appendCommandLog(layoutReceiveResult);
                                    runner_.run(backend_.removeRemoteFileRequest(layoutRemotePath, targetId), [](const CommandResult&) {});
                                    activeCommandId_ = 0;
                                    if (!layoutReceiveResult.succeeded() || !QFileInfo::exists(layoutLocalPath)) {
                                        setErrorMessage(tr("UI layout download failed."));
                                        setBusy(false);
                                        return;
                                    }

                                    QFile file(layoutLocalPath);
                                    if (!file.open(QIODevice::ReadOnly)) {
                                        setErrorMessage(tr("Could not read UI layout: %1").arg(layoutLocalPath));
                                        setBusy(false);
                                        return;
                                    }

                                    const QByteArray data = file.readAll();
                                    setUiNodes(UiAutomationBackend::parseLayout(data));
                                    setStatus(tr("UI snapshot captured: %n node(s).", nullptr, parsedUiNodes_.size()));
                                    setBusy(false);
                                });
                        });
                });
        });
}

void DeviceRuntimeController::refreshUiLayout(const QString& bundleName)
{
    if (busy_) {
        setErrorMessage(tr("Another device operation is still running."));
        return;
    }

    const QString remotePath = QStringLiteral("/data/local/tmp/reark-ui-layout.json");
    const QString localPath = makeLocalLayoutPath();
    setBusy(true, tr("Refresh UI layout"));
    setErrorMessage({});

    activeCommandId_ = runner_.run(
        uiBackend_.dumpLayoutRequest(remotePath, currentTargetId(), bundleName),
        [this, remotePath, localPath](const CommandResult& dumpResult) {
            appendCommandLog(dumpResult);
            if (!dumpResult.succeeded()) {
                activeCommandId_ = 0;
                setErrorMessage(tr("UI layout dump failed."));
                setBusy(false);
                return;
            }

            activeCommandId_ = runner_.run(
                backend_.receiveFileRequest(remotePath, localPath, currentTargetId()),
                [this, remotePath, localPath](const CommandResult& receiveResult) {
                    appendCommandLog(receiveResult);
                    runner_.run(backend_.removeRemoteFileRequest(remotePath, currentTargetId()), [](const CommandResult&) {});
                    activeCommandId_ = 0;
                    if (!receiveResult.succeeded() || !QFileInfo::exists(localPath)) {
                        setErrorMessage(tr("UI layout download failed."));
                        setBusy(false);
                        return;
                    }

                    QFile file(localPath);
                    if (!file.open(QIODevice::ReadOnly)) {
                        setErrorMessage(tr("Could not read UI layout: %1").arg(localPath));
                        setBusy(false);
                        return;
                    }

                    const QByteArray data = file.readAll();
                    setUiNodes(UiAutomationBackend::parseLayout(data));
                    setStatus(tr("UI layout captured: %n node(s).", nullptr, parsedUiNodes_.size()));
                    setBusy(false);
                });
        });
}

void DeviceRuntimeController::tapUi(int x, int y)
{
    runOperation(
        tr("Tap UI"),
        uiBackend_.tapRequest(x, y, currentTargetId()),
        [this, x, y](const CommandResult& result) {
            if (result.succeeded()) {
                setStatus(tr("Tapped (%1, %2).").arg(x).arg(y));
            }
        });
}

void DeviceRuntimeController::tapUiNode(int nodeIndex)
{
    if (nodeIndex < 0 || nodeIndex >= parsedUiNodes_.size()) {
        setErrorMessage(tr("UI node is not available."));
        return;
    }
    const QPoint point = parsedUiNodes_.at(nodeIndex).center();
    tapUi(point.x(), point.y());
}

void DeviceRuntimeController::inputUiTextAt(int x, int y, const QString& text)
{
    if (text.isEmpty()) {
        setErrorMessage(tr("Text is required."));
        return;
    }
    runOperation(
        tr("Input UI text"),
        uiBackend_.inputTextAtRequest(x, y, text, currentTargetId()));
}

void DeviceRuntimeController::inputFocusedUiText(const QString& text)
{
    if (text.isEmpty()) {
        setErrorMessage(tr("Text is required."));
        return;
    }
    runOperation(
        tr("Input focused text"),
        uiBackend_.inputFocusedTextRequest(text, currentTargetId()));
}

void DeviceRuntimeController::pressDeviceKey(const QString& key)
{
    const QString trimmed = key.trimmed();
    if (trimmed.isEmpty()) {
        setErrorMessage(tr("Key is required."));
        return;
    }
    runOperation(
        tr("Press key"),
        uiBackend_.keyEventRequest(trimmed, currentTargetId()));
}

void DeviceRuntimeController::swipeDevice(int fromX, int fromY, int toX, int toY, int velocity)
{
    runOperation(
        tr("Swipe UI"),
        uiBackend_.swipeRequest(fromX, fromY, toX, toY, currentTargetId(), velocity));
}

void DeviceRuntimeController::startScreenRefresh(int intervalMs)
{
    if (screenRefreshTimer_.isActive()) {
        setScreenRefreshStatus(tr("Screen auto refresh is already running."));
        return;
    }

    const int boundedIntervalMs = std::clamp(
        intervalMs,
        kMinScreenRefreshIntervalMs,
        kMaxScreenRefreshIntervalMs);
    screenRefreshFrameCount_ = 0;
    screenRefreshFps_ = 0.0;
    screenRefreshElapsed_.restart();
    screenRefreshTimer_.start(boundedIntervalMs);
    setScreenRefreshStatus(tr("Screen auto refresh started."));
    emit screenRefreshStateChanged();
    emit screenRefreshFrameChanged();
    captureAutoRefreshFrame();
}

void DeviceRuntimeController::stopScreenRefresh()
{
    if (!screenRefreshTimer_.isActive()) {
        setScreenRefreshStatus(tr("Screen refresh stopped."));
        return;
    }

    screenRefreshTimer_.stop();
    setScreenRefreshStatus(tr("Screen refresh stopped."));
    emit screenRefreshStateChanged();
}

void DeviceRuntimeController::captureAutoRefreshFrame()
{
    if (!screenRefreshTimer_.isActive()) {
        return;
    }
    captureScreenshot(ScreenshotRequestKind::AutoRefresh);
}

void DeviceRuntimeController::cancel()
{
    if (activeCommandId_ != 0) {
        runner_.cancel(activeCommandId_);
    }
}

void DeviceRuntimeController::clearOutput()
{
    if (!commandLog_.isEmpty()) {
        commandLog_.clear();
        emit commandLogChanged();
    }
    if (!hilogText_.isEmpty()) {
        hilogText_.clear();
        emit hilogTextChanged();
    }
    if (!uiNodes_.isEmpty() || !uiNodeSummary_.isEmpty() || !parsedUiNodes_.isEmpty()) {
        uiNodes_.clear();
        filteredUiNodes_.clear();
        parsedUiNodes_.clear();
        uiNodeSummary_.clear();
        emit uiNodesChanged();
    }
    setErrorMessage({});
}

void DeviceRuntimeController::runOperation(
    const QString& operation,
    const CommandRequest& request,
    std::function<void(const CommandResult&)> onFinished)
{
    if (busy_) {
        setErrorMessage(tr("Another device operation is still running."));
        return;
    }

    setBusy(true, operation);
    setErrorMessage({});
    activeCommandId_ = runner_.run(request, [this, operation, onFinished = std::move(onFinished)](const CommandResult& result) {
        activeCommandId_ = 0;
        appendCommandLog(result);
        if (result.succeeded()) {
            setStatus(tr("%1 completed.").arg(operation));
        } else {
            setErrorMessage(tr("%1 failed.").arg(operation));
        }
        if (onFinished) {
            onFinished(result);
        }
        setBusy(false);
    });
}

void DeviceRuntimeController::appendCommandLog(const CommandResult& result)
{
    if (!commandLog_.isEmpty()) {
        commandLog_ += QStringLiteral("\n\n");
    }
    commandLog_ += HdcDeviceBackend::resultSummary(result);
    emit commandLogChanged();
}

void DeviceRuntimeController::setBusy(bool busy, const QString& operation)
{
    if (busy_ == busy && activeOperation_ == operation) {
        return;
    }
    busy_ = busy;
    activeOperation_ = busy ? operation : QString();
    emit busyChanged();
}

void DeviceRuntimeController::setStatus(const QString& status)
{
    if (status_ == status) {
        return;
    }
    status_ = status;
    emit statusChanged();
}

void DeviceRuntimeController::setErrorMessage(const QString& errorMessage)
{
    if (errorMessage_ == errorMessage) {
        return;
    }
    errorMessage_ = errorMessage;
    emit errorMessageChanged();
    if (!errorMessage_.isEmpty()) {
        setStatus(errorMessage_);
    }
}

void DeviceRuntimeController::setScreenRefreshStatus(const QString& status)
{
    if (screenRefreshStatus_ == status) {
        return;
    }
    screenRefreshStatus_ = status;
    emit screenRefreshStatusChanged();
}

void DeviceRuntimeController::setDevices(const QList<HdcDeviceTarget>& targets)
{
    devices_ = HdcDeviceBackend::targetsToVariantList(targets);
    if (!targets.isEmpty()
        && (selectedDeviceId_.isEmpty()
            || std::none_of(targets.cbegin(), targets.cend(), [this](const HdcDeviceTarget& target) {
                return target.id == selectedDeviceId_;
            }))) {
        selectedDeviceId_ = targets.first().id;
        emit selectedDeviceChanged();
    } else if (targets.isEmpty() && !selectedDeviceId_.isEmpty()) {
        selectedDeviceId_.clear();
        emit selectedDeviceChanged();
    }
    emit devicesChanged();
}

void DeviceRuntimeController::setUiNodes(const QList<UiAutomationNode>& nodes)
{
    parsedUiNodes_ = nodes;
    uiNodes_ = UiAutomationBackend::nodesToVariantList(parsedUiNodes_);
    uiNodeSummary_ = UiAutomationBackend::nodesSummary(parsedUiNodes_);
    refreshFilteredUiNodes();
    emit uiNodesChanged();
}

void DeviceRuntimeController::refreshFilteredUiNodes()
{
    const QString filter = uiNodeFilter_.trimmed();
    if (filter.isEmpty()) {
        filteredUiNodes_ = uiNodes_;
        return;
    }

    const QList<UiAutomationNode> matches = UiAutomationBackend::findNodes(parsedUiNodes_, filter, 500);
    filteredUiNodes_ = UiAutomationBackend::nodesToVariantList(matches);
}

QString DeviceRuntimeController::currentTargetId() const
{
    return selectedDeviceId_.trimmed();
}

QString DeviceRuntimeController::makeLocalScreenshotPath() const
{
    QDir dir(QDir::tempPath());
    dir.mkpath(QStringLiteral("ReArk"));
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    return dir.filePath(QStringLiteral("ReArk/reark-device-%1.jpeg").arg(timestamp));
}

QString DeviceRuntimeController::makeLocalLayoutPath() const
{
    QDir dir(QDir::tempPath());
    dir.mkpath(QStringLiteral("ReArk"));
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    return dir.filePath(QStringLiteral("ReArk/reark-ui-layout-%1.json").arg(timestamp));
}
