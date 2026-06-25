#include "device/UiAutomationBackend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <utility>

namespace {

bool stringFlag(const QJsonObject& attributes, const QString& name)
{
    return attributes.value(name).toString().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}

QString attributeText(const QJsonObject& attributes, const QString& name)
{
    return attributes.value(name).toString().trimmed();
}

QRect parseBounds(const QString& value)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(\[\s*(-?\d+)\s*,\s*(-?\d+)\s*\]\s*\[\s*(-?\d+)\s*,\s*(-?\d+)\s*\])"));
    const QRegularExpressionMatch match = pattern.match(value);
    if (!match.hasMatch()) {
        return {};
    }

    const int left = match.captured(1).toInt();
    const int top = match.captured(2).toInt();
    const int right = match.captured(3).toInt();
    const int bottom = match.captured(4).toInt();
    return QRect(QPoint(left, top), QPoint(right, bottom)).normalized();
}

void flattenNode(const QJsonObject& object, int depth, QList<UiAutomationNode>& nodes)
{
    const QJsonObject attributes = object.value(QStringLiteral("attributes")).toObject();
    UiAutomationNode node;
    node.index = nodes.size();
    node.depth = depth;
    node.text = attributeText(attributes, QStringLiteral("text"));
    node.originalText = attributeText(attributes, QStringLiteral("originalText"));
    node.id = attributeText(attributes, QStringLiteral("id"));
    node.key = attributeText(attributes, QStringLiteral("key"));
    node.type = attributeText(attributes, QStringLiteral("type"));
    node.description = attributeText(attributes, QStringLiteral("description"));
    node.bundleName = attributeText(attributes, QStringLiteral("bundleName"));
    node.abilityName = attributeText(attributes, QStringLiteral("abilityName"));
    node.hierarchy = attributeText(attributes, QStringLiteral("hierarchy"));
    node.bounds = parseBounds(attributeText(attributes, QStringLiteral("bounds")));
    node.clickable = stringFlag(attributes, QStringLiteral("clickable"));
    node.enabled = stringFlag(attributes, QStringLiteral("enabled"));
    node.focused = stringFlag(attributes, QStringLiteral("focused"));
    node.scrollable = stringFlag(attributes, QStringLiteral("scrollable"));
    node.visible = stringFlag(attributes, QStringLiteral("visible"));

    const bool hasUsefulIdentity = !node.text.isEmpty()
        || !node.originalText.isEmpty()
        || !node.id.isEmpty()
        || !node.key.isEmpty()
        || !node.type.isEmpty()
        || node.clickable
        || node.focused
        || node.scrollable;
    if (hasUsefulIdentity) {
        nodes.append(node);
    }

    const QJsonArray children = object.value(QStringLiteral("children")).toArray();
    for (const QJsonValue& child : children) {
        if (child.isObject()) {
            flattenNode(child.toObject(), depth + 1, nodes);
        }
    }
}

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString nodeSearchText(const UiAutomationNode& node)
{
    return QStringList {
        node.text,
        node.originalText,
        node.id,
        node.key,
        node.type,
        node.description,
        node.bundleName,
        node.abilityName
    }.join(QLatin1Char(' ')).toCaseFolded();
}

QString escapeUitestText(QString text)
{
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return text;
}

bool textMatches(const QString& value, const QString& pattern, bool exact)
{
    const QString needle = pattern.trimmed().toCaseFolded();
    if (needle.isEmpty()) {
        return true;
    }

    const QString haystack = value.trimmed().toCaseFolded();
    return exact ? haystack == needle : haystack.contains(needle);
}

bool anyTextMatches(const QStringList& values, const QString& pattern, bool exact)
{
    const QString needle = pattern.trimmed();
    if (needle.isEmpty()) {
        return true;
    }

    return std::any_of(values.cbegin(), values.cend(), [&needle, exact](const QString& value) {
        return textMatches(value, needle, exact);
    });
}

