import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

ToolButton {
    id: root

    property string buttonType: "minimize"
    readonly property bool darkTheme: Material.theme === Material.Dark
    readonly property bool aiButton: buttonType === "ai"
    readonly property string iconGlyph: {
        if (buttonType === "ai") {
            return ""
        }
        if (buttonType === "minimize") {
            return "\uE921"
        }
        if (buttonType === "maximize") {
            return "\uE922"
        }
        if (buttonType === "restore") {
            return "\uE923"
        }
        return "\uE8BB"
    }
    readonly property color iconColor: root.hovered && root.buttonType === "close"
                                      ? "#ffffff"
                                      : (root.darkTheme ? (root.aiButton ? "#8fded8" : "#d9dde3") : (root.aiButton ? "#006b67" : "#202020"))
    readonly property color hoverColor: root.buttonType === "close"
                                      ? "#c42b1c"
                                      : (root.darkTheme ? "#555555" : "#e5e5e5")

    implicitWidth: 46
    implicitHeight: 32
    display: AbstractButton.IconOnly
    padding: 0

    background: Rectangle {
        color: root.hovered ? root.hoverColor : "transparent"
    }

    contentItem: Item {
        Text {
            visible: !root.aiButton
            anchors.centerIn: parent
            text: root.iconGlyph
            color: root.iconColor
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            renderType: Text.NativeRendering
        }

        Row {
            visible: root.aiButton
            anchors.centerIn: parent
            spacing: 2

            Text {
                text: "\u2726"
                color: root.iconColor
                font.family: "Segoe UI Symbol"
                font.pixelSize: 10
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
                renderType: Text.NativeRendering
            }

            Text {
                text: "AI"
                color: root.iconColor
                font.family: "Segoe UI"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
                renderType: Text.NativeRendering
            }
        }
    }
}
