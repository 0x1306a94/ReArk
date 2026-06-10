import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    property var agentController: null

    readonly property bool darkTheme: Material.theme === Material.Dark
    readonly property bool agentAvailable: agentController !== null && agentController.available
    readonly property bool agentRunning: agentController !== null && agentController.running
    readonly property string agentTranscript: agentController !== null ? agentController.transcript : ""
    readonly property string agentError: agentController !== null ? agentController.errorMessage : ""
    readonly property string agentStatus: agentController !== null ? agentController.status : ""
    readonly property string unavailableStatus: agentStatus.length > 0
            ? agentStatus
            : qsTr("Smart analysis is temporarily unavailable.")
    readonly property string statusText: agentError.length > 0
            ? agentError
            : (!agentAvailable ? unavailableStatus : agentStatus)
    readonly property color pageTopColor: darkTheme ? "#111923" : "#e8f0fb"
    readonly property color pageBottomColor: darkTheme ? "#0d121a" : "#f4f8fc"
    readonly property color panelColor: darkTheme ? "#151d28" : "#fbfcff"
    readonly property color panelHoverColor: darkTheme ? "#1a2532" : "#ffffff"
    readonly property color primaryTextColor: darkTheme ? "#eef5ff" : "#0f172a"
    readonly property color secondaryTextColor: darkTheme ? "#98a7bb" : "#748094"
    readonly property color borderColor: darkTheme ? "#2c3848" : "#d3dce9"
    readonly property color iconColor: darkTheme ? "#cbd8ea" : "#14213d"
    readonly property color accentColor: darkTheme ? "#6f8cff" : "#5d83f4"
    readonly property color accentHoverColor: darkTheme ? "#809aff" : "#4e74e4"
    readonly property color accentPressedColor: darkTheme ? "#5874e7" : "#446bdd"
    readonly property color newChatColor: darkTheme ? "#182231" : "#f8fbff"
    readonly property color newChatHoverColor: darkTheme ? "#202c3d" : "#ffffff"
    readonly property color newChatBorderColor: darkTheme ? "#344255" : "#d7e0ed"
    readonly property real panelShadowOpacity: darkTheme ? 0.28 : 0.13
    readonly property real buttonShadowOpacity: darkTheme ? 0.22 : 0.1

    gradient: Gradient {
        GradientStop {
            position: 0
            color: root.pageTopColor
        }
        GradientStop {
            position: 1
            color: root.pageBottomColor
        }
    }

    Button {
        id: newChatButton

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 18
        anchors.rightMargin: 24
        width: Math.max(106, newChatContent.implicitWidth + 26)
        height: 34
        padding: 0
        hoverEnabled: true
        enabled: !root.agentRunning
        opacity: enabled ? 1.0 : 0.55
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.75
            shadowOpacity: root.buttonShadowOpacity
            shadowVerticalOffset: 5
        }

        background: Rectangle {
            radius: height / 2
            color: newChatButton.hovered ? root.newChatHoverColor : root.newChatColor
            border.width: 1
            border.color: root.newChatBorderColor
        }

        contentItem: Row {
            id: newChatContent

            anchors.centerIn: parent
            spacing: 7

            Icon {
                name: "new-chat"
                color: root.primaryTextColor
                width: 13
                height: 13
                strokeWidth: 1.8
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: qsTr("New Chat")
                color: root.primaryTextColor
                font.pixelSize: 13
                font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
                renderType: Text.NativeRendering
            }
        }

        onClicked: {
            if (root.agentController !== null) {
                root.agentController.newChat()
            }
            promptInput.text = ""
            promptInput.forceActiveFocus()
        }
    }

    ColumnLayout {
        width: Math.min(930, Math.max(660, parent.width - 264))
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 20
        spacing: 17

        Label {
            Layout.fillWidth: true
            text: qsTr("What would you like to ask?")
            color: root.primaryTextColor
            font.pixelSize: 34
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.agentTranscript.length > 0 ? 190 : 0
            visible: root.agentTranscript.length > 0
            radius: 8
            color: root.panelColor
            border.width: 1
            border.color: root.borderColor
            clip: true

            ScrollView {
                anchors.fill: parent
                anchors.margins: 14
                clip: true

                TextArea {
                    text: root.agentTranscript
                    readOnly: true
                    wrapMode: TextEdit.Wrap
                    color: root.primaryTextColor
                    selectedTextColor: "#ffffff"
                    selectionColor: root.accentColor
                    font.pixelSize: 13
                    background: null
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            radius: 8
            color: root.panelColor
            border.width: 1
            border.color: root.borderColor
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 0.55
                shadowOpacity: root.panelShadowOpacity
                shadowVerticalOffset: 5
            }

            TextEdit {
                id: promptInput

                anchors.left: parent.left
                anchors.right: sendButton.left
                anchors.top: parent.top
                anchors.bottom: toolRow.top
                anchors.leftMargin: 18
                anchors.rightMargin: 16
                anchors.topMargin: 18
                anchors.bottomMargin: 8
                wrapMode: TextEdit.Wrap
                color: root.primaryTextColor
                selectedTextColor: "#ffffff"
                selectionColor: root.accentColor
                cursorVisible: activeFocus
                font.pixelSize: 13
                enabled: !root.agentRunning

                Keys.onPressed: function(event) {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            && (event.modifiers & Qt.ControlModifier)) {
                        root.submitPrompt()
                        event.accepted = true
                    }
                }
            }

            Label {
                anchors.left: promptInput.left
                anchors.top: promptInput.top
                text: qsTr("Ask anything about this app")
                color: root.secondaryTextColor
                font.pixelSize: 12
                visible: promptInput.text.length === 0
            }

            Label {
                anchors.left: promptInput.left
                anchors.right: sendButton.left
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 14
                text: root.statusText
                color: root.agentError.length > 0 ? "#ef6f75" : root.secondaryTextColor
                font.pixelSize: 11
                elide: Text.ElideRight
                visible: text.length > 0
                    && (!root.agentAvailable || root.agentRunning || root.agentError.length > 0 || !toolRow.visible)
            }

            Row {
                id: toolRow

                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 26
                anchors.bottomMargin: 23
                spacing: 19
                visible: root.agentAvailable && root.agentError.length === 0 && !root.agentRunning

                Icon {
                    name: "paperclip"
                    color: root.iconColor
                    width: 15
                    height: 15
                    strokeWidth: 1.9
                    anchors.verticalCenter: parent.verticalCenter
                }

                Icon {
                    name: "diamond"
                    color: root.iconColor
                    width: 15
                    height: 15
                    strokeWidth: 1.9
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            RoundIconButton {
                id: sendButton

                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 16
                anchors.bottomMargin: 13
                diameter: 38
                iconSize: 17
                iconName: root.agentRunning ? "close" : "arrow-up"
                backgroundColor: root.accentColor
                hoverColor: root.accentHoverColor
                pressedColor: root.accentPressedColor
                iconColor: "#ffffff"
                toolTipText: root.agentRunning
                    ? qsTr("Cancel")
                    : (!root.agentAvailable ? qsTr("Smart analysis unavailable") : qsTr("Send"))
                enabled: root.agentAvailable && (root.agentRunning || promptInput.text.trim().length > 0)
                opacity: enabled ? 1.0 : 0.55
                onClicked: {
                    if (root.agentRunning) {
                        root.agentController.cancel()
                    } else {
                        root.submitPrompt()
                    }
                }
            }
        }
    }

    function submitPrompt() {
        if (!root.agentAvailable || root.agentRunning) {
            return
        }
        const text = promptInput.text.trim()
        if (text.length <= 0) {
            return
        }
        root.agentController.ask(text)
        promptInput.text = ""
    }
}
