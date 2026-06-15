import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: licenseWindow

    width: 560
    height: 560
    minimumWidth: 560
    minimumHeight: 400
    visible: false
    title: qsTr("License")
    modality: Qt.ApplicationModal
    flags: Qt.WindowCloseButtonHint | Qt.CustomizeWindowHint | Qt.Dialog | Qt.WindowTitleHint

    property string currentTheme: "dark"
    property string licenseText: ""
    property bool copied: false
    property var closeCallback: null
    readonly property bool darkTheme: currentTheme === "system"
                                      ? Qt.styleHints.colorScheme === Qt.Dark
                                      : currentTheme === "dark"
    readonly property color backgroundColor: darkTheme ? "#1e1e1e" : "#ffffff"
    readonly property color dividerColor: darkTheme ? "#34383d" : "#d5dcdf"
    readonly property color secondaryTextColor: darkTheme ? "#a6a6a6" : "#5f6872"
    readonly property color buttonHoverColor: darkTheme ? "#2a2d31" : "#eceff1"
    readonly property color buttonPressedColor: darkTheme ? "#34383d" : "#dde3e7"

    color: backgroundColor
    Material.theme: darkTheme ? Material.Dark : Material.Light
    Material.accent: "#3f8fd2"

    onClosing: {
        if (closeCallback) {
            closeCallback()
        }
        destroy()
    }

    Component.onCompleted: loadLicense()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Text {
            Layout.fillWidth: true
            Layout.margins: 24
            Layout.bottomMargin: 14
            text: qsTr("The complete Apache License 2.0 text for ReArk.")
            color: licenseWindow.secondaryTextColor
            font.pointSize: 10
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: licenseWindow.dividerColor
        }

        Flickable {
            id: licenseFlickable

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: width
            contentHeight: licenseTextEdit.height + 44
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            TextEdit {
                id: licenseTextEdit

                x: 24
                y: 22
                width: Math.max(0, licenseFlickable.width - 48)
                height: implicitHeight
                text: licenseWindow.licenseText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                color: Material.foreground
                selectedTextColor: "#ffffff"
                selectionColor: Material.accent
                font.family: "Cascadia Mono, Consolas, Courier New, monospace"
                font.pointSize: 9
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: licenseWindow.dividerColor
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            TextActionButton {
                text: licenseWindow.copied ? qsTr("Copied") : qsTr("Copy")
                enabled: licenseWindow.licenseText.length > 0
                onClicked: {
                    applicationController.copyTextToClipboard(licenseWindow.licenseText)
                    licenseWindow.copied = true
                    copiedResetTimer.restart()
                }
            }

            TextActionButton {
                text: qsTr("Close")
                onClicked: licenseWindow.close()
            }
        }
    }

    component TextActionButton: AbstractButton {
        id: actionButton

        implicitWidth: Math.max(72, contentItem.implicitWidth + 28)
        implicitHeight: 34
        hoverEnabled: true
        font.pointSize: 10
        contentItem: Text {
            text: actionButton.text
            color: actionButton.enabled ? Material.foreground : licenseWindow.secondaryTextColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font: actionButton.font
        }
        background: Rectangle {
            radius: 4
            color: !actionButton.enabled
                   ? "transparent"
                   : actionButton.down
                     ? licenseWindow.buttonPressedColor
                     : actionButton.hovered
                       ? licenseWindow.buttonHoverColor
                       : "transparent"
            border.width: 1
            border.color: actionButton.hovered || actionButton.down
                          ? licenseWindow.dividerColor
                          : "transparent"
        }
    }

    Timer {
        id: copiedResetTimer

        interval: 1600
        repeat: false
        onTriggered: licenseWindow.copied = false
    }

    function loadLicense() {
        const text = applicationController.licenseText()
        if (text.length > 0) {
            licenseWindow.licenseText = text
            Qt.callLater(function() {
                licenseFlickable.contentY = 0
                licenseTextEdit.cursorPosition = 0
            })
        } else {
            licenseWindow.licenseText = qsTr("Unable to load the license text.")
        }
    }
}
