import QtQuick
import QtQuick.Controls

// One tile in the library: a score cover or a folder with a fan of previews.
Item {
    id: card

    property bool isFolder: false
    property string titleText: ""
    property string subText: ""
    property string metaText: ""
    property string thumb: ""
    property var previews: []

    signal clicked()
    signal secondaryClicked()

    readonly property real coverH: width * 1.32

    Rectangle {
        id: cover
        x: 0
        y: 0
        width: card.width
        height: card.coverH
        radius: 8
        color: card.isFolder ? Theme.surface : "#fbfaf6"
        border.color: hover.hovered ? Theme.accent : Theme.border
        border.width: hover.hovered ? 2 : 1
        clip: true

        // Score cover
        Image {
            anchors.fill: parent
            anchors.margins: 1
            visible: !card.isFolder && card.thumb !== ""
            source: card.thumb
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            sourceSize.width: 360
        }
        Label {
            anchors.centerIn: parent
            visible: !card.isFolder && card.thumb === ""
            text: "♪"
            color: "#bdb8a8"
            font.pixelSize: cover.width * 0.35
        }

        // Folder: up to three previews fanned out behind a tab
        Item {
            anchors.fill: parent
            visible: card.isFolder

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0; color: Theme.surface2 }
                    GradientStop { position: 1; color: Theme.surface }
                }
            }
            Repeater {
                model: card.isFolder ? card.previews : []
                delegate: Image {
                    required property int index
                    required property string modelData
                    width: cover.width * 0.56
                    height: width * 1.32
                    x: cover.width * (0.12 + index * 0.13)
                    y: cover.height * (0.10 + index * 0.05)
                    rotation: (index - 1) * 6
                    source: modelData
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: 240
                    z: index
                }
            }
            Rectangle { // folder front
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.height * 0.34
                color: Qt.rgba(0.89, 0.71, 0.35, 0.92)
                z: 10
                Rectangle {
                    x: 0
                    y: -height + 1
                    width: parent.width * 0.38
                    height: 12
                    topRightRadius: 8
                    color: parent.color
                }
                Label {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 12
                    text: card.metaText
                    color: Theme.accentText
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }
        }

        HoverHandler { id: hover }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: card.clicked()
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: card.secondaryClicked()
        }
        TapHandler { // long-press on touch
            acceptedDevices: PointerDevice.TouchScreen
            onLongPressed: card.secondaryClicked()
        }
    }

    Column {
        anchors.top: cover.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 1
        Label {
            width: parent.width
            text: card.titleText
            color: Theme.text
            font.pixelSize: 14
            font.weight: Font.Medium
            elide: Text.ElideRight
        }
        Label {
            width: parent.width
            visible: text !== ""
            text: card.subText
            color: Theme.textDim
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        Label {
            width: parent.width
            visible: text !== "" && !card.isFolder
            text: card.metaText
            color: Theme.accent
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }
}
