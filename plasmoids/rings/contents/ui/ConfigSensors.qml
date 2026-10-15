import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// The sensors, one id per line, outermost ring first.
KCM.SimpleKCM {
    property var cfg_sensors: []
    property var cfg_sensorsDefault: []

    ColumnLayout {
        QQC2.TextArea {
            id: lines

            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 10
            font.family: "monospace"
            Component.onCompleted: text = cfg_sensors.join("\n")
            onTextChanged: cfg_sensors = text.split("\n").map(line => line.trim()).filter(line => line !== "")
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.7
            text: i18n("One sensor per line, the outermost ring first. `kstatsviewer --list` lists this computer's sensors.")
        }
    }
}
