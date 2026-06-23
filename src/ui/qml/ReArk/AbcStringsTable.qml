import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    property var rows: []
    property bool darkTheme: false
    property color editorColor: darkTheme ? "#171819" : "#ffffff"
    property color panelColor: darkTheme ? "#1b1d20" : "#eef2f4"
    property color dividerColor: darkTheme ? "#34383d" : "#d5dcdf"
    property color hoverColor: darkTheme ? "#24282d" : "#edf4f5"
    property color selectedColor: darkTheme ? "#243546" : "#d9edf0"
    property color mutedTextColor: darkTheme ? "#9aa1a9" : "#5f6872"
    property color fieldColor: darkTheme ? "#151719" : "#ffffff"
    property color accentColor: darkTheme ? "#5ca6d6" : "#2f80a8"
    property string presetFilter: "all"
    property int selectedRow: -1
    property var contextRow: ({})
    property var xrefRows: []
    property bool xrefsBusy: false
    property string xrefsQuery: ""
    property string xrefsError: ""
    property var contextXrefRow: ({})

    readonly property int offsetWidth: 96
    readonly property int lengthWidth: 58
    readonly property int sourceWidth: 96
    readonly property int xrefBytecodeWidth: 112
    readonly property int xrefOperandWidth: 56

    signal findXrefsRequested(string query, string kind)
    signal traceFlowRequested(string query, string kind)
    signal openXrefEvidenceRequested(string query, string kind)
    signal navigateXrefRequested(var row)
    signal clearXrefsRequested()

    color: editorColor
    clip: true

    function filteredRows() {
        const query = filterField.text.trim().toLowerCase()
        if (query.length === 0 && presetFilter === "all") {
            return root.rows
        }

        const result = []
        for (let i = 0; i < root.rows.length; ++i) {
            const row = root.rows[i]
            const value = row.value || ""
            if (presetFilter === "hash" && !/^[0-9a-fA-F]{32,64}$/.test(value)) {
                continue
            }
            if (presetFilter === "base64" && !/^[A-Za-z0-9+/]{24,}={0,2}$/.test(value)) {
                continue
            }
            if (presetFilter === "long" && value.length < 20) {
                continue
            }
            const haystack = [
                row.offset || "",
                row.sourceKind || row.source_kind || "",
                row.type || "",
                row.classification || "",
                row.context || "",
                value
            ].join("\n").toLowerCase()
            if (query.length === 0 || haystack.indexOf(query) >= 0) {
                result.push(row)
            }
        }
        return result
    }

    function copyRowValue(row) {
        decompilerController.copyTextToClipboard(row.value || "")
    }

    function copyRowOffset(row) {
        decompilerController.copyTextToClipboard(row.offset || "")
    }

    function sourceFor(row) {
        return row.sourceKind || row.source_kind || row.type || ""
    }

    function copyRowTsv(row) {
        decompilerController.copyTextToClipboard([
            row.offset || "",
            row.length === undefined ? "" : row.length,
            sourceFor(row),
            row.value || ""
        ].join("\t"))
    }

    function copyXrefRow(row) {
        decompilerController.copyTextToClipboard([
            row.location || "",
            row.targetText || row.targetOffset || "",
            row.instructionOffset || "",
            row.operandIndex === undefined ? "" : row.operandIndex
        ].join("\t"))
    }

    function openXrefContextMenu(row, owner, x, y) {
        contextXrefRow = row || {}
        xrefContextMenu.popup(owner, x, y)
    }

    function xrefTarget(row) {
        return row.targetText || row.targetOffset || ""
    }

    function xrefDetail(row) {
        if ((row.targetText || "").length > 0 && (row.targetOffset || "").length > 0) {
            return row.targetOffset
        }
        return row.kind || ""
    }

    function xrefSourceHint(row) {
        return row.sourceFile || row.className || ""
    }

    function xrefOperandText(row) {
        if (row.operandIndex === undefined || row.operandIndex < 0) {
            return ""
        }
        return row.operandIndex
    }

    function openContextMenu(row, rowIndex, owner, x, y) {
        selectedRow = rowIndex
        contextRow = row || {}
        stringsContextMenu.popup(owner, x, y)
    }

    function hasXrefPanel() {
        return xrefsBusy
                || xrefsQuery.length > 0
                || xrefRows.length > 0
                || xrefsError.length > 0
    }

    component HeaderLabel: Label {
        color: root.mutedTextColor
        font.pixelSize: 11
        font.weight: Font.DemiBold
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    component PresetButton: ToolButton {
        id: button

        property string preset: "all"
        property string label: ""

        Layout.preferredWidth: 54
        Layout.preferredHeight: 28
        padding: 0
        text: label
        checkable: true
        checked: root.presetFilter === preset
        font.pixelSize: 11
        font.weight: checked ? Font.DemiBold : Font.Normal
        onClicked: root.presetFilter = preset

        background: Rectangle {
            radius: 3
            color: button.checked
                   ? root.selectedColor
                   : button.hovered ? root.hoverColor : "transparent"
            border.width: button.checked ? 1 : 0
            border.color: root.accentColor
        }
    }

    component CellLabel: Label {
        color: Material.foreground
        font.pixelSize: 12
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: panelColor
            border.width: 1
            border.color: dividerColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Label {
                    text: qsTr("Strings")
                    color: Material.foreground
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    Layout.preferredWidth: 260
                    Layout.preferredHeight: 28
                    radius: 3
                    color: root.fieldColor
                    border.width: 1
                    border.color: filterField.activeFocus ? root.accentColor : root.dividerColor

                    Basic.TextField {
                        id: filterField

                        anchors.fill: parent
                        anchors.leftMargin: 9
                        anchors.rightMargin: 9
                        selectByMouse: true
                        placeholderText: qsTr("Filter")
                        color: Material.foreground
                        placeholderTextColor: root.mutedTextColor
                        selectedTextColor: root.darkTheme ? "#ffffff" : "#000000"
                        selectionColor: root.darkTheme ? "#1f4d78" : "#b8daf0"
                        verticalAlignment: TextInput.AlignVCenter
                        font.pixelSize: 12
                        background: null
                    }
                }

                RowLayout {
                    Layout.preferredHeight: 28
                    spacing: 2

                    PresetButton {
                        preset: "all"
                        label: qsTr("All")
                    }

                    PresetButton {
                        preset: "hash"
                        label: qsTr("Hash")
                    }

                    PresetButton {
                        preset: "base64"
                        label: qsTr("Base64")
                    }

                    PresetButton {
                        preset: "long"
                        label: qsTr("Long")
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("%1 / %2").arg(stringsList.count).arg(root.rows.length)
                    color: mutedTextColor
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: root.darkTheme ? "#1c1f22" : "#f5f8f9"
            border.width: 1
            border.color: dividerColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                HeaderLabel {
                    Layout.preferredWidth: root.offsetWidth
                    text: qsTr("Offset")
                }

                HeaderLabel {
                    Layout.preferredWidth: root.lengthWidth
                    text: qsTr("Len")
                }

                HeaderLabel {
                    Layout.preferredWidth: root.sourceWidth
                    text: qsTr("Source")
                }

                HeaderLabel {
                    Layout.fillWidth: true
                    text: qsTr("Value")
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: stringsList

                anchors.fill: parent
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.filteredRows()

                delegate: Rectangle {
                    id: rowDelegate

                    required property var modelData
                    required property int index

                    width: stringsList.width
                    height: 30
                    color: index === root.selectedRow
                           ? root.selectedColor
                           : rowMouse.containsMouse
                           ? root.hoverColor
                           : index % 2 === 0 ? "transparent" : (root.darkTheme ? "#191b1e" : "#fbfcfd")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        CellLabel {
                            Layout.preferredWidth: root.offsetWidth
                            text: rowDelegate.modelData.offset || ""
                            color: root.accentColor
                        }

                        CellLabel {
                            Layout.preferredWidth: root.lengthWidth
                            text: rowDelegate.modelData.length === undefined ? "" : rowDelegate.modelData.length
                            color: root.mutedTextColor
                        }

                        CellLabel {
                            Layout.preferredWidth: root.sourceWidth
                            text: root.sourceFor(rowDelegate.modelData)
                            color: root.mutedTextColor
                        }

                        CellLabel {
                            Layout.fillWidth: true
                            text: rowDelegate.modelData.value || ""
                            font.family: "Consolas"
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: function(mouse) {
                            root.selectedRow = rowDelegate.index
                            if (mouse.button === Qt.RightButton) {
                                root.openContextMenu(rowDelegate.modelData, rowDelegate.index, rowDelegate, mouse.x, mouse.y)
                            }
                        }
                        onDoubleClicked: function(mouse) {
                            if (mouse.button === Qt.LeftButton) {
                                root.copyRowValue(rowDelegate.modelData)
                            }
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            CompactMenu {
                id: stringsContextMenu
                minimumItemWidth: 190

                Action {
                    text: qsTr("Copy Value")
                    enabled: (root.contextRow.value || "").length > 0
                    onTriggered: root.copyRowValue(root.contextRow)
                }

                Action {
                    text: qsTr("Copy Offset")
                    enabled: (root.contextRow.offset || "").length > 0
                    onTriggered: root.copyRowOffset(root.contextRow)
                }

                Action {
                    text: qsTr("Copy Row")
                    enabled: (root.contextRow.offset || "").length > 0
                              || (root.contextRow.value || "").length > 0
                    onTriggered: root.copyRowTsv(root.contextRow)
                }

                CompactMenuSeparator {}

                Action {
                    text: qsTr("Find XRefs")
                    enabled: (root.contextRow.value || "").length > 0
                    onTriggered: root.findXrefsRequested(root.contextRow.value || "", "string")
                }

                Action {
                    text: qsTr("Open XRef Evidence")
                    enabled: (root.contextRow.value || "").length > 0
                    onTriggered: root.openXrefEvidenceRequested(root.contextRow.value || "", "string")
                }

                Action {
                    text: qsTr("Trace Call Args Evidence")
                    enabled: (root.contextRow.value || "").length > 0
                    onTriggered: root.traceFlowRequested(root.contextRow.value || "", "string")
                }
            }

            Label {
                anchors.centerIn: parent
                visible: stringsList.count === 0
                text: root.rows.length === 0 ? qsTr("Loading strings") : qsTr("No strings match")
                color: mutedTextColor
                font.pixelSize: 13
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.hasXrefPanel()
                                    ? Math.min(236, Math.max(150, root.height * 0.32))
                                    : 0
            visible: root.hasXrefPanel()
            color: root.darkTheme ? "#181b1e" : "#f7fafb"
            border.width: 1
            border.color: root.dividerColor
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    color: root.darkTheme ? "#1d2024" : "#eef3f5"
                    border.width: 1
                    border.color: root.dividerColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        spacing: 8

                        Label {
                            text: qsTr("XRefs")
                            color: Material.foreground
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.xrefsBusy
                                  ? qsTr("Searching %1").arg(root.xrefsQuery)
                                  : root.xrefsError.length > 0
                                  ? root.xrefsError
                                  : root.xrefsQuery.length > 0
                                  ? qsTr("%1 result(s) for %2").arg(root.xrefRows.length).arg(root.xrefsQuery)
                                  : ""
                            color: root.xrefsError.length > 0 ? "#c45b55" : root.mutedTextColor
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                            verticalAlignment: Text.AlignVCenter
                        }

                        BusyIndicator {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            running: root.xrefsBusy
                            visible: running
                        }

                        ToolButton {
                            Layout.preferredWidth: 62
                            Layout.preferredHeight: 24
                            padding: 0
                            text: qsTr("Evidence")
                            enabled: root.xrefsQuery.length > 0
                            font.pixelSize: 11
                            ToolTip.text: qsTr("Open Raw XRef Evidence")
                            ToolTip.visible: hovered
                            onClicked: root.openXrefEvidenceRequested(root.xrefsQuery, "string")
                        }

                        ToolButton {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 24
                            padding: 0
                            text: "×"
                            font.pixelSize: 15
                            ToolTip.text: qsTr("Close XRefs")
                            ToolTip.visible: hovered
                            onClicked: root.clearXrefsRequested()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    color: root.darkTheme ? "#1a1d20" : "#f3f7f8"
                    border.width: 1
                    border.color: root.dividerColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        HeaderLabel {
                            Layout.fillWidth: true
                            text: qsTr("Reference")
                        }

                        HeaderLabel {
                            Layout.fillWidth: true
                            text: qsTr("Target")
                        }

                        HeaderLabel {
                            Layout.preferredWidth: root.xrefBytecodeWidth
                            text: qsTr("Bytecode")
                        }

                        HeaderLabel {
                            Layout.preferredWidth: root.xrefOperandWidth
                            text: qsTr("Index")
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: xrefsList

                        anchors.fill: parent
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: root.xrefRows

                        delegate: Rectangle {
                            id: xrefDelegate

                            required property var modelData
                            required property int index

                            width: xrefsList.width
                            height: 42
                            color: xrefMouse.containsMouse
                                   ? root.hoverColor
                                   : index % 2 === 0 ? "transparent" : (root.darkTheme ? "#191b1e" : "#fbfcfd")

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 260
                                    spacing: 1

                                    CellLabel {
                                        Layout.fillWidth: true
                                        text: xrefDelegate.modelData.location || ""
                                        font.weight: Font.DemiBold
                                    }

                                    CellLabel {
                                        Layout.fillWidth: true
                                        text: root.xrefSourceHint(xrefDelegate.modelData)
                                        color: root.mutedTextColor
                                        font.pixelSize: 10
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 220
                                    spacing: 1

                                    CellLabel {
                                        Layout.fillWidth: true
                                        text: root.xrefTarget(xrefDelegate.modelData)
                                        font.family: "Consolas"
                                    }

                                    CellLabel {
                                        Layout.fillWidth: true
                                        text: root.xrefDetail(xrefDelegate.modelData)
                                        color: root.mutedTextColor
                                        font.pixelSize: 10
                                    }
                                }

                                CellLabel {
                                    Layout.preferredWidth: root.xrefBytecodeWidth
                                    text: xrefDelegate.modelData.instructionOffset || ""
                                    color: root.accentColor
                                    font.family: "Consolas"
                                }

                                CellLabel {
                                    Layout.preferredWidth: root.xrefOperandWidth
                                    text: root.xrefOperandText(xrefDelegate.modelData)
                                    color: root.mutedTextColor
                                    horizontalAlignment: Text.AlignRight
                                }
                            }

                            MouseArea {
                                id: xrefMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        root.openXrefContextMenu(xrefDelegate.modelData, xrefDelegate, mouse.x, mouse.y)
                                    }
                                }
                                onDoubleClicked: function(mouse) {
                                    if (mouse.button === Qt.LeftButton) {
                                        root.navigateXrefRequested(xrefDelegate.modelData)
                                    }
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }
                    }

                    CompactMenu {
                        id: xrefContextMenu
                        minimumItemWidth: 178

                        Action {
                            text: qsTr("Go to Source")
                            enabled: (root.contextXrefRow.location || "").length > 0
                                     || (root.contextXrefRow.sourceQuery || "").length > 0
                            onTriggered: root.navigateXrefRequested(root.contextXrefRow)
                        }

                        CompactMenuSeparator {}

                        Action {
                            text: qsTr("Copy Location")
                            enabled: (root.contextXrefRow.location || "").length > 0
                            onTriggered: decompilerController.copyTextToClipboard(root.contextXrefRow.location || "")
                        }

                        Action {
                            text: qsTr("Copy Target")
                            enabled: ((root.contextXrefRow.targetText || root.contextXrefRow.targetOffset || "")).length > 0
                            onTriggered: decompilerController.copyTextToClipboard(
                                             root.contextXrefRow.targetText
                                             || root.contextXrefRow.targetOffset
                                             || "")
                        }

                        Action {
                            text: qsTr("Copy Row")
                            enabled: (root.contextXrefRow.location || "").length > 0
                                     || (root.contextXrefRow.targetText || "").length > 0
                            onTriggered: root.copyXrefRow(root.contextXrefRow)
                        }

                        CompactMenuSeparator {}

                        Action {
                            text: qsTr("Open XRef Evidence")
                            enabled: root.xrefsQuery.length > 0
                            onTriggered: root.openXrefEvidenceRequested(root.xrefsQuery, "string")
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: !root.xrefsBusy
                                 && root.xrefsError.length === 0
                                 && root.xrefsQuery.length > 0
                                 && xrefsList.count === 0
                        text: qsTr("No xrefs found")
                        color: root.mutedTextColor
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
