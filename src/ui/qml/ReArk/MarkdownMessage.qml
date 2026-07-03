import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string markdown: ""
    property bool markdownEnabled: true
    property bool darkTheme: true
    property color textColor: "#e7e7e7"
    property color accentColor: "#3f8fd2"
    property int textPixelSize: 13
    property string emptyText: ""
    property var clipboardController: null
    property bool streaming: false
    property bool suppressRawMarkdownFallback: false

    readonly property string displayText: markdown.length > 0 ? markdown : emptyText
    readonly property bool hasRenderer: typeof markdownRenderer !== "undefined"
    readonly property bool effectiveMarkdownEnabled: markdownEnabled
    readonly property bool richReady: effectiveMarkdownEnabled && renderedBlocks.length > 0
    readonly property bool waitingForRichRender: effectiveMarkdownEnabled && hasRenderer && !richReady
    readonly property bool rawMarkdownSuppressed: suppressRawMarkdownFallback
                                                 && effectiveMarkdownEnabled
                                                 && hasRenderer
                                                 && markdown.length > 0
                                                 && !richReady

    property var renderedBlocks: []
    property int renderRequestId: 0
    property bool renderInFlight: false
    property bool renderDirty: false

    implicitWidth: Math.max(1, richReady ? blockColumn.implicitWidth : plainBody.implicitWidth)
    implicitHeight: Math.max(1, richReady ? blockColumn.implicitHeight
                                          : (rawMarkdownSuppressed ? 1 : plainBody.implicitHeight))

    function renderNow() {
        if (effectiveMarkdownEnabled && hasRenderer) {
            if (renderInFlight) {
                renderDirty = true
                return
            }
            renderInFlight = true
            renderDirty = false
            renderRequestId = markdownRenderer.renderBlocksAsync(displayText, darkTheme, !streaming)
        }
    }

    function scheduleRender() {
        if (effectiveMarkdownEnabled && hasRenderer) {
            renderTimer.restart()
        } else {
            renderTimer.stop()
            renderRequestId = 0
            renderInFlight = false
            renderDirty = false
            renderedBlocks = []
        }
    }

    onDisplayTextChanged: scheduleRender()
    onStreamingChanged: scheduleRender()
    onMarkdownEnabledChanged: {
        if (effectiveMarkdownEnabled) {
            renderTimer.stop()
            renderNow()
        } else {
            scheduleRender()
        }
    }
    onDarkThemeChanged: scheduleRender()
    Component.onCompleted: scheduleRender()

    Timer {
        id: renderTimer

        interval: root.streaming ? 75 : 45
        repeat: false
        onTriggered: root.renderNow()
    }

    Connections {
        target: root.hasRenderer ? markdownRenderer : null

        function onBlocksReady(requestId, blocks) {
            if (requestId === root.renderRequestId && root.effectiveMarkdownEnabled) {
                root.renderedBlocks = blocks
                root.renderInFlight = false
                if (root.renderDirty) {
                    root.renderDirty = false
                    root.scheduleRender()
                }
            }
        }
    }

    TextEdit {
        id: plainBody

        width: Math.max(1, root.width)
        visible: !root.richReady && !root.rawMarkdownSuppressed
        readOnly: true
        selectByMouse: true
        text: root.displayText
        color: root.textColor
        selectedTextColor: "#ffffff"
        selectionColor: root.accentColor
        wrapMode: TextEdit.Wrap
        textFormat: TextEdit.PlainText
        font.pixelSize: root.textPixelSize
        renderType: Text.NativeRendering
        opacity: root.displayText.length > 0 ? (root.waitingForRichRender ? 0.88 : 1.0) : 0.66
    }

    Column {
        id: blockColumn

        width: Math.max(1, root.width)
        visible: root.richReady
        spacing: 10

        Repeater {
            model: root.renderedBlocks

            delegate: Loader {
                id: blockLoader

                property var blockData: modelData

                width: blockColumn.width
                sourceComponent: blockData.type === "code"
                    ? codeBlockComponent
                    : blockData.type === "table" ? tableBlockComponent : htmlBlockComponent
                height: item ? item.implicitHeight : 0
            }
        }
    }

    Component {
        id: htmlBlockComponent

        Item {
            id: htmlBlock

            readonly property var block: parent ? parent.blockData : ({})

            width: parent ? parent.width : root.width
            implicitHeight: htmlText.contentHeight

            TextEdit {
                id: htmlText

                width: parent.width
                height: contentHeight
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.RichText
                text: htmlBlock.block.html || ""
                color: root.textColor
                selectedTextColor: "#ffffff"
                selectionColor: root.accentColor
                font.pixelSize: root.textPixelSize
                renderType: Text.NativeRendering

                onLinkActivated: function(link) {
                    Qt.openUrlExternally(link)
                }
            }
        }
    }

    Component {
        id: codeBlockComponent

        MarkdownCodeBlock {
            block: parent ? parent.blockData : ({})
            width: parent ? parent.width : root.width
            darkTheme: root.darkTheme
            accentColor: root.accentColor
            clipboardController: root.clipboardController
        }
    }

    Component {
        id: tableBlockComponent

        MarkdownTableBlock {
            block: parent ? parent.blockData : ({})
            width: parent ? parent.width : root.width
            darkTheme: root.darkTheme
            textColor: root.textColor
            accentColor: root.accentColor
            textPixelSize: root.textPixelSize
        }
    }
}
