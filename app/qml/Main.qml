import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

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

    globalDrawer: Kirigami.GlobalDrawer {
        isMenu: false
        modal: !root.wideScreen
        actions: root.sections
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