bool nodeMatchesSelector(const UiAutomationNode& node, const UiNodeSelector& selector)
{
    if (!textMatches(nodeSearchText(node), selector.query, false)) {
        return false;
    }
    if (!anyTextMatches({ node.text, node.originalText }, selector.text, selector.exactText)) {
        return false;
    }
    if (!anyTextMatches({ node.id, node.key }, selector.id, selector.exactId)) {
        return false;
    }
    if (!textMatches(node.type, selector.type, selector.exactType)) {
        return false;
    }
    if (selector.clickable.has_value() && node.clickable != *selector.clickable) {
        return false;
    }
    if (selector.enabled.has_value() && node.enabled != *selector.enabled) {
        return false;
    }
    if (selector.visible.has_value() && node.visible != *selector.visible) {
        return false;
    }
    return true;
}

} // namespace

QPoint UiAutomationNode::center() const
{
    return bounds.center();
}

QString UiAutomationNode::label() const
{
    if (!text.isEmpty()) {
        return text;
    }
    if (!originalText.isEmpty()) {
        return originalText;
    }
    if (!id.isEmpty()) {
        return id;
    }
    if (!key.isEmpty()) {
        return key;
    }
    return type;
}

QVariantMap UiAutomationNode::toVariantMap() const
{
    const QPoint point = center();
    QVariantMap item;
    item.insert(QStringLiteral("index"), index);
    item.insert(QStringLiteral("depth"), depth);
    item.insert(QStringLiteral("text"), text);
    item.insert(QStringLiteral("originalText"), originalText);
    item.insert(QStringLiteral("id"), id);
    item.insert(QStringLiteral("key"), key);
    item.insert(QStringLiteral("type"), type);
    item.insert(QStringLiteral("description"), description);
    item.insert(QStringLiteral("bundleName"), bundleName);
    item.insert(QStringLiteral("abilityName"), abilityName);
    item.insert(QStringLiteral("hierarchy"), hierarchy);
    item.insert(QStringLiteral("bounds"), QStringLiteral("[%1,%2][%3,%4]")
        .arg(bounds.left())
        .arg(bounds.top())
        .arg(bounds.right())
        .arg(bounds.bottom()));
    item.insert(QStringLiteral("left"), bounds.left());
    item.insert(QStringLiteral("top"), bounds.top());
    item.insert(QStringLiteral("right"), bounds.right());
    item.insert(QStringLiteral("bottom"), bounds.bottom());
    item.insert(QStringLiteral("width"), std::max(0, bounds.right() - bounds.left()));
    item.insert(QStringLiteral("height"), std::max(0, bounds.bottom() - bounds.top()));
    item.insert(QStringLiteral("centerX"), point.x());
    item.insert(QStringLiteral("centerY"), point.y());
    item.insert(QStringLiteral("clickable"), clickable);
    item.insert(QStringLiteral("enabled"), enabled);
    item.insert(QStringLiteral("focused"), focused);
    item.insert(QStringLiteral("scrollable"), scrollable);
    item.insert(QStringLiteral("visible"), visible);
    item.insert(QStringLiteral("label"), label());
    return item;
}

UiAutomationBackend::UiAutomationBackend(HdcDeviceBackend deviceBackend)
    : deviceBackend_(std::move(deviceBackend))
{
}

