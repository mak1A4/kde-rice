import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.Page {
    title: i18n("Effects")

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        icon.name: "preferences-desktop-effects"
        text: i18n("Not here yet")
        explanation: i18n("kde-rice's own KWin effects and their settings.")
    }
}
