import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    property var controller: null
    property string packagePath: ""
    property string packageName: ""
    property var installablePackages: []
    property string resignInstallDialogTitle: ""
    property string resignInstallDialogMessage: ""
    property string lastAppliedBundleName: ""
    property string lastAppliedAbilityName: ""
    property int selectedUiNodeIndex: -1
    property var activeRuntimeOutput: null
    property string pendingInstallPackagePath: ""
    readonly property bool hasUiEvidence: controller !== null && controller.filteredUiNodes.length > 0
    readonly property bool hasCommandEvidence: controller !== null && controller.commandLog.length > 0
    readonly property bool hasHilogEvidence: controller !== null && controller.hilogText.length > 0
    readonly property bool hasEvidenceDetails: hasUiEvidence || hasCommandEvidence || hasHilogEvidence
    readonly property bool hasSelectedDevice: controller !== null && controller.selectedDeviceId.length > 0
    readonly property bool refreshOperationActive: controller !== null && controller.screenRefreshBusy
    readonly property bool darkTheme: Material.theme === Material.Dark
    readonly property color pageColor: darkTheme ? "#1e1e1e" : "#f5f7f8"
    readonly property color panelColor: darkTheme ? "#202226" : "#ffffff"
    readonly property color headerColor: darkTheme ? "#1b1d20" : "#eef2f4"
    readonly property color inputColor: darkTheme ? "#171819" : "#ffffff"
    readonly property color dividerColor: darkTheme ? "#34383d" : "#d5dcdf"
    readonly property color focusColor: darkTheme ? "#3f8fd2" : "#2f80c1"
    readonly property color secondaryTextColor: darkTheme ? "#a6a6a6" : "#5f6872"
    readonly property color dangerColor: darkTheme ? "#f48771" : "#a1260d"
    readonly property int runtimeActionWidth: 72

    signal backRequested()

    color: pageColor

    Shortcut {
        sequences: [StandardKey.SelectAll]
        context: Qt.ApplicationShortcut
        enabled: root.activeRuntimeOutput !== null && root.activeRuntimeOutput.acceptsKeyboardShortcut()
        onActivated: root.activeRuntimeOutput.selectAllText()
    }

    Shortcut {
        sequences: [StandardKey.Copy]
        context: Qt.ApplicationShortcut
        enabled: root.activeRuntimeOutput !== null && root.activeRuntimeOutput.acceptsKeyboardShortcut()
                 && root.activeRuntimeOutput.hasSelection()
        onActivated: root.activeRuntimeOutput.copySelection()
    }

    onPackagePathChanged: {
        root.lastAppliedBundleName = ""
        root.lastAppliedAbilityName = ""
        root.refreshActiveLaunchMetadata()
    }

    onInstallablePackagesChanged: {
        root.refreshActiveLaunchMetadata()
    }

    onControllerChanged: {
        root.refreshActiveLaunchMetadata()
    }

    Component.onCompleted: {
        root.refreshActiveLaunchMetadata()
    }

    Timer {
        id: deferredInstallTimer
        interval: 120
        repeat: true
        onTriggered: {
            if (root.controller === null || root.pendingInstallPackagePath.length === 0) {
                stop()
                root.pendingInstallPackagePath = ""
                return
            }
            if (root.refreshOperationActive) {
                return
            }
            const packagePath = root.pendingInstallPackagePath
            root.pendingInstallPackagePath = ""
            stop()
            root.controller.installPackage(packagePath)
        }
    }

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
                spacing: 8

                RuntimeIconButton {
                    iconName: "arrow-left"
                    toolTip: qsTr("Back")
                    onClicked: root.backRequested()
                }

                Item {
                    Layout.fillWidth: true
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
                Layout.fillWidth: false
                Layout.fillHeight: true
                Layout.minimumWidth: 360
                Layout.preferredWidth: Math.min(560, root.width * 0.44)
                Layout.maximumWidth: 620
                color: pageColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.controller !== null
                                 && root.controller.uiNodes.length > 0
                                 && root.screenImageSource().length > 0

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
                        id: screenStage

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: pageColor
                        clip: true

                        Rectangle {
                            id: screenSurface

                            readonly property real deviceAspect: screenshotImage.sourceSize.width > 0
                                                              && screenshotImage.sourceSize.height > 0
                                                              ? screenshotImage.sourceSize.width / screenshotImage.sourceSize.height
                                                              : 1260 / 2720

                            anchors.centerIn: parent
                            width: Math.min(parent.width, parent.height * deviceAspect + 24)
                            height: Math.min(parent.height, (width - 24) / deviceAspect + 24)
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
                                width: Math.min(parent.width - 40, 320)
                                visible: root.screenImageSource().length === 0
                                text: qsTr("Capture a snapshot or start auto refresh to inspect the device screen")
                                color: secondaryTextColor
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.controller !== null && root.controller.screenRefreshRunning
                              ? root.controller.screenRefreshStatus
                              : root.screenImageSource().length > 0
                                ? qsTr("Screenshot updated")
                                : ""
                        color: secondaryTextColor
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: dividerColor
            }

            Rectangle {
                Layout.preferredWidth: 480
                Layout.minimumWidth: 420
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: panelColor

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Flickable {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.hasEvidenceDetails
                                                ? Math.min(contentHeight + 18, parent.height * 0.56)
                                                : parent.height
                        clip: true
                        contentWidth: width
                        contentHeight: controlsColumn.implicitHeight + 18
                        boundsBehavior: Flickable.StopAtBounds
                        boundsMovement: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        ColumnLayout {
                            id: controlsColumn

                            width: parent.width - 28
                            x: 14
                            y: 12
                            spacing: 12

                            RuntimeGroup {
                                title: qsTr("Status")

                                Label {
                                    Layout.fillWidth: true
                                    text: root.controller !== null && root.controller.errorMessage.length > 0
                                          ? root.controller.errorMessage
                                          : root.controller !== null ? root.controller.status : ""
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

                            RuntimeGroup {
                                title: qsTr("Screen")

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    FormRowLabel {
                                        text: qsTr("Capture")
                                    }

                                    RuntimeButton {
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 1
                                        text: qsTr("Screenshot")
                                        enabled: root.hasSelectedDevice && !root.controller.busy
                                        onClicked: root.controller.captureScreenshot()
                                    }

                                    RuntimeButton {
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 1
                                        text: qsTr("Inspect UI")
                                        enabled: root.hasSelectedDevice && !root.controller.busy
                                        onClicked: root.controller.captureUiSnapshot(bundleField.text)
                                    }

                                    RuntimeButton {
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 1
                                        text: root.controller !== null && root.controller.screenRefreshRunning
                                              ? qsTr("Refreshing")
                                              : qsTr("Auto Refresh")
                                        tone: "primary"
                                        enabled: root.hasSelectedDevice
                                                 && !root.controller.busy
                                                 && !root.controller.screenRefreshRunning
                                        onClicked: root.controller.startScreenRefresh()
                                    }

                                    RuntimeButton {
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 1
                                        text: qsTr("Stop")
                                        enabled: root.controller !== null
                                                 && root.controller.screenRefreshRunning
                                        onClicked: root.controller.stopScreenRefresh()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    FormRowLabel {
                                        text: qsTr("Keys")
                                    }

                                    RuntimeButton {
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 1
                                        text: qsTr("Back")
                                        enabled: root.hasSelectedDevice && !root.controller.busy
                                        onClicked: root.controller.pressDeviceKey("Back")
                                    }

                                    RuntimeButton {
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 1
                                        text: qsTr("Home")
                                        enabled: root.hasSelectedDevice && !root.controller.busy
                                        onClicked: root.controller.pressDeviceKey("Home")
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 72
                                    text: root.controller !== null
                                          ? root.controller.screenRefreshStatus
                                          : qsTr("Screen refresh stopped.")
                                    visible: root.controller !== null && root.controller.screenRefreshRunning
                                    color: secondaryTextColor
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            RuntimeGroup {
                                title: qsTr("Application")

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    FormRowLabel {
                                        text: qsTr("Package")
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.packageName.length > 0
                                              ? root.packageName
                                              : qsTr("No package loaded")
                                        color: root.packageName.length > 0 ? Material.foreground : secondaryTextColor
                                        font.pixelSize: 12
                                        font.weight: root.packageName.length > 0 ? Font.DemiBold : Font.Normal
                                        elide: Text.ElideMiddle
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    RuntimeButton {
                                        Layout.preferredWidth: root.runtimeActionWidth
                                        text: qsTr("Install")
                                        visible: root.packagePath.length > 0
                                        enabled: root.canInstallActivePackage()
                                        toolTip: root.installUnavailableReason()
                                        onClicked: root.installActivePackage()
                                    }

                                    RuntimeButton {
                                        Layout.preferredWidth: root.runtimeActionWidth
                                        text: qsTr("Cancel")
                                        visible: root.packagePath.length > 0
                                        enabled: root.controller !== null && root.controller.busy
                                        onClicked: root.controller.cancel()
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    FormRowLabel {
                                        text: qsTr("Launch")
                                    }

                                    RuntimeTextField {
                                        id: bundleField
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 230
                                        placeholderText: qsTr("Bundle")
                                    }

                                    RuntimeTextField {
                                        id: abilityField
                                        Layout.preferredWidth: 150
                                        placeholderText: qsTr("Ability")
                                    }

                                    RuntimeButton {
                                        Layout.preferredWidth: root.runtimeActionWidth
                                        text: qsTr("Start")
                                        enabled: root.controller !== null
                                                 && root.hasSelectedDevice
                                                 && !root.controller.busy
                                                 && bundleField.text.trim().length > 0
                                        onClicked: root.controller.startAbility(bundleField.text, abilityField.text)
                                    }
                                }
                            }

                            RuntimeGroup {
                                title: qsTr("Interact")
                                visible: root.controller !== null && root.controller.uiNodes.length > 0

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    FormRowLabel {
                                        text: qsTr("Tap")
                                    }

                                    RuntimeTextField {
                                        id: tapXField
                                        Layout.preferredWidth: 72
                                        placeholderText: qsTr("x")
                                        validator: IntValidator { bottom: 0; top: 100000 }
                                    }

                                    RuntimeTextField {
                                        id: tapYField
                                        Layout.preferredWidth: 72
                                        placeholderText: qsTr("y")
                                        validator: IntValidator { bottom: 0; top: 100000 }
                                    }

                                    RuntimeButton {
                                        Layout.preferredWidth: 72
                                        text: qsTr("Tap")
                                        enabled: root.controller !== null
                                                 && root.hasSelectedDevice
                                                 && !root.controller.busy
                                                 && tapXField.acceptableInput
                                                 && tapYField.acceptableInput
                                        onClicked: root.controller.tapUi(parseInt(tapXField.text), parseInt(tapYField.text))
                                    }

                                    RuntimeButton {
                                        Layout.preferredWidth: 92
                                        text: qsTr("Tap Node")
                                        enabled: root.controller !== null
                                                 && root.hasSelectedDevice
                                                 && !root.controller.busy
                                                 && root.selectedUiNodeIndex >= 0
                                        onClicked: root.controller.tapUiNode(root.selectedUiNodeIndex)
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    FormRowLabel {
                                        text: qsTr("Type")
                                    }

                                    RuntimeTextField {
                                        id: uiTextField
                                        Layout.fillWidth: true
                                        placeholderText: qsTr("text")
                                    }

                                    RuntimeButton {
                                        Layout.preferredWidth: 72
                                        text: qsTr("Type")
                                        enabled: root.controller !== null
                                                 && root.hasSelectedDevice
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
                            }

                            RuntimeGroup {
                                title: qsTr("Evidence")

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    FormRowLabel {
                                        text: qsTr("Hilog")
                                    }

                                    RuntimeComboBox {
                                        id: hilogLevelCombo
                                        Layout.preferredWidth: 92
                                        model: [
                                            qsTr("All"),
                                            qsTr("Debug"),
                                            qsTr("Info"),
                                            qsTr("Warn"),
                                            qsTr("Error"),
                                            qsTr("Fatal")
                                        ]
                                    }

                                    RuntimeTextField {
                                        id: hilogFilterField
                                        Layout.fillWidth: true
                                        placeholderText: qsTr("filter")
                                    }

                                    RuntimeButton {
                                        text: qsTr("Read")
                                        Layout.preferredWidth: root.runtimeActionWidth
                                        enabled: root.hasSelectedDevice && !root.controller.busy
                                        onClicked: root.controller.readHilog(
                                            hilogFilterField.text,
                                            root.hilogLevelValue(),
                                            800)
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    visible: root.controller !== null && root.controller.uiNodes.length > 0

                                    FormRowLabel {
                                        text: qsTr("UI")
                                    }

                                    RuntimeTextField {
                                        id: uiNodeFilterField
                                        Layout.fillWidth: true
                                        placeholderText: qsTr("filter text / id / type")
                                        onTextChanged: {
                                            if (root.controller !== null) {
                                                root.controller.uiNodeFilter = text
                                            }
                                        }
                                    }
                                }
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
                        visible: root.hasEvidenceDetails

                        TabButton { text: qsTr("UI") }
                        TabButton { text: qsTr("Commands") }
                        TabButton { text: qsTr("Hilog") }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: evidenceTabs.currentIndex
                        visible: root.hasEvidenceDetails

                        RuntimeOutput {
                            text: root.uiNodesEvidenceText()
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
            if (root.hasUiEvidence) {
                evidenceTabs.currentIndex = 0
            } else {
                root.ensureEvidenceTab()
            }
        }

        function onCommandLogChanged() {
            if (root.hasCommandEvidence) {
                evidenceTabs.currentIndex = 1
            } else {
                root.ensureEvidenceTab()
            }
        }

        function onHilogTextChanged() {
            if (root.hasHilogEvidence) {
                evidenceTabs.currentIndex = 2
            } else {
                root.ensureEvidenceTab()
            }
        }

        function onLaunchMetadataChanged() {
            root.syncLaunchFields(false)
        }

        function onResignInstallConfirmationRequested(title, message) {
            root.resignInstallDialogTitle = title
            root.resignInstallDialogMessage = message
            resignInstallDialog.resolved = false
            resignInstallDialog.open()
        }
    }

    Dialog {
        id: resignInstallDialog

        property bool resolved: false

        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        width: Math.min(root.width - 48, 460)
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        padding: 0
        title: ""

        onClosed: {
            if (!resolved && root.controller !== null) {
                root.controller.rejectResignInstall()
            }
            resolved = true
        }

        background: Rectangle {
            radius: 6
            color: root.panelColor
            border.width: 1
            border.color: root.dividerColor
        }

        contentItem: ColumnLayout {
            spacing: 0

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: 18
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: root.resignInstallDialogTitle
                    color: Material.foreground
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: root.resignInstallDialogMessage
                    color: root.secondaryTextColor
                    font.pixelSize: 12
                    lineHeight: 1.25
                    wrapMode: Text.WordWrap
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: root.dividerColor
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 12
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                RuntimeButton {
                    text: qsTr("Cancel")
                    tone: "quiet"
                    Layout.preferredWidth: 92
                    onClicked: {
                        resignInstallDialog.resolved = true
                        resignInstallDialog.close()
                        if (root.controller !== null) {
                            root.controller.rejectResignInstall()
                        }
                    }
                }

                RuntimeButton {
                    text: qsTr("Re-sign and Install")
                    tone: "primary"
                    Layout.preferredWidth: 150
                    onClicked: {
                        resignInstallDialog.resolved = true
                        resignInstallDialog.close()
                        if (root.controller !== null) {
                            root.controller.approveResignInstall()
                        }
                    }
                }
            }
        }
    }

    component RuntimeGroup: Rectangle {
        id: group

        property string title: ""
        default property alias content: groupBody.data

        Layout.fillWidth: true
        implicitHeight: groupBody.implicitHeight + 20
        radius: 4
        color: root.darkTheme ? "#24262a" : "#f8fafb"
        border.width: 1
        border.color: root.dividerColor

        ColumnLayout {
            id: groupBody

            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: group.title
                color: root.secondaryTextColor
                font.pixelSize: 11
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }
    }

    component FormRowLabel: Label {
        Layout.preferredWidth: 56
        Layout.alignment: Qt.AlignTop
        Layout.topMargin: 7
        color: root.secondaryTextColor
        font.pixelSize: 11
        horizontalAlignment: Text.AlignLeft
        elide: Text.ElideRight
    }

    component RuntimeIconButton: AbstractButton {
        id: iconButton

        property string iconName: ""
        property string toolTip: ""

        Layout.preferredWidth: 30
        Layout.preferredHeight: 30
        implicitWidth: 30
        implicitHeight: 30
        padding: 0
        hoverEnabled: true
        ToolTip.text: toolTip
        ToolTip.visible: hovered && toolTip.length > 0
        ToolTip.delay: 600

        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                width: 14
                height: 14
                name: iconButton.iconName
                color: iconButton.enabled
                       ? Material.foreground
                       : root.secondaryTextColor
            }
        }

        background: Rectangle {
            radius: 3
            color: !iconButton.enabled
                   ? "transparent"
                   : iconButton.down
                   ? (root.darkTheme ? "#202327" : "#d6dde1")
                   : iconButton.hovered
                   ? (root.darkTheme ? "#2a2d31" : "#e7ecef")
                   : "transparent"
        }
    }

    component RuntimeButton: AbstractButton {
        property string tone: "normal"
        property string toolTip: ""
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
        readonly property color disabledColor: tone === "quiet"
                                               ? "transparent"
                                               : (root.darkTheme ? "#25282c" : "#edf1f3")
        readonly property color disabledBorderColor: tone === "quiet"
                                                     ? "transparent"
                                                     : (root.darkTheme ? "#353a40" : "#d4dce1")

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
                   ? parent.disabledColor
                   : parent.down
                   ? parent.pressColor
                   : parent.hovered
                   ? parent.hoverColor
                   : parent.baseColor
            border.width: parent.tone === "quiet" && !parent.hovered && parent.enabled ? 0 : 1
            border.color: !parent.enabled
                          ? parent.disabledBorderColor
                          : parent.tone === "primary"
                          ? root.focusColor
                          : root.dividerColor
        }

        ToolTip.text: toolTip
        ToolTip.visible: hovered && toolTip.length > 0
        ToolTip.delay: 500
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
                text: combo.delegateText(comboDelegate.model, comboDelegate.modelData)
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

        function delegateText(model, modelData) {
            const role = combo.textRole || ""
            if (role.length > 0
                    && model !== null
                    && model !== undefined
                    && model[role] !== undefined
                    && model[role] !== null) {
                return model[role]
            }
            return modelData === null || modelData === undefined ? "" : modelData
        }
    }

    component RuntimeTextField: ReArkTextField {
        implicitHeight: 30
        radius: 3
        backgroundColor: root.inputColor
        borderColor: root.dividerColor
        focusBorderColor: root.focusColor
        textColor: Material.foreground
        placeholderColor: root.secondaryTextColor
        selectionColor: root.focusColor
        textPixelSize: 12
    }

    component RuntimeOutput: Rectangle {
        id: outputRoot

        property alias text: outputText.text

        activeFocusOnTab: true
        color: root.inputColor
        border.width: 1
        border.color: outputRoot.activeFocus || outputText.activeFocus ? root.focusColor : root.dividerColor
        clip: true

        Keys.priority: Keys.BeforeItem
        Keys.onShortcutOverride: function(event) {
            if (outputRoot.isKeyboardShortcut(event)) {
                event.accepted = true
            }
        }
        Keys.onPressed: function(event) {
            outputRoot.handleKeyboardShortcut(event)
        }

        onActiveFocusChanged: {
            if (activeFocus) {
                root.activeRuntimeOutput = outputRoot
            }
        }

        ScrollView {
            id: outputScroll
            anchors.fill: parent
            clip: true

            Component.onCompleted: {
                if (contentItem !== null) {
                    contentItem.boundsBehavior = Flickable.StopAtBounds
                    contentItem.boundsMovement = Flickable.StopAtBounds
                }
            }

            TextArea {
                id: outputText
                width: Math.max(outputScroll.availableWidth, implicitWidth)
                height: Math.max(outputScroll.availableHeight, implicitHeight)
                leftPadding: 10
                rightPadding: 10
                topPadding: 8
                bottomPadding: 8
                readOnly: true
                activeFocusOnPress: true
                persistentSelection: true
                selectByMouse: true
                selectByKeyboard: true
                wrapMode: TextArea.NoWrap
                color: Material.foreground
                selectedTextColor: "#ffffff"
                selectionColor: root.focusColor
                font.family: "Consolas"
                font.pixelSize: 11
                background: Item {}

                Keys.priority: Keys.BeforeItem
                Keys.onShortcutOverride: function(event) {
                    if (outputRoot.isKeyboardShortcut(event)) {
                        event.accepted = true
                    }
                }
                Keys.onPressed: function(event) {
                    outputRoot.handleKeyboardShortcut(event)
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        root.activeRuntimeOutput = outputRoot
                    }
                }
            }

            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AsNeeded
        }

        function acceptsKeyboardShortcut() {
            return outputRoot.visible && (outputRoot.activeFocus || outputText.activeFocus)
        }

        function activateOutput() {
            root.activeRuntimeOutput = outputRoot
            outputRoot.forceActiveFocus()
            outputText.forceActiveFocus()
        }

        function hasSelection() {
            return outputText.selectedText.length > 0
        }

        function copySelection() {
            outputRoot.activateOutput()
            outputText.copy()
        }

        function isKeyboardShortcut(event) {
            return event.matches(StandardKey.SelectAll)
                   || (event.matches(StandardKey.Copy) && outputRoot.hasSelection())
        }

        function handleKeyboardShortcut(event) {
            if (event.matches(StandardKey.SelectAll)) {
                outputRoot.selectAllText()
                event.accepted = true
            } else if (event.matches(StandardKey.Copy) && outputRoot.hasSelection()) {
                outputRoot.copySelection()
                event.accepted = true
            }
        }

        function selectAllText() {
            outputRoot.activateOutput()
            outputText.selectAll()
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

    function uiNodesEvidenceText() {
        if (controller === null || controller.filteredUiNodes.length === 0) {
            return ""
        }

        const rows = []
        for (let i = 0; i < controller.filteredUiNodes.length; ++i) {
            const node = controller.filteredUiNodes[i]
            const id = node.id !== undefined && node.id.length > 0 ? node.id : "-"
            const label = node.label !== undefined ? node.label : ""
            const clickable = node.clickable ? "clickable" : ""
            rows.push("#" + node.index
                      + " " + node.type
                      + " " + id
                      + "  " + node.bounds
                      + "  " + clickable
                      + "  " + label)
        }
        return rows.join("\n")
    }

    function syncLaunchFields(force) {
        if (controller === null) {
            return
        }

        const bundle = controller.launchBundleName || ""
        const ability = controller.launchAbilityName || ""
        if (bundle.length > 0
                && (force
                    || bundleField.text.trim().length === 0
                    || bundleField.text === root.lastAppliedBundleName)) {
            bundleField.text = bundle
            root.lastAppliedBundleName = bundle
        }
        if (ability.length > 0
                && (force
                    || abilityField.text.trim().length === 0
                    || abilityField.text === root.lastAppliedAbilityName)) {
            abilityField.text = ability
            root.lastAppliedAbilityName = ability
        }
    }

    function refreshActiveLaunchMetadata() {
        if (root.controller !== null) {
            root.controller.refreshLaunchMetadata(root.activeInstallablePackagePath())
        }
    }

    function activeInstallablePackagePath() {
        const hapPackages = root.installableHapPackages()
        if (hapPackages.length === 1) {
            return hapPackages[0].path
        }
        const currentPath = root.packagePath.toLowerCase()
        return currentPath.endsWith(".hap") ? root.packagePath : ""
    }

    function canInstallActivePackage() {
        return root.controller !== null
                && root.hasSelectedDevice
                && root.activeInstallablePackagePath().length > 0
                && (!root.controller.busy || root.refreshOperationActive)
    }

    function installUnavailableReason() {
        if (root.controller === null) {
            return qsTr("Device runtime is not ready.")
        }
        if (!root.hasSelectedDevice) {
            return qsTr("Select a connected device first.")
        }
        if (root.activeInstallablePackagePath().length === 0) {
            return qsTr("No installable HAP module was resolved from this package.")
        }
        if (root.controller.busy && !root.refreshOperationActive) {
            return root.controller.activeOperation.length > 0
                    ? qsTr("Waiting for %1 to finish.").arg(root.controller.activeOperation)
                    : qsTr("Waiting for the current device operation to finish.")
        }
        return ""
    }

    function installActivePackage() {
        if (root.controller === null) {
            return
        }
        const packagePath = root.activeInstallablePackagePath()
        if (packagePath.length === 0) {
            return
        }
        if (root.controller.screenRefreshRunning) {
            root.controller.stopScreenRefresh()
        }
        if (root.refreshOperationActive) {
            root.pendingInstallPackagePath = packagePath
            deferredInstallTimer.restart()
            return
        }
        root.controller.installPackage(packagePath)
    }

    function installableHapPackages() {
        const result = []
        if (root.installablePackages === null || root.installablePackages === undefined) {
            return result
        }
        for (let i = 0; i < root.installablePackages.length; ++i) {
            const item = root.installablePackages[i]
            if (item === null || item === undefined || item.path === undefined) {
                continue
            }
            const path = item.path || ""
            if (path.length > 0 && path.toLowerCase().endsWith(".hap")) {
                result.push(item)
            }
        }
        return result
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

    function ensureEvidenceTab() {
        if (!hasEvidenceDetails) {
            return
        }
        if (evidenceTabs.currentIndex === 0 && hasUiEvidence) {
            return
        }
        if (evidenceTabs.currentIndex === 1 && hasCommandEvidence) {
            return
        }
        if (evidenceTabs.currentIndex === 2 && hasHilogEvidence) {
            return
        }
        evidenceTabs.currentIndex = hasUiEvidence ? 0 : (hasCommandEvidence ? 1 : 2)
    }

    function hilogLevelValue() {
        switch (hilogLevelCombo.currentIndex) {
        case 1:
            return "D"
        case 2:
            return "I"
        case 3:
            return "W"
        case 4:
            return "E"
        case 5:
            return "F"
        default:
            return ""
        }
    }
}
