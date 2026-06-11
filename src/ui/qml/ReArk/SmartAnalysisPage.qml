import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    property var agentController: null
    property string draftText: ""
    property int copiedMessageIndex: -1

    readonly property bool darkTheme: Material.theme === Material.Dark
    readonly property bool agentAvailable: agentController !== null && agentController.available
    readonly property bool agentRunning: agentController !== null && agentController.running
    readonly property var agentMessages: agentController !== null ? agentController.messages : []
    readonly property bool hasMessages: agentController !== null && agentController.hasMessages
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
    readonly property color userBubbleColor: darkTheme ? "#24405f" : "#dbeafe"
    readonly property color assistantBubbleColor: darkTheme ? "#151d28" : "#ffffff"
    readonly property color primaryTextColor: darkTheme ? "#eef5ff" : "#0f172a"
    readonly property color secondaryTextColor: darkTheme ? "#98a7bb" : "#748094"
    readonly property color mutedTextColor: darkTheme ? "#718095" : "#8a96a8"
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
    readonly property int contentWidth: Math.min(930, Math.max(660, width - 264))

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
        enabled: !root.agentRunning && root.hasMessages
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
            root.draftText = ""
            promptInput.forceActiveFocus()
        }
    }

    Label {
        id: emptyTitle

        width: root.contentWidth
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: composer.top
        anchors.bottomMargin: 18
        visible: !root.hasMessages
        text: qsTr("What would you like to ask?")
        color: root.primaryTextColor
        font.pixelSize: 34
        font.weight: Font.Bold
        horizontalAlignment: Text.AlignHCenter
    }

    ListView {
        id: chatList

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: composer.top
        anchors.topMargin: 66
        anchors.bottomMargin: 18
        visible: root.hasMessages
        clip: true
        model: root.agentMessages
        spacing: 16
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: 1200

        header: Item {
            width: chatList.width
            height: 10
        }

        footer: Item {
            width: chatList.width
            height: 12
        }

        delegate: Item {
            id: messageDelegate

            required property int index
            required property var modelData

            readonly property bool userMessage: modelData.role === "user"
            readonly property bool streaming: modelData.state === "streaming"
            readonly property string messageText: modelData.text || ""
            readonly property string messageTime: modelData.time || ""
            readonly property bool copied: root.copiedMessageIndex === index
            readonly property real maxBubbleWidth: Math.min(root.contentWidth * 0.78, 720)

            width: chatList.width
            height: messageColumn.implicitHeight

            ColumnLayout {
                id: messageColumn

                width: root.contentWidth
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 7

                Label {
                    Layout.alignment: messageDelegate.userMessage ? Qt.AlignRight : Qt.AlignLeft
                    text: messageDelegate.userMessage ? qsTr("You") : qsTr("ReArk Agent")
                    color: root.mutedTextColor
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    Layout.alignment: messageDelegate.userMessage ? Qt.AlignRight : Qt.AlignLeft
                    Layout.maximumWidth: messageDelegate.maxBubbleWidth
                    implicitWidth: Math.min(messageBody.implicitWidth + 30, messageDelegate.maxBubbleWidth)
                    implicitHeight: messageBody.implicitHeight + 22
                    radius: 8
                    color: messageDelegate.userMessage ? root.userBubbleColor : root.assistantBubbleColor
                    border.width: messageDelegate.userMessage ? 0 : 1
                    border.color: root.borderColor
                    layer.enabled: !messageDelegate.userMessage
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowBlur: 0.35
                        shadowOpacity: root.darkTheme ? 0.16 : 0.08
                        shadowVerticalOffset: 3
                    }

                    TextEdit {
                        id: messageBody

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: 15
                        anchors.rightMargin: 15
                        anchors.topMargin: 11
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.PlainText
                        text: messageDelegate.messageText.length > 0
                              ? messageDelegate.messageText
                              : (messageDelegate.streaming ? qsTr("Thinking...") : "")
                        color: root.primaryTextColor
                        selectedTextColor: "#ffffff"
                        selectionColor: root.accentColor
                        font.pixelSize: 13
                        opacity: messageDelegate.messageText.length > 0 ? 1.0 : 0.66
                    }
                }

                RowLayout {
                    Layout.alignment: messageDelegate.userMessage ? Qt.AlignRight : Qt.AlignLeft
                    spacing: 8

                    AbstractButton {
                        id: copyButton

                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        padding: 0
                        hoverEnabled: true
                        enabled: messageDelegate.messageText.length > 0
                        opacity: enabled ? (hovered || messageDelegate.copied ? 1.0 : 0.68) : 0.34

                        Accessible.name: qsTr("Copy message")
                        ToolTip.text: qsTr("Copy")
                        ToolTip.visible: hovered && enabled && !messageDelegate.copied
                        ToolTip.delay: 450

                        background: Rectangle {
                            radius: 4
                            color: messageDelegate.copied
                                ? (root.darkTheme ? "#263952" : "#dbeafe")
                                : copyButton.hovered
                                ? (root.darkTheme ? "#202b3a" : "#e7edf7")
                                : "transparent"
                        }

                        contentItem: Icon {
                            anchors.centerIn: parent
                            name: messageDelegate.copied ? "check" : "copy"
                            width: 10
                            height: 10
                            color: messageDelegate.copied ? root.accentColor : root.mutedTextColor
                        }

                        onClicked: {
                            if (root.agentController !== null) {
                                root.agentController.copyTextToClipboard(messageDelegate.messageText)
                            }
                            root.copiedMessageIndex = messageDelegate.index
                            copiedResetTimer.restart()
                        }
                    }

                    Label {
                        text: messageDelegate.messageTime
                        color: root.mutedTextColor
                        font.pixelSize: 11
                        verticalAlignment: Text.AlignVCenter
                        visible: text.length > 0
                    }
                }
            }
        }

        onCountChanged: Qt.callLater(positionViewAtEnd)
        onContentHeightChanged: if (root.agentRunning) Qt.callLater(positionViewAtEnd)
    }

    Rectangle {
        id: composer

        width: root.contentWidth
        height: root.hasMessages ? 124 : 130
        anchors.horizontalCenter: parent.horizontalCenter
        radius: 8
        color: root.panelColor
        border.width: 1
        border.color: promptInput.activeFocus ? root.accentColor : root.borderColor
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.55
            shadowOpacity: root.panelShadowOpacity
            shadowVerticalOffset: 5
        }

        states: [
            State {
                when: root.hasMessages
                AnchorChanges {
                    target: composer
                    anchors.bottom: root.bottom
                }
                PropertyChanges {
                    target: composer
                    anchors.bottomMargin: 34
                    anchors.verticalCenterOffset: 0
                }
            },
            State {
                when: !root.hasMessages
                AnchorChanges {
                    target: composer
                    anchors.verticalCenter: root.verticalCenter
                }
                PropertyChanges {
                    target: composer
                    anchors.bottomMargin: 0
                    anchors.verticalCenterOffset: 82
                }
            }
        ]

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
            text: root.draftText

            onTextChanged: {
                if (root.draftText !== text) {
                    root.draftText = text
                }
            }

            Keys.onPressed: function(event) {
                if (event.key !== Qt.Key_Return && event.key !== Qt.Key_Enter) {
                    return
                }
                if (event.modifiers & Qt.ControlModifier) {
                    promptInput.insert(promptInput.cursorPosition, "\n")
                    event.accepted = true
                    return
                }
                root.submitPrompt()
                event.accepted = true
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
            disabledColor: root.darkTheme ? "#667181" : "#b8c2d3"
            iconColor: "#ffffff"
            toolTipText: root.agentRunning
                ? qsTr("Cancel")
                : (!root.agentAvailable ? qsTr("Smart analysis unavailable") : qsTr("Send"))
            enabled: root.agentAvailable && (root.agentRunning || root.draftText.trim().length > 0)
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

    Connections {
        target: root.agentController
        ignoreUnknownSignals: true

        function onMessagesChanged() {
            if (root.hasMessages) {
                Qt.callLater(chatList.positionViewAtEnd)
            }
        }
    }

    Timer {
        id: copiedResetTimer

        interval: 1800
        repeat: false
        onTriggered: root.copiedMessageIndex = -1
    }

    function submitPrompt() {
        if (!root.agentAvailable || root.agentRunning) {
            return
        }
        const text = root.draftText.trim()
        if (text.length <= 0) {
            return
        }
        root.agentController.ask(text)
        root.draftText = ""
        promptInput.text = ""
        Qt.callLater(chatList.positionViewAtEnd)
    }
}
