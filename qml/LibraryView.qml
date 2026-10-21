import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import OpenScore

Item {
    id: root
    signal openScore(string path)

    readonly property bool searching: library.searchText !== ""

    Rectangle { anchors.fill: parent; color: Theme.bg }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Top bar ----------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 12

                Label {
                    text: "OpenScore"
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    color: Theme.text
                }
                BusyIndicator {
                    running: library.scanning
                    visible: library.scanning
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                }
                Item { Layout.fillWidth: true }

                TextField {
                    id: searchField
                    Layout.preferredWidth: 300
                    placeholderText: "Search title, composer or folder"
                    text: library.searchText
                    onTextEdited: library.searchText = text
                }
                ComboBox {
                    Layout.preferredWidth: 150
                    model: ["Recent first", "Name", "Date added"]
                    currentIndex: library.sortMode
                    onActivated: library.sortMode = currentIndex
                }
                Button { text: "New folder"; flat: true; onClicked: newFolderPopup.open() }
                Button { text: "Rescan"; flat: true; onClicked: library.rescan() }
                Button { text: "Library folder…"; flat: true; onClicked: folderDialog.open() }
                Button { text: "Pedals"; flat: true; onClicked: settingsDlg.open() }
            }
        }

        // ---- Library grid -----------------------------------------------
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            leftMargin: 24
            rightMargin: 24
            topMargin: 8
            bottomMargin: 24
            boundsBehavior: Flickable.StopAtBounds

            readonly property real usable: width - leftMargin - rightMargin
            readonly property int cols: Math.max(2, Math.floor(usable / 190))
            cellWidth: Math.floor(usable / cols)
            cellHeight: (cellWidth - 16) * 1.32 + 16 + 64
            model: library.browser

            ScrollBar.vertical: ScrollBar {}

            header: Item {
                width: grid.usable
                implicitHeight: headerCol.implicitHeight + 12

                Column {
                    id: headerCol
                    width: parent.width
                    spacing: 12

                    // Recently viewed
                    Column {
                        width: parent.width
                        spacing: 10
                        visible: !root.searching && library.folder === "" && library.recentCount > 0
                        topPadding: 12

                        Label {
                            text: "Recently viewed"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: 1.2
                            color: Theme.textDim
                        }
                        ListView {
                            id: recentList
                            width: parent.width
                            height: 150 * 1.32 + 72
                            orientation: ListView.Horizontal
                            spacing: 16
                            clip: true
                            model: library.recent
                            boundsBehavior: Flickable.StopAtBounds
                            delegate: Item {
                                id: rcell
                                required property string name
                                required property string path
                                required property string composer
                                required property string lastOpenedText
                                required property string thumb
                                width: 150
                                height: 150 * 1.32 + 72

                                EntryCard {
                                    anchors.fill: parent
                                    titleText: rcell.name
                                    subText: rcell.composer
                                    metaText: rcell.lastOpenedText
                                    thumb: rcell.thumb
                                    onClicked: root.openScore(rcell.path)
                                    onSecondaryClicked: {
                                        ctxMenu.targetPath = rcell.path
                                        ctxMenu.popup()
                                    }
                                }
                            }
                        }
                    }

                    // Breadcrumbs / search label
                    Row {
                        spacing: 4
                        height: 36
                        topPadding: 8

                        Label {
                            visible: root.searching
                            text: "Results for “" + library.searchText + "”"
                            color: Theme.text
                            font.pixelSize: 16
                            font.weight: Font.Medium
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Repeater {
                            model: root.searching ? [] : library.breadcrumbs
                            delegate: Row {
                                required property var modelData
                                required property int index
                                spacing: 4
                                anchors.verticalCenter: parent.verticalCenter
                                Label {
                                    visible: index > 0
                                    text: "/"
                                    color: Theme.textDim
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Label {
                                    text: modelData.name
                                    font.pixelSize: 16
                                    font.weight: index === library.breadcrumbs.length - 1 ? Font.DemiBold : Font.Normal
                                    color: index === library.breadcrumbs.length - 1 ? Theme.text : Theme.accent
                                    TapHandler { onTapped: library.openFolder(modelData.rel) }
                                }
                            }
                        }
                    }
                }
            }

            delegate: Item {
                id: cell
                required property bool isFolder
                required property string name
                required property string path
                required property string rel
                required property string composer
                required property int count
                required property int pages
                required property string lastOpenedText
                required property string thumb
                required property var previews

                width: grid.cellWidth
                height: grid.cellHeight

                EntryCard {
                    anchors.fill: parent
                    anchors.margins: 8
                    isFolder: cell.isFolder
                    titleText: cell.name
                    subText: cell.isFolder
                             ? (cell.lastOpenedText !== "" ? "Opened " + cell.lastOpenedText.toLowerCase() : "Not opened yet")
                             : cell.composer
                    metaText: cell.isFolder
                              ? cell.count + (cell.count === 1 ? " score" : " scores")
                              : (cell.lastOpenedText !== "" ? cell.lastOpenedText : (cell.pages + " pages"))
                    thumb: cell.thumb
                    previews: cell.previews
                    onClicked: cell.isFolder ? library.openFolder(cell.rel) : root.openScore(cell.path)
                    onSecondaryClicked: {
                        ctxMenu.targetPath = cell.path
                        ctxMenu.popup()
                    }
                }
            }
        }
    }

    // ---- Empty state --------------------------------------------------------
    Column {
        anchors.centerIn: parent
        spacing: 12
        visible: library.totalScores === 0 && !library.scanning && !root.searching
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "No scores yet"
            font.pixelSize: 26
            font.weight: Font.DemiBold
            color: Theme.text
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            text: "Drop PDFs into\n" + library.rootPath + "\nor choose a different folder."
            color: Theme.textDim
            font.pixelSize: 15
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Choose library folder…"
            onClicked: folderDialog.open()
        }
    }

    // ---- Dialogs / menus ------------------------------------------------------
    SettingsDialog { id: settingsDlg }

    FolderDialog {
        id: folderDialog
        title: "Choose your scores folder"
        onAccepted: library.setRoot(selectedFolder.toString())
    }

    Menu {
        id: ctxMenu
        property string targetPath: ""
        MenuItem {
            text: "Show in file manager"
            onTriggered: Qt.openUrlExternally(library.folderUrl(ctxMenu.targetPath))
        }
    }

    Popup {
        id: newFolderPopup
        anchors.centerIn: parent
        modal: true
        width: 360
        padding: 20
        onOpened: { folderName.text = ""; folderName.forceActiveFocus() }
        background: Rectangle { color: Theme.surface2; radius: Theme.radius; border.color: Theme.border }
        ColumnLayout {
            width: parent.width
            spacing: 12
            Label { text: "New folder in " + (library.folder === "" ? "Library" : library.folder); color: Theme.text; font.pixelSize: 16 }
            TextField {
                id: folderName
                Layout.fillWidth: true
                placeholderText: "e.g. Bach, Orchestra, Jazz charts"
                onAccepted: create.clicked()
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button { text: "Cancel"; flat: true; onClicked: newFolderPopup.close() }
                Button {
                    id: create
                    text: "Create"
                    highlighted: true
                    onClicked: { if (library.createFolder(folderName.text)) newFolderPopup.close() }
                }
            }
        }
    }
}
