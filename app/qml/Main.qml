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

    function show(page: Component): void {
        pageStack.clear();
        pageStack.push(page);
    }

    globalDrawer: Kirigami.GlobalDrawer {
        isMenu: false
        modal: !root.wideScreen
        collapsible: true
        showHeaderWhenCollapsed: true
        actions: root.sections
    }

    Component { id: appearance; AppearancePage {} }
    Component { id: windows; WindowsPage {} }
    Component { id: panels; PanelsPage {} }
    Component { id: effects; EffectsPage {} }

    Component.onCompleted: sections[1].trigger()
}
