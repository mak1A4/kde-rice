import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import io.github.mak1a4.rice

// Choosing the picture for one icon name: the same name in every installed
// theme, the pictures of another name (search), or a file of one's own.
Kirigami.Dialog {
    id: picker

    required property IconModel icons
    // the icon name being changed
    property string name
    // the name whose pictures are shown (the same, or one searched for)
    property string shown
    property var found: []

    function choose(iconName) {
        name = iconName;
        shown = iconName;
        search.text = "";
        found = [];
        open();
    }

    title: i18n("Icon for %1", name)
    preferredWidth: Kirigami.Units.gridUnit * 40
    preferredHeight: Kirigami.Units.gridUnit * 34
    standardButtons: Kirigami.Dialog.NoButton
    padding: Kirigami.Units.largeSpacing

    customFooterActions: [
        Kirigami.Action {
            text: i18n("Use a File…")
            icon.name: "document-open"
            onTriggered: files.open()
        },
        Kirigami.Action {
            text: i18n("Back to %1's", picker.icons.baseVariant)
            icon.name: "edit-undo"
            enabled: picker.icons.overridden(picker.name) || picker.icons.revision < 0
            onTriggered: {
                picker.icons.resetIcon(picker.name);
                picker.close();
            }
        }
    ]

    FileDialog {
        id: files
        title: i18n("A picture for %1", picker.name)
        nameFilters: [i18n("Icons (*.svg *.svgz *.png)")]
        onAccepted: {
            if (picker.icons.setIcon(picker.name, selectedFile)) {
                picker.close();
            }
        }
    }

    Connections {
        target: picker.icons
        function onSearchFinished(text, names) {
            if (text === search.text) {
                picker.found = names;
            }
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.SearchField {
            id: search
            Layout.fillWidth: true
            placeholderText: i18n("Pictures of another icon name (folder-blue, firefox, …)")
            onTextChanged: settle.restart()
            Timer {
                id: settle
                interval: 250
                onTriggered: picker.icons.search(search.text)
            }
        }

        // names found, a row of chips to pick from
        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing
            visible: picker.found.length > 0 || picker.icons.searching
            QQC2.BusyIndicator {
                running: picker.icons.searching
                visible: running
                height: Kirigami.Units.gridUnit * 1.5
                width: height
            }
            Repeater {
                model: picker.found.slice(0, 60)
                QQC2.Button {
                    required property string modelData
                    text: modelData
                    flat: modelData !== picker.shown
                    highlighted: modelData === picker.shown
                    onClicked: picker.shown = modelData
                }
            }
        }

        Kirigami.Heading {
            level: 4
            text: picker.shown === picker.name ? i18n("“%1” in every theme", picker.name) : i18n("“%1” in every theme, for %2", picker.shown, picker.name)
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 20
            clip: true
            cellWidth: Kirigami.Units.gridUnit * 6
            cellHeight: Kirigami.Units.gridUnit * 6.5
            model: picker.shown === "" ? [] : picker.icons.candidates(picker.shown)

            delegate: QQC2.ItemDelegate {
                required property var modelData
                width: grid.cellWidth - Kirigami.Units.smallSpacing
                height: grid.cellHeight - Kirigami.Units.smallSpacing
                onClicked: {
                    if (picker.icons.setIcon(picker.name, modelData.url)) {
                        picker.close();
                    }
                }
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Kirigami.Units.iconSizes.large
                        Layout.preferredHeight: Kirigami.Units.iconSizes.large
                        source: modelData.url
                        sourceSize.width: Kirigami.Units.iconSizes.large * 2
                        sourceSize.height: Kirigami.Units.iconSizes.large * 2
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData.themeName
                        elide: Text.ElideRight
                        font: Kirigami.Theme.smallFont
                    }
                }
                QQC2.ToolTip.text: String(modelData.url).replace(/^file:\/\//, "")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: grid.count === 0
                text: i18n("No theme has “%1”", picker.shown)
                explanation: i18n("Search for another name above, or use a file of your own.")
            }
        }
    }
}
