#include "device/UiAutomationBackend.h"

#include <QCoreApplication>
#include <QTextStream>

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

    HdcDeviceBackend deviceBackend;
    UiAutomationBackend backend(deviceBackend);

    const CommandRequest dumpRequest = backend.dumpLayoutRequest(
        QStringLiteral("/data/local/tmp/reark-layout.json"),
        QStringLiteral("target-1"),
        QStringLiteral("com.example.app"));
    if (dumpRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("shell"),
            QStringLiteral("uitest"),
            QStringLiteral("dumpLayout"),
            QStringLiteral("-p"),
            QStringLiteral("/data/local/tmp/reark-layout.json"),
            QStringLiteral("-b"),
            QStringLiteral("com.example.app") })) {
        return fail(QStringLiteral("dumpLayout argv changed unexpectedly"));
    }

    const CommandRequest tapRequest = backend.tapRequest(120, 240, QStringLiteral("target-1"));
    if (tapRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("shell"),
            QStringLiteral("uitest"),
            QStringLiteral("uiInput"),
            QStringLiteral("click"),
            QStringLiteral("120"),
            QStringLiteral("240") })) {
        return fail(QStringLiteral("tap argv changed unexpectedly"));
    }

    const CommandRequest textRequest = backend.inputTextAtRequest(
        10,
        20,
        QStringLiteral("hello\nworld"),
        QStringLiteral("target-1"));
    if (textRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("shell"),
            QStringLiteral("uitest uiInput inputText 10 20 'hello world'") })) {
        return fail(QStringLiteral("inputText should pass one quoted remote shell command to hdc shell"));
    }

    const QString specialText = QStringLiteral("u}\"xJ x\"K|y<z\\$a'b");
    const CommandRequest specialTextRequest = backend.inputFocusedTextRequest(
        specialText,
        QStringLiteral("target-1"));
    if (specialTextRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("shell"),
            QStringLiteral("uitest uiInput text 'u}\"xJ x\"K|y<z\\$a'\\''b'") })) {
        return fail(QStringLiteral("focused input text should protect shell-special characters for hdc shell uitest"));
    }

    const CommandRequest uinputTextRequest = backend.uinputKeyboardTextRequest(
        specialText,
        QStringLiteral("target-1"));
    if (uinputTextRequest.arguments != QStringList({
            QStringLiteral("-t"),
            QStringLiteral("target-1"),
            QStringLiteral("shell"),
            QStringLiteral("uinput -K -t 'u}\"xJ x\"K|y<z\\$a'\\''b'") })) {
        return fail(QStringLiteral("uinput text should protect shell-special characters for hdc shell"));
    }

    const QByteArray layout = R"json(
{
  "attributes": {"bounds":"[0,0][100,100]","type":"Root","visible":"true"},
  "children": [
    {
      "attributes": {
        "bounds":"[10,20][110,120]",
        "clickable":"true",
        "enabled":"true",
        "focused":"false",
        "id":"login_button",
        "key":"login_button",
        "text":"Login",
        "type":"Button",
        "visible":"true"
      },
      "children": []
    },
    {
      "attributes": {
        "bounds":"[10,140][210,190]",
        "clickable":"false",
        "enabled":"true",
        "focused":"true",
        "id":"password",
        "text":"",
        "type":"TextInput",
        "visible":"true"
      },
      "children": []
    }
  ]
}
)json";

    const QList<UiAutomationNode> nodes = UiAutomationBackend::parseLayout(layout);
    if (nodes.size() != 3) {
        return fail(QStringLiteral("layout parser should keep useful root and child nodes"));
    }

    const QList<UiAutomationNode> matches = UiAutomationBackend::findNodes(
        nodes,
        QStringLiteral("login"));
    if (matches.size() != 1 || matches.first().text != QStringLiteral("Login")) {
        return fail(QStringLiteral("selector search should match text/id/key/type fields"));
    }

    const QPoint center = matches.first().center();
    if (center.x() != 60 || center.y() != 70) {
        return fail(QStringLiteral("node center should be derived from bounds"));
    }

    const QVariantList variants = UiAutomationBackend::nodesToVariantList(matches);
    if (variants.size() != 1 || variants.first().toMap().value(QStringLiteral("clickable")).toBool() != true) {
        return fail(QStringLiteral("node variant should expose action metadata"));
    }
    if (variants.first().toMap().value(QStringLiteral("left")).toInt() != 10
        || variants.first().toMap().value(QStringLiteral("top")).toInt() != 20
        || variants.first().toMap().value(QStringLiteral("width")).toInt() != 100
        || variants.first().toMap().value(QStringLiteral("height")).toInt() != 100) {
        return fail(QStringLiteral("node variant should expose numeric bounds"));
    }

    UiNodeSelector selector;
    selector.text = QStringLiteral("Login");
    selector.exactText = true;
    selector.type = QStringLiteral("button");
    selector.clickable = true;
    selector.enabled = true;
    selector.visible = true;
    const QList<UiAutomationNode> structuredMatches = UiAutomationBackend::findNodes(nodes, selector);
    if (structuredMatches.size() != 1 || structuredMatches.first().id != QStringLiteral("login_button")) {
        return fail(QStringLiteral("structured selector should combine text/type/state constraints"));
    }

    selector.text = QStringLiteral("log");
    selector.exactText = true;
    if (!UiAutomationBackend::findNodes(nodes, selector).isEmpty()) {
        return fail(QStringLiteral("exact text selector should not match substrings"));
    }

    UiNodeSelector inputSelector;
    inputSelector.id = QStringLiteral("pass");
    inputSelector.type = QStringLiteral("TextInput");
    inputSelector.visible = true;
    const QList<UiAutomationNode> inputMatches = UiAutomationBackend::findNodes(nodes, inputSelector);
    if (inputMatches.size() != 1 || !inputMatches.first().focused) {
        return fail(QStringLiteral("structured selector should match id contains plus type"));
    }

    QTextStream(stdout) << "UI automation backend tests passed\n";
    return 0;
}
