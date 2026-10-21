import QtQuick
import QtQuick.Controls
import OpenScore

ApplicationWindow {
    id: win
    visible: true
    width: 1280
    height: 860
    minimumWidth: 640
    minimumHeight: 480
    title: "OpenScore"
    color: Theme.bg

    palette {
        window: Theme.bg
        windowText: Theme.text
        base: Theme.surface
        text: Theme.text
        button: Theme.surface2
        buttonText: Theme.text
        highlight: Theme.accent
        highlightedText: Theme.accentText
        placeholderText: Theme.textDim
        toolTipBase: Theme.surface2
        toolTipText: Theme.text
    }

    function openScore(path) {
        stack.push(readerComp, { path: path })
    }

    // Page turns must feel instant: no animated transitions.
    StackView {
        id: stack
        anchors.fill: parent
        initialItem: libraryComp
        pushEnter: Transition {}
        pushExit: Transition {}
        popEnter: Transition {}
        popExit: Transition {}
    }

    Component {
        id: libraryComp
        LibraryView {
            onOpenScore: path => win.openScore(path)
        }
    }

    Component {
        id: readerComp
        ReaderView {
            onCloseRequested: stack.pop()
        }
    }

    Component.onCompleted: {
        if (startupFile !== "")
            openScore(startupFile)
    }
}
