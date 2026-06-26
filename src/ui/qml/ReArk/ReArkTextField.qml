import QtQuick

Rectangle {
    id: root

    property alias text: input.text
    property alias echoMode: input.echoMode
    property alias validator: input.validator
    property alias inputMethodHints: input.inputMethodHints
    property string placeholderText: ""
    property color backgroundColor: "#ffffff"
    property color borderColor: "#d6d8dc"
    property color focusBorderColor: "#3b82f6"
    property color textColor: "#1f2937"
    property color placeholderColor: "#7a7f87"
    property color selectionColor: "#3b82f6"
    property color selectedTextColor: "#ffffff"
    property int textPixelSize: 13
    property int horizontalPadding: 8
    readonly property bool acceptableInput: input.acceptableInput

    signal accepted()

    implicitWidth: 160
    implicitHeight: 32
    radius: 2
    color: backgroundColor
    border.width: 1
    border.color: input.activeFocus ? focusBorderColor : borderColor
    clip: true

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: root.horizontalPadding
        anchors.rightMargin: root.horizontalPadding
        visible: input.text.length === 0
        text: root.placeholderText
        color: root.placeholderColor
        opacity: root.enabled ? 1.0 : 0.6
        font.pixelSize: root.textPixelSize
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    TextInput {
        id: input

        anchors.fill: parent
        anchors.leftMargin: root.horizontalPadding
        anchors.rightMargin: root.horizontalPadding
        clip: true
        enabled: root.enabled
        selectByMouse: true
        color: root.enabled ? root.textColor : root.placeholderColor
        selectionColor: root.selectionColor
        selectedTextColor: root.selectedTextColor
        font.pixelSize: root.textPixelSize
        verticalAlignment: TextInput.AlignVCenter
        onAccepted: root.accepted()
    }
}
