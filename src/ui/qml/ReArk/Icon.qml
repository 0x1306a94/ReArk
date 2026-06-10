import QtQuick
import QtQuick.Controls.impl

Item {
    id: root

    property string name: ""
    property color color: "#1f3354"
    property real strokeWidth: 0
    property bool filled: false

    readonly property string iconSource: {
        if (name === "paperclip") {
            return "qrc:/icons/fontawesome/paperclip.svg"
        }
        if (name === "diamond") {
            return "qrc:/icons/fontawesome/gem.svg"
        }
        if (name === "arrow-up") {
            return "qrc:/icons/fontawesome/arrow-up.svg"
        }
        if (name === "close") {
            return "qrc:/icons/close-small.svg"
        }
        if (name === "new-chat") {
            return "qrc:/icons/fontawesome/square-plus.svg"
        }
        return ""
    }

    implicitWidth: 16
    implicitHeight: 16
    visible: iconSource.length > 0

    ColorImage {
        anchors.fill: parent
        source: root.iconSource
        color: root.color
        sourceSize.width: Math.max(1, Math.round(root.width * 2))
        sourceSize.height: Math.max(1, Math.round(root.height * 2))
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }
}
