import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: updateWindow

    width: 560
    height: 520
    minimumWidth: 460
    minimumHeight: 420
    visible: false
    title: qsTr("Software Update")
    modality: Qt.ApplicationModal
    flags: Qt.WindowCloseButtonHint | Qt.CustomizeWindowHint | Qt.Dialog | Qt.WindowTitleHint

    property string currentTheme: "dark"
    property string version: ""
    property string changelog: ""
    property string releaseUrl: ""
    property string releaseDate: ""
    property var closeCallback: null
    readonly property bool darkTheme: currentTheme === "system"
                                      ? Qt.styleHints.colorScheme === Qt.Dark
                                      : currentTheme === "dark"
    readonly property color backgroundColor: darkTheme ? "#1e1e1e" : "#ffffff"
    readonly property color panelColor: darkTheme ? "#202226" : "#f5f7f8"
    readonly property color dividerColor: darkTheme ? "#34383d" : "#d5dcdf"
    readonly property color secondaryTextColor: darkTheme ? "#a6a6a6" : "#5f6872"
    readonly property color buttonHoverColor: darkTheme ? "#2a2d31" : "#eceff1"
    readonly property color buttonPressedColor: darkTheme ? "#34383d" : "#dde3e7"
    readonly property color primaryButtonColor: darkTheme ? "#24384a" : "#e8f2fb"
    readonly property color primaryButtonHoverColor: darkTheme ? "#2b4359" : "#dcecf8"
    readonly property color primaryButtonPressedColor: darkTheme ? "#33506a" : "#cfe4f4"
    readonly property color primaryButtonBorderColor: darkTheme ? "#3f8fd2" : "#9ac7e8"
    readonly property color primaryButtonTextColor: darkTheme ? "#d9efff" : "#1e5f91"

    color: backgroundColor
    Material.theme: darkTheme ? Material.Dark : Material.Light
    Material.accent: "#3f8fd2"
    onClosing: {
        if (closeCallback) {
            closeCallback()
        }
        Qt.callLater(function() {
            updateWindow.destroy()
        })
    }

    Rectangle {
        anchors.fill: parent
        color: updateWindow.backgroundColor

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: qsTr("What's New")
                color: Material.foreground
                font.pointSize: 18
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: updateWindow.releaseSummary()
                color: updateWindow.secondaryTextColor
                font.pointSize: 10
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                id: changelogPanel

                Layout.fillWidth: true
                Layout.fillHeight: true
                color: updateWindow.panelColor
                radius: 4
                border.width: 1
                border.color: updateWindow.dividerColor
                clip: true

                Flickable {
                    id: changelogFlickable

                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.topMargin: 10
                    anchors.rightMargin: changelogScrollbar.visible ? 22 : 12
                    anchors.bottomMargin: 10
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    boundsMovement: Flickable.StopAtBounds
                    contentWidth: width
                    contentHeight: changelogContent.implicitHeight

                    MarkdownMessage {
                        id: changelogContent

                        width: changelogFlickable.width
                        markdown: updateWindow.changelog
                        markdownEnabled: true
                        darkTheme: updateWindow.darkTheme
                        textColor: Material.foreground
                        accentColor: Material.accent
                        textPixelSize: 13
                        emptyText: qsTr("No changelog information is available for this release.")
                    }
                }

                Rectangle {
                    id: changelogScrollbar

                    readonly property real contentExtent: Math.max(1, changelogFlickable.contentHeight)
                    readonly property real scrollRange: Math.max(0, changelogFlickable.contentHeight - changelogFlickable.height)

                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    width: 6
                    radius: 3
                    visible: changelogFlickable.contentHeight > changelogFlickable.height + 1
                    color: "transparent"

                    Rectangle {
                        width: parent.width
                        height: Math.min(
                                    parent.height,
                                    Math.max(28, parent.height * changelogFlickable.height / changelogScrollbar.contentExtent))
                        y: changelogScrollbar.scrollRange <= 0
                           ? 0
                           : (parent.height - height) * changelogFlickable.contentY / changelogScrollbar.scrollRange
                        radius: 3
                        color: updateWindow.darkTheme ? "#5b626a" : "#9aa6ad"
                        opacity: 0.62
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item {
                    Layout.fillWidth: true
                }

                UpdateButton {
                    text: qsTr("Open Release Page")
                    primary: true
                    enabled: updateWindow.releaseUrl.length > 0
                    onClicked: {
                        updateController.openReleasePage(updateWindow.releaseUrl)
                        updateWindow.close()
                    }
                }

                UpdateButton {
                    text: qsTr("Later")
                    onClicked: updateWindow.close()
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }

    component UpdateButton: AbstractButton {
        id: button

        property bool primary: false

        implicitWidth: Math.max(72, contentItem.implicitWidth + 28)
        implicitHeight: 34
        hoverEnabled: true
        font.pointSize: 10

        contentItem: Text {
            text: button.text
            color: button.enabled && button.primary
                   ? updateWindow.primaryButtonTextColor
                   : button.enabled
                   ? Material.foreground
                   : updateWindow.secondaryTextColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font: button.font
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 4
            color: !button.enabled
                   ? "transparent"
                   : button.down
                     ? (button.primary ? updateWindow.primaryButtonPressedColor : updateWindow.buttonPressedColor)
                     : button.hovered
                       ? (button.primary ? updateWindow.primaryButtonHoverColor : updateWindow.buttonHoverColor)
                       : button.primary
                         ? updateWindow.primaryButtonColor
                         : "transparent"
            border.width: 1
            border.color: button.primary && button.enabled
                          ? updateWindow.primaryButtonBorderColor
                          : button.hovered || button.down
                          ? updateWindow.dividerColor
                          : "transparent"
        }
    }

    function formatReleaseDate(value) {
        const date = new Date(value)
        if (isNaN(date.getTime())) {
            return value
        }
        return Qt.formatDateTime(date, "yyyy-MM-dd")
    }

    function releaseSummary() {
        if (updateWindow.releaseDate.length > 0) {
            return qsTr("New version: ReArk <b>%1</b> | Release date: %2")
                    .arg(updateWindow.version)
                    .arg(updateWindow.formatReleaseDate(updateWindow.releaseDate))
        }
        return qsTr("New version: ReArk <b>%1</b>").arg(updateWindow.version)
    }
}
