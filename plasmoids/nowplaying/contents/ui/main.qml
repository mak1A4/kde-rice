import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.private.mpris as Mpris
import org.kde.kirigami as Kirigami
import "common" as Common

// What is playing, from any MPRIS player (Plasma's media controller picks
// the player the same way): its cover, title and artist, how far it is, and
// back, play/pause, next. The card is tinted with the cover's colour.
//
// Laid out by its shape: the cover beside the rest when the widget is wide,
// above it when it is not.
PlasmoidItem {
    id: root

    readonly property Mpris.PlayerContainer player: mpris.currentPlayer
    readonly property bool playing: player?.playbackStatus === Mpris.PlaybackStatus.Playing
    readonly property bool hasCover: (player?.artUrl ?? "") != ""
    readonly property bool wide: width >= height * 1.4

    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    preferredRepresentation: fullRepresentation
    switchWidth: Kirigami.Units.gridUnit * 8
    switchHeight: Kirigami.Units.gridUnit * 6
    Plasmoid.icon: "media-playback-start"
    toolTipMainText: player?.track ?? ""
    toolTipSubText: player?.artist ?? ""

    Mpris.Mpris2Model {
        id: mpris
    }

    // The player says where it is only when asked (or after a seek):
    // asked every second while it plays.
    Timer {
        interval: 1000
        repeat: true
        running: root.playing
        triggeredOnStart: true
        onTriggered: root.player.updatePosition()
    }

    Kirigami.ImageColors {
        id: coverColours

        source: root.hasCover ? root.player.artUrl : ""
    }

    function time(microseconds) {
        const s = Math.max(0, Math.round(microseconds / 1000000));
        const h = Math.floor(s / 3600);
        const m = Math.floor(s / 60) % 60;
        const mm = h > 0 ? String(m).padStart(2, "0") : String(m);
        return (h > 0 ? h + ":" : "") + mm + ":" + String(s % 60).padStart(2, "0");
    }

    fullRepresentation: Common.Card {
        id: card

        Layout.minimumWidth: Kirigami.Units.gridUnit * 8
        Layout.minimumHeight: Kirigami.Units.gridUnit * 6
        Layout.preferredWidth: Kirigami.Units.gridUnit * 20
        Layout.preferredHeight: Kirigami.Units.gridUnit * 8

        // the theme's background, a third of the way to the cover's colour
        tint: root.hasCover && coverColours.dominant.valid
            ? Kirigami.ColorUtils.tintWithAlpha(Kirigami.Theme.backgroundColor, coverColours.dominant, 0.35)
            : Kirigami.Theme.backgroundColor
        Behavior on tint {
            ColorAnimation {
                duration: Kirigami.Units.longDuration * 2
            }
        }

        GridLayout {
            anchors.fill: parent
            visible: root.player !== null
            columns: root.wide ? 2 : 1
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.largeSpacing

            // the cover, or the player's icon where there is none
            Item {
                id: cover

                readonly property real side: root.wide
                    ? parent.height
                    : Math.min(parent.width, parent.height - info.implicitHeight - Kirigami.Units.largeSpacing)

                Layout.preferredWidth: side
                Layout.preferredHeight: side
                Layout.alignment: root.wide ? Qt.AlignVCenter : Qt.AlignHCenter
                visible: side >= Kirigami.Units.gridUnit * 2

                Kirigami.ShadowedRectangle {
                    anchors.fill: parent
                    radius: Kirigami.Units.cornerRadius * 2
                    color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.1)
                    shadow.size: Kirigami.Units.largeSpacing
                    shadow.color: Qt.rgba(0, 0, 0, 0.3)
                    shadow.yOffset: 1

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: parent.width * 0.5
                        height: width
                        visible: coverImage.status !== Image.Ready
                        source: root.player?.iconName || "media-album-cover"
                    }
                }

                Image {
                    id: coverImage

                    anchors.fill: parent
                    visible: false
                    source: root.hasCover ? root.player.artUrl : ""
                    sourceSize: Qt.size(cover.side * Screen.devicePixelRatio, cover.side * Screen.devicePixelRatio)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }

                Kirigami.ShadowedTexture {
                    anchors.fill: parent
                    visible: root.hasCover && coverImage.status === Image.Ready
                    radius: Kirigami.Units.cornerRadius * 2
                    source: coverImage
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: root.player?.canRaise ?? false
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: root.player.Raise()
                }
            }

            ColumnLayout {
                id: info

                Layout.fillWidth: true
                Layout.fillHeight: root.wide
                spacing: 0

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: root.player?.track || root.player?.identity || ""
                    font.weight: Font.DemiBold
                    font.pixelSize: Kirigami.Units.gridUnit * 0.9
                    elide: Text.ElideRight
                    horizontalAlignment: root.wide ? Text.AlignLeft : Text.AlignHCenter
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: root.player?.artist ?? ""
                    visible: text !== ""
                    opacity: 0.75
                    elide: Text.ElideRight
                    horizontalAlignment: root.wide ? Text.AlignLeft : Text.AlignHCenter
                }

                Item {
                    Layout.fillHeight: true
                    visible: root.wide
                }

                // how far it is; clicked, it goes there
                Item {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    implicitHeight: Kirigami.Units.gridUnit * 0.6
                    visible: (root.player?.length ?? 0) > 0

                    Rectangle {
                        id: track

                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: 4
                        radius: height / 2
                        color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.2)

                        Rectangle {
                            height: parent.height
                            radius: parent.radius
                            color: Kirigami.Theme.textColor
                            width: parent.width * Math.min(1, (root.player?.position ?? 0) / Math.max(1, root.player?.length ?? 1))
                            Behavior on width {
                                enabled: root.playing
                                NumberAnimation {
                                    duration: 1000
                                }
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: root.player?.canSeek ?? false
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: mouse => root.player.position = root.player.length * mouse.x / width
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: (root.player?.length ?? 0) > 0

                    PlasmaComponents.Label {
                        text: root.time(root.player?.position ?? 0)
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    PlasmaComponents.Label {
                        text: "-" + root.time((root.player?.length ?? 0) - (root.player?.position ?? 0))
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Kirigami.Units.largeSpacing

                    PlasmaComponents.ToolButton {
                        icon.name: "media-skip-backward"
                        enabled: root.player?.canGoPrevious ?? false
                        onClicked: root.player.Previous()
                    }
                    PlasmaComponents.ToolButton {
                        icon.name: root.playing ? "media-playback-pause" : "media-playback-start"
                        icon.width: Kirigami.Units.iconSizes.medium
                        icon.height: Kirigami.Units.iconSizes.medium
                        enabled: root.playing ? (root.player?.canPause ?? false) : (root.player?.canPlay ?? false)
                        onClicked: root.player.PlayPause()
                    }
                    PlasmaComponents.ToolButton {
                        icon.name: "media-skip-forward"
                        enabled: root.player?.canGoNext ?? false
                        onClicked: root.player.Next()
                    }
                }
            }
        }

        // nothing playing
        ColumnLayout {
            anchors.centerIn: parent
            visible: root.player === null
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                source: "media-album-cover"
                opacity: 0.6
            }
            PlasmaComponents.Label {
                Layout.alignment: Qt.AlignHCenter
                text: i18n("Nothing playing")
                opacity: 0.6
            }
        }
    }
}