CommandRequest UiAutomationBackend::dumpLayoutRequest(
    const QString& remotePath,
    const QString& targetId,
    const QString& bundleName,
    int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("uitest")
              << QStringLiteral("dumpLayout")
              << QStringLiteral("-p")
              << remotePath;
    const QString bundle = bundleName.trimmed();
    if (!bundle.isEmpty()) {
        arguments << QStringLiteral("-b") << bundle;
    }
    return {
        .program = deviceBackend_.resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest UiAutomationBackend::tapRequest(
    int x,
    int y,
    const QString& targetId,
    int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("uitest")
              << QStringLiteral("uiInput")
              << QStringLiteral("click")
              << QString::number(x)
              << QString::number(y);
    return {
        .program = deviceBackend_.resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest UiAutomationBackend::inputTextAtRequest(
    int x,
    int y,
    const QString& text,
    const QString& targetId,
    int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("uitest")
              << QStringLiteral("uiInput")
              << QStringLiteral("inputText")
              << QString::number(x)
              << QString::number(y)
              << escapeUitestText(text);
    return {
        .program = deviceBackend_.resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest UiAutomationBackend::inputFocusedTextRequest(
    const QString& text,
    const QString& targetId,
    int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("uitest")
              << QStringLiteral("uiInput")
              << QStringLiteral("text")
              << escapeUitestText(text);
    return {
        .program = deviceBackend_.resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest UiAutomationBackend::keyEventRequest(
    const QString& key,
    const QString& targetId,
    int timeoutMs) const
{
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("uitest")
              << QStringLiteral("uiInput")
              << QStringLiteral("keyEvent")
              << key.trimmed();
    return {
        .program = deviceBackend_.resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

CommandRequest UiAutomationBackend::swipeRequest(
    int fromX,
    int fromY,
    int toX,
    int toY,
    const QString& targetId,
    int velocity,
    int timeoutMs) const
{
    const int boundedVelocity = std::clamp(velocity, 200, 40000);
    QStringList arguments = targetArguments(targetId);
    arguments << QStringLiteral("shell")
              << QStringLiteral("uitest")
              << QStringLiteral("uiInput")
              << QStringLiteral("swipe")
              << QString::number(fromX)
              << QString::number(fromY)
              << QString::number(toX)
              << QString::number(toY)
              << QString::number(boundedVelocity);
    return {
        .program = deviceBackend_.resolvedProgram(),
        .arguments = arguments,
        .timeoutMs = timeoutMs
    };
}

QList<UiAutomationNode> UiAutomationBackend::parseLayout(const QByteArray& json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject()) {
        return {};
    }

    QList<UiAutomationNode> nodes;
    flattenNode(document.object(), 0, nodes);
    for (int i = 0; i < nodes.size(); ++i) {
        nodes[i].index = i;
    }
    return nodes;
}

QVariantList UiAutomationBackend::nodesToVariantList(const QList<UiAutomationNode>& nodes)
{
    QVariantList result;
    for (const UiAutomationNode& node : nodes) {
        result.append(node.toVariantMap());
    }
    return result;
}

QList<UiAutomationNode> UiAutomationBackend::findNodes(
    const QList<UiAutomationNode>& nodes,
    const QString& query,
    int limit)
{
    UiNodeSelector selector;
    selector.query = query;
    return findNodes(nodes, selector, limit);
}

QList<UiAutomationNode> UiAutomationBackend::findNodes(
    const QList<UiAutomationNode>& nodes,
    const UiNodeSelector& selector,
    int limit)
{
    QList<UiAutomationNode> matches;
    const int boundedLimit = std::clamp(limit <= 0 ? 50 : limit, 1, 500);
    for (const UiAutomationNode& node : nodes) {
        if (nodeMatchesSelector(node, selector)) {
            matches.append(node);
            if (matches.size() >= boundedLimit) {
                break;
            }
        }
    }
    return matches;
}

QString UiAutomationBackend::nodesSummary(const QList<UiAutomationNode>& nodes, int maxNodes)
{
    QStringList lines;
    const int count = std::min(int(nodes.size()), std::max(0, maxNodes));
    for (int i = 0; i < count; ++i) {
        const UiAutomationNode& node = nodes.at(i);
        const QPoint point = node.center();
        lines.append(QStringLiteral("#%1 depth=%2 type=%3 id=%4 text=%5 bounds=[%6,%7][%8,%9] center=(%10,%11) clickable=%12 enabled=%13 visible=%14")
            .arg(node.index)
            .arg(node.depth)
            .arg(node.type.isEmpty() ? QStringLiteral("-") : node.type)
            .arg(node.id.isEmpty() ? QStringLiteral("-") : node.id)
            .arg(node.label().isEmpty() ? QStringLiteral("-") : node.label())
            .arg(node.bounds.left())
            .arg(node.bounds.top())
            .arg(node.bounds.right())
            .arg(node.bounds.bottom())
            .arg(point.x())
            .arg(point.y())
            .arg(boolText(node.clickable))
            .arg(boolText(node.enabled))
            .arg(boolText(node.visible)));
    }
    if (nodes.size() > count) {
        lines.append(QStringLiteral("[truncated: %1 more node(s)]").arg(nodes.size() - count));
    }
    return lines.join(QLatin1Char('\n'));
}

QStringList UiAutomationBackend::targetArguments(const QString& targetId) const
{
    return deviceBackend_.targetArguments(targetId);
}
