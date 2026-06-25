import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    property var controller: null
    property string packagePath: ""
    property string packageName: ""
    property int selectedUiNodeIndex: -1
    readonly property bool darkTheme: Material.theme === Material.Dark
    readonly property color pageColor: darkTheme ? "#1e1e1e" : "#f5f7f8"
    readonly property color panelColor: darkTheme ? "#202226" : "#ffffff"
    readonly property color headerColor: darkTheme ? "#1b1d20" : "#eef2f4"
    readonly property color inputColor: darkTheme ? "#171819" : "#ffffff"
    readonly property color dividerColor: darkTheme ? "#34383d" : "#d5dcdf"
    readonly property color focusColor: darkTheme ? "#3f8fd2" : "#2f80c1"
    readonly property color secondaryTextColor: darkTheme ? "#a6a6a6" : "#5f6872"
    readonly property color dangerColor: darkTheme ? "#f48771" : "#a1260d"

    signal backRequested()

    color: pageColor

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: headerColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                RuntimeButton {
                    text: qsTr("Back")
                    Layout.preferredWidth: 54
                    implicitHeight: 28
                    tone: "quiet"
                    onClicked: root.backRequested()
                }

                Label {
                    text: qsTr("Device Runtime")
                    color: Material.foreground
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    Layout.fillWidth: true
                    text: root.packageName.length > 0 ? root.packageName : qsTr("No package loaded")
                    color: secondaryTextColor
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                    verticalAlignment: Text.AlignVCenter
                }

                RuntimeComboBox {
                    id: deviceCombo

                    Layout.preferredWidth: 260
                    model: root.controller !== null ? root.controller.devices : []
                    textRole: "display"
                    valueRole: "id"
                    enabled: root.controller !== null && !root.controller.busy
                    onActivated: {
                        if (root.controller !== null) {
                            root.controller.selectedDeviceId = currentValue
                        }
                    }
                }

                RuntimeButton {
                    text: qsTr("Refresh")
                    Layout.preferredWidth: 86
                    implicitHeight: 30
                    enabled: root.controller !== null && !root.controller.busy
                    onClicked: root.controller.refreshDevices()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: dividerColor
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: pageColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        CheckBox {
                            id: overlayCheck
                            text: qsTr("Overlay")
                            checked: true
                            enabled: root.controller !== null
                                     && root.controller.uiNodes.length > 0
                                     && root.screenImageSource().length > 0
                            font.pixelSize: 12
                        }

                        Label {
                            text: root.controller !== null
                                  ? qsTr("%1 node(s)").arg(root.controller.uiNodes.length)
                                  : qsTr("0 node(s)")
                            color: secondaryTextColor
                            font.pixelSize: 12
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.selectedUiNodeIndex >= 0
                                  ? qsTr("selected node #%1").arg(root.selectedUiNodeIndex)
                                  : qsTr("no node selected")
                            color: secondaryTextColor
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        id: screenSurface

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: inputColor
                        border.width: 1
                        border.color: dividerColor
                        clip: true

                        Image {
                            id: screenshotImage

                            anchors.fill: parent
                            anchors.margins: 12
                            fillMode: Image.PreserveAspectFit
                            source: root.screenImageSource()
                            asynchronous: true
                            cache: false
                        }

                        Repeater {
                            model: root.controller !== null && overlayCheck.checked
                                   ? root.controller.filteredUiNodes
                                   : []

                            delegate: Rectangle {
                                required property var modelData
                                required property int index

                                readonly property bool selected: root.selectedUiNodeIndex === modelData.index

                                visible: modelData.visible
                                         && modelData.width > 0
                                         && modelData.height > 0
                                         && screenshotImage.status === Image.Ready
                                x: root.overlayX(modelData.left)
                                y: root.overlayY(modelData.top)
                                width: Math.max(2, modelData.width * root.overlayScale())
                                height: Math.max(2, modelData.height * root.overlayScale())
                                color: selected
                                       ? (root.darkTheme ? "#2f80c166" : "#2f80c144")
                                       : "transparent"
                                border.width: selected ? 2 : 1
                                border.color: selected
                                              ? root.focusColor
                                              : (modelData.clickable ? "#d18f28" : (root.darkTheme ? "#7f8a92" : "#7b8790"))
                                radius: 1

                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton
                                    hoverEnabled: true
                                    onClicked: root.selectedUiNodeIndex = modelData.index
                                    onDoubleClicked: {
                                        root.selectedUiNodeIndex = modelData.index
                                        if (root.controller !== null && !root.controller.busy) {
                                            root.controller.tapUiNode(modelData.index)
                                        }
                                    }
                                }
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: root.screenImageSource().length === 0
                            text: qsTr("Capture a snapshot or start auto refresh to inspect the device screen")
                            color: secondaryTextColor
                            font.pixelSize: 14
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.controller !== null && root.controller.screenRefreshRunning
                              ? root.controller.screenRefreshStatus
                              : root.controller !== null ? root.controller.screenshotPath : ""
                        color: secondaryTextColor
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: dividerColor
            }

            Rectangle {
                Layout.preferredWidth: 390
                Layout.fillHeight: true
                color: panelColor

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Flickable {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(contentHeight + 18, parent.height * 0.54)
                        clip: true
                        contentWidth: width
                        contentHeight: controlsColumn.implicitHeight + 18
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        ColumnLayout {
                            id: controlsColumn

                            width: parent.width - 28
                            x: 14
                            y: 12
                            spacing: 12

                            SectionCaption {
                                text: qsTr("Package")
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                RuntimeButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Install")
                                    tone: "primary"
                                    enabled: root.controller !== null
                                             && !root.controller.busy
                                             && root.packagePath.length > 0
                                    onClicked: root.controller.installPackage(root.packagePath)
                                }

                                RuntimeButton {
                                    text: qsTr("Cancel")
                                    Layout.preferredWidth: 92
                                    tone: "quiet"
                                    enabled: root.controller !== null && root.controller.busy
                                    onClicked: root.controller.cancel()
                                }
                            }

                            SectionCaption {
                                text: qsTr("Launch")
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 8
                                rowSpacing: 8

                                Label {
                                    text: qsTr("Bundle")
                                    color: secondaryTextColor
                                    font.pixelSize: 12
                                }

                                RuntimeTextField {
                                    id: bundleField
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("com.example.app")
                                }

                                Label {
                                    text: qsTr("Ability")
                                    color: secondaryTextColor
                                    font.pixelSize: 12
                                }

                                RuntimeTextField {
                                    id: abilityField
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("EntryAbility")
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                RuntimeButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Start")
                                    tone: "primary"
                                    enabled: root.controller !== null
                                             && !root.controller.busy
                                             && bundleField.text.trim().length > 0
                                    onClicked: root.controller.startAbility(bundleField.text, abilityField.text)
                                }

                                RuntimeButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Screenshot")
                                    enabled: root.controller !== null && !root.controller.busy
                                    onClicked: root.controller.captureScreenshot()
                                }

                                RuntimeButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Snapshot")
                                    enabled: root.controller !== null && !root.controller.busy
                                    onClicked: root.controller.captureUiSnapshot(bundleField.text)
                                }
                            }

                            SectionCaption {
                                text: qsTr("Inspect")
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                RuntimeButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Layout")
                                    enabled: root.controller !== null && !root.controller.busy
                                    onClicked: root.controller.refreshUiLayout(bundleField.text)
                                }

                                RuntimeButton {
                                    text: qsTr("Back")
                                    Layout.preferredWidth: 74
                                    enabled: root.controller !== null && !root.controller.busy
                                    onClicked: root.controller.pressDeviceKey("Back")
                                }

                                RuntimeButton {
                                    text: qsTr("Home")
                                    Layout.preferredWidth: 74
                                    enabled: root.controller !== null && !root.controller.busy
                                    onClicked: root.controller.pressDeviceKey("Home")
                                }
                            }

                            SectionCaption {
                                text: qsTr("Screen")
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                RuntimeButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Refresh")
                                    enabled: root.controller !== null && !root.controller.busy
                                    onClicked: root.controller.captureScreenshot()
                                }

                                RuntimeButton {
                                    Layout.fillWidth: true
                                    text: root.controller !== null && root.controller.screenRefreshRunning
                                          ? qsTr("Refreshing")
                                          : qsTr("Auto Refresh")
                                    tone: "primary"
                                    enabled: root.controller !== null
                                             && !root.controller.busy
                                             && !root.controller.screenRefreshRunning
                                    onClicked: root.controller.startScreenRefresh()
                                }

                                RuntimeButton {
                                    Layout.preferredWidth: 92
                                    text: qsTr("Stop")
                                    tone: "quiet"
                                    enabled: root.controller !== null
                                             && root.controller.screenRefreshRunning
                                    onClicked: root.controller.stopScreenRefresh()
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.controller !== null
                                      ? root.controller.screenRefreshStatus
                                      : qsTr("Screen refresh stopped.")
                                color: secondaryTextColor
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            SectionCaption {
                                text: qsTr("Interact")
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 4
                                columnSpacing: 8
                                rowSpacing: 8

                                RuntimeTextField {
                                    id: tapXField
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("x")
                                    validator: IntValidator { bottom: 0; top: 100000 }
                                }

                                RuntimeTextField {
                                    id: tapYField
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("y")
                                    validator: IntValidator { bottom: 0; top: 100000 }
                                }

                                RuntimeButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Tap")
                                    enabled: root.controller !== null
                                             && !root.controller.busy
                                             && tapXField.acceptableInput
                                             && tapYField.acceptableInput
                                    onClicked: root.controller.tapUi(parseInt(tapXField.text), parseInt(tapYField.text))
                                }

                                RuntimeButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Tap Node")
                                    enabled: root.controller !== null
                                             && !root.controller.busy
                                             && root.selectedUiNodeIndex >= 0
                                    onClicked: root.controller.tapUiNode(root.selectedUiNodeIndex)
                                }

                                RuntimeTextField {
                                    id: uiTextField
                                    Layout.columnSpan: 3
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("text")
                                }

                                RuntimeButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Type")
                                    enabled: root.controller !== null
                                             && !root.controller.busy
                                             && uiTextField.text.length > 0
                                    onClicked: {
                                        if (tapXField.acceptableInput && tapYField.acceptableInput) {
                                            root.controller.inputUiTextAt(parseInt(tapXField.text), parseInt(tapYField.text), uiTextField.text)
                                        } else {
                                            root.controller.inputFocusedUiText(uiTextField.text)
                                        }
                                    }
                                }
                            }

                            SectionCaption {
                                text: qsTr("Evidence")
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                RuntimeTextField {
                                    id: hilogFilterField
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("hilog filter")
                                }

                                RuntimeButton {
                                    text: qsTr("Hilog")
                                    Layout.preferredWidth: 82
                                    enabled: root.controller !== null && !root.controller.busy
                                    onClicked: root.controller.readHilog(hilogFilterField.text, 800)
                                }
                            }

                            RuntimeTextField {
                                id: uiNodeFilterField
                                Layout.fillWidth: true
                                placeholderText: qsTr("filter UI text / id / type")
                                onTextChanged: {
                                    if (root.controller !== null) {
                                        root.controller.uiNodeFilter = text
                                    }
                                }
                            }

                            SectionCaption {
                                text: qsTr("Status")
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.controller !== null ? root.controller.status : ""
                                color: root.controller !== null && root.controller.errorMessage.length > 0
                                       ? dangerColor
                                       : secondaryTextColor
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            ProgressBar {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 3
                                indeterminate: root.controller !== null && root.controller.busy
                                visible: root.controller !== null && root.controller.busy
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: dividerColor
                    }

                    TabBar {
                        id: evidenceTabs

                        Layout.fillWidth: true

                        TabButton { text: qsTr("UI") }
                        TabButton { text: qsTr("Commands") }
                        TabButton { text: qsTr("Hilog") }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: evidenceTabs.currentIndex

                        Rectangle {
                            color: inputColor
                            clip: true

                            ListView {
                                anchors.fill: parent
                                clip: true
                                model: root.controller !== null ? root.controller.filteredUiNodes : []
                                boundsBehavior: Flickable.StopAtBounds

                                delegate: Rectangle {
                                    required property var modelData
                                    required property int index

                                    width: ListView.view.width
                                    height: Math.max(36, nodeText.implicitHeight + 12)
                                    color: root.selectedUiNodeIndex === modelData.index
                                           ? (root.darkTheme ? "#2f4a5f" : "#dcecf7")
                                           : inputColor

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: root.selectedUiNodeIndex = modelData.index
                                        onDoubleClicked: {
                                            root.selectedUiNodeIndex = modelData.index
                                            if (root.controller !== null && !root.controller.busy) {
                                                root.controller.tapUiNode(modelData.index)
                                            }
                                        }
                                    }

                                    Text {
                                        id: nodeText
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 10
                                        verticalAlignment: Text.AlignVCenter
                                        color: Material.foreground
                                        font.family: "Consolas"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        text: "#" + modelData.index
                                              + " " + modelData.type
                                              + " " + (modelData.id.length > 0 ? modelData.id : "-")
                                              + "  " + modelData.bounds
                                              + "  " + (modelData.clickable ? "clickable" : "")
                                              + "  " + modelData.label
                                    }
                                }

                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                            }
                        }

                        RuntimeOutput {
                            text: root.controller !== null ? root.controller.commandLog : ""
                        }

                        RuntimeOutput {
                            text: root.controller !== null ? root.controller.hilogText : ""
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: root.controller
        ignoreUnknownSignals: true

        function onDevicesChanged() {
            root.syncSelectedDevice()
        }

        function onSelectedDeviceChanged() {
            root.syncSelectedDevice()
        }

        function onUiNodesChanged() {
            root.selectedUiNodeIndex = -1
        }
    }

    component SectionCaption: Label {
        Layout.fillWidth: true
        topPadding: 2
        text: ""
        color: root.secondaryTextColor
        font.pixelSize: 11
        font.weight: Font.DemiBold
        verticalAlignment: Text.AlignVCenter
    }

    component RuntimeButton: AbstractButton {
        property string tone: "normal"
        readonly property color baseColor: tone === "primary"
                                           ? (root.darkTheme ? "#23495d" : "#d8eaf3")
                                           : tone === "quiet"
                                           ? "transparent"
                                           : (root.darkTheme ? "#2a2d31" : "#e7ecef")
        readonly property color hoverColor: tone === "primary"
                                            ? (root.darkTheme ? "#2d5e77" : "#c8e2ef")
                                            : tone === "quiet"
                                            ? (root.darkTheme ? "#2a2d31" : "#e7ecef")
                                            : (root.darkTheme ? "#34383d" : "#dce4e8")
        readonly property color pressColor: tone === "primary"
                                            ? (root.darkTheme ? "#1d3d4f" : "#b8d8e8")
                                            : (root.darkTheme ? "#202327" : "#cfd9de")

        implicitHeight: 30
        implicitWidth: Math.max(72, contentItem.implicitWidth + 26)
        padding: 0
        enabled: true

        contentItem: Label {
            text: parent.text
            color: parent.enabled
                   ? Material.foreground
                   : root.secondaryTextColor
            opacity: parent.enabled ? 1.0 : 0.54
            font.pixelSize: 12
            font.weight: parent.tone === "primary" ? Font.DemiBold : Font.Normal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 3
            color: !parent.enabled
                   ? (root.darkTheme ? "#222529" : "#edf1f3")
                   : parent.down
                   ? parent.pressColor
                   : parent.hovered
                   ? parent.hoverColor
                   : parent.baseColor
            border.width: parent.tone === "quiet" && !parent.hovered ? 0 : 1
            border.color: !parent.enabled
                          ? "transparent"
                          : parent.tone === "primary"
                          ? root.focusColor
                          : root.dividerColor
        }
    }

    component RuntimeComboBox: ComboBox {
        id: combo

        implicitHeight: 30
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0

        contentItem: Text {
            leftPadding: 10
            rightPadding: 26
            text: combo.displayText.length > 0 ? combo.displayText : qsTr("No device")
            color: combo.enabled ? Material.foreground : root.secondaryTextColor
            opacity: combo.enabled ? 1.0 : 0.54
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        indicator: Canvas {
            id: comboChevron

            x: combo.width - width - 10
            y: (combo.height - height) / 2
            width: 9
            height: 6
            opacity: combo.enabled ? 1.0 : 0.45

            Connections {
                target: combo.popup

                function onVisibleChanged() {
                    comboChevron.requestPaint()
                }
            }

            onPaint: {
                const context = getContext("2d")
                context.reset()
                context.lineWidth = 1.6
                context.lineCap = "round"
                context.lineJoin = "round"
                context.strokeStyle = root.secondaryTextColor
                context.beginPath()
                if (combo.popup.visible) {
                    context.moveTo(1, 5)
                    context.lineTo(width / 2, 1)
                    context.lineTo(width - 1, 5)
                } else {
                    context.moveTo(1, 1)
                    context.lineTo(width / 2, 5)
                    context.lineTo(width - 1, 1)
                }
                context.stroke()
            }
        }

        background: Rectangle {
            radius: 3
            color: root.inputColor
            border.width: 1
            border.color: combo.activeFocus || combo.popup.visible ? root.focusColor : root.dividerColor
        }

        delegate: ItemDelegate {
            id: comboDelegate

            required property int index
            required property var model
            required property var modelData

            width: combo.width
            height: 28
            padding: 0
            highlighted: combo.highlightedIndex === index

            contentItem: Text {
                leftPadding: 10
                rightPadding: 10
                text: combo.textRole.length > 0 && comboDelegate.model[combo.textRole] !== undefined
                      ? comboDelegate.model[combo.textRole]
                      : comboDelegate.modelData
                color: comboDelegate.enabled ? Material.foreground : root.secondaryTextColor
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                color: comboDelegate.highlighted
                       ? (root.darkTheme ? "#263947" : "#dcecf7")
                       : comboDelegate.hovered
                       ? (root.darkTheme ? "#24272b" : "#edf3f6")
                       : root.inputColor
            }
        }

        popup: Popup {
            y: combo.height + 4
            width: combo.width
            implicitHeight: Math.min(contentItem.implicitHeight + 2, 220)
            padding: 1
            modal: false
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

            contentItem: ListView {
                implicitHeight: contentHeight
                clip: true
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            background: Rectangle {
                radius: 3
                color: root.inputColor
                border.width: 1
                border.color: root.focusColor
            }
        }
    }

    component RuntimeTextField: TextField {
        implicitHeight: 30
        font.pixelSize: 12
        selectByMouse: true
        color: Material.foreground
        placeholderTextColor: root.secondaryTextColor
        background: Rectangle {
            radius: 3
            color: root.inputColor
            border.width: 1
            border.color: parent.activeFocus ? root.focusColor : root.dividerColor
        }
    }

    component RuntimeOutput: Rectangle {
        property alias text: outputText.text

        color: root.inputColor
        border.width: 1
        border.color: root.dividerColor
        clip: true

        ScrollView {
            anchors.fill: parent
            clip: true

            TextArea {
                id: outputText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.NoWrap
                color: Material.foreground
                selectedTextColor: "#ffffff"
                selectionColor: root.focusColor
                font.family: "Consolas"
                font.pixelSize: 11
                background: Item {}
            }
        }
    }

    function screenImageSource() {
        return screenshotUrl()
    }

    function screenshotUrl() {
        if (controller === null || controller.screenshotPath.length === 0) {
            return ""
        }
        let path = controller.screenshotPath.replace(/\\/g, "/")
        if (!path.startsWith("/")) {
            path = "/" + path
        }
        return "file://" + path
    }

    function overlayScale() {
        if (screenshotImage.status !== Image.Ready || screenshotImage.sourceSize.width <= 0) {
            return 1
        }
        return screenshotImage.paintedWidth / screenshotImage.sourceSize.width
    }

    function overlayImageLeft() {
        if (screenshotImage.status !== Image.Ready) {
            return 0
        }
        return screenshotImage.x + (screenshotImage.width - screenshotImage.paintedWidth) / 2
    }

    function overlayImageTop() {
        if (screenshotImage.status !== Image.Ready) {
            return 0
        }
        return screenshotImage.y + (screenshotImage.height - screenshotImage.paintedHeight) / 2
    }

    function overlayX(deviceX) {
        return overlayImageLeft() + deviceX * overlayScale()
    }

    function overlayY(deviceY) {
        return overlayImageTop() + deviceY * overlayScale()
    }

    function syncSelectedDevice() {
        if (controller === null) {
            return
        }
        const selected = controller.selectedDeviceId
        for (let i = 0; i < deviceCombo.count; ++i) {
            if (deviceCombo.valueAt(i) === selected) {
                deviceCombo.currentIndex = i
                return
            }
        }
        deviceCombo.currentIndex = deviceCombo.count > 0 ? 0 : -1
    }
}
