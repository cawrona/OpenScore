import QtQuick
import QtQuick.Controls

// Compact toggle used in the reader toolbars.
AbstractButton {
    id: b
    implicitHeight: 40
    implicitWidth: Math.max(40, label.implicitWidth + 24)
    checkable: false
    property bool active: false

    background: Rectangle {
        radius: 8
        color: b.active ? Theme.accent : (b.down ? Theme.border : (b.hovered ? Theme.surface2 : "transparent"))
        opacity: b.enabled ? 1 : 0.35
    }
    contentItem: Label {
        id: label
        text: b.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: b.active ? Theme.accentText : Theme.text
        font.pixelSize: 14
        font.weight: Font.Medium
        opacity: b.enabled ? 1 : 0.35
    }
}
