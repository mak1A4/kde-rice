import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import org.kde.kitemmodels
import io.github.mak1a4.rice

// The themes: the chosen one with its backgrounds on top, then every theme
// of one kind (dark or light) as a card. A click on a card applies it.
Kirigami.ScrollablePage {
    id: page

    title: i18n("Appearance")

    // dark or light themes listed; the kind in use at first
    property bool showDark: themes.currentIsDark
    onShowDarkChanged: shown.invalidateFilter()

    ThemeModel {
        id: themes
    }

    actions: [
        Kirigami.Action {
            text: i18n("Edit a Copy")
            icon.name: "document-edit"
            tooltip: i18n("Copy the current theme into your themes folder and open it. Saving the file applies it.")
            enabled: themes.current !== ""
            onTriggered: {
                const file = themes.copyTheme(themes.current);
                if (file.toString() !== "") {
                    themes.choose(file.toString().replace(/^.*\//, "").replace(/\.json$/, ""));
                    Qt.openUrlExternally(file);
                }
            }
        },
        Kirigami.Action {
            text: i18n("Update Login Screen")
            icon.name: "preferences-system-login"
            tooltip: i18n("Give the login screen your theme's colours, icons, fonts and keyboard layout (asks for your password). Its background follows every change of theme by itself.")
            onTriggered: Session.start("pkexec", ["/usr/lib/kde-rice/login-screen"])
        },
        Kirigami.Action {
            text: i18n("Themes Folder")
            icon.name: "folder-open"
            onTriggered: Qt.openUrlExternally("file://" + themes.userDir)
        }
    ]

    header: QQC2.ToolBar {
        RowLayout {
            anchors.fill: parent

            QQC2.TabBar {
                currentIndex: page.showDark ? 0 : 1
                QQC2.TabButton {
                    text: i18n("Dark")
                    width: implicitWidth
                    onClicked: page.showDark = true
                }
                QQC2.TabButton {
                    text: i18n("Light")
                    width: implicitWidth
                    onClicked: page.showDark = false
                }
            }

            Item {
                Layout.fillWidth: true
            }

            QQC2.BusyIndicator {
                running: themes.busy
                visible: running
                Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                Layout.preferredWidth: Layout.preferredHeight
            }
        }
    }

    GridView {
        id: grid

        readonly property int columns: Math.max(1, Math.floor(width / (Kirigami.Units.gridUnit * 11)))

        cellWidth: Math.floor(width / columns)
        // a 16:10 picture and the strip with the name under it
        cellHeight: Math.round(cellWidth * 10 / 16) + Kirigami.Units.gridUnit * 2

        model: KSortFilterProxyModel {
            id: shown

            sourceModel: themes
            filterRowCallback: (row, parent) => sourceModel.data(sourceModel.index(row, 0, parent), ThemeModel.DarkRole) === page.showDark
        }

        header: ColumnLayout {
            width: grid.width
            spacing: Kirigami.Units.smallSpacing

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                type: Kirigami.MessageType.Warning
                visible: themes.problems.length > 0
                text: themes.problems.join("\n")
            }

            Kirigami.Heading {
                Layout.margins: Kirigami.Units.smallSpacing
                level: 3
                text: themes.current === "" ? i18n("No theme chosen yet") : i18n("Background for %1", themes.currentTitle)
            }

            ListView {
                id: backgrounds

                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 6
                Layout.bottomMargin: Kirigami.Units.largeSpacing
                visible: themes.current !== ""
                orientation: ListView.Horizontal
                spacing: Kirigami.Units.smallSpacing
                leftMargin: Kirigami.Units.smallSpacing
                clip: true
                model: {
                    themes.revision;
                    return themes.current === "" ? [] : themes.backgrounds(themes.current);
                }
                footer: QQC2.ToolButton {
                    height: backgrounds.height
                    width: height * 1.2
                    icon.name: "list-add"
                    text: i18n("Add…")
                    display: QQC2.AbstractButton.TextUnderIcon
                    onClicked: Qt.openUrlExternally(themes.backgroundsFolder(themes.current))
                    QQC2.ToolTip.text: i18n("Put pictures or videos into this theme's folder")
                    QQC2.ToolTip.visible: hovered
                }
                delegate: QQC2.ItemDelegate {
                    required property var modelData

                    width: height * 16 / 9
                    height: backgrounds.height
                    padding: 3
                    highlighted: modelData.chosen
                    onClicked: themes.chooseBackground(themes.current, modelData.file)
                    contentItem: Rectangle {
                        color: Kirigami.Theme.alternateBackgroundColor
                        radius: Kirigami.Units.cornerRadius
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: modelData.thumbnail ?? ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                        }
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: Kirigami.Units.iconSizes.medium
                            height: width
                            visible: modelData.file === "" || (modelData.video && !!modelData.thumbnail)
                            source: modelData.file === "" ? "edit-none" : "media-playback-start"
                        }
                    }
                    QQC2.ToolTip.text: modelData.file === "" ? i18n("No background: Plasma's own wallpaper") : modelData.file
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
            }
        }

        delegate: Item {
            id: cell

            required property string name
            required property string title
            required property bool current
            required property bool own
            required property color bg
            required property color surface
            required property color fg
            required property color accent
            required property var swatches
            required property url thumbnail

            width: grid.cellWidth
            height: grid.cellHeight

            ThemeCard {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                title: cell.title
                current: cell.current
                own: cell.own
                bg: cell.bg
                surface: cell.surface
                fg: cell.fg
                accent: cell.accent
                swatches: cell.swatches
                thumbnail: cell.thumbnail
                onClicked: themes.choose(cell.name)
            }
        }
    }
}
