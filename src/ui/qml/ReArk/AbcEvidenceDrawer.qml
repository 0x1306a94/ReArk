import QtQuick
import QtQuick.Controls
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
    property bool framed: true
    property bool showHeader: true

    signal closeRequested()

    readonly property string evidenceContent: decompilerController.abcEvidenceContent
    readonly property bool evidenceBusy: decompilerController.abcEvidenceBusy
    readonly property bool hasEvidence: evidenceContent.length > 0

    color: panelColor
    border.width: framed ? 1 : 0
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

    component EvidenceTextField: ReArkTextField {
        property int preferredWidth: 120

        Layout.preferredWidth: preferredWidth
        Layout.fillWidth: preferredWidth <= 0
        Layout.preferredHeight: 30
        radius: 3
        backgroundColor: root.fieldColor
        borderColor: root.dividerColor
        focusBorderColor: root.accentColor
        textColor: Material.foreground
        placeholderColor: root.mutedTextColor
        selectedTextColor: root.darkTheme ? "#ffffff" : "#000000"
        selectionColor: root.darkTheme ? "#1f4d78" : "#b8daf0"
        textPixelSize: 12
        horizontalPadding: 9

        onAccepted: root.requestCurrent()
    }

    component EvidenceSpinBox: SpinBox {
        Layout.preferredWidth: 78
        Layout.preferredHeight: 30
        editable: true
        font.pixelSize: 12
        from: 0
        to: 1000
    }

    component SectionLabel: Label {
        Layout.fillWidth: true
        color: root.mutedTextColor
        font.pixelSize: 11
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }

    component FieldLabel: Label {
        color: root.mutedTextColor
        font.pixelSize: 11
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    component EvidenceButton: AbstractButton {
        id: button

        property string tone: "normal"
        readonly property bool primary: tone === "primary"
        readonly property bool quiet: tone === "quiet"

        Layout.fillWidth: true
        Layout.preferredHeight: primary ? 32 : 30
        leftPadding: 10
        rightPadding: 10
        hoverEnabled: true
        font.pixelSize: 11
        font.weight: primary ? Font.DemiBold : Font.Normal

        background: Rectangle {
            radius: 3
            color: !button.enabled
                   ? (button.quiet ? "transparent" : root.hoverColor)
                   : button.down
                   ? (button.primary ? Qt.darker(root.accentColor, 1.18) : root.hoverColor)
                   : button.primary
                   ? root.accentColor
                   : button.hovered
                   ? root.hoverColor
                   : button.quiet
                   ? "transparent"
                   : root.surfaceColor
            border.width: button.primary || (button.quiet && !button.hovered) ? 0 : 1
            border.color: button.hovered ? root.accentColor : root.dividerColor
        }

        contentItem: Label {
            text: button.text
            color: button.primary ? "#ffffff" : Material.foreground
            opacity: button.enabled ? 1.0 : 0.42
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font: button.font
        }
    }

    component ModeButton: AbstractButton {
        id: button

        property string modeName: ""

        ButtonGroup.group: modeGroup
        checkable: true
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        leftPadding: 8
        rightPadding: 8
        hoverEnabled: true
        text: modeName
        font.pixelSize: 12
        font.weight: checked ? Font.DemiBold : Font.Normal

        background: Rectangle {
            radius: 3
            color: button.checked
                   ? root.selectedColor
                   : button.hovered
                   ? root.hoverColor
                   : "transparent"
            border.width: button.checked || button.hovered ? 1 : 0
            border.color: button.checked ? root.accentColor : root.dividerColor
        }

        contentItem: Label {
            text: button.text
            color: Material.foreground
            opacity: button.enabled ? 1.0 : 0.45
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font: button.font
        }
    }

    component IconButton: ToolButton {
        id: button

        property string iconName: ""
        property int buttonSize: 28
        property int iconSize: 14

        Layout.preferredWidth: buttonSize
        Layout.preferredHeight: buttonSize
        padding: 0
        hoverEnabled: true

        background: Rectangle {
            radius: 3
            color: button.hovered ? root.hoverColor : "transparent"
            border.width: 0
        }

        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                width: button.iconSize
                height: button.iconSize
                name: button.iconName
                color: Material.foreground
                opacity: button.enabled ? 0.82 : 0.28
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.showHeader ? 44 : 0
            visible: root.showHeader
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

                IconButton {
                    iconName: "close"
                    ToolTip.text: qsTr("Close")
                    ToolTip.visible: hovered
                    onClicked: root.closeRequested()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            color: root.surfaceColor
            border.width: 1
            border.color: root.dividerColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Label {
                    Layout.preferredWidth: 42
                    text: qsTr("Task")
                    color: root.mutedTextColor
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    verticalAlignment: Text.AlignVCenter
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

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

                EvidenceButton {
                    Layout.fillWidth: false
                    Layout.preferredWidth: 96
                    tone: "primary"
                    text: root.evidenceBusy ? qsTr("Reading") : qsTr("Run")
                    enabled: !root.evidenceBusy
                    onClicked: root.requestCurrent()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                id: controlRail

                Layout.fillHeight: true
                Layout.minimumWidth: 300
                Layout.preferredWidth: Math.min(350, Math.max(310, root.width * 0.24))
                Layout.maximumWidth: 370
                color: root.panelColor
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 14

                    ScrollView {
                        id: controlScroll

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        Component.onCompleted: {
                            if (contentItem !== null) {
                                contentItem.boundsBehavior = Flickable.StopAtBounds
                                contentItem.boundsMovement = Flickable.StopAtBounds
                            }
                        }

                        ColumnLayout {
                            width: Math.max(controlScroll.availableWidth, 272)
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                SectionLabel {
                                    text: qsTr("Quick scans")
                                }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 6
                                    rowSpacing: 6

                                    EvidenceButton {
                                        text: qsTr("Hashes")
                                        onClicked: root.quickScan("hashes")
                                    }

                                    EvidenceButton {
                                        text: qsTr("Base64")
                                        onClicked: root.quickScan("base64")
                                    }

                                    EvidenceButton {
                                        text: qsTr("Long strings")
                                        onClicked: root.quickScan("long")
                                    }

                                    EvidenceButton {
                                        text: qsTr("ABC tree")
                                        onClicked: {
                                            root.selectMode(treeMode)
                                            decompilerController.requestAbcTreeEvidence(root.normalizedPath(), treeLimitField.value)
                                        }
                                    }

                                    EvidenceButton {
                                        Layout.columnSpan: 2
                                        text: root.currentLiteralOffset().length > 0
                                              ? qsTr("Use current literal")
                                              : qsTr("Find literal candidates")
                                        onClicked: root.useLiteralFromView()
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: root.dividerColor
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                SectionLabel {
                                    text: qsTr("Parameters")
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 7
                                    visible: literalMode.checked

                                    FieldLabel {
                                        text: qsTr("Literal offset")
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
                                    spacing: 7
                                    visible: stringsMode.checked

                                    FieldLabel {
                                        text: qsTr("String pattern")
                                    }

                                    EvidenceTextField {
                                        id: stringPatternField
                                        Layout.fillWidth: true
                                        preferredWidth: 0
                                        placeholderText: qsTr("[0-9a-f]{64}")
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 3
                                        columnSpacing: 6
                                        rowSpacing: 4

                                        FieldLabel {
                                            text: qsTr("Min")
                                        }

                                        FieldLabel {
                                            text: qsTr("Max")
                                        }

                                        FieldLabel {
                                            text: qsTr("Limit")
                                        }

                                        EvidenceSpinBox {
                                            id: minLengthField
                                            Layout.fillWidth: true
                                            value: 8
                                            to: 4096
                                        }

                                        EvidenceSpinBox {
                                            id: maxLengthField
                                            Layout.fillWidth: true
                                            value: 128
                                            to: 16384
                                        }

                                        EvidenceSpinBox {
                                            id: stringLimitField
                                            Layout.fillWidth: true
                                            value: 80
                                            from: 1
                                            to: 1000
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 7
                                    visible: treeMode.checked

                                    FieldLabel {
                                        text: qsTr("Tree limit")
                                    }

                                    EvidenceSpinBox {
                                        id: treeLimitField
                                        Layout.fillWidth: true
                                        value: 200
                                        to: 5000
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 7
                                    visible: xrefsMode.checked || flowsMode.checked

                                    FieldLabel {
                                        text: xrefsMode.checked ? qsTr("XRef query") : qsTr("Flow source")
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
                                        spacing: 7

                                        ComboBox {
                                            id: kindCombo

                                            Layout.fillWidth: true
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

                                        EvidenceSpinBox {
                                            id: resultLimitField
                                            Layout.preferredWidth: 82
                                            value: 80
                                            from: 1
                                            to: 1000
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: root.dividerColor
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                SectionLabel {
                                    text: qsTr("Input")
                                }

                                FieldLabel {
                                    text: qsTr("ABC path")
                                }

                                EvidenceTextField {
                                    id: pathField
                                    Layout.fillWidth: true
                                    preferredWidth: 0
                                    text: "modules.abc"
                                    placeholderText: qsTr("modules.abc")
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: root.dividerColor
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: root.surfaceColor

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        Layout.leftMargin: 16
                        Layout.rightMargin: 12
                        spacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Evidence")
                                color: Material.foreground
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: decompilerController.abcEvidenceTitle.length > 0
                                      ? decompilerController.abcEvidenceTitle
                                      : qsTr("Run a query to collect ABC evidence")
                                color: root.mutedTextColor
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                visible: text.length > 0
                            }
                        }

                        BusyIndicator {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22
                            running: root.evidenceBusy
                            visible: running
                        }

                        IconButton {
                            iconName: "copy"
                            enabled: root.hasEvidence && !root.evidenceBusy
                            ToolTip.text: qsTr("Copy Evidence")
                            ToolTip.visible: hovered
                            onClicked: decompilerController.copyTextToClipboard(root.evidenceContent)
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: root.dividerColor
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 14
                        color: root.darkTheme ? "#141516" : "#fbfcfd"
                        border.width: 1
                        border.color: root.dividerColor
                        radius: 3
                        clip: true

                        CodeView {
                            anchors.fill: parent
                            visible: root.hasEvidence || root.evidenceBusy
                            code: root.evidenceContent
                            highlightTheme: root.highlightTheme
                            syntax: "Markdown"
                        }

                        Column {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 48, 360)
                            spacing: 6
                            visible: !root.hasEvidence && !root.evidenceBusy

                            Label {
                                width: parent.width
                                text: qsTr("No evidence collected.")
                                color: Material.foreground
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                width: parent.width
                                text: qsTr("Choose a task or preset, then run the query.")
                                color: root.mutedTextColor
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
