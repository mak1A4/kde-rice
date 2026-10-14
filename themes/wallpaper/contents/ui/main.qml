import QtCore
import QtQuick
import QtQuick.Effects
import QtMultimedia
import Qt.labs.folderlistmodel
import org.kde.plasma.plasmoid

// The background of the kde-rice theme, as a Plasma wallpaper: the desktop
// and the lock screen can both be set to it, and then show the same picture
// or video.
//
// It has no settings. It shows the one file in a directory kde-rice fills
// when a theme is chosen (~/.local/share/kde-rice/background). A new file
// there is shown at once: it opens out from the middle of the screen, as a
// growing disc, over the one before. (The first file, at the start, is
// simply there.)
WallpaperItem {
    id: root

    FolderListModel {
        id: own

        folder: StandardPaths.writableLocation(StandardPaths.GenericDataLocation) + "/kde-rice/background"
        showDirs: false
    }

    // Looked at whenever the folder's listing changes in any way: kde-rice
    // swaps one file for the next at once, so the folder has one file before
    // and after, and a binding on the count alone never looks again.
    property url source: ""

    function look() {
        source = own.count > 0 ? own.get(0, "fileUrl") : "";
    }

    Connections {
        target: own
        function onCountChanged() {
            root.look();
        }
        function onRowsInserted() {
            root.look();
        }
        function onRowsRemoved() {
            root.look();
        }
        function onDataChanged() {
            root.look();
        }
        function onModelReset() {
            root.look();
        }
    }

    // A picture or a video, filling the screen; `ready` once there is
    // something of it to see.
    component Layer: Item {
        // not "layer": every Item has a property of that name, and inside
        // the video's own component that is what the name would mean
        id: pane

        property url source: ""
        readonly property bool video: /\.(mp4|webm|mkv|mov)$/i.test(source)
        readonly property bool ready: source != "" && (video ? (player.item?.started ?? false) : image.status === Image.Ready)

        anchors.fill: parent

        Image {
            id: image

            anchors.fill: parent
            visible: !pane.video
            source: pane.video ? "" : pane.source
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            // no larger in memory than on screen
            sourceSize: Qt.size(pane.width * Screen.devicePixelRatio, pane.height * Screen.devicePixelRatio)
        }

        // only made for a video: nothing of it runs for a picture
        Loader {
            id: player

            anchors.fill: parent
            active: pane.video
            sourceComponent: VideoOutput {
                id: output

                // the first frames have been shown
                readonly property bool started: media.position > 0

                fillMode: VideoOutput.PreserveAspectCrop

                // no audio output: a background is silent
                MediaPlayer {
                    id: media

                    source: pane.source
                    videoOutput: output
                    loops: MediaPlayer.Infinite
                    Component.onCompleted: play()
                    onSourceChanged: play()
                }
            }
        }
    }

    // Two of them: the one that is shown, and the one a new background is
    // loaded into and then opened over it.
    property Layer front: first
    readonly property Layer back: front === first ? second : first
    // a new background is in `back`, not yet ready to be seen
    property bool waiting: false
    property bool revealing: false
    // how far the disc has opened, 0 to 1
    property real opened: 0

    // A change of background is its file taken away and another put there,
    // and for a moment the directory may hold none, or two: looked at when
    // it has come to rest.
    onSourceChanged: settle.restart()

    Timer {
        id: settle

        interval: 150
        onTriggered: root.change()
    }

    Component.onCompleted: change()

    function change() {
        if (source == front.source && !waiting)
            return;
        // one that was on its way is overtaken
        if (revealing)
            reveal.complete();
        if (front.source == "" || source == "") {
            // the first, or none at all: nothing to open over
            waiting = false;
            back.source = "";
            front.source = source;
            return;
        }
        back.source = source;
        waiting = true;
        begin();
    }

    function begin() {
        if (!waiting || !back.ready)
            return;
        waiting = false;
        opened = 0;
        revealing = true;
        reveal.restart();
    }

    NumberAnimation {
        id: reveal

        target: root
        property: "opened"
        from: 0
        to: 1
        duration: 900
        easing.type: Easing.InOutCubic
        onFinished: {
            const old = root.front;
            root.front = root.back;
            root.revealing = false;
            // the one underneath is done with: a video in it stops
            old.source = "";
        }
    }

    // under a picture that does not fill the screen, and until one is there
    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    // The shape the new background is seen through: a disc from the middle,
    // which at the end reaches the corners. Never seen itself.
    Item {
        id: disc

        anchors.fill: parent
        visible: false
        layer.enabled: root.revealing

        Rectangle {
            readonly property real reach: Math.hypot(disc.width, disc.height) * 1.02

            anchors.centerIn: parent
            width: reach * root.opened
            height: width
            radius: width / 2
        }
    }

    Layer {
        id: first

        z: root.front === first ? 0 : 1
        opacity: root.front === first || root.revealing ? 1 : 0
        onReadyChanged: root.begin()
        // through the disc while it is the one coming, otherwise as it is
        layer.enabled: root.revealing && root.front !== first
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: disc
        }
    }

    Layer {
        id: second

        z: root.front === second ? 0 : 1
        opacity: root.front === second || root.revealing ? 1 : 0
        onReadyChanged: root.begin()
        layer.enabled: root.revealing && root.front !== second
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: disc
        }
    }
}
