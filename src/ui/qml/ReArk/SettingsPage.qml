import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: root

    property var settingsController: null
    property var signingController: null

    readonly property bool darkTheme: Material.theme === Material.Dark
    readonly property color pageColor: darkTheme ? "#1e1e1e" : "#f3f3f3"
    readonly property color sidebarColor: darkTheme ? "#1b1d20" : "#f8f8f8"
    readonly property color rowHoverColor: darkTheme ? "#282b30" : "#e8e8e8"
    readonly property color rowSelectedColor: darkTheme ? "#2a3038" : "#e4e6f1"
    readonly property color primaryTextColor: darkTheme ? "#d8d8d8" : "#1f1f1f"
    readonly property color titleTextColor: darkTheme ? "#e7e7e7" : "#1f1f1f"
    readonly property color secondaryTextColor: darkTheme ? "#a6a6a6" : "#5f5f5f"
    readonly property color subtleTextColor: darkTheme ? "#8a8a8a" : "#6f6f6f"
    readonly property color borderColor: darkTheme ? "#34383d" : "#d0d0d0"
    readonly property color inputColor: darkTheme ? "#202226" : "#ffffff"
    readonly property color inputFocusColor: darkTheme ? "#3f8fd2" : "#2f80c1"
    readonly property color buttonColor: darkTheme ? "#3f8fd2" : "#2f80c1"
    readonly property color buttonHoverColor: darkTheme ? "#52a0df" : "#2b72ad"
    readonly property color dangerTextColor: darkTheme ? "#f48771" : "#a1260d"
    readonly property color successTextColor: darkTheme ? "#89d185" : "#187c31"
    readonly property color warningTextColor: darkTheme ? "#d7ba7d" : "#996f00"

    property bool showApiKey: false
    property bool showEmbeddingApiKey: false
    property bool showHarmonyKeystorePassword: false
    property bool showHarmonyKeyPassword: false
    property string searchQuery: ""
    property string saveMessage: ""
    property string signingSaveMessage: ""
    property string signingMaterialStatusText: ""
    property string signingMaterialStatusTone: "error"
    property string activeProviderDraftId: ""
    property var providerDrafts: ({})
    property bool loadingDraft: false
    readonly property var providerOptions: settingsController !== null
            ? settingsController.agentProviders
            : []
    readonly property string validationMessage: settingsController !== null
            ? settingsController.agentValidationMessage
            : ""
    readonly property string signingValidationMessage: signingController !== null
            ? signingController.harmonySigningValidationMessage
            : ""
    color: pageColor

    onVisibleChanged: {
        if (visible) {
            loadDraft()
        }
    }

    Component.onCompleted: loadDraft()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: root.pageColor
            border.width: 0

            ReArkTextField {
                id: searchInput
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 26
                anchors.rightMargin: 26
                height: 28
                placeholderText: qsTr("Search settings")
                radius: 2
                backgroundColor: root.inputColor
                borderColor: root.borderColor
                focusBorderColor: root.inputFocusColor
                textColor: root.primaryTextColor
                placeholderColor: root.subtleTextColor
                selectionColor: root.inputFocusColor
                textPixelSize: 13
                horizontalPadding: 9
                onTextChanged: root.searchQuery = text
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.borderColor
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 150
                Layout.fillHeight: true
                color: root.sidebarColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 22
                    anchors.rightMargin: 12
                    anchors.topMargin: 16
                    anchors.bottomMargin: 16
                    spacing: 0

                    ListView {
                        id: settingsNavigation

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: 0
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        interactive: contentHeight > height
                        model: ListModel {
                            ListElement {
                                title: "Agent"
                            }
                            ListElement {
                                title: "Knowledge"
                            }
                            ListElement {
                                title: "Signing"
                            }
                        }

                        delegate: ItemDelegate {
                            id: navDelegate

                            required property int index
                            required property string title

                            width: settingsNavigation.width
                            height: 26
                            padding: 0
                            hoverEnabled: true
                            onClicked: settingsNavigation.currentIndex = index

                            background: Rectangle {
                                radius: 3
                                color: settingsNavigation.currentIndex === navDelegate.index
                                       ? root.rowSelectedColor
                                       : (navDelegate.hovered ? root.rowHoverColor : "transparent")
                            }

                            contentItem: Label {
                                leftPadding: 10
                                rightPadding: 8
                                verticalAlignment: Text.AlignVCenter
                                text: navDelegate.title === "Agent"
                                      ? qsTr("Agent")
                                      : (navDelegate.title === "Knowledge"
                                         ? qsTr("Knowledge")
                                         : (navDelegate.title === "Signing" ? qsTr("Signing") : navDelegate.title))
                                color: settingsNavigation.currentIndex === navDelegate.index
                                       ? root.titleTextColor
                                       : root.secondaryTextColor
                                font.pixelSize: 13
                                font.weight: settingsNavigation.currentIndex === navDelegate.index
                                             ? Font.DemiBold
                                             : Font.Normal
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    width: 1
                    color: root.borderColor
                }
            }

            StackLayout {
                id: settingsStack

                currentIndex: settingsNavigation.currentIndex
                Layout.fillWidth: true
                Layout.fillHeight: true

                Item {
                    id: agentPage

                    Flickable {
                        id: agentFlickable

                        anchors.fill: parent
                        clip: true
                        contentWidth: width
                        contentHeight: agentContent.implicitHeight + 44
                        boundsBehavior: Flickable.StopAtBounds

                        ColumnLayout {
                            id: agentContent

                            width: Math.min(980, Math.max(560, agentFlickable.width - agentScrollBar.width - 72))
                            x: 38
                            y: 28
                            spacing: 0

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Agent Runtime")
                                color: root.titleTextColor
                                font.pixelSize: 26
                                font.weight: Font.DemiBold
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.topMargin: 8
                                Layout.bottomMargin: 24
                                text: qsTr("Configure the model provider and endpoint used by ReArk smart analysis.")
                                color: root.secondaryTextColor
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }

                            SettingRow {
                                id: providerRow

                                title: qsTr("Agent: Provider")
                                description: qsTr("Wuwe LLM provider preset or OpenAI-compatible endpoint.")

                                SettingsComboBox {
                                    id: providerField
                                    Layout.preferredWidth: 260
                                    model: root.providerOptions
                                    textRole: "displayName"
                                    valueRole: "id"
                                    onActivated: root.switchProviderDraft(root.providerIdAt(currentIndex))
                                }
                            }

                            SettingRow {
                                id: baseUrlRow

                                title: qsTr("Agent: Base URL")
                                description: qsTr("Provider endpoint. Keep provider defaults here, or override for compatible gateways.")

                                SettingsTextField {
                                    id: baseUrlField
                                    Layout.preferredWidth: 460
                                }
                            }

                            SettingRow {
                                id: modelRow

                                title: qsTr("Agent: Model")
                                description: qsTr("Model name sent with each Agent request.")

                                SettingsTextField {
                                    id: modelField
                                    Layout.preferredWidth: 460
                                }
                            }

                            SettingRow {
                                id: apiKeyRow

                                title: qsTr("Agent: API Key")
                                description: qsTr("Leave empty for local OpenAI-compatible services that do not require authentication.")

                                ColumnLayout {
                                    spacing: 8

                                    RowLayout {
                                        spacing: 12

                                        SettingsTextField {
                                            id: apiKeyField
                                            Layout.preferredWidth: 460
                                            echoMode: root.showApiKey ? TextInput.Normal : TextInput.Password
                                        }

                                        SettingsIconButton {
                                            id: showApiKeyBox
                                            iconName: root.showApiKey ? "eye-off" : "eye"
                                            toolTipText: root.showApiKey
                                                         ? qsTr("Hide password")
                                                         : qsTr("Show password")
                                            checkable: true
                                            checked: root.showApiKey
                                            onClicked: root.showApiKey = checked
                                        }
                                    }
                                }
                            }

                            SettingRow {
                                id: requireApiKeyRow

                                title: qsTr("Agent: Require API Key")
                                description: qsTr("Require an API key for remote model endpoints.")

                                SettingsCheckBox {
                                    id: requireApiKeyBox
                                    text: qsTr("Require API key")
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                Layout.topMargin: 8
                                visible: root.anyAgentSettingVisible()
                                color: root.borderColor
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.topMargin: 12
                                Layout.bottomMargin: 18
                                visible: !root.anyAgentSettingVisible()
                                text: qsTr("No settings found")
                                color: root.secondaryTextColor
                                font.pixelSize: 13
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 18
                                spacing: 10
                                visible: root.anyAgentSettingVisible()

                                SettingsButton {
                                    id: agentSaveButton

                                    text: qsTr("Save")
                                    onClicked: root.saveAgentSettings()
                                    tone: "primary"
                                }

                                SettingsButton {
                                    text: qsTr("Reset")
                                    tone: "quiet"
                                    onClicked: {
                                        if (root.settingsController !== null) {
                                            root.settingsController.resetAgentRuntimeSettings()
                                        }
                                        root.loadDraft()
                                        root.saveMessage = qsTr("Agent settings reset.")
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.validationMessage.length > 0 ? root.validationMessage : root.saveMessage
                                    color: root.validationMessage.length > 0 ? root.dangerTextColor : root.subtleTextColor
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {
                            id: agentScrollBar

                            policy: ScrollBar.AsNeeded
                        }
                    }
                }

                Item {
                    id: knowledgePage

                    Flickable {
                        id: knowledgeFlickable

                        anchors.fill: parent
                        clip: true
                        contentWidth: width
                        contentHeight: knowledgeContent.implicitHeight + 44
                        boundsBehavior: Flickable.StopAtBounds

                        ColumnLayout {
                            id: knowledgeContent

                            width: Math.min(980, Math.max(560, knowledgeFlickable.width - knowledgeScrollBar.width - 72))
                            x: 38
                            y: 28
                            spacing: 0

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Knowledge")
                                color: root.titleTextColor
                                font.pixelSize: 26
                                font.weight: Font.DemiBold
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.topMargin: 8
                                Layout.bottomMargin: 24
                                text: qsTr("Configure reference knowledge indexing for RAG-assisted analysis.")
                                color: root.secondaryTextColor
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }

                            SettingRow {
                                id: embeddingBaseUrlRow

                                title: qsTr("Knowledge: Embedding Base URL")
                                description: qsTr("OpenAI-compatible embedding endpoint used to index reference knowledge.")

                                SettingsTextField {
                                    id: embeddingBaseUrlField
                                    Layout.preferredWidth: 460
                                }
                            }

                            SettingRow {
                                id: embeddingModelRow

                                title: qsTr("Knowledge: Embedding Model")
                                description: qsTr("Embedding model used by the reference knowledge index.")

                                SettingsTextField {
                                    id: embeddingModelField
                                    Layout.preferredWidth: 460
                                }
                            }

                            SettingRow {
                                id: embeddingApiKeyRow

                                title: qsTr("Knowledge: Embedding API Key")
                                description: qsTr("Leave empty for local embedding services that do not require authentication.")

                                RowLayout {
                                    spacing: 12

                                    SettingsTextField {
                                        id: embeddingApiKeyField
                                        Layout.preferredWidth: 460
                                        echoMode: root.showEmbeddingApiKey ? TextInput.Normal : TextInput.Password
                                    }

                                    SettingsIconButton {
                                        id: showEmbeddingApiKeyBox
                                        iconName: root.showEmbeddingApiKey ? "eye-off" : "eye"
                                        toolTipText: root.showEmbeddingApiKey
                                                     ? qsTr("Hide password")
                                                     : qsTr("Show password")
                                        checkable: true
                                        checked: root.showEmbeddingApiKey
                                        onClicked: root.showEmbeddingApiKey = checked
                                    }
                                }
                            }

                            SettingRow {
                                id: embeddingRequireApiKeyRow

                                title: qsTr("Knowledge: Embedding API Key Required")
                                description: qsTr("Require an API key before indexing reference knowledge.")

                                SettingsCheckBox {
                                    id: embeddingRequireApiKeyBox
                                    text: qsTr("Require embedding API key")
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                Layout.topMargin: 8
                                visible: root.anyKnowledgeSettingVisible()
                                color: root.borderColor
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.topMargin: 12
                                Layout.bottomMargin: 18
                                visible: !root.anyKnowledgeSettingVisible()
                                text: qsTr("No settings found")
                                color: root.secondaryTextColor
                                font.pixelSize: 13
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 18
                                spacing: 10
                                visible: root.anyKnowledgeSettingVisible()

                                SettingsButton {
                                    id: knowledgeSaveButton

                                    text: qsTr("Save")
                                    onClicked: root.saveAgentSettings()
                                    tone: "primary"
                                }

                                SettingsButton {
                                    text: qsTr("Reset")
                                    tone: "quiet"
                                    onClicked: {
                                        if (root.settingsController !== null) {
                                            root.settingsController.resetKnowledgeSettings()
                                        }
                                        root.loadDraft()
                                        root.saveMessage = qsTr("Knowledge settings reset.")
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.validationMessage.length > 0 ? root.validationMessage : root.saveMessage
                                    color: root.validationMessage.length > 0 ? root.dangerTextColor : root.subtleTextColor
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {
                            id: knowledgeScrollBar

                            policy: ScrollBar.AsNeeded
                        }
                    }
                }

                Item {
                    id: signingPage

                    Flickable {
                        id: signingFlickable

                        anchors.fill: parent
                        clip: true
                        contentWidth: width
                        contentHeight: signingContent.implicitHeight + 44
                        boundsBehavior: Flickable.StopAtBounds

                        ColumnLayout {
                            id: signingContent

                            width: Math.min(980, Math.max(560, signingFlickable.width - signingScrollBar.width - 72))
                            x: 38
                            y: 28
                            spacing: 0

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Application Signing")
                                color: root.titleTextColor
                                font.pixelSize: 26
                                font.weight: Font.DemiBold
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.topMargin: 8
                                Layout.bottomMargin: 24
                                text: qsTr("Configure Harmony / HAP signing material for packaging and device deployment workflows.")
                                color: root.secondaryTextColor
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }

                            SettingRow {
                                id: harmonyKeystorePathRow

                                title: qsTr("Harmony: Keystore (.p12)")
                                description: qsTr("Private keystore used by the Harmony signing tool.")

                                RowLayout {
                                    spacing: 8

                                    SettingsTextField {
                                        id: harmonyKeystorePathField
                                        Layout.preferredWidth: 460
                                        onTextChanged: root.refreshSigningMaterialStatus()
                                    }

                                    SettingsIconButton {
                                        toolTipText: qsTr("Select keystore")
                                        onClicked: root.openSigningFileDialog(harmonyKeystorePathField, [qsTr("Harmony keystore (*.p12)")])
                                    }
                                }
                            }

                            SettingRow {
                                id: harmonyKeystorePasswordRow

                                title: qsTr("Harmony: Keystore Password")
                                description: qsTr("Stored with the same local protection used for ReArk secrets.")

                                RowLayout {
                                    spacing: 8

                                    SettingsTextField {
                                        id: harmonyKeystorePasswordField
                                        Layout.preferredWidth: 460
                                        echoMode: root.showHarmonyKeystorePassword ? TextInput.Normal : TextInput.Password
                                        onTextChanged: root.refreshSigningMaterialStatus()
                                    }

                                    SettingsIconButton {
                                        iconName: root.showHarmonyKeystorePassword ? "eye-off" : "eye"
                                        toolTipText: root.showHarmonyKeystorePassword
                                                     ? qsTr("Hide password")
                                                     : qsTr("Show password")
                                        checkable: true
                                        checked: root.showHarmonyKeystorePassword
                                        onClicked: root.showHarmonyKeystorePassword = checked
                                    }
                                }
                            }

                            SettingRow {
                                id: harmonyKeyAliasRow

                                title: qsTr("Harmony: Key Alias")
                                description: qsTr("Alias of the private key inside the keystore.")

                                SettingsTextField {
                                    id: harmonyKeyAliasField
                                    Layout.preferredWidth: 460
                                    onTextChanged: root.refreshSigningMaterialStatus()
                                }
                            }

                            SettingRow {
                                id: harmonyKeyPasswordRow

                                title: qsTr("Harmony: Key Password")
                                description: qsTr("Optional key password. Leave empty to omit -keyPwd.")

                                RowLayout {
                                    spacing: 8

                                    SettingsTextField {
                                        id: harmonyKeyPasswordField
                                        Layout.preferredWidth: 460
                                        echoMode: root.showHarmonyKeyPassword ? TextInput.Normal : TextInput.Password
                                    }

                                    SettingsIconButton {
                                        iconName: root.showHarmonyKeyPassword ? "eye-off" : "eye"
                                        toolTipText: root.showHarmonyKeyPassword
                                                     ? qsTr("Hide password")
                                                     : qsTr("Show password")
                                        checkable: true
                                        checked: root.showHarmonyKeyPassword
                                        onClicked: root.showHarmonyKeyPassword = checked
                                    }
                                }
                            }

                            SettingRow {
                                id: harmonyProfilePathRow

                                title: qsTr("Harmony: Profile (.p7b)")
                                description: qsTr("Provision profile issued for the app bundle and signing certificate.")

                                RowLayout {
                                    spacing: 8

                                    SettingsTextField {
                                        id: harmonyProfilePathField
                                        Layout.preferredWidth: 460
                                        onTextChanged: root.refreshSigningMaterialStatus()
                                    }

                                    SettingsIconButton {
                                        toolTipText: qsTr("Select profile")
                                        onClicked: root.openSigningFileDialog(harmonyProfilePathField, [qsTr("Harmony profile (*.p7b)")])
                                    }
                                }
                            }

                            SettingRow {
                                id: harmonyCertificatePathRow

                                title: qsTr("Harmony: Certificate (.cer)")
                                description: qsTr("Application signing certificate paired with the keystore key.")

                                RowLayout {
                                    spacing: 8

                                    SettingsTextField {
                                        id: harmonyCertificatePathField
                                        Layout.preferredWidth: 460
                                        onTextChanged: root.refreshSigningMaterialStatus()
                                    }

                                    SettingsIconButton {
                                        toolTipText: qsTr("Select certificate")
                                        onClicked: root.openSigningFileDialog(harmonyCertificatePathField, [qsTr("Harmony certificate (*.cer)")])
                                    }
                                }
                            }

                            SettingRow {
                                id: signingMaterialStatusRow

                                title: qsTr("Harmony: Signing Status")
                                description: qsTr("Local status check for signing files, profile validity, and certificate validity.")

                                RowLayout {
                                    spacing: 8

                                    Rectangle {
                                        Layout.preferredWidth: 8
                                        Layout.preferredHeight: 8
                                        radius: 4
                                        color: root.signingMaterialStatusTone === "ok"
                                               ? root.successTextColor
                                               : root.signingMaterialStatusTone === "warning"
                                                 ? root.warningTextColor
                                                 : root.dangerTextColor
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.signingMaterialStatusText
                                        color: root.signingMaterialStatusTone === "ok"
                                               ? root.successTextColor
                                               : root.signingMaterialStatusTone === "warning"
                                                 ? root.warningTextColor
                                                 : root.dangerTextColor
                                        font.pixelSize: 13
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                Layout.topMargin: 8
                                visible: root.anySigningSettingVisible()
                                color: root.borderColor
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.topMargin: 12
                                Layout.bottomMargin: 18
                                visible: !root.anySigningSettingVisible()
                                text: qsTr("No settings found")
                                color: root.secondaryTextColor
                                font.pixelSize: 13
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 18
                                spacing: 10
                                visible: root.anySigningSettingVisible()

                                SettingsButton {
                                    id: signingSaveButton

                                    text: qsTr("Save")
                                    onClicked: root.saveHarmonySigningSettings()
                                    tone: "primary"
                                }

                                SettingsButton {
                                    text: qsTr("Reset")
                                    tone: "quiet"
                                    onClicked: {
                                        if (root.signingController !== null) {
                                            root.signingController.resetHarmonySigningSettings()
                                        }
                                        root.loadDraft()
                                        root.signingSaveMessage = qsTr("Harmony signing settings reset.")
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.signingValidationMessage.length > 0
                                          ? root.signingValidationMessage
                                          : root.signingSaveMessage
                                    color: root.signingValidationMessage.length > 0 ? root.dangerTextColor : root.subtleTextColor
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {
                            id: signingScrollBar

                            policy: ScrollBar.AsNeeded
                        }
                    }
                }
            }
        }
    }

    component SettingRow: ColumnLayout {
        id: rowRoot

        property string title: ""
        property string description: ""
        default property alias controls: controlSlot.data

        Layout.fillWidth: true
        visible: root.matchesSetting(title, description)
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.borderColor
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 14
            text: rowRoot.title
            color: root.primaryTextColor
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }

        Label {
            Layout.fillWidth: true
            text: rowRoot.description
            color: root.secondaryTextColor
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        RowLayout {
            id: controlSlot

            Layout.fillWidth: true
            Layout.bottomMargin: 16
        }
    }

    component SettingsTextField: ReArkTextField {
        implicitWidth: 460
        implicitHeight: 32
        radius: 2
        backgroundColor: root.inputColor
        borderColor: root.borderColor
        focusBorderColor: root.inputFocusColor
        textColor: root.primaryTextColor
        placeholderColor: root.secondaryTextColor
        selectionColor: root.inputFocusColor
        textPixelSize: 13
    }

    component SettingsButton: AbstractButton {
        id: buttonRoot

        property string tone: "normal"

        implicitWidth: Math.max(72, buttonLabel.implicitWidth + 28)
        implicitHeight: 32
        Layout.preferredHeight: implicitHeight
        leftPadding: 14
        rightPadding: 14
        topPadding: 0
        bottomPadding: 0
        hoverEnabled: true

        contentItem: Label {
            id: buttonLabel

            text: buttonRoot.text
            color: !buttonRoot.enabled
                   ? root.subtleTextColor
                   : buttonRoot.tone === "quiet"
                     ? root.primaryTextColor
                     : "#ffffff"
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 2
            color: !buttonRoot.enabled
                   ? (root.darkTheme ? "#25272b" : "#eeeeee")
                   : buttonRoot.tone === "quiet"
                       ? (buttonRoot.hovered ? root.rowHoverColor : "transparent")
                       : (buttonRoot.hovered ? root.buttonHoverColor : root.buttonColor)
            border.width: 0
        }
    }

    component SettingsCheckBox: CheckBox {
        id: checkRoot

        implicitHeight: 24
        spacing: 8
        hoverEnabled: true
        font.pixelSize: 13

        indicator: Rectangle {
            implicitWidth: 16
            implicitHeight: 16
            x: 0
            y: Math.round((checkRoot.height - height) / 2)
            radius: 2
            color: checkRoot.checked
                   ? root.buttonColor
                   : (checkRoot.hovered ? root.rowHoverColor : root.inputColor)
            border.width: 1
            border.color: checkRoot.checked
                          ? root.buttonColor
                          : (checkRoot.activeFocus ? root.inputFocusColor : root.borderColor)

            Item {
                anchors.centerIn: parent
                width: 10
                height: 8
                visible: checkRoot.checked

                Rectangle {
                    x: 1
                    y: 4
                    width: 2
                    height: 5
                    radius: 1
                    rotation: -45
                    color: "#ffffff"
                }

                Rectangle {
                    x: 5
                    y: 0
                    width: 2
                    height: 10
                    radius: 1
                    rotation: 45
                    color: "#ffffff"
                }
            }
        }

        contentItem: Text {
            leftPadding: checkRoot.indicator.width + checkRoot.spacing
            text: checkRoot.text
            color: checkRoot.enabled ? root.primaryTextColor : root.subtleTextColor
            font: checkRoot.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component SettingsIconButton: AbstractButton {
        id: iconButtonRoot

        property string toolTipText: ""
        property string iconName: "folder-open"

        implicitWidth: 32
        implicitHeight: 32
        Layout.preferredWidth: implicitWidth
        Layout.preferredHeight: implicitHeight
        hoverEnabled: true

        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                width: 16
                height: 16
                name: iconButtonRoot.iconName
                color: iconButtonRoot.enabled
                       ? (iconButtonRoot.checked || iconButtonRoot.hovered ? root.primaryTextColor : root.secondaryTextColor)
                       : root.subtleTextColor
                opacity: iconButtonRoot.enabled ? 1.0 : 0.45
            }
        }

        background: Rectangle {
            radius: 2
            color: !iconButtonRoot.enabled
                   ? "transparent"
                   : iconButtonRoot.down
                     ? root.rowSelectedColor
                     : (iconButtonRoot.checked
                        ? root.rowSelectedColor
                        : (iconButtonRoot.hovered ? root.rowHoverColor : "transparent"))
            border.width: 0
        }

        ToolTip.text: iconButtonRoot.toolTipText
        ToolTip.visible: iconButtonRoot.hovered && iconButtonRoot.toolTipText.length > 0
        ToolTip.delay: 450
    }

    component SettingsComboBox: ComboBox {
        id: comboRoot

        implicitWidth: 260
        implicitHeight: 32
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        font.pixelSize: 13
        hoverEnabled: true

        contentItem: Text {
            leftPadding: 9
            rightPadding: 26
            text: comboRoot.displayText
            color: root.primaryTextColor
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font: comboRoot.font
        }

        indicator: Canvas {
            id: settingsComboChevron

            x: comboRoot.width - width - 10
            y: Math.round((comboRoot.height - height) / 2)
            width: 9
            height: 6
            opacity: comboRoot.enabled ? 1.0 : 0.45

            Connections {
                target: comboRoot.popup

                function onVisibleChanged() {
                    settingsComboChevron.requestPaint()
                }
            }

            onPaint: {
                const context = getContext("2d")
                context.reset()
                context.lineWidth = 1.6
                context.lineCap = "round"
                context.lineJoin = "round"
                context.strokeStyle = root.secondaryTextColor
                context.beginPath()
                if (comboRoot.popup.visible) {
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
            radius: 2
            color: comboRoot.hovered || comboRoot.popup.visible
                   ? (root.darkTheme ? "#24272b" : "#f7fafb")
                   : root.inputColor
            border.width: 1
            border.color: comboRoot.activeFocus || comboRoot.popup.visible
                          ? root.inputFocusColor
                          : root.borderColor
        }

        delegate: ItemDelegate {
            id: comboDelegate

            required property int index
            required property var model
            required property var modelData

            width: comboRoot.width
            height: 28
            padding: 0
            highlighted: comboRoot.highlightedIndex === index

            contentItem: Text {
                leftPadding: 9
                rightPadding: 9
                text: comboRoot.delegateText(comboDelegate.model, comboDelegate.modelData)
                color: comboDelegate.enabled ? root.primaryTextColor : root.subtleTextColor
                font: comboRoot.font
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                color: comboDelegate.highlighted
                       ? (root.darkTheme ? "#2a3038" : "#e4eef7")
                       : comboDelegate.hovered
                         ? root.rowHoverColor
                         : root.inputColor
            }
        }

        popup: Popup {
            y: comboRoot.height + 4
            width: comboRoot.width
            implicitHeight: Math.min(contentItem.implicitHeight + 2, 220)
            padding: 1
            modal: false
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

            contentItem: ListView {
                implicitHeight: contentHeight
                clip: true
                model: comboRoot.popup.visible ? comboRoot.delegateModel : null
                currentIndex: comboRoot.highlightedIndex
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            background: Rectangle {
                radius: 3
                color: root.inputColor
                border.width: 1
                border.color: root.inputFocusColor
            }
        }

        function delegateText(model, modelData) {
            const role = comboRoot.textRole || ""
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

    FileDialog {
        id: signingFileDialog

        property var targetField: null

        title: qsTr("Select signing file")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            if (targetField !== null) {
                targetField.text = root.localPathFromUrl(selectedFile)
            }
            targetField = null
        }
        onRejected: targetField = null
    }

    Connections {
        target: root.settingsController
        ignoreUnknownSignals: true

        function onAgentSettingsChanged() {
            if (root.visible) {
                root.loadDraft()
            }
        }

    }

    Connections {
        target: root.signingController
        ignoreUnknownSignals: true

        function onSigningSettingsChanged() {
            if (root.visible) {
                root.loadDraft()
            }
        }
    }

    function loadDraft() {
        loadingDraft = true
        if (settingsController !== null) {
            providerDrafts = ({})
            root.setProviderCurrent(settingsController.agentProvider)
            activeProviderDraftId = settingsController.agentProvider
            baseUrlField.text = settingsController.agentBaseUrl
            modelField.text = settingsController.agentModel
            apiKeyField.text = settingsController.agentApiKey
            requireApiKeyBox.checked = settingsController.agentRequireApiKey
            embeddingBaseUrlField.text = settingsController.agentEmbeddingBaseUrl
            embeddingModelField.text = settingsController.agentEmbeddingModel
            embeddingApiKeyField.text = settingsController.agentEmbeddingApiKey
            embeddingRequireApiKeyBox.checked = settingsController.agentEmbeddingRequireApiKey
            saveMessage = ""
        }
        if (signingController !== null) {
            harmonyKeystorePathField.text = signingController.harmonySigningKeystorePath
            harmonyKeystorePasswordField.text = signingController.harmonySigningKeystorePassword
            harmonyKeyAliasField.text = signingController.harmonySigningKeyAlias
            harmonyKeyPasswordField.text = signingController.harmonySigningKeyPassword
            harmonyProfilePathField.text = signingController.harmonySigningProfilePath
            harmonyCertificatePathField.text = signingController.harmonySigningCertificatePath
            signingSaveMessage = ""
            root.refreshSigningMaterialStatus()
        }
        loadingDraft = false
    }

    function matchesSetting(title, description) {
        const query = searchQuery.trim().toLowerCase()
        if (query.length === 0) {
            return true
        }

        return title.toLowerCase().indexOf(query) !== -1
            || description.toLowerCase().indexOf(query) !== -1
    }

    function anyAgentSettingVisible() {
        return providerRow.visible
            || baseUrlRow.visible
            || modelRow.visible
            || apiKeyRow.visible
            || requireApiKeyRow.visible
    }

    function anyKnowledgeSettingVisible() {
        return embeddingBaseUrlRow.visible
            || embeddingModelRow.visible
            || embeddingApiKeyRow.visible
            || embeddingRequireApiKeyRow.visible
    }

    function anySigningSettingVisible() {
        return harmonyKeystorePathRow.visible
            || harmonyKeystorePasswordRow.visible
            || harmonyKeyAliasRow.visible
            || harmonyKeyPasswordRow.visible
            || harmonyProfilePathRow.visible
            || harmonyCertificatePathRow.visible
            || signingMaterialStatusRow.visible
    }

    function saveAgentSettings() {
        if (settingsController === null) {
            return false
        }

        root.captureProviderDraft(root.providerIdAt(providerField.currentIndex))
        const saved = settingsController.saveAgentSettings(
            root.providerIdAt(providerField.currentIndex),
            baseUrlField.text,
            apiKeyField.text,
            modelField.text,
            requireApiKeyBox.checked,
            settingsController.agentPythonInterpreterPath,
            settingsController.agentEnableRestrictedPythonBackend,
            embeddingBaseUrlField.text,
            embeddingApiKeyField.text,
            embeddingModelField.text,
            embeddingRequireApiKeyBox.checked)
        saveMessage = saved ? qsTr("Agent settings saved.") : ""
        return saved
    }

    function saveHarmonySigningSettings() {
        if (signingController === null) {
            return false
        }

        const saved = signingController.saveHarmonySigningSettings(
            harmonyKeystorePathField.text,
            harmonyKeystorePasswordField.text,
            harmonyKeyAliasField.text,
            harmonyKeyPasswordField.text,
            harmonyProfilePathField.text,
            harmonyCertificatePathField.text)
        signingSaveMessage = saved ? qsTr("Harmony signing settings saved.") : ""
        root.refreshSigningMaterialStatus()
        return saved
    }

    function refreshSigningMaterialStatus() {
        if (signingController === null
                || harmonyKeystorePathField === null
                || harmonyKeystorePasswordField === null
                || harmonyKeyAliasField === null
                || harmonyProfilePathField === null
                || harmonyCertificatePathField === null) {
            signingMaterialStatusTone = "error"
            signingMaterialStatusText = qsTr("Signing status is unavailable.")
            return
        }

        const status = signingController.inspectHarmonySigningSettings(
            harmonyKeystorePathField.text,
            harmonyKeystorePasswordField.text,
            harmonyKeyAliasField.text,
            harmonyProfilePathField.text,
            harmonyCertificatePathField.text)
        signingMaterialStatusTone = status.tone !== undefined ? status.tone : "error"
        signingMaterialStatusText = status.summary !== undefined ? status.summary : qsTr("Signing status is unavailable.")
    }

    function openSigningFileDialog(targetField, filters) {
        signingFileDialog.targetField = targetField
        signingFileDialog.nameFilters = filters
        signingFileDialog.open()
    }

    function localPathFromUrl(url) {
        let value = String(url)
        if (value.startsWith("file:///")) {
            value = value.substring(8)
        } else if (value.startsWith("file://")) {
            value = value.substring(7)
        }
        return decodeURIComponent(value).replace(/\//g, "\\")
    }

    function providerIdAt(index) {
        if (index < 0 || index >= providerOptions.length) {
            return ""
        }
        const item = providerOptions[index]
        if (item === undefined || item === null) {
            return ""
        }
        return item.id !== undefined ? item.id : String(item)
    }

    function setProviderCurrent(providerId) {
        for (let i = 0; i < providerOptions.length; ++i) {
            if (providerIdAt(i) === providerId) {
                providerField.currentIndex = i
                activeProviderDraftId = providerId
                return
            }
        }
        providerField.currentIndex = providerOptions.length > 0 ? 0 : -1
        activeProviderDraftId = providerIdAt(providerField.currentIndex)
    }

    function switchProviderDraft(providerId) {
        if (loadingDraft || settingsController === null || providerId.length === 0) {
            return
        }

        root.captureProviderDraft(activeProviderDraftId)
        root.applyProviderSettings(providerId)
        activeProviderDraftId = providerId
    }

    function captureProviderDraft(providerId) {
        if (providerId === undefined || providerId === null || providerId.length === 0) {
            return
        }

        const drafts = Object.assign({}, providerDrafts)
        drafts[providerId] = {
            baseUrl: baseUrlField.text,
            model: modelField.text,
            apiKey: apiKeyField.text,
            requireApiKey: requireApiKeyBox.checked
        }
        providerDrafts = drafts
    }

    function providerDraft(providerId) {
        if (providerDrafts === undefined || providerDrafts === null) {
            return undefined
        }
        return providerDrafts[providerId]
    }

    function applyProviderSettings(providerId) {
        if (settingsController === null || providerId.length === 0) {
            return
        }

        const draft = root.providerDraft(providerId)
        const settings = draft !== undefined
                ? draft
                : settingsController.agentProviderSettings(providerId)
        baseUrlField.text = settings.baseUrl !== undefined ? settings.baseUrl : ""
        modelField.text = settings.model !== undefined ? settings.model : ""
        apiKeyField.text = settings.apiKey !== undefined ? settings.apiKey : ""
        requireApiKeyBox.checked = settings.requireApiKey !== undefined
                ? settings.requireApiKey
                : settings.apiKeyRequired === true
    }
}
