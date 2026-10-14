import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// One result: its icon, title and subtitle, its category at the end, and on
// the selected row the match's extra actions as buttons.
Item {
    id: row

    required property int index
    required property string title
    required property string subtitle
    required property string category
    // an icon name or a QIcon
    required property var iconSource
    required property string fallbackIcon
    // [{text, icon}]
    required property var actions
    required property bool matchEnabled

    property bool selected: false
    // the action chosen with Tab; -1 is none
    property int activeAction: -1

    // the pointer moved over the row, at this point of the window
    signal pointerMoved(point scenePosition)
    signal clicked()
    signal actionClicked(int action)

    Rectangle {
        anchors.fill: parent
        radius: Kirigami.Units.cornerRadius
        color: Qt.alpha(Kirigami.Theme.highlightColor, 0.25)
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.highlightColor, 0.5)
        visible: row.selected
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPositionChanged: mouse => row.pointerMoved(mapToItem(null, mouse.x, mouse.y))
        onClicked: row.clicked()
    }

    RowLayout {
        anchors {
            fill: parent
            leftMargin: Kirigami.Units.largeSpacing
            rightMargin: Kirigami.Units.largeSpacing
        }
        spacing: Kirigami.Units.largeSpacing
        opacity: row.matchEnabled ? 1 : 0.6

        Kirigami.Icon {
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            source: row.iconSource
            fallback: row.fallbackIcon
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                Layout.fillWidth: true
                text: row.title
                textFormat: Text.PlainText
                // multi-line results are cut to their first line
                maximumLineCount: 1
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
            }
            QQC2.Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: row.subtitle
                textFormat: Text.PlainText
                maximumLineCount: 1
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                font: Kirigami.Theme.smallFont
                opacity: 0.7
            }
        }

        Repeater {
            model: row.selected ? row.actions : []

            QQC2.ToolButton {
                id: button

                required property int index
                required property var modelData

                text: modelData.text
                icon.name: modelData.icon
                display: modelData.icon ? QQC2.AbstractButton.IconOnly : QQC2.AbstractButton.TextOnly
                // the keyboard stays in the search field
                focusPolicy: Qt.NoFocus
                // chosen with Tab: shown as pressed, as KRunner does
                down: row.activeAction === index || pressed
                onClicked: row.actionClicked(index)

                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered && display === QQC2.AbstractButton.IconOnly
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }

        QQC2.Label {
            visible: text.length > 0
            text: row.category
            textFormat: Text.PlainText
            font: Kirigami.Theme.smallFont
            opacity: 0.5
        }
    }
}
