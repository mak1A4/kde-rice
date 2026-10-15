import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.ksysguard.sensors as Sensors
import org.kde.kirigami as Kirigami
import "common" as Common

// ksystemstats' sensors as rings, one inside the other (outermost first),
// each in one of the theme's colours: its accent (the colour scheme's focus
// colour: its highlight is the accent paled for selections), then its
// positive, neutral and negative colours, its link colours. Beside them, or below
// when the widget is not wide, what each ring is and its value.
//
// Which sensors is the setting `sensors`; one this computer does not have
// (a second GPU) is left out.
PlasmoidItem {
    id: root

    readonly property bool wide: width >= height * 1.3
    readonly property list<color> colours: [Kirigami.Theme.focusColor, Kirigami.Theme.positiveTextColor, Kirigami.Theme.neutralTextColor, Kirigami.Theme.negativeTextColor, Kirigami.Theme.visitedLinkColor, Kirigami.Theme.linkColor]

    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    preferredRepresentation: fullRepresentation
    Plasmoid.icon: "utilities-system-monitor"

    // A sensor, with what the rings and the list show of it.
    component Entry: Item {
        id: entry

        required property string modelData
        readonly property bool ok: sensor.status === Sensors.Sensor.Status.Ready
        readonly property bool temperature: /temperature$/i.test(modelData)
        readonly property real maximum: temperature ? 100 : (sensor.maximum > 0 ? sensor.maximum : 100)
        readonly property real fraction: ok ? Math.max(0, Math.min(1, sensor.value / maximum)) : 0
        readonly property string label: {
            if (/^cpu\/all\/usage$/.test(modelData))
                return i18n("CPU");
            if (/^memory\/physical\/used(Percent)?$/.test(modelData))
                return i18n("Memory");
            if (/^gpu\/[^/]+\/usage$/.test(modelData))
                return i18n("GPU");
            if (/^cpu\/.*temperature$/i.test(modelData))
                return i18n("CPU temp.");
            if (/^gpu\/.*temperature$/i.test(modelData))
                return i18n("GPU temp.");
            if (/^disk\/.*usedPercent$/.test(modelData))
                return i18n("Disk");
            return sensor.shortName;
        }
        readonly property string valueText: {
            if (!ok)
                return "";
            if (temperature)
                return Math.round(sensor.value) + "°";
            if (maximum === 100)
                return Math.round(sensor.value) + "%";
            return sensor.formattedValue;
        }

        Sensors.Sensor {
            id: sensor

            sensorId: entry.modelData
            updateRateLimit: 1500
        }
    }

    Repeater {
        id: entries

        model: Plasmoid.configuration.sensors
        delegate: Entry {}
    }

    // the entries there are, in order: those that are ready
    readonly property var shown: {
        const list = [];
        for (let i = 0; i < entries.count; i++) {
            const e = entries.itemAt(i);
            if (e && e.ok)
                list.push(e);
        }
        return list;
    }

    fullRepresentation: Common.Card {
        Layout.minimumWidth: Kirigami.Units.gridUnit * 6
        Layout.minimumHeight: Kirigami.Units.gridUnit * 6
        Layout.preferredWidth: Kirigami.Units.gridUnit * 16
        Layout.preferredHeight: Kirigami.Units.gridUnit * 9

        GridLayout {
            anchors.fill: parent
            columns: root.wide ? 2 : 1
            columnSpacing: Kirigami.Units.gridUnit
            rowSpacing: Kirigami.Units.largeSpacing

            Item {
                id: rings

                readonly property real side: root.wide
                    ? Math.min(parent.height, parent.width * 0.5)
                    : Math.min(parent.width, parent.height - legend.implicitHeight - Kirigami.Units.largeSpacing)
                readonly property int count: Math.max(1, root.shown.length)
                // the rings fill the outer three quarters of the radius
                readonly property real thickness: side / 2 * 0.75 / count * 0.8
                readonly property real gap: side / 2 * 0.75 / count * 0.2

                Layout.preferredWidth: side
                Layout.preferredHeight: side
                Layout.alignment: Qt.AlignCenter

                Repeater {
                    model: root.shown

                    Shape {
                        id: ring

                        required property var modelData
                        required property int index
                        readonly property real radius: rings.side / 2 - rings.thickness / 2 - index * (rings.thickness + rings.gap)
                        readonly property color colour: root.colours[index % root.colours.length]
                        property real fraction: modelData.fraction

                        Behavior on fraction {
                            NumberAnimation {
                                duration: Kirigami.Units.veryLongDuration
                                easing.type: Easing.OutCubic
                            }
                        }

                        anchors.fill: parent
                        preferredRendererType: Shape.CurveRenderer

                        // the track
                        ShapePath {
                            strokeColor: Qt.rgba(ring.colour.r, ring.colour.g, ring.colour.b, 0.2)
                            strokeWidth: rings.thickness
                            fillColor: "transparent"
                            capStyle: ShapePath.FlatCap

                            PathAngleArc {
                                centerX: rings.side / 2
                                centerY: rings.side / 2
                                radiusX: ring.radius
                                radiusY: ring.radius
                                startAngle: 0
                                sweepAngle: 360
                            }
                        }

                        // the value, clockwise from the top
                        ShapePath {
                            strokeColor: ring.colour
                            strokeWidth: rings.thickness
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap

                            PathAngleArc {
                                centerX: rings.side / 2
                                centerY: rings.side / 2
                                radiusX: ring.radius
                                radiusY: ring.radius
                                startAngle: -90
                                sweepAngle: Math.max(0.5, 360 * ring.fraction)
                            }
                        }
                    }
                }
            }

            GridLayout {
                id: legend

                Layout.fillWidth: root.wide
                Layout.alignment: root.wide ? Qt.AlignVCenter : Qt.AlignHCenter
                columns: root.wide ? 1 : 2
                columnSpacing: Kirigami.Units.gridUnit * 1.5
                rowSpacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: root.shown

                    RowLayout {
                        required property var modelData
                        required property int index

                        Layout.fillWidth: root.wide
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            Layout.preferredWidth: Kirigami.Units.smallSpacing * 2
                            Layout.preferredHeight: Layout.preferredWidth
                            radius: width / 2
                            color: root.colours[index % root.colours.length]
                        }
                        PlasmaComponents.Label {
                            Layout.fillWidth: root.wide
                            text: modelData.label
                            opacity: 0.75
                            elide: Text.ElideRight
                        }
                        PlasmaComponents.Label {
                            text: modelData.valueText
                            font.weight: Font.DemiBold
                            font.features: { "tnum": 1 }
                        }
                    }
                }
            }
        }

        PlasmaComponents.Label {
            anchors.centerIn: parent
            visible: root.shown.length === 0
            text: i18n("No sensors")
            opacity: 0.6
        }
    }
}
