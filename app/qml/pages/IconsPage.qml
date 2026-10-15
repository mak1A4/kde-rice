import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

import io.github.mak1a4.rice

// kde-rice's icon theme: a base theme, and the user's own picture for any
// icon name on top of it.
FormCard.FormCardPage {
    id: page

    title: i18n("Icons")

    IconModel {
        id: icons
    }

    IconPicker {
        id: picker
        icons: icons
    }

    // one icon: its picture now, its name, and what can be done with it
    component IconRow: FormCard.AbstractFormDelegate {
        id: row

        property string label
        property string iconName
        property url picture
        property bool fixed: false
        property bool overridden: false
        property bool trimmed: false
        property string desktopId

        onClicked: {
            const name = fixed ? icons.themable(desktopId) : iconName;
            if (name !== "") {
                picker.choose(name);
            }
        }

        contentItem: RowLayout {
            spacing: Kirigami.Units.largeSpacing

            Image {
                Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                source: row.picture
                sourceSize.width: Kirigami.Units.iconSizes.medium * 2
                sourceSize.height: Kirigami.Units.iconSizes.medium * 2
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                QQC2.Label {
                    Layout.fillWidth: true
                    text: row.label
                    elide: Text.ElideRight
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: row.fixed ? i18n("a fixed picture: changing it gives the app an icon of its own")
                                    : row.iconName + (row.overridden ? i18n(" · yours") : row.trimmed ? i18n(" · trimmed") : "")
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                    elide: Text.ElideRight
                }
            }

            QQC2.Button {
                text: i18n("Reset")
                visible: !row.fixed && row.overridden
                enabled: !icons.busy
                onClicked: icons.resetIcon(row.iconName)
            }
            QQC2.Button {
                text: i18n("Change…")
                icon.name: "edit-image"
                onClicked: row.clicked()
            }
        }
    }

    FormCard.FormHeader {
        title: i18n("Icon theme")
        trailing: QQC2.BusyIndicator {
            // the theme being written, or the pictures being found
            running: icons.busy
            visible: running
            implicitHeight: Kirigami.Units.iconSizes.smallMedium
            implicitWidth: implicitHeight
        }
    }

    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            id: baseTheme
            text: i18n("Base theme")
            description: icons.baseVariant !== icons.base
                ? i18n("Every icon you have not chosen yourself. Now %1, for the theme's light or dark.", icons.baseVariant)
                : i18n("Every icon you have not chosen yourself.")
            enabled: !icons.busy
            model: icons.themes
            textRole: "name"
            valueRole: "id"
            onActivated: icons.setBase(currentValue)
            // kept at the base in use: it can be changed elsewhere
            // (kde-rice-theme) while the page is open
            function follow() {
                currentIndex = indexOfValue(icons.base);
            }
            Component.onCompleted: follow()
            Connections {
                target: icons
                function onChanged() {
                    baseTheme.follow();
                }
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("Trim icons no theme has")
            description: i18n("Apps whose icon only hicolor or the pixmaps have, drawn with a margin of their own, made to fill their square like the base theme's.")
            enabled: !icons.busy
            checked: icons.trimFallbacks
            onToggled: icons.trimFallbacks = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: i18n("Refresh")
            description: i18n("The icon theme written again, for applications installed since login: their icons trimmed too. Done by itself at every login.")
            icon.name: "view-refresh"
            enabled: !icons.busy
            onClicked: icons.refresh()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            visible: !icons.inUse
            enabled: !icons.busy
            text: i18n("Use kde-rice's icons")
            description: i18n("Your own icons on top of the base theme, for all of KDE.")
            icon.name: "preferences-desktop-icons"
            onClicked: icons.use()
        }
        FormCard.FormTextDelegate {
            visible: icons.inUse
            text: i18n("kde-rice's icons are in use")
            description: i18n("Changes show at once in KDE's applications; some others when they start again.")
        }

        FormCard.FormDelegateSeparator {
            visible: !icons.whiteSurInstalled
        }
        FormCard.FormTextDelegate {
            visible: !icons.whiteSurInstalled
            text: i18n("kde-rice's WhiteSur is not installed yet")
            description: i18n("It comes with kde-rice: run install.sh in the kde-rice folder.")
        }
    }

    FormCard.FormHeader {
        title: i18n("Taskbar")
    }

    FormCard.FormCard {
        Repeater {
            model: icons.taskbar
            delegate: IconRow {
                required property var modelData
                label: modelData.name
                iconName: modelData.icon
                picture: modelData.picture
                fixed: modelData.fixed
                overridden: modelData.overridden
                trimmed: modelData.trimmed
                desktopId: modelData.id
            }
        }
    }

    FormCard.FormHeader {
        title: i18n("Any icon")
    }

    FormCard.FormCard {
        FormCard.AbstractFormDelegate {
            background: null
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Kirigami.SearchField {
                    id: anyIcon
                    Layout.fillWidth: true
                    placeholderText: i18n("An icon's name: folder, system-file-manager, audio-volume-high, …")
                    onTextChanged: anySettle.restart()
                    Timer {
                        id: anySettle
                        interval: 250
                        onTriggered: icons.search(anyIcon.text)
                    }
                }

                Connections {
                    target: icons
                    function onSearchFinished(text, names) {
                        if (text === anyIcon.text && !picker.opened) {
                            anyResults.model = names;
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: Math.max(1, Math.floor(width / (Kirigami.Units.gridUnit * 6)))
                    columnSpacing: Kirigami.Units.smallSpacing
                    rowSpacing: Kirigami.Units.smallSpacing

                    Repeater {
                        id: anyResults
                        model: []
                        delegate: QQC2.ItemDelegate {
                            required property string modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 5
                            onClicked: picker.choose(modelData)
                            contentItem: ColumnLayout {
                                spacing: Kirigami.Units.smallSpacing
                                Image {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                                    source: {
                                        icons.revision;
                                        return icons.current(modelData);
                                    }
                                    sourceSize.width: Kirigami.Units.iconSizes.medium * 2
                                    sourceSize.height: Kirigami.Units.iconSizes.medium * 2
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                }
                                QQC2.Label {
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    text: modelData
                                    elide: Text.ElideMiddle
                                    font: Kirigami.Theme.smallFont
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    FormCard.FormHeader {
        title: i18n("Your icons")
        visible: icons.overrides.length > 0
    }

    FormCard.FormCard {
        visible: icons.overrides.length > 0
        Repeater {
            model: icons.overrides
            delegate: IconRow {
                required property var modelData
                label: modelData.name
                iconName: modelData.name
                picture: modelData.picture
                overridden: true
            }
        }
    }
}
