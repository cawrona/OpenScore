import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Page turners, pedals and shortcuts. Bluetooth pedals normally show up as a
// keyboard (arrows / PageUp / PageDown); MIDI pedals send CC or program change.
// "Learn" listens for the next key or MIDI message and binds it.
Popup {
    id: dlg
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(600, (parent ? parent.width : 600) - 48)
    padding: 24
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onClosed: actionMap.cancelLearn()

    // While learning, every key (Escape cancels) goes to the learner.
    Keys.onPressed: event => {
        if (actionMap.learning !== "" && !event.isAutoRepeat && actionMap.handleKey(event.key, event.modifiers))
            event.accepted = true
    }

    background: Rectangle {
        color: Theme.surface2
        radius: Theme.radius
        border.color: Theme.border
    }

    ColumnLayout {
        width: parent.width
        spacing: 14

        Label {
            text: "Page turners & shortcuts"
            color: Theme.text
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textDim
            text: "Press Learn, then step on your pedal, press a key, or send a MIDI message. Re-assigning an input moves it from its previous action."
        }

        Repeater {
            model: actionMap.actions
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 12

                Label {
                    Layout.preferredWidth: 150
                    text: modelData.label
                    color: Theme.text
                    font.pixelSize: 14
                }
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    color: Theme.textDim
                    font.pixelSize: 13
                    text: {
                        actionMap.revision // re-evaluate when bindings change
                        const l = actionMap.bindingLabels(modelData.id)
                        return l.length > 0 ? l.join("  ·  ") : "Unassigned"
                    }
                }
                Button {
                    text: actionMap.learning === modelData.id ? "Listening…" : "Learn"
                    highlighted: actionMap.learning === modelData.id
                    onClicked: actionMap.learn(modelData.id)
                }
                Button {
                    text: "✕"
                    flat: true
                    onClicked: actionMap.clearBindings(modelData.id)
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: 13
                text: !midi.available
                      ? "MIDI support was not built in (install librtmidi-dev and rebuild)."
                      : (midi.ports.length > 0 ? "MIDI inputs: " + midi.ports.join(", ") : "No MIDI inputs found.")
            }
            Button {
                visible: midi.available
                text: "Rescan MIDI"
                flat: true
                onClicked: midi.refresh()
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button { text: "Reset to defaults"; flat: true; onClicked: actionMap.resetDefaults() }
            Button { text: "Done"; highlighted: true; onClicked: dlg.close() }
        }
    }
}
