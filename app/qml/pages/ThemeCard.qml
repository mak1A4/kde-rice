import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// One theme: its background (or, without one, a little window in its
// colours) above a strip in its own colours with its name and its six
// terminal colours.
QQC2.AbstractButton {
    id: card

    property string title
    property bool current
    property bool own
    property color bg
    property color surface
    property color fg
    property color accent
    property var swatches: []
    property url thumbnail

    hoverEnabled: true

    background: Rectangle {
        radius: Kirigami.Units.cornerRadius * 2
        color: card.bg
        border.width: card.current ? 3 : (card.hovered ? 2 : 1)
        border.color: card.current ? card.accent : (card.hovered ? Kirigami.Theme.hoverColor : Qt.alpha(Kirigami.Theme.textColor, 0.15))
    }

    contentItem: ColumnLayout {
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 3
            clip: true

            Image {
                id: picture

                anchors.fill: parent
                source: card.thumbnail
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: status === Image.Ready
            }

            // without a background: a window in the theme's colours
            Rectangle {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                visible: !picture.visible
                radius: Kirigami.Units.cornerRadius
                color: card.surface

                Rectangle {
                    x: Kirigami.Units.largeSpacing
                    y: Kirigami.Units.largeSpacing
                    width: parent.width * 0.5
                    height: Kirigami.Units.smallSpacing * 2
                    radius: height / 2
                    color: card.fg
                }
                Rectangle {
                    x: Kirigami.Units.largeSpacing
                    y: Kirigami.Units.largeSpacing * 2.5
                    width: parent.width * 0.3
                    height: Kirigami.Units.smallSpacing * 2
                    radius: height / 2
                    color: Qt.alpha(card.fg, 0.5)
                }
                Rectangle {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Kirigami.Units.largeSpacing
                    width: Kirigami.Units.gridUnit * 3
                    height: Kirigami.Units.gridUnit * 1.2
                    radius: height / 2
                    color: card.accent
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing * 2
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                Layout.fillWidth: true
                text: card.title
                color: card.fg
                elide: Text.ElideRight
                font.bold: card.current
            }

            Kirigami.Icon {
                visible: card.own
                source: "user-identity"
                color: card.fg
                isMask: true
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                QQC2.ToolTip.text: i18n("Your own theme")
                QQC2.ToolTip.visible: card.hovered
            }

            Repeater {
                model: card.swatches
                Rectangle {
                    required property color modelData
                    width: Kirigami.Units.smallSpacing * 2.5
                    height: width
                    radius: width / 2
                    color: modelData
                }
            }
        }
    }
}
