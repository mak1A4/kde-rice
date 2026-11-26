import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import io.github.mak1a4.rice

import "pages"

Kirigami.ApplicationWindow {
    id: root

    title: i18n("kde-rice")
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 34
    minimumWidth: Kirigami.Units.gridUnit * 26
    minimumHeight: Kirigami.Units.gridUnit * 20

    // one entry per part of the desktop; each page is what can be changed there
    readonly property list<Kirigami.Action> sections: [
        Kirigami.Action {
            text: i18n("Appearance")
            icon.name: "preferences-desktop-theme-global"
            checkable: true
            onTriggered: root.show(appearance)
        },
        Kirigami.Action {
            text: i18n("Icons")
            icon.name: "preferences-desktop-icons"
            checkable: true
            onTriggered: root.show(iconsPage)
        },
        Kirigami.Action {
            text: i18n("Windows")
            icon.name: "preferences-system-windows"
            checkable: true
            onTriggered: root.show(windows)
        },
        Kirigami.Action {
            text: i18n("Panels")
            icon.name: "preferences-desktop-plasma"
            checkable: true
            onTriggered: root.show(panels)
        },
        Kirigami.Action {
            text: i18n("Effects")
            icon.name: "preferences-desktop-effects"
            checkable: true
            onTriggered: root.show(effects)
        }
    ]

    QQC2.ActionGroup {
        id: sectionGroup
        actions: root.sections
    }

    // Each page is made when first shown and kept from then on: the page
    // stack only destroys pages it made itself, so a page made by its loader
    // comes back as it was (its scroll position, a search typed, pictures
    // loaded) and what it reads at its start (the themes, the icon theme
    // written) is read once, not at every click on the sidebar.
    function show(loader: Loader): void {
        loader.active = true;
        if (pageStack.currentItem === loader.item) {
            return;
        }
        pageStack.clear();
        pageStack.push(loader.item);
    }

    // After kde-rice is turned off or on, every page made anew: what they
    // showed (the icon theme in use, the title bars, the Plasma style ...)
    // is what there was before.
    function reloadPages(): void {
        const shown = [appearance, iconsPage, windows, panels, effects].find(loader => loader.item && pageStack.currentItem === loader.item);
        pageStack.clear();
        for (const loader of [appearance, iconsPage, windows, panels, effects]) {
            loader.active = false;
        }
        if (shown) {
            root.show(shown);
        }
    }

    Connections {
        target: RiceSwitch
        function onChanged() {
            root.reloadPages();
        }
    }

    globalDrawer: Kirigami.GlobalDrawer {
        isMenu: false
        modal: !root.wideScreen
        actions: root.sections

        // kde-rice as a whole, off and on again
        footer: ColumnLayout {
            spacing: 0

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                visible: RiceSwitch.problems.length > 0 && !RiceSwitch.busy
                type: Kirigami.MessageType.Warning
                text: RiceSwitch.problems.join("\n")
            }

            QQC2.SwitchDelegate {
                id: riceSwitch

                Layout.fillWidth: true
                text: RiceSwitch.busy ? (RiceSwitch.on ? i18n("Turning kde-rice off…") : i18n("Turning kde-rice on…"))
                    : RiceSwitch.on ? i18n("kde-rice is on") : i18n("kde-rice is off")
                icon.name: RiceSwitch.on ? "preferences-desktop-theme-global" : "edit-undo"
                enabled: !RiceSwitch.busy
                checked: RiceSwitch.on
                onToggled: {
                    const wanted = checked;
                    checked = Qt.binding(() => RiceSwitch.on);
                    if (wanted) {
                        RiceSwitch.turnOn();
                    } else {
                        confirmOff.open();
                    }
                }

                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                QQC2.ToolTip.text: RiceSwitch.on
                    ? i18n("Off: Plasma as it comes, or as you had it before kde-rice.")
                    : i18n("On: kde-rice as it was before it was turned off.")
            }

            QQC2.BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: RiceSwitch.busy
                visible: running
            }
        }
    }

    Kirigami.PromptDialog {
        id: confirmOff

        title: i18n("Turn kde-rice off?")
        subtitle: i18n("Everything kde-rice changes goes back to what you had before it, else to Plasma's own: the colour scheme, icons, Plasma style, title bars, wallpaper and lock screen, KWin's plugins, the gaps, the launcher and its Meta+Space, the tray, kde-rice's desktop widgets, the splash screen at login, the application style, the theme files of GTK, the terminals, btop and VS Code, and T3 Code's theme; applets wearing a kde-rice icon get their own. Plasma restarts (the windows stay). If the login screen has kde-rice's wallpaper, you are asked for your password to take it off.\n\nYour kde-rice settings are kept: turning it on again brings everything back.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: RiceSwitch.turnOff()
    }

    Loader { id: appearance; active: false; sourceComponent: AppearancePage {} }
    Loader { id: iconsPage; active: false; sourceComponent: IconsPage {} }
    Loader { id: windows; active: false; sourceComponent: WindowsPage {} }
    Loader { id: panels; active: false; sourceComponent: PanelsPage {} }
    Loader { id: effects; active: false; sourceComponent: EffectsPage {} }

    // --page on the command line, else Appearance
    Component.onCompleted: {
        const pages = ["appearance", "icons", "windows", "panels", "effects"];
        sections[Math.max(0, pages.indexOf(initialPage))].trigger();
    }
}
