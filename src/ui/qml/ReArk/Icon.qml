import QtQuick
import QtQuick.Controls.impl

Item {
    id: root

    property string name: ""
    property color color: "#3f8fd2"
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
        if (name === "arrow-left") {
            return "qrc:/icons/fontawesome/arrow-left.svg"
        }
        if (name === "close") {
            return "qrc:/icons/close-small.svg"
        }
        if (name === "new-chat") {
            return "qrc:/icons/fontawesome/square-plus.svg"
        }
        if (name === "copy") {
            return "qrc:/icons/fontawesome/copy.svg"
        }
        if (name === "runtime-arrow-left") {
            return "qrc:/icons/runtime-arrow-left.svg"
        }
        if (name === "runtime-copy") {
            return "qrc:/icons/runtime-copy.svg"
        }
        if (name === "runtime-fit") {
            return "qrc:/icons/runtime-fit.svg"
        }
        if (name === "runtime-grid") {
            return "qrc:/icons/runtime-grid.svg"
        }
        if (name === "runtime-home") {
            return "qrc:/icons/runtime-home.svg"
        }
        if (name === "runtime-minus") {
            return "qrc:/icons/runtime-minus.svg"
        }
        if (name === "runtime-more") {
            return "qrc:/icons/runtime-more.svg"
        }
        if (name === "runtime-package") {
            return "qrc:/icons/runtime-package.svg"
        }
        if (name === "runtime-plus") {
            return "qrc:/icons/runtime-plus.svg"
        }
        if (name === "runtime-refresh-cw") {
            return "qrc:/icons/runtime-refresh-cw.svg"
        }
        if (name === "runtime-rotate-ccw") {
            return "qrc:/icons/runtime-rotate-ccw.svg"
        }
        if (name === "runtime-scan") {
            return "qrc:/icons/runtime-scan.svg"
        }
        if (name === "runtime-search") {
            return "qrc:/icons/runtime-search.svg"
        }
        if (name === "runtime-terminal") {
            return "qrc:/icons/runtime-terminal.svg"
        }
        if (name === "folder-open") {
            return "qrc:/icons/folder-open-white.svg"
        }
        if (name === "eye") {
            return "qrc:/icons/eye.svg"
        }
        if (name === "eye-off") {
            return "qrc:/icons/eye-off.svg"
        }
        if (name === "check") {
            return "qrc:/icons/fontawesome/check.svg"
        }
        if (name === "pencil") {
            return "qrc:/icons/fontawesome/pencil.svg"
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
