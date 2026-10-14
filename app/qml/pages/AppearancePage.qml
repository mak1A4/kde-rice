import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.Page {
    title: i18n("Appearance")

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        icon.name: "preferences-desktop-theme-global"
        text: i18n("Not here yet")
        explanation: i18n("Colours, Plasma style, icons, cursors, splash and lock screen, switched together as one theme.")
    }
}
