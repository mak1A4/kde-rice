import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import io.github.mak1a4.rice.launcher

// The popup: a transparent window over the whole screen (on Wayland a
// layer-shell surface above everything) with a box in its upper third, the
// search field on top and the results below it. A click beside the box closes
// it.
//
// Keys: Up/Down or Ctrl+P/Ctrl+N move, Page Up/Page Down move by a page,
// Enter runs the selected result (or the action chosen with Tab), Shift+Enter
// its first action, Tab/Shift+Tab walk through its actions. Escape empties
// the field, and with the field already empty closes: one key for "start
// again", two for "go away", as in Spotlight. Moving the pointer selects a
// row, a click runs it.
Window {
    id: root

    required property Launcher launcher
    required property SearchModel results

    // rows shown before the list scrolls
    readonly property int visibleRows: 8
    readonly property int rowHeight: Math.round(Kirigami.Units.gridUnit * 2.75)

    // which of the selected row's actions Enter runs; -1 is the row itself
    property int activeAction: -1

    // Where the pointer last was, in the window, and whether it has to move
    // properly before it selects again (after the keys moved the selection,
    // or at opening). Rows that move under a still pointer report it as
    // moving at the same place; a mouse beside the keyboard shakes a pixel
    // or two while one types.
    property point pointerAt: Qt.point(-1, -1)
    property bool pointerHeld: true

    function pointerMoved(position: point): bool {
        if (position.x === pointerAt.x && position.y === pointerAt.y) {
            return false;
        }
        if (pointerHeld) {
            if (pointerAt.x >= 0 && Math.hypot(position.x - pointerAt.x, position.y - pointerAt.y) <= 4) {
                return false;
            }
            const first = pointerAt.x < 0;
            pointerAt = position;
            if (first) {
                return false;
            }
            pointerHeld = false;
            return true;
        }
        pointerAt = position;
        return true;
    }

    function moveBy(delta: int): void {
        pointerHeld = true;
        results.move(delta);
    }

    // Tab: the selected row's actions in turn, and back to the row itself
    function cycleAction(step: int): void {
        const count = results.actionCount(results.currentIndex);
        if (count === 0) {
            return;
        }
        activeAction = (activeAction + 1 + step + count + 1) % (count + 1) - 1;
    }

    title: i18n("kde-rice Launcher")
    color: "transparent"
    flags: Qt.FramelessWindowHint
    width: Kirigami.Units.gridUnit * 60
    height: Kirigami.Units.gridUnit * 40
    visible: false

    Connections {
        target: root.launcher

        function onOpened(text: string): void {
            field.text = text;
            field.cursorPosition = text.length;
            field.forceActiveFocus();
            root.activeAction = -1;
            root.pointerAt = Qt.point(-1, -1);
            root.pointerHeld = true;
        }

        function onShownChanged(): void {
            // nothing of this search left for the next opening, not even in
            // its first frame
            if (!root.launcher.shown) {
                field.clear();
            }
        }
    }

    Connections {
        target: root.results

        function onCurrentIndexChanged(): void {
            root.activeAction = -1;
            // after the view has taken in a new list
            Qt.callLater(() => {
                if (root.results.currentIndex >= 0) {
                    view.positionViewAtIndex(root.results.currentIndex, ListView.Contain);
                }
            });
        }

        function onQueryReplaced(query: string): void {
            field.text = query;
            field.cursorPosition = query.length;
        }
    }

    // beside the box
    MouseArea {
        anchors.fill: parent
        onPressed: root.launcher.close()
    }

    Kirigami.ShadowedRectangle {
        id: box

        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false

        width: Math.min(Kirigami.Units.gridUnit * 44, root.width - Kirigami.Units.gridUnit * 2)
        height: content.implicitHeight
        x: Math.round((root.width - width) / 2)
        y: Math.round(root.height / 5)

        radius: Kirigami.Units.cornerRadius * 3
        color: Kirigami.Theme.backgroundColor
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
        shadow {
            size: Kirigami.Units.gridUnit * 2
            yOffset: Kirigami.Units.smallSpacing
            color: Qt.rgba(0, 0, 0, 0.35)
        }

        // a click on the box is not one beside it
        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            id: content

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
                Layout.leftMargin: Kirigami.Units.largeSpacing * 2
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                    source: "search"
                    opacity: 0.7
                }

                QQC2.TextField {
                    id: field

                    Layout.fillWidth: true
                    focus: true
                    background: null
                    leftPadding: 0
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.6
                    placeholderText: i18nc("@info:placeholder", "Search…")
                    inputMethodHints: Qt.ImhNoPredictiveText

                    onTextChanged: root.results.query = text

                    Keys.onPressed: event => {
                        const ctrl = event.modifiers & Qt.ControlModifier;
                        if (event.key === Qt.Key_Up || (ctrl && event.key === Qt.Key_P)) {
                            root.moveBy(-1);
                        } else if (event.key === Qt.Key_Down || (ctrl && event.key === Qt.Key_N)) {
                            root.moveBy(1);
                        } else if (event.key === Qt.Key_PageUp) {
                            root.moveBy(-root.visibleRows);
                        } else if (event.key === Qt.Key_PageDown) {
                            root.moveBy(root.visibleRows);
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            root.results.runCurrent((event.modifiers & Qt.ShiftModifier) ? 0 : root.activeAction);
                        } else if (event.key === Qt.Key_Tab) {
                            root.cycleAction(1);
                        } else if (event.key === Qt.Key_Backtab) {
                            root.cycleAction(-1);
                        } else if (event.key === Qt.Key_Escape) {
                            if (text.length > 0) {
                                clear();
                            } else {
                                root.launcher.close();
                            }
                        } else {
                            return;
                        }
                        event.accepted = true;
                    }
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
                visible: view.count > 0 || nothingFound.visible
            }

            ListView {
                id: view

                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(count, root.visibleRows) * root.rowHeight + (count > 0 ? Kirigami.Units.smallSpacing * 2 : 0)
                Layout.margins: count > 0 ? Kirigami.Units.smallSpacing : 0
                visible: count > 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.results

                Behavior on Layout.preferredHeight {
                    NumberAnimation {
                        duration: Kirigami.Units.shortDuration
                        easing.type: Easing.OutCubic
                    }
                }

                QQC2.ScrollBar.vertical: QQC2.ScrollBar {
                    id: scrollBar
                }

                delegate: ResultRow {
                    // the desktop style's scroll bar takes room of its own
                    width: ListView.view.width - (scrollBar.visible ? scrollBar.width : 0)
                    height: root.rowHeight
                    selected: index === root.results.currentIndex
                    activeAction: selected ? root.activeAction : -1

                    onPointerMoved: position => {
                        if (root.pointerMoved(position)) {
                            root.results.select(index);
                        }
                    }
                    onClicked: root.results.run(index, -1)
                    onActionClicked: action => root.results.run(index, action)
                }
            }

            QQC2.Label {
                id: nothingFound

                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing * 2
                visible: field.text.trim().length > 0 && root.results.fresh && !root.results.querying && root.results.count === 0
                text: i18nc("@info", "Nothing found")
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.6
            }
        }
    }
}
