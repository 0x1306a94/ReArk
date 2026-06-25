#ifndef REARK_UI_AUTOMATION_BACKEND_H
#define REARK_UI_AUTOMATION_BACKEND_H

#include "core/CommandRunner.h"
#include "device/HdcDeviceBackend.h"

#include <QRect>
#include <QVariantList>

#include <optional>

struct UiAutomationNode {
    int index = -1;
    int depth = 0;
    QString text;
    QString originalText;
    QString id;
    QString key;
    QString type;
    QString description;
    QString bundleName;
    QString abilityName;
    QString hierarchy;
    QRect bounds;
    bool clickable = false;
    bool enabled = false;
    bool focused = false;
    bool scrollable = false;
    bool visible = false;

    [[nodiscard]] QPoint center() const;
    [[nodiscard]] QString label() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

struct UiNodeSelector {
    QString query;
    QString text;
    QString id;
    QString type;
    bool exactText = false;
    bool exactId = false;
    bool exactType = false;
    std::optional<bool> clickable;
    std::optional<bool> enabled;
    std::optional<bool> visible;
};

class UiAutomationBackend {
public:
    explicit UiAutomationBackend(HdcDeviceBackend deviceBackend = {});

    [[nodiscard]] CommandRequest dumpLayoutRequest(
        const QString& remotePath,
        const QString& targetId,
        const QString& bundleName = {},
        int timeoutMs = 10000) const;
    [[nodiscard]] CommandRequest tapRequest(
        int x,
        int y,
        const QString& targetId,
        int timeoutMs = 5000) const;
    [[nodiscard]] CommandRequest inputTextAtRequest(
        int x,
        int y,
        const QString& text,
        const QString& targetId,
        int timeoutMs = 5000) const;
    [[nodiscard]] CommandRequest inputFocusedTextRequest(
        const QString& text,
        const QString& targetId,
        int timeoutMs = 5000) const;
    [[nodiscard]] CommandRequest keyEventRequest(
        const QString& key,
        const QString& targetId,
        int timeoutMs = 5000) const;
    [[nodiscard]] CommandRequest swipeRequest(
        int fromX,
        int fromY,
        int toX,
        int toY,
        const QString& targetId,
        int velocity = 600,
        int timeoutMs = 5000) const;

    [[nodiscard]] static QList<UiAutomationNode> parseLayout(const QByteArray& json);
    [[nodiscard]] static QVariantList nodesToVariantList(const QList<UiAutomationNode>& nodes);
    [[nodiscard]] static QList<UiAutomationNode> findNodes(
        const QList<UiAutomationNode>& nodes,
        const QString& query,
        int limit = 50);
    [[nodiscard]] static QList<UiAutomationNode> findNodes(
        const QList<UiAutomationNode>& nodes,
        const UiNodeSelector& selector,
        int limit = 50);
    [[nodiscard]] static QString nodesSummary(
        const QList<UiAutomationNode>& nodes,
        int maxNodes = 80);

private:
    [[nodiscard]] QStringList targetArguments(const QString& targetId) const;

    HdcDeviceBackend deviceBackend_;
};

#endif // REARK_UI_AUTOMATION_BACKEND_H
