import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    property bool darkTheme: false
    property color panelColor: darkTheme ? "#1b1d20" : "#f6f9fa"
    property color surfaceColor: darkTheme ? "#171819" : "#ffffff"
    property color fieldColor: darkTheme ? "#151719" : "#ffffff"
    property color dividerColor: darkTheme ? "#34383d" : "#d5dcdf"
    property color hoverColor: darkTheme ? "#282b30" : "#e8eef0"
    property color selectedColor: darkTheme ? "#253544" : "#d8ecef"
    property color mutedTextColor: darkTheme ? "#9aa1a9" : "#5f6872"
    property color accentColor: darkTheme ? "#5ca6d6" : "#2f80a8"
    property string highlightTheme: "GitHub Dark"
    property string activePath: ""
    property string activeText: ""

    signal closeRequested()

    readonly property string evidenceContent: decompilerController.abcEvidenceContent
    readonly property bool evidenceBusy: decompilerController.abcEvidenceBusy

    color: panelColor
    border.width: 1
    border.color: dividerColor
    clip: true

    function normalizedPath() {
        return pathField.text.trim().length > 0 ? pathField.text.trim() : "modules.abc"
    }

    function requestCurrent() {
        const path = normalizedPath()
        if (modeGroup.checkedButton === literalMode) {
            decompilerController.requestAbcLiteralEvidence(literalField.text, path)
        } else if (modeGroup.checkedButton === stringsMode) {
            decompilerController.requestAbcStringSearch(
                        stringPatternField.text,
                        minLengthField.value,
                        maxLengthField.value,
                        stringLimitField.value,
                        path)
        } else if (modeGroup.checkedButton === treeMode) {
            decompilerController.requestAbcTreeEvidence(path, treeLimitField.value)
        } else if (modeGroup.checkedButton === xrefsMode) {
            decompilerController.requestAbcXrefs(
                        xrefQueryField.text,
                        kindCombo.currentValue,
                        resultLimitField.value,
                        path)
        } else {
            decompilerController.requestAbcFlows(
                        flowQueryField.text,
                        kindCombo.currentValue,
                        resultLimitField.value,
                        path)
        }
    }

    function selectMode(button) {
        button.checked = true
    }

    function quickScan(kind) {
        selectMode(stringsMode)
        if (kind === "hashes") {
            stringPatternField.text = "[0-9A-Fa-f]{32,64}"
            minLengthField.value = 32
            maxLengthField.value = 128
            stringLimitField.value = 120
            decompilerController.requestAbcStringSearch(
                        stringPatternField.text,
                        minLengthField.value,
                        maxLengthField.value,
                        stringLimitField.value,
                        normalizedPath())
        } else if (kind === "base64") {
            stringPatternField.text = "[A-Za-z0-9+/]{24,}={0,2}"
            minLengthField.value = 24
            maxLengthField.value = 512
            stringLimitField.value = 120
            decompilerController.requestAbcStringSearch(
                        stringPatternField.text,
                        minLengthField.value,
                        maxLengthField.value,
                        stringLimitField.value,
                        normalizedPath())
        } else {
            stringPatternField.text = ""
            minLengthField.value = 20
            maxLengthField.value = 512
            stringLimitField.value = 160
            decompilerController.requestAbcStringSearch(
                        stringPatternField.text,
                        minLengthField.value,
                        maxLengthField.value,
                        stringLimitField.value,
                        normalizedPath())
        }
    }

    function currentLiteralOffset() {
        const match = root.activeText.match(/literal@0x[0-9A-Fa-f]+/m)
        return match ? match[0] : ""
    }

    function useLiteralFromView() {
        const offset = currentLiteralOffset()
        if (offset.length === 0) {
            selectMode(stringsMode)
            quickScan("long")
            return
        }
        selectMode(literalMode)
        literalField.text = offset
        decompilerController.requestAbcLiteralEvidence(literalField.text, normalizedPath())
    }

    function seedFromActivePath(path) {
        if (!path || path.length === 0) {
            return
        }
        const lower = path.toLowerCase()
        if (lower.endsWith(".abc") || lower.indexOf(".abc/") >= 0 || lower.indexOf(".abc\\") >= 0) {
            pathField.text = path
        }
    }

    onActivePathChanged: seedFromActivePath(activePath)

    component EvidenceTextField: Rectangle {
        id: fieldFrame

        property alias text: input.text
        property alias placeholderText: input.placeholderText
        property int preferredWidth: 120

        Layout.preferredWidth: preferredWidth
        Layout.fillWidth: preferredWidth <= 0
        Layout.preferredHeight: 30
        radius: 3
        color: root.fieldColor
        border.width: 1
        border.color: input.activeFocus ? root.accentColor : root.dividerColor

        Basic.TextField {
            id: input

            anchors.fill: parent
            anchors.leftMargin: 9
            anchors.rightMargin: 9
            selectByMouse: true
            color: Material.foreground
            placeholderTextColor: root.mutedTextColor
            selectedTextColor: root.darkTheme ? "#ffffff" : "#000000"
            selectionColor: root.darkTheme ? "#1f4d78" : "#b8daf0"
            verticalAlignment: TextInput.AlignVCenter
            font.pixelSize: 12
            background: null

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.requestCurrent()
                    event.accepted = true
                }
            }
        }
    }

    component EvidenceSpinBox: SpinBox {
        id: spin

        Layout.preferredWidth: 78
        Layout.preferredHeight: 30
        editable: true
        font.pixelSize: 12
        from: 0
        to: 1000
    }

    component ModeButton: ToolButton {
        id: button

        property string modeName: ""

        ButtonGroup.group: modeGroup
        checkable: true
        Layout.fillWidth: true
        Layout.preferredHeight: 30
        padding: 0
        hoverEnabled: true
        text: modeName
        font.pixelSize: 11
        font.weight: checked ? Font.DemiBold : Font.Normal

        background: Rectangle {
            radius: 3
            color: button.checked ? root.selectedColor : button.hovered ? root.hoverColor : "transparent"
            border.width: button.checked ? 1 : 0
            border.color: root.accentColor
        }
    }

    component QuickButton: ToolButton {
        id: button

        property string label: ""

        Layout.fillWidth: true
        Layout.preferredHeight: 30
        padding: 0
        hoverEnabled: true
        text: label
        font.pixelSize: 11
        font.weight: Font.DemiBold

        background: Rectangle {
            radius: 3
            color: button.hovered ? root.hoverColor : root.surfaceColor
            border.width: 1
            border.color: button.hovered ? root.accentColor : root.dividerColor
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: root.darkTheme ? "#181a1d" : "#eef3f5"
            border.width: 1
            border.color: root.dividerColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: 3
                    Layout.preferredHeight: 20
                    radius: 1
                    color: root.accentColor
                }

                Label {
                    Layout.fillWidth: true
                    text: decompilerController.abcEvidenceTitle.length > 0
                          ? decompilerController.abcEvidenceTitle
                          : qsTr("ABC Evidence")
                    color: Material.foreground
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                BusyIndicator {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    running: root.evidenceBusy
                    visible: running
                }

                ToolButton {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    padding: 0
                    text: "×"
                    font.pixelSize: 16
                    ToolTip.text: qsTr("Close")
                    ToolTip.visible: hovered
                    onClicked: root.closeRequested()
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 308
            clip: true

            ColumnLayout {
                width: Math.max(root.width - 18, 280)
                spacing: 9

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Layout.topMargin: 12
                    spacing: 7

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Quick scan")
                        color: root.mutedTextColor
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        QuickButton {
                            label: qsTr("Hashes")
                            onClicked: root.quickScan("hashes")
                        }

                        QuickButton {
                            label: qsTr("Base64")
                            onClicked: root.quickScan("base64")
                        }

                        QuickButton {
                            label: qsTr("Long")
                            onClicked: root.quickScan("long")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        QuickButton {
                            label: qsTr("Tree")
                            onClicked: {
                                root.selectMode(treeMode)
                                decompilerController.requestAbcTreeEvidence(root.normalizedPath(), treeLimitField.value)
                            }
                        }

                        QuickButton {
                            label: root.currentLiteralOffset().length > 0
                                   ? qsTr("Resolve literal")
                                   : qsTr("Find candidates")
                            onClicked: root.useLiteralFromView()
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Layout.topMargin: 12
                    spacing: 7

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Target")
                        color: root.mutedTextColor
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    EvidenceTextField {
                        id: pathField
                        Layout.fillWidth: true
                        preferredWidth: 0
                        text: "modules.abc"
                        placeholderText: qsTr("modules.abc")
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 4

                    ButtonGroup {
                        id: modeGroup
                    }

                    ModeButton {
                        id: literalMode
                        modeName: qsTr("Literal")
                        checked: true
                    }

                    ModeButton {
                        id: stringsMode
                        modeName: qsTr("Strings")
                    }

                    ModeButton {
                        id: treeMode
                        modeName: qsTr("Tree")
                    }

                    ModeButton {
                        id: xrefsMode
                        modeName: qsTr("XRefs")
                    }

                    ModeButton {
                        id: flowsMode
                        modeName: qsTr("Flows")
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Layout.preferredHeight: 1
                    color: root.dividerColor
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 8
                    visible: literalMode.checked

                    Label {
                        text: qsTr("Offset")
                        color: root.mutedTextColor
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    EvidenceTextField {
                        id: literalField
                        Layout.fillWidth: true
                        preferredWidth: 0
                        placeholderText: qsTr("literal@0x5757 or 0x5757")
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 8
                    visible: stringsMode.checked

                    Label {
                        text: qsTr("Pattern")
                        color: root.mutedTextColor
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    EvidenceTextField {
                        id: stringPatternField
                        Layout.fillWidth: true
                        preferredWidth: 0
                        placeholderText: qsTr("[0-9a-f]{64}")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: qsTr("Min")
                            color: root.mutedTextColor
                            font.pixelSize: 11
                        }

                        EvidenceSpinBox {
                            id: minLengthField
                            value: 8
                            to: 4096
                        }

                        Label {
                            text: qsTr("Max")
                            color: root.mutedTextColor
                            font.pixelSize: 11
                        }

                        EvidenceSpinBox {
                            id: maxLengthField
                            value: 128
                            to: 16384
                        }

                        Label {
                            text: qsTr("Limit")
                            color: root.mutedTextColor
                            font.pixelSize: 11
                        }

                        EvidenceSpinBox {
                            id: stringLimitField
                            value: 80
                            from: 1
                            to: 1000
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 8
                    visible: treeMode.checked

                    Label {
                        text: qsTr("Tree limit")
                        color: root.mutedTextColor
                        font.pixelSize: 11
                    }

                    EvidenceSpinBox {
                        id: treeLimitField
                        value: 200
                        to: 5000
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 8
                    visible: xrefsMode.checked || flowsMode.checked

                    Label {
                        text: xrefsMode.checked ? qsTr("XRef query") : qsTr("Flow source")
                        color: root.mutedTextColor
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    EvidenceTextField {
                        id: xrefQueryField
                        Layout.fillWidth: true
                        preferredWidth: 0
                        visible: xrefsMode.checked
                        placeholderText: qsTr("passwordHash, 0x5757, method name")
                    }

                    EvidenceTextField {
                        id: flowQueryField
                        Layout.fillWidth: true
                        preferredWidth: 0
                        visible: flowsMode.checked
                        placeholderText: qsTr("literal offset or string")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ComboBox {
                            id: kindCombo

                            Layout.preferredWidth: 112
                            Layout.preferredHeight: 30
                            textRole: "text"
                            valueRole: "value"
                            font.pixelSize: 12
                            model: [
                                { text: qsTr("Any"), value: "any" },
                                { text: qsTr("String"), value: "string" },
                                { text: qsTr("Literal"), value: "literal" },
                                { text: qsTr("Method"), value: "method" }
                            ]
                        }

                        Label {
                            text: qsTr("Limit")
                            color: root.mutedTextColor
                            font.pixelSize: 11
                        }

                        EvidenceSpinBox {
                            id: resultLimitField
                            value: 80
                            from: 1
                            to: 1000
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Layout.bottomMargin: 12
                    spacing: 8

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        text: root.evidenceBusy ? qsTr("Reading") : qsTr("Run")
                        enabled: !root.evidenceBusy
                        onClicked: root.requestCurrent()
                    }

                    ToolButton {
                        Layout.preferredWidth: 58
                        Layout.preferredHeight: 32
                        text: qsTr("Copy")
                        enabled: root.evidenceContent.length > 0 && !root.evidenceBusy
                        ToolTip.text: qsTr("Copy Evidence")
                        ToolTip.visible: hovered
                        onClicked: decompilerController.copyTextToClipboard(root.evidenceContent)
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.dividerColor
        }

        CodeView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            code: root.evidenceContent.length > 0
                  ? root.evidenceContent
                  : qsTr("# Pick Hashes, Base64, Long, Tree, or Resolve literal")
            highlightTheme: root.highlightTheme
            syntax: "Markdown"
        }
    }
}
