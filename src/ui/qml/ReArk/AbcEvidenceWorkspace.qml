import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    property string packageName: ""
    property string highlightTheme: "GitHub Dark"
    property string activePath: ""
    property string activeText: ""
    readonly property bool darkTheme: Material.theme === Material.Dark
    readonly property color pageColor: darkTheme ? "#1e1e1e" : "#f5f7f8"
    readonly property color headerColor: darkTheme ? "#181a1d" : "#eef3f5"
    readonly property color dividerColor: darkTheme ? "#34383d" : "#d5dcdf"
    readonly property color hoverColor: darkTheme ? "#282b30" : "#e8eef0"
    readonly property color secondaryTextColor: darkTheme ? "#a6a6a6" : "#5f6872"

    signal backRequested()

    color: pageColor

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: root.headerColor
            border.width: 1
            border.color: root.dividerColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                ToolButton {
                    id: backButton

                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    padding: 0
                    hoverEnabled: true
                    ToolTip.text: qsTr("Back")
                    ToolTip.visible: hovered
                    onClicked: root.backRequested()

                    background: Rectangle {
                        radius: 3
                        color: parent.hovered ? root.hoverColor : "transparent"
                    }

                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            name: "arrow-left"
                            color: Material.foreground
                            opacity: backButton.enabled ? 0.84 : 0.3
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("ABC Evidence")
                        color: Material.foreground
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.packageName.length > 0
                        text: root.packageName
                        color: root.secondaryTextColor
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                    }
                }

                BusyIndicator {
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    running: decompilerController.abcEvidenceBusy
                    visible: running
                }
            }
        }

        AbcEvidenceDrawer {
            Layout.fillWidth: true
            Layout.fillHeight: true
            darkTheme: root.darkTheme
            highlightTheme: root.highlightTheme
            activePath: root.activePath
            activeText: root.activeText
            framed: false
            showHeader: false
            onCloseRequested: root.backRequested()
        }
    }
}
