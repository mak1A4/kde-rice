import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.Page {
    title: i18n("Panels")

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        icon.name: "preferences-desktop-plasma"
        text: i18n("Not here yet")
        explanation: i18n("The bar, dock and widgets, laid out the same way each time.")
    }
}
