import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import OpenScore

FocusScope {
    id: root
    focus: true

    property string path: ""
    signal closeRequested()

    property string tool: "nav"            // nav | pen | highlighter | eraser
    property color inkColor: "#d62828"
    property real thickness: 3
    property bool chromeVisible: true
    property bool twoUp: false
    readonly property bool drawing: tool !== "nav"
    readonly property bool showChrome: chromeVisible || drawing

    function close() {
        library.setLastPage(root.path, scoreView.currentPage)
        notes.flush()
        root.closeRequested()
    }

    Component.onCompleted: {
        root.forceActiveFocus()
        hideTimer.start()
    }

    Keys.onPressed: event => {
        if (event.isAutoRepeat)
            return
        if (actionMap.handleKey(event.key, event.modifiers)) {
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            root.close()
            event.accepted = true
        }
    }

    Connections {
        target: actionMap
        function onTriggered(action) {
            switch (action) {
            case "next": scoreView.next(); break
            case "prev": scoreView.prev(); break
            case "first": scoreView.first(); break
            case "last": scoreView.last(); break
            case "undo": notes.undo(); break
            case "redo": notes.redo(); break
            case "twoUp": root.twoUp = !root.twoUp; break
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 2500
        onTriggered: root.chromeVisible = false
    }

    Rectangle { anchors.fill: parent; color: "#0b0b0d" }

    // ---- The score ----------------------------------------------------------
    ScoreView {
        id: scoreView
        anchors.fill: parent
        source: root.path
        pagesPerView: root.twoUp ? 2 : 1

        onDocumentOpened: {
            notes.load(scoreView.sha1)
            const p = library.lastPage(root.path)
            if (p > 0)
                scoreView.goToPage(p)
            library.noteOpened(root.path)
        }
    }

    AnnotationStore { id: notes }

    // Touch (and everything, in navigate mode) turns pages: left third back,
    // right third forward, middle toggles the toolbars.
    Item {
        id: tapLayer
        anchors.fill: parent
        TapHandler {
            acceptedDevices: root.drawing ? PointerDevice.TouchScreen : PointerDevice.AllDevices
            onTapped: eventPoint => {
                const x = eventPoint.position.x / root.width
                if (x < 0.3)
                    scoreView.prev()
                else if (x > 0.7)
                    scoreView.next()
                else {
                    hideTimer.stop()
                    root.chromeVisible = !root.chromeVisible
                }
            }
        }
    }

    InkLayer {
        id: inkLayer
        anchors.fill: parent
        view: scoreView
        store: notes
        tool: root.tool === "nav" ? "pen" : root.tool
        color: root.inkColor
        thickness: root.thickness
        enabled: root.drawing
    }

    // ---- Status ---------------------------------------------------------------
    Label {
        anchors.centerIn: parent
        visible: scoreView.loading || scoreView.error !== ""
        text: scoreView.error !== "" ? scoreView.error : "Opening…"
        color: Theme.textDim
        font.pixelSize: 18
    }

    // ---- Top bar -------------------------------------------------------------
    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56
        color: Qt.rgba(0.07, 0.07, 0.08, 0.94)
        visible: opacity > 0
        opacity: root.showChrome ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 120 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8

            ToolButtonX { text: "‹  Library"; onClicked: root.close() }
            Item { Layout.fillWidth: true }
            Label {
                text: scoreView.title !== "" ? scoreView.title : root.path.split("/").pop()
                color: Theme.text
                font.pixelSize: 16
                font.weight: Font.Medium
                elide: Text.ElideMiddle
                Layout.maximumWidth: root.width * 0.5
            }
            Label {
                visible: scoreView.pageCount > 0
                color: Theme.textDim
                font.pixelSize: 14
                text: {
                    const a = scoreView.currentPage + 1
                    const b = Math.min(scoreView.pageCount, a + scoreView.pagesPerView - 1)
                    return (b > a ? a + "–" + b : a) + " / " + scoreView.pageCount
                }
            }
            Item { Layout.fillWidth: true }
            ToolButtonX { text: "Pedals"; onClicked: settings.open() }
        }
    }

    // ---- Tool bar -------------------------------------------------------------
    Rectangle {
        id: toolBar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        width: toolRow.implicitWidth + 20
        height: 56
        radius: 28
        color: Qt.rgba(0.07, 0.07, 0.08, 0.94)
        border.color: Theme.border
        visible: opacity > 0
        opacity: root.showChrome ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 120 } }

        RowLayout {
            id: toolRow
            anchors.centerIn: parent
            spacing: 4

            ToolButtonX { text: "Turn"; active: root.tool === "nav"; onClicked: root.tool = "nav" }
            ToolButtonX { text: "Pen"; active: root.tool === "pen"; onClicked: root.tool = "pen" }
            ToolButtonX { text: "Highlight"; active: root.tool === "highlighter"; onClicked: root.tool = "highlighter" }
            ToolButtonX { text: "Erase"; active: root.tool === "eraser"; onClicked: root.tool = "eraser" }

            Rectangle { width: 1; height: 28; color: Theme.border; Layout.leftMargin: 6; Layout.rightMargin: 6 }

            Repeater {
                model: ["#111111", "#d62828", "#1d4ed8", "#15803d", "#f2c200"]
                delegate: Rectangle {
                    required property string modelData
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    radius: 13
                    color: modelData
                    border.width: root.inkColor.toString() === modelData ? 3 : 1
                    border.color: root.inkColor.toString() === modelData ? Theme.accent : Theme.border
                    opacity: root.tool === "eraser" || root.tool === "nav" ? 0.4 : 1
                    TapHandler { onTapped: { root.inkColor = modelData; if (root.tool === "nav" || root.tool === "eraser") root.tool = "pen" } }
                }
            }

            Slider {
                Layout.preferredWidth: 90
                from: 1
                to: 10
                stepSize: 1
                value: root.thickness
                onMoved: root.thickness = value
            }

            Rectangle { width: 1; height: 28; color: Theme.border; Layout.leftMargin: 6; Layout.rightMargin: 6 }

            ToolButtonX { text: "Undo"; enabled: notes.canUndo; onClicked: notes.undo() }
            ToolButtonX { text: "Redo"; enabled: notes.canRedo; onClicked: notes.redo() }
            ToolButtonX { text: "Clear page"; enabled: scoreView.pageCount > 0; onClicked: notes.clearPage(scoreView.currentPage) }

            Rectangle { width: 1; height: 28; color: Theme.border; Layout.leftMargin: 6; Layout.rightMargin: 6 }

            ToolButtonX { text: "2-up"; active: root.twoUp; onClicked: root.twoUp = !root.twoUp }
        }
    }

    SettingsDialog { id: settings }
}
