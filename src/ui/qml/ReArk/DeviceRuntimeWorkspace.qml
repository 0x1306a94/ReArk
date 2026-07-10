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
    property string downgradeRecoveryDialogTitle: ""
    property string downgradeRecoveryDialogMessage: ""
    property string lastAppliedBundleName: ""
    property string lastAppliedAbilityName: ""
    property bool launchFieldsReady: false
    property int selectedUiNodeIndex: -1
    property int consoleTabIndex: 0
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
    readonly property color runtimeSurfaceColor: darkTheme ? "#0f1821" : "#edf3f6"
    readonly property color runtimePanelColor: darkTheme ? "#13202b" : "#f7fafb"
    readonly property color runtimeStageColor: darkTheme ? "#091118" : "#dfe8ed"
    readonly property color runtimeLineColor: darkTheme ? "#243240" : "#ccd7dc"
    readonly property color focusColor: darkTheme ? "#3f8fd2" : "#2f80c1"
    readonly property color secondaryTextColor: darkTheme ? "#a6a6a6" : "#5f6872"
    readonly property color dangerColor: darkTheme ? "#f48771" : "#a1260d"
    readonly property int runtimeActionWidth: 72
    readonly property int runtimeFieldSpacing: 8
    readonly property int runtimeCoordinateFieldWidth: 72
    readonly property int launchBundleFieldWidth: runtimeCoordinateFieldWidth * 2 + runtimeFieldSpacing

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
        if (resignInstallDialog.opened) {
            resignInstallDialog.resolved = true
            resignInstallDialog.close()
        }
        if (downgradeRecoveryDialog.opened) {
            downgradeRecoveryDialog.resolved = true
            downgradeRecoveryDialog.close()
        }
        deferredInstallTimer.stop()
        root.pendingInstallPackagePath = ""
        if (root.controller !== null) {
            root.controller.resetForPackageChange()
        }
        root.resetLaunchFieldsForPackageChange()
        root.refreshActiveLaunchMetadata()
    }

    onInstallablePackagesChanged: {
        root.refreshActiveLaunchMetadata()
    }

    onControllerChanged: {
        root.refreshActiveLaunchMetadata()
    }

    Component.onCompleted: {
        root.launchFieldsReady = true
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

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 320
                Layout.minimumWidth: 300
                Layout.maximumWidth: 340
                Layout.fillHeight: true
                color: darkTheme ? "#181b20" : "#eef3f6"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        RuntimeIconButton {
                            iconName: "arrow-left"
                            toolTip: qsTr("Back")
                            onClicked: root.backRequested()
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Devices")
                            color: Material.foreground
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        RuntimeComboBox {
                            id: deviceCombo

                            Layout.fillWidth: true
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

                        RuntimeIconButton {
                            iconName: "refresh-cw"
                            toolTip: qsTr("Refresh devices")
                            enabled: root.controller !== null && !root.controller.busy
                            onClicked: root.controller.refreshDevices()
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 352
                        radius: 4
                        color: panelColor
                        border.width: 0
                        clip: true

                        ColumnLayout {
                            id: deviceInfoBody

                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.topMargin: 12
                            anchors.bottomMargin: 10
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.deviceNameText()
                                        color: Material.foreground
                                        font.pixelSize: 17
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.modelNameText()
                                        color: secondaryTextColor
                                        font.pixelSize: 11
                                        elide: Text.ElideMiddle
                                    }
                                }

                                Rectangle {
                                    Layout.preferredWidth: Math.max(64, sidebarStateLabel.implicitWidth + 16)
                                    Layout.preferredHeight: 22
                                    radius: 3
                                    color: darkTheme ? "#12314a" : "#dceef9"
                                    border.width: 1
                                    border.color: darkTheme ? "#2d5f86" : "#b8d9eb"
                                    visible: root.hasSelectedDevice

                                    Label {
                                        id: sidebarStateLabel

                                        anchors.centerIn: parent
                                        text: root.deviceInfoValue("stateDisplay", root.deviceInfoValue("state", qsTr("online")))
                                        color: darkTheme ? "#cbe8ff" : "#225f86"
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                Layout.topMargin: 3
                                Layout.bottomMargin: 2
                                color: root.darkTheme ? "#26303a" : "#dfe7eb"
                            }

                            SidebarInfoRow {
                                label: qsTr("型号代码")
                                value: root.deviceInfoValue("modelCode", root.deviceInfoValue("model", "-"))
                            }

                            SidebarInfoRow {
                                label: qsTr("HarmonyOS 版本")
                                value: root.deviceInfoValue("harmonyVersion", root.harmonyVersionFallbackText())
                            }

                            SidebarInfoRow {
                                label: qsTr("软件版本")
                                value: root.softwareVersionText()
                                maximumLines: 2
                            }

                            SidebarInfoRow {
                                label: qsTr("序列号")
                                value: root.deviceInfoValue("id", root.controller !== null ? root.controller.selectedDeviceId : "-")
                            }

                            SidebarInfoRow {
                                label: qsTr("IMEI")
                                value: root.deviceInfoValue("imei", "-")
                                maximumLines: 2
                            }

                            SidebarInfoRow {
                                label: qsTr("运行内存")
                                value: root.deviceInfoValue("runningMemory", "-")
                            }

                            SidebarInfoRow {
                                label: qsTr("存储")
                                value: root.deviceInfoValue("storage", "-")
                                maximumLines: 2
                            }

                            SidebarInfoRow {
                                label: qsTr("屏幕")
                                value: root.screenResolutionText()
                            }

                            SidebarInfoRow {
                                label: qsTr("API")
                                value: root.deviceInfoValue("apiVersion", "-")
                            }

                            SidebarInfoRow {
                                label: qsTr("ABI")
                                value: root.deviceInfoValue("abi", "-")
                                maximumLines: 2
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Capture / Screen")
                        color: Material.foreground
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    RuntimeButton {
                        Layout.fillWidth: true
                        text: qsTr("Screenshot")
                        tone: "primary"
                        enabled: root.hasSelectedDevice && root.controller !== null && !root.controller.busy
                        onClicked: root.controller.captureScreenshot()
                    }

                    RuntimeButton {
                        Layout.fillWidth: true
                        text: qsTr("Inspect UI")
                        enabled: root.hasSelectedDevice && root.controller !== null && !root.controller.busy
                        onClicked: root.controller.captureUiSnapshot(bundleField.text)
                    }

                    RuntimeButton {
                        Layout.fillWidth: true
                        text: root.controller !== null && root.controller.screenRefreshRunning
                              ? qsTr("Stop Auto Refresh")
                              : qsTr("Auto Refresh")
                        enabled: root.hasSelectedDevice
                                 && root.controller !== null
                                 && (!root.controller.busy || root.controller.screenRefreshRunning)
                        onClicked: {
                            if (root.controller.screenRefreshRunning) {
                                root.controller.stopScreenRefresh()
                            } else {
                                root.controller.startScreenRefresh()
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        text: qsTr("Quick Tools")
                        color: Material.foreground
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        columnSpacing: 8
                        rowSpacing: 8

                        QuickToolTile {
                            text: qsTr("检查 UI")
                            iconName: "runtime-search"
                            actionEnabled: root.hasSelectedDevice && root.controller !== null && !root.controller.busy
                            onClicked: root.controller.captureUiSnapshot(bundleField.text)
                        }

                        QuickToolTile {
                            text: qsTr("自动刷新")
                            iconName: "runtime-refresh-cw"
                            active: root.controller !== null && root.controller.screenRefreshRunning
                            actionEnabled: root.hasSelectedDevice
                                           && root.controller !== null
                                           && (!root.controller.busy || root.controller.screenRefreshRunning)
                            onClicked: {
                                if (root.controller.screenRefreshRunning) {
                                    root.controller.stopScreenRefresh()
                                } else {
                                    root.controller.startScreenRefresh()
                                }
                            }
                        }

                        QuickToolTile {
                            text: qsTr("返回")
                            iconName: "runtime-arrow-left"
                            actionEnabled: root.hasSelectedDevice && root.controller !== null && !root.controller.busy
                            onClicked: root.controller.pressDeviceKey("Back")
                        }

                        QuickToolTile {
                            text: qsTr("主页")
                            iconName: "runtime-home"
                            actionEnabled: root.hasSelectedDevice && root.controller !== null && !root.controller.busy
                            onClicked: root.controller.pressDeviceKey("Home")
                        }

                        QuickToolTile {
                            text: qsTr("旋转")
                            iconName: "runtime-rotate-ccw"
                            available: false
                        }

                        QuickToolTile {
                            text: qsTr("ADB Shell")
                            iconName: "runtime-terminal"
                            available: false
                        }

                        QuickToolTile {
                            text: qsTr("安装应用")
                            iconName: "runtime-package"
                            actionEnabled: root.canInstallActivePackage()
                            onClicked: root.installActivePackage()
                        }

                        QuickToolTile {
                            text: qsTr("更多工具")
                            iconName: "runtime-more"
                            available: false
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: dividerColor
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: runtimeSurfaceColor

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: headerColor

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 0

                            Rectangle {
                                Layout.preferredWidth: Math.min(190, Math.max(120, currentDeviceTabTitle.implicitWidth + 52))
                                Layout.preferredHeight: 30
                                Layout.alignment: Qt.AlignBottom
                                radius: 3
                                color: panelColor
                                border.width: 1
                                border.color: dividerColor
                                visible: root.hasSelectedDevice

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 2
                                    color: focusColor
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 8
                                    spacing: 6

                                    Rectangle {
                                        Layout.preferredWidth: 6
                                        Layout.preferredHeight: 6
                                        radius: 3
                                        color: "#4aa36b"
                                    }

                                    Label {
                                        id: currentDeviceTabTitle

                                        Layout.fillWidth: true
                                        text: root.selectedDeviceTitle()
                                        color: Material.foreground
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    Label {
                                        text: "x"
                                        color: secondaryTextColor
                                        font.pixelSize: 12
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }

                            Item {
                                Layout.fillWidth: true
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
                            Layout.minimumWidth: 610
                            color: runtimeSurfaceColor
                            clip: true

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 0

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
                                    color: runtimeSurfaceColor

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 4

                                        RuntimeIconButton {
                                            iconName: "scan"
                                            toolTip: qsTr("Inspect UI")
                                            enabled: root.hasSelectedDevice && root.controller !== null && !root.controller.busy
                                            onClicked: root.controller.captureUiSnapshot(bundleField.text)
                                        }

                                        RuntimeIconButton {
                                            iconName: "maximize"
                                            toolTip: qsTr("Fit preview")
                                            enabled: root.screenImageSource().length > 0
                                        }

                                        RuntimeIconButton {
                                            iconName: "search"
                                            toolTip: qsTr("Locate selected node")
                                            enabled: root.selectedUiNodeIndex >= 0
                                        }

                                        RuntimeIconButton {
                                            iconName: "copy"
                                            toolTip: qsTr("Copy selected node summary")
                                            enabled: root.selectedUiNodeIndex >= 0
                                        }

                                        Rectangle {
                                            Layout.preferredWidth: 1
                                            Layout.preferredHeight: 16
                                            color: runtimeLineColor
                                        }

                                        RuntimeIconButton {
                                            iconName: "minus"
                                            toolTip: qsTr("Zoom out")
                                            enabled: root.screenImageSource().length > 0
                                        }

                                        Label {
                                            Layout.preferredWidth: 42
                                            text: qsTr("50%")
                                            color: secondaryTextColor
                                            font.pixelSize: 11
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }

                                        RuntimeIconButton {
                                            iconName: "plus"
                                            toolTip: qsTr("Zoom in")
                                            enabled: root.screenImageSource().length > 0
                                        }

                                        RuntimeIconButton {
                                            iconName: "grid-2x2"
                                            toolTip: qsTr("Toggle overlay")
                                            enabled: root.controller !== null
                                                     && root.controller.uiNodes.length > 0
                                                     && root.screenImageSource().length > 0
                                            onClicked: overlayCheck.checked = !overlayCheck.checked
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.controller !== null && root.controller.busy
                                                  ? root.controller.activeOperation
                                                  : root.controller !== null ? root.controller.status : ""
                                            color: root.controller !== null && root.controller.errorMessage.length > 0
                                                   ? dangerColor
                                                   : secondaryTextColor
                                            font.pixelSize: 11
                                            horizontalAlignment: Text.AlignHCenter
                                            elide: Text.ElideRight
                                        }

                                        Label {
                                            text: root.controller !== null
                                                  ? qsTr("%1 nodes").arg(root.controller.uiNodes.length)
                                                  : qsTr("0 nodes")
                                            color: secondaryTextColor
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }

                                        CheckBox {
                                            id: overlayCheck
                                            visible: false
                                            checked: true
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: runtimeLineColor
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: 0

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.minimumWidth: 330
                                        color: runtimeStageColor
                                        clip: true

                                        Rectangle {
                                            id: screenStage

                                            anchors.fill: parent
                                            anchors.margins: 10
                                            color: runtimeStageColor
                                            clip: true

                                            Rectangle {
                                                id: screenSurface

                                                readonly property real deviceAspect: screenshotImage.sourceSize.width > 0
                                                                                  && screenshotImage.sourceSize.height > 0
                                                                                  ? screenshotImage.sourceSize.width / screenshotImage.sourceSize.height
                                                                                  : 1260 / 2720

                                                anchors.centerIn: parent
                                                width: Math.min(parent.width - 56, (parent.height - 28) * deviceAspect + 14)
                                                height: Math.min(parent.height - 28, (width - 14) / deviceAspect + 14)
                                                radius: 5
                                                color: darkTheme ? "#121820" : "#f7fafb"
                                                border.width: 1
                                                border.color: screenshotImage.status === Image.Ready
                                                              ? (darkTheme ? "#2d7fbc" : focusColor)
                                                              : dividerColor
                                                clip: true

                                                Image {
                                                    id: screenshotImage

                                                    anchors.fill: parent
                                                    anchors.margins: 7
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
                                                    font.pixelSize: 12
                                                    horizontalAlignment: Text.AlignHCenter
                                                    wrapMode: Text.WordWrap
                                                }
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.preferredWidth: 1
                                        Layout.fillHeight: true
                                        color: runtimeLineColor
                                    }

                                    Rectangle {
                                        Layout.preferredWidth: 286
                                        Layout.minimumWidth: 260
                                        Layout.maximumWidth: 320
                                        Layout.fillHeight: true
                                        color: runtimeSurfaceColor

                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 8

                                            RowLayout {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 28
                                                spacing: 8

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: qsTr("当前选中节点")
                                                    color: Material.foreground
                                                    font.pixelSize: 11
                                                    font.weight: Font.DemiBold
                                                    elide: Text.ElideRight
                                                }

                                                RuntimeButton {
                                                    Layout.preferredWidth: 62
                                                    text: qsTr("点击")
                                                    enabled: root.controller !== null
                                                             && root.hasSelectedDevice
                                                             && !root.controller.busy
                                                             && root.selectedUiNodeIndex >= 0
                                                    onClicked: root.controller.tapUiNode(root.selectedUiNodeIndex)
                                                }
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                color: runtimePanelColor
                                                clip: true

                                                ColumnLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: 10
                                                    spacing: 8

                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        Layout.preferredHeight: 42
                                                        spacing: 8

                                                        ColumnLayout {
                                                            Layout.fillWidth: true
                                                            spacing: 2

                                                            Label {
                                                                Layout.fillWidth: true
                                                                text: root.selectedNodeValue("type", qsTr("未选中节点"))
                                                                color: Material.foreground
                                                                font.pixelSize: 12
                                                                font.weight: Font.DemiBold
                                                                elide: Text.ElideRight
                                                            }

                                                            Label {
                                                                Layout.fillWidth: true
                                                                text: root.selectedNodeValue("id", root.selectedNodeText())
                                                                color: secondaryTextColor
                                                                font.pixelSize: 10
                                                                elide: Text.ElideMiddle
                                                            }
                                                        }

                                                        RuntimeIconButton {
                                                            iconName: "copy"
                                                            toolTip: qsTr("Copy")
                                                            enabled: root.selectedUiNodeIndex >= 0
                                                        }

                                                        RuntimeIconButton {
                                                            iconName: "maximize"
                                                            toolTip: qsTr("Locate")
                                                            enabled: root.selectedUiNodeIndex >= 0
                                                        }
                                                    }

                                                    Rectangle {
                                                        Layout.fillWidth: true
                                                        Layout.preferredHeight: 1
                                                        color: runtimeLineColor
                                                    }

                                                    Flickable {
                                                        Layout.fillWidth: true
                                                        Layout.fillHeight: true
                                                        clip: true
                                                        contentWidth: width
                                                        contentHeight: nodePropertyColumn.implicitHeight
                                                        boundsBehavior: Flickable.StopAtBounds
                                                        boundsMovement: Flickable.StopAtBounds
                                                        ScrollBar.vertical: ScrollBar {
                                                            policy: ScrollBar.AsNeeded
                                                        }

                                                        ColumnLayout {
                                                            id: nodePropertyColumn

                                                            width: parent.width
                                                            spacing: 0

                                                            RuntimePropertyRow { label: qsTr("坐标"); value: root.selectedNodePositionText() }
                                                            RuntimePropertyRow { label: qsTr("尺寸"); value: root.selectedNodeSizeText() }
                                                            RuntimePropertyRow { label: qsTr("资源 ID"); value: root.selectedNodeValue("id", "-") }
                                                            RuntimePropertyRow { label: qsTr("文本"); value: root.selectedNodeText() }
                                                            RuntimePropertyRow { label: qsTr("内容描述"); value: root.selectedNodeValue("description", root.selectedNodeText()) }
                                                            RuntimePropertyRow { label: qsTr("类名"); value: root.selectedNodeValue("type", "-") }
                                                            RuntimePropertyRow { label: qsTr("可见性"); value: root.selectedNodeBoolText("visible") }
                                                            RuntimePropertyRow { label: qsTr("启用"); value: root.selectedNodeBoolText("enabled") }
                                                            RuntimePropertyRow { label: qsTr("焦点"); value: root.selectedNodeBoolText("focused") }
                                                            RuntimePropertyRow { label: qsTr("点击able"); value: root.selectedNodeBoolText("clickable") }
                                                            RuntimePropertyRow { label: qsTr("长按able"); value: root.selectedNodeBoolText("longClickable") }
                                                            RuntimePropertyRow { label: qsTr("选中"); value: root.selectedNodeBoolText("selected") }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.fillHeight: true
                            color: runtimeLineColor
                        }

                        Rectangle {
                            Layout.preferredWidth: 360
                            Layout.minimumWidth: 330
                            Layout.maximumWidth: 400
                            Layout.fillHeight: true
                            color: runtimeSurfaceColor
                            clip: true

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 0

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 36
                                    color: runtimeSurfaceColor

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 4

                                        RuntimeTabButton { text: qsTr("Console"); selected: root.consoleTabIndex === 0; onClicked: root.consoleTabIndex = 0 }
                                        RuntimeTabButton { text: qsTr("App"); selected: root.consoleTabIndex === 1; onClicked: root.consoleTabIndex = 1 }
                                        RuntimeTabButton { text: qsTr("Input"); selected: root.consoleTabIndex === 2; onClicked: root.consoleTabIndex = 2 }

                                        Item { Layout.fillWidth: true }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: runtimeLineColor
                                }

                                StackLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    currentIndex: root.consoleTabIndex

                                    Flickable {
                                        clip: true
                                        contentWidth: width
                                        contentHeight: consoleColumn.implicitHeight + 20
                                        boundsBehavior: Flickable.StopAtBounds
                                        boundsMovement: Flickable.StopAtBounds
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                        ColumnLayout {
                                            id: consoleColumn

                                            width: parent.width - 24
                                            x: 12
                                            y: 10
                                            spacing: 10

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
                                                title: qsTr("Hilog")

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 8

                                                    RuntimeComboBox {
                                                        id: hilogLevelCombo
                                                        Layout.preferredWidth: 94
                                                        model: [qsTr("All"), qsTr("Debug"), qsTr("Info"), qsTr("Warn"), qsTr("Error"), qsTr("Fatal")]
                                                    }

                                                    RuntimeTextField {
                                                        id: hilogFilterField
                                                        Layout.fillWidth: true
                                                        placeholderText: qsTr("filter")
                                                    }

                                                    RuntimeButton {
                                                        text: qsTr("Read")
                                                        Layout.preferredWidth: root.runtimeActionWidth
                                                        enabled: root.hasSelectedDevice && root.controller !== null && !root.controller.busy
                                                        onClicked: root.controller.readHilog(hilogFilterField.text, root.hilogLevelValue(), 800)
                                                    }
                                                }
                                            }

                                            RuntimeOutput {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 150
                                                text: root.controller !== null ? root.controller.commandLog : ""
                                            }
                                        }
                                    }

                                    Flickable {
                                        clip: true
                                        contentWidth: width
                                        contentHeight: appColumn.implicitHeight + 20
                                        boundsBehavior: Flickable.StopAtBounds
                                        boundsMovement: Flickable.StopAtBounds
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                        ColumnLayout {
                                            id: appColumn

                                            width: parent.width - 24
                                            x: 12
                                            y: 10
                                            spacing: 10

                                            RuntimeGroup {
                                                title: qsTr("Package")

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 8

                                                    Label {
                                                        Layout.fillWidth: true
                                                        text: root.packageName.length > 0 ? root.packageName : qsTr("No package loaded")
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
                                            }

                                            RuntimeGroup {
                                                title: qsTr("Launch")

                                                RuntimeTextField {
                                                    id: bundleField
                                                    Layout.fillWidth: true
                                                    placeholderText: qsTr("Bundle")
                                                }

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: root.runtimeFieldSpacing

                                                    RuntimeTextField {
                                                        id: abilityField
                                                        Layout.fillWidth: true
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
                                        }
                                    }

                                    Flickable {
                                        clip: true
                                        contentWidth: width
                                        contentHeight: inputColumn.implicitHeight + 20
                                        boundsBehavior: Flickable.StopAtBounds
                                        boundsMovement: Flickable.StopAtBounds
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                        ColumnLayout {
                                            id: inputColumn

                                            width: parent.width - 24
                                            x: 12
                                            y: 10
                                            spacing: 10

                                            RuntimeGroup {
                                                title: qsTr("Screen Keys")

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 8

                                                    RuntimeButton {
                                                        Layout.fillWidth: true
                                                        text: qsTr("Back")
                                                        enabled: root.hasSelectedDevice && root.controller !== null && !root.controller.busy
                                                        onClicked: root.controller.pressDeviceKey("Back")
                                                    }

                                                    RuntimeButton {
                                                        Layout.fillWidth: true
                                                        text: qsTr("Home")
                                                        enabled: root.hasSelectedDevice && root.controller !== null && !root.controller.busy
                                                        onClicked: root.controller.pressDeviceKey("Home")
                                                    }
                                                }
                                            }

                                            RuntimeGroup {
                                                title: qsTr("Pointer")

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: root.runtimeFieldSpacing

                                                    RuntimeTextField {
                                                        id: tapXField
                                                        Layout.preferredWidth: root.runtimeCoordinateFieldWidth
                                                        placeholderText: qsTr("x")
                                                        validator: IntValidator { bottom: 0; top: 100000 }
                                                    }

                                                    RuntimeTextField {
                                                        id: tapYField
                                                        Layout.preferredWidth: root.runtimeCoordinateFieldWidth
                                                        placeholderText: qsTr("y")
                                                        validator: IntValidator { bottom: 0; top: 100000 }
                                                    }

                                                    RuntimeButton {
                                                        Layout.preferredWidth: root.runtimeCoordinateFieldWidth
                                                        text: qsTr("Tap")
                                                        enabled: root.controller !== null
                                                                 && root.hasSelectedDevice
                                                                 && !root.controller.busy
                                                                 && tapXField.acceptableInput
                                                                 && tapYField.acceptableInput
                                                        onClicked: root.controller.tapUi(parseInt(tapXField.text), parseInt(tapYField.text))
                                                    }

                                                    RuntimeButton {
                                                        Layout.preferredWidth: 86
                                                        text: qsTr("Tap Node")
                                                        enabled: root.controller !== null
                                                                 && root.hasSelectedDevice
                                                                 && !root.controller.busy
                                                                 && root.selectedUiNodeIndex >= 0
                                                        onClicked: root.controller.tapUiNode(root.selectedUiNodeIndex)
                                                    }
                                                }
                                            }

                                            RuntimeGroup {
                                                title: qsTr("Keyboard")

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 8

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
                        visible: root.hasEvidenceDetails
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.hasEvidenceDetails ? Math.max(180, Math.min(260, root.height * 0.28)) : 0
                        visible: root.hasEvidenceDetails
                        color: inputColor

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                id: evidenceTabs

                                property int currentIndex: 0

                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                color: darkTheme ? "#151719" : "#f5f8fa"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 4

                                    RuntimeTabButton { text: qsTr("UI"); selected: evidenceTabs.currentIndex === 0; onClicked: evidenceTabs.currentIndex = 0 }
                                    RuntimeTabButton { text: qsTr("Commands"); selected: evidenceTabs.currentIndex === 1; onClicked: evidenceTabs.currentIndex = 1 }
                                    RuntimeTabButton { text: qsTr("Hilog"); selected: evidenceTabs.currentIndex === 2; onClicked: evidenceTabs.currentIndex = 2 }

                                    Item { Layout.fillWidth: true }
                                }
                            }

                            StackLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                currentIndex: evidenceTabs.currentIndex

                                RuntimeOutput { text: root.uiNodesEvidenceText() }
                                RuntimeOutput { text: root.controller !== null ? root.controller.commandLog : "" }
                                RuntimeOutput { text: root.controller !== null ? root.controller.hilogText : "" }
                            }
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

        function onInstallDowngradeRecoveryConfirmationRequested(title, message) {
            root.downgradeRecoveryDialogTitle = title
            root.downgradeRecoveryDialogMessage = message
            downgradeRecoveryDialog.resolved = false
            downgradeRecoveryDialog.open()
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

    Dialog {
        id: downgradeRecoveryDialog

        property bool resolved: false

        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        width: Math.min(root.width - 48, 500)
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        padding: 0
        title: ""

        onClosed: {
            if (!resolved && root.controller !== null) {
                root.controller.rejectDowngradeRecovery()
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
                    text: root.downgradeRecoveryDialogTitle
                    color: Material.foreground
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: root.downgradeRecoveryDialogMessage
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
                        downgradeRecoveryDialog.resolved = true
                        downgradeRecoveryDialog.close()
                        if (root.controller !== null) {
                            root.controller.rejectDowngradeRecovery()
                        }
                    }
                }

                RuntimeButton {
                    text: qsTr("Overwrite Install")
                    tone: "primary"
                    Layout.preferredWidth: 160
                    onClicked: {
                        downgradeRecoveryDialog.resolved = true
                        downgradeRecoveryDialog.close()
                        if (root.controller !== null) {
                            root.controller.approveDowngradeRecovery()
                        }
                    }
                }
            }
        }
    }

    component QuickToolTile: AbstractButton {
        id: toolTile

        property string iconName: ""
        property bool active: false
        property bool available: true
        property bool actionEnabled: true

        Layout.fillWidth: true
        Layout.preferredHeight: 64
        implicitWidth: 52
        implicitHeight: 64
        padding: 0
        hoverEnabled: available
        enabled: available && actionEnabled

        contentItem: ColumnLayout {
            spacing: 4

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 44
                Layout.preferredHeight: 38
                radius: 3
                color: !toolTile.available
                       ? (root.darkTheme ? "#1a1d21" : "#e6ecef")
                       : toolTile.down
                       ? (root.darkTheme ? "#152637" : "#d3e5ef")
                       : toolTile.hovered || toolTile.active
                       ? (root.darkTheme ? "#162a3c" : "#dcecf5")
                       : (root.darkTheme ? "#20262d" : "#e8eef2")
                border.width: toolTile.active || toolTile.hovered ? 1 : 1
                border.color: toolTile.active || toolTile.hovered
                              ? root.focusColor
                              : (root.darkTheme ? "#2d3742" : "#ccd7dc")

                Icon {
                    anchors.centerIn: parent
                    width: 19
                    height: 19
                    name: toolTile.iconName
                    color: !toolTile.enabled
                           ? root.secondaryTextColor
                           : toolTile.active || toolTile.hovered
                           ? root.focusColor
                           : Material.foreground
                    opacity: toolTile.enabled ? 1.0 : 0.5
                }
            }

            Label {
                Layout.fillWidth: true
                text: toolTile.text
                color: root.secondaryTextColor
                opacity: toolTile.enabled ? 1.0 : 0.5
                font.pixelSize: 10
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        background: Item {}
    }

    component DeviceInfoMetric: Rectangle {
        property string label: ""
        property string value: "-"

        Layout.fillWidth: true
        Layout.preferredHeight: 30
        radius: 3
        color: root.darkTheme ? "#191b1f" : "#f2f5f7"
        border.width: 1
        border.color: root.darkTheme ? "#2b2f34" : "#e0e6e9"

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 7
            anchors.rightMargin: 7
            anchors.topMargin: 3
            anchors.bottomMargin: 3
            spacing: 0

            Label {
                Layout.fillWidth: true
                text: label
                color: secondaryTextColor
                font.pixelSize: 9
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Label {
                Layout.fillWidth: true
                text: value.length > 0 ? value : "-"
                color: Material.foreground
                font.pixelSize: 11
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
    }

    component SidebarInfoRow: RowLayout {
        property string label: ""
        property string value: "-"
        property int maximumLines: 1

        Layout.fillWidth: true
        Layout.preferredHeight: Math.max(22, sidebarValue.implicitHeight + 6)
        spacing: 10

        Label {
            Layout.preferredWidth: 88
            text: label
            color: secondaryTextColor
            font.pixelSize: 10
            elide: Text.ElideRight
            maximumLineCount: 1
            verticalAlignment: Text.AlignVCenter
        }

        Label {
            id: sidebarValue

            Layout.fillWidth: true
            text: value.length > 0 ? value : "-"
            color: Material.foreground
            font.pixelSize: 10
            horizontalAlignment: Text.AlignRight
            lineHeight: 1.12
            wrapMode: maximumLines > 1 ? Text.WrapAnywhere : Text.NoWrap
            elide: Text.ElideRight
            maximumLineCount: maximumLines
            verticalAlignment: Text.AlignVCenter
        }
    }

    component RuntimePropertyRow: Rectangle {
        property string label: ""
        property string value: "-"

        Layout.fillWidth: true
        implicitHeight: Math.max(28, rowValue.implicitHeight + 11)
        color: "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: root.darkTheme ? "#20303d" : "#dfe7eb"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            spacing: 10

            Label {
                Layout.preferredWidth: 74
                text: label
                color: secondaryTextColor
                font.pixelSize: 10
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Label {
                id: rowValue

                Layout.fillWidth: true
                text: value.length > 0 ? value : "-"
                color: Material.foreground
                font.pixelSize: 10
                lineHeight: 1.15
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 3
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    component RuntimeTabButton: AbstractButton {
        id: tabButton

        property bool selected: false

        Layout.preferredWidth: Math.max(74, contentItem.implicitWidth + 24)
        Layout.preferredHeight: 28
        implicitHeight: 28
        padding: 0
        hoverEnabled: true

        contentItem: Label {
            text: tabButton.text
            color: tabButton.selected
                   ? (root.darkTheme ? "#d7f4ff" : "#1f6f9d")
                   : (tabButton.hovered ? Material.foreground : root.secondaryTextColor)
            font.pixelSize: 11
            font.weight: tabButton.selected ? Font.DemiBold : Font.Normal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: tabButton.hovered && !tabButton.selected
                   ? (root.darkTheme ? "#172330" : "#e5edf1")
                   : "transparent"
            border.width: 0

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 2
                color: root.focusColor
                visible: tabButton.selected
            }
        }
    }

    component RuntimeGroup: Rectangle {
        id: group

        property string title: ""
        default property alias content: groupBody.data

        Layout.fillWidth: true
        implicitHeight: groupBody.implicitHeight + 20
        radius: 3
        color: root.runtimePanelColor
        border.width: 0

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
        property int iconSize: 15

        Layout.preferredWidth: 28
        Layout.preferredHeight: 28
        implicitWidth: 28
        implicitHeight: 28
        padding: 0
        hoverEnabled: true
        ToolTip.text: toolTip
        ToolTip.visible: hovered && toolTip.length > 0
        ToolTip.delay: 600

        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                width: iconButton.iconSize
                height: iconButton.iconSize
                name: iconButton.resolvedIconName()
                color: iconButton.enabled
                       ? (iconButton.hovered ? root.focusColor : Material.foreground)
                       : root.secondaryTextColor
                opacity: iconButton.enabled ? 1.0 : 0.42
            }
        }

        background: Rectangle {
            radius: 3
            color: !iconButton.enabled
                   ? "transparent"
                   : iconButton.down
                    ? (root.darkTheme ? "#202327" : "#d6dde1")
                    : iconButton.hovered
                    ? (root.darkTheme ? "#1a2732" : "#e7ecef")
                    : "transparent"
        }

        function resolvedIconName() {
            if (iconName === "copy") {
                return "runtime-copy"
            }
            if (iconName === "refresh-cw") {
                return "runtime-refresh-cw"
            }
            if (iconName === "scan") {
                return "runtime-scan"
            }
            if (iconName === "maximize") {
                return "runtime-fit"
            }
            if (iconName === "search") {
                return "runtime-search"
            }
            if (iconName === "minus") {
                return "runtime-minus"
            }
            if (iconName === "plus") {
                return "runtime-plus"
            }
            if (iconName === "grid-2x2") {
                return "runtime-grid"
            }
            return iconName
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
                context.clearRect(0, 0, width, height)
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
            border.color: combo.activeFocus || combo.popup.visible ? root.focusColor : root.runtimeLineColor
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
        borderColor: root.runtimeLineColor
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
        border.color: outputRoot.activeFocus || outputText.activeFocus ? root.focusColor : root.runtimeLineColor
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

    function deviceInfoValue(key, fallback) {
        if (controller === null
                || controller.selectedDeviceInfo === undefined
                || controller.selectedDeviceInfo === null) {
            return fallback
        }
        const value = controller.selectedDeviceInfo[key]
        if (value === undefined || value === null) {
            return fallback
        }
        const text = String(value).trim()
        return text.length > 0 ? text : fallback
    }

    function selectedDeviceTitle() {
        return root.deviceNameText()
    }

    function deviceNameText() {
        if (!root.hasSelectedDevice) {
            return qsTr("No device selected")
        }
        const displayName = root.deviceInfoValue("deviceName", "")
        if (displayName.length > 0) {
            return displayName
        }
        const modelName = root.deviceInfoValue("modelName", root.deviceInfoValue("productName", ""))
        if (modelName.length > 0) {
            return root.normalizedPhoneName(modelName)
        }
        const model = root.deviceInfoValue("model", "")
        if (model.length > 0) {
            return model
        }
        return root.deviceInfoValue("id", root.controller.selectedDeviceId)
    }

    function selectedDeviceSubtitle() {
        return root.modelNameText()
    }

    function modelNameText() {
        if (!root.hasSelectedDevice) {
            return qsTr("Connect a device with HDC to inspect runtime state.")
        }
        const modelName = root.deviceInfoValue("modelName", root.deviceInfoValue("productName", ""))
        if (modelName.length > 0) {
            return modelName
        }
        const model = root.deviceInfoValue("modelCode", root.deviceInfoValue("model", ""))
        if (model.length > 0) {
            return model
        }
        return root.deviceInfoValue("id", root.controller.selectedDeviceId)
    }

    function normalizedPhoneName(value) {
        let text = String(value).trim()
        const prefixes = [ "HUAWEI ", "HONOR " ]
        for (let i = 0; i < prefixes.length; ++i) {
            if (text.toUpperCase().startsWith(prefixes[i])) {
                text = text.slice(prefixes[i].length).trim()
                break
            }
        }
        return text.length > 0 ? text : value
    }

    function versionNumberFromText(value) {
        const match = String(value).match(/\d+(?:\.\d+){2,}/)
        return match === null ? "" : match[0]
    }

    function systemVersionText() {
        if (!root.hasSelectedDevice) {
            return "-"
        }
        const model = root.deviceInfoValue("model", "")
        const softwareVersion = root.deviceInfoValue("softwareVersion", root.deviceInfoValue("system", ""))
        const version = root.versionNumberFromText(softwareVersion)
        if (model.length > 0 && version.length > 0) {
            return model + " " + version
        }
        if (softwareVersion.length > 0) {
            const paren = softwareVersion.indexOf("(")
            return paren > 0 ? softwareVersion.slice(0, paren).trim() : softwareVersion
        }
        return root.deviceInfoValue("ohosFullName", "-")
    }

    function softwareVersionText() {
        return root.deviceInfoValue("softwareVersion",
               root.deviceInfoValue("system",
               root.deviceInfoValue("ohosFullName", "-")))
    }

    function harmonyVersionFallbackText() {
        const version = root.versionNumberFromText(root.softwareVersionText())
        if (version.length === 0) {
            return "-"
        }
        const parts = version.split(".")
        return parts.length >= 3 ? parts.slice(0, 3).join(".") : version
    }

    function normalizedResolutionText(width, height) {
        const first = Math.round(width)
        const second = Math.round(height)
        if (first <= 0 || second <= 0) {
            return "-"
        }
        return Math.max(first, second) + " x " + Math.min(first, second)
    }

    function screenResolutionText() {
        const probed = root.deviceInfoValue("screenResolution", "")
        if (probed.length > 0) {
            return probed
        }
        if (screenshotImage.status === Image.Ready
                && screenshotImage.sourceSize.width > 0
                && screenshotImage.sourceSize.height > 0) {
            return root.normalizedResolutionText(screenshotImage.sourceSize.width, screenshotImage.sourceSize.height)
        }
        return "-"
    }

    function selectedNode() {
        if (controller === null || root.selectedUiNodeIndex < 0) {
            return null
        }
        for (let i = 0; i < controller.filteredUiNodes.length; ++i) {
            const node = controller.filteredUiNodes[i]
            if (node.index === root.selectedUiNodeIndex) {
                return node
            }
        }
        for (let j = 0; j < controller.uiNodes.length; ++j) {
            const fallbackNode = controller.uiNodes[j]
            if (fallbackNode.index === root.selectedUiNodeIndex) {
                return fallbackNode
            }
        }
        return null
    }

    function selectedNodeTitle() {
        const node = root.selectedNode()
        if (node === null) {
            return qsTr("No node selected")
        }
        const type = node.type !== undefined && node.type.length > 0 ? node.type : qsTr("Node")
        const id = node.id !== undefined && node.id.length > 0 ? node.id : ""
        return id.length > 0 ? type + "  " + id : type + "  #" + node.index
    }

    function selectedNodeBounds() {
        const node = root.selectedNode()
        if (node === null) {
            return "-"
        }
        if (node.bounds !== undefined && node.bounds.length > 0) {
            return node.bounds
        }
        return "[" + node.left + "," + node.top + "][" + node.right + "," + node.bottom + "]"
    }

    function selectedNodeText() {
        const node = root.selectedNode()
        if (node === null) {
            return "-"
        }
        const label = node.label !== undefined ? String(node.label).trim() : ""
        if (label.length > 0) {
            return label
        }
        const id = node.id !== undefined ? String(node.id).trim() : ""
        return id.length > 0 ? id : "-"
    }

    function selectedNodeValue(key, fallback) {
        const node = root.selectedNode()
        if (node === null || node[key] === undefined || node[key] === null) {
            return fallback
        }
        const value = String(node[key]).trim()
        return value.length > 0 ? value : fallback
    }

    function selectedNodeBoolText(key) {
        const node = root.selectedNode()
        if (node === null || node[key] === undefined || node[key] === null) {
            return "-"
        }
        return node[key] ? "true" : "false"
    }

    function selectedNodeSize() {
        const node = root.selectedNode()
        if (node === null || node.width === undefined || node.height === undefined) {
            return "-"
        }
        return node.width + " x " + node.height
    }

    function selectedNodeSizeText() {
        const node = root.selectedNode()
        if (node === null || node.width === undefined || node.height === undefined) {
            return "-"
        }
        return "W " + node.width + "    H " + node.height
    }

    function selectedNodePositionText() {
        const node = root.selectedNode()
        if (node === null || node.left === undefined || node.top === undefined) {
            return "-"
        }
        return "X " + node.left + "    Y " + node.top
    }

    function selectedNodeSummary() {
        const node = root.selectedNode()
        if (node === null) {
            return qsTr("No node selected")
        }
        return root.selectedNodeTitle() + "  " + root.selectedNodeBounds()
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

    function resetLaunchFieldsForPackageChange() {
        root.lastAppliedBundleName = ""
        root.lastAppliedAbilityName = ""
        if (!root.launchFieldsReady) {
            return
        }
        bundleField.text = ""
        abilityField.text = ""
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
