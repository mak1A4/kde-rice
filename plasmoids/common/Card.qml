import QtCore
import QtQuick
import QtQuick.Effects
import QtQuick.Window
import Qt.labs.folderlistmodel
import org.kde.kirigami as Kirigami

// A desktop widget's card: frosted glass over the kde-rice theme's background.
//
// Plasma cannot blur what lies behind a widget on the desktop: the wallpaper
// is drawn in the same window, not under it. But the background is a file
// kde-rice knows (the one in ~/.local/share/kde-rice/background, which the
// wallpaper plugin shows), so the card draws that picture itself, at a
// quarter of its size (it is blurred anyway), placed where the wallpaper has
// it, and blurs the part of it under the card. Under a video, or with no
// background, the card is the theme's background colour instead.
//
// What goes into the card is its content, inside `padding`.
Item {
    id: card

    default property alias content: inside.data
    property real padding: Kirigami.Units.gridUnit
    property real radius: Kirigami.Units.gridUnit
    // the veil over the glass: the theme's background colour, which the
    // theme's text colour reads well on, half seen through
    property color tint: Kirigami.Theme.backgroundColor
    property real veil: 0.5

    // ---- the background, as the wallpaper finds it

    FolderListModel {
        id: folder

        folder: StandardPaths.writableLocation(StandardPaths.GenericDataLocation) + "/kde-rice/background"
        showDirs: false
        onStatusChanged: card.look()
        onCountChanged: card.look()
        onRowsInserted: card.look()
        onRowsRemoved: card.look()
        onDataChanged: card.look()
        onModelReset: card.look()
    }

    property url background: ""
    readonly property bool glass: background != "" && !/\.(mp4|webm|mkv|mov)$/i.test(background)

    function look() {
        if (folder.status === FolderListModel.Ready) {
            background = folder.count > 0 ? folder.get(0, "fileUrl") : "";
        }
    }

    // ---- where the card is on the wallpaper

    // The item the wallpaper fills: the nearest one around the card that is
    // as large as the window (the desktop's containment; in its edit mode,
    // which shrinks the desktop, it is shrunk with it). Positions in it are
    // positions on the wallpaper.
    property Item stage: null
    property point origin: Qt.point(0, 0)
    readonly property size stageSize: stage ? Qt.size(stage.width, stage.height) : Qt.size(Window.width, Window.height)
    // all that is followed for the card's position: its own and its parents'
    property var followed: []

    function findStage() {
        const window = card.Window.window;
        for (let p = card.parent; p; p = p.parent) {
            if (window && Math.abs(p.width - window.width) < 1 && Math.abs(p.height - window.height) < 1) {
                return p;
            }
        }
        return null;
    }

    function place() {
        origin = card.mapToItem(stage, 0, 0);
    }

    // The card is moved by its parents (Plasma moves the applet's container
    // when it is dragged), which tell no one: each one's position is followed,
    // and a new parent anywhere is followed afresh.
    function follow() {
        for (const p of followed) {
            p.xChanged.disconnect(place);
            p.yChanged.disconnect(place);
            p.widthChanged.disconnect(place);
            p.heightChanged.disconnect(place);
            p.parentChanged.disconnect(follow);
        }
        stage = findStage();
        const chain = [];
        for (let p = card; p && p !== stage; p = p.parent) {
            chain.push(p);
        }
        if (stage) {
            chain.push(stage);
        }
        for (const p of chain) {
            p.xChanged.connect(place);
            p.yChanged.connect(place);
            p.widthChanged.connect(place);
            p.heightChanged.connect(place);
            p.parentChanged.connect(follow);
        }
        followed = chain;
        place();
    }

    Component.onCompleted: {
        look();
        follow();
    }
    Window.onWindowChanged: follow()

    // ---- the glass

    // how far beyond the card the picture is blurred, so that its edges are
    // blurred with what lies beyond them, not with nothing
    readonly property real margin: Kirigami.Units.gridUnit * 2

    // the picture under the card and around it, before it is blurred
    Item {
        id: backdrop

        visible: false
        width: card.width + 2 * card.margin
        height: card.height + 2 * card.margin
        clip: true

        Image {
            x: card.margin - card.origin.x
            y: card.margin - card.origin.y
            width: card.stageSize.width
            height: card.stageSize.height
            source: card.glass ? card.background : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            smooth: true
            sourceSize: Qt.size(Math.ceil(card.stageSize.width / 4), Math.ceil(card.stageSize.height / 4))
        }
    }

    MultiEffect {
        id: blurred

        visible: false
        width: backdrop.width
        height: backdrop.height
        source: backdrop
        autoPaddingEnabled: false
        blurEnabled: true
        blur: 1
        blurMax: 48
        // a little more colour, as frosted glass in the light has
        saturation: 0.25
    }

    // the card's part of it
    ShaderEffectSource {
        id: frosted

        visible: false
        width: card.width
        height: card.height
        sourceItem: card.glass ? blurred : null
        sourceRect: Qt.rect(card.margin, card.margin, card.width, card.height)
    }

    Kirigami.ShadowedTexture {
        anchors.fill: parent
        visible: card.glass
        source: frosted
        radius: card.radius
        color: card.tint
        shadow.size: Kirigami.Units.gridUnit
        shadow.yOffset: 2
        shadow.color: Qt.rgba(0, 0, 0, 0.25)
    }

    // the veil, and the card on its own where there is no glass
    Kirigami.ShadowedRectangle {
        anchors.fill: parent
        radius: card.radius
        color: Qt.rgba(card.tint.r, card.tint.g, card.tint.b, card.glass ? card.veil : 0.9)
        border.width: 1
        border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)
        shadow.size: card.glass ? 0 : Kirigami.Units.gridUnit
        shadow.yOffset: 2
        shadow.color: Qt.rgba(0, 0, 0, 0.25)
    }

    Item {
        id: inside

        anchors.fill: parent
        anchors.margins: card.padding
    }
}
