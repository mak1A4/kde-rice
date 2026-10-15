import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

import io.github.mak1a4.rice

FormCard.FormCardPage {
    id: page

    title: i18n("Windows")

    // The scale of the screen the hub is on: its window's, as Plasma gives
    // it (1.33 at 133 %); the screen's own is rounded up to a whole number
    readonly property real screenScale: page.Window.window ? page.Window.window.devicePixelRatio : 1
    // The smallest step in logical pixels that is a whole number of the
    // screen's pixels (3 at 133 %, 4 at 125 %, 2 at 150 %): gaps and corners
    // in such steps fall on the pixel grid, where others leave window edges
    // and the corners' outline between two pixels, blurred and jagged.
    readonly property int pixelStep: {
        for (let n = 1; n <= 8; ++n) {
            if (Math.abs(n * screenScale - Math.round(n * screenScale)) < 0.01) {
                return n;
            }
        }
        return 1;
    }
    readonly property string stepHint: pixelStep > 1 ? i18n("In steps of %1, whole pixels at %2 % scale.", pixelStep, Math.round(screenScale * 100)) : ""

    function snapped(value) {
        return Math.round(value / pixelStep) * pixelStep;
    }

    KWinPlugin {
        id: corners
        pluginId: "kderice_roundcorners"
    }

    ConfigEntry {
        id: radius
        file: "kwinrc"
        group: "KDERiceRoundCorners"
        key: "Radius"
        defaultValue: 5
        kwin: true
    }

    ConfigEntry {
        id: gaps
        file: "kde-ricerc"
        group: "Gaps"
        key: "Enabled"
        defaultValue: false
    }

    ConfigEntry {
        id: gapSize
        file: "kde-ricerc"
        group: "Gaps"
        key: "Size"
        defaultValue: 18
    }

    FormCard.FormHeader {
        title: i18n("Gaps")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18n("Gaps around maximized windows")
            description: i18n("Maximized and tiled windows keep a gap to the screen's edges and to the panel, so a floating panel stays floating.")
            checked: gaps.value
            onToggled: {
                gaps.value = checked;
                // off, it quits by itself
                if (checked) {
                    Session.start("kde-rice-gaps");
                }
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSpinBoxDelegate {
            label: i18n("Gap (logical pixels)")
            description: page.stepHint
            enabled: gaps.value
            from: 0
            to: 60
            stepSize: page.pixelStep
            Binding on value {
                value: gapSize.value
            }
            onValueChanged: {
                // typed, or stepped from a value off the grid: onto it
                if (value !== page.snapped(value)) {
                    value = page.snapped(value);
                } else if (value !== gapSize.value) {
                    gapSize.value = value;
                }
            }
        }
    }

    FormCard.FormHeader {
        title: i18n("Corners")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18n("Rounded corners on every window")
            description: corners.installed
                ? i18n("Maximized windows too, and windows that draw their own square corners.")
                : i18n("Not installed: build and install kwin/plugins/roundcorners.")
            enabled: corners.installed
            checked: corners.enabled
            onToggled: corners.enabled = checked
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSwitchDelegate {
            text: i18n("kde-rice title bars")
            description: KWin.riceDecorationInstalled
                ? i18n("Breeze with the same corners, kept when a window is maximized or tiled.")
                : i18n("Not installed: build and install kwin/decoration/breeze.")
            enabled: KWin.riceDecorationInstalled
            checked: KWin.decoration === KWin.riceDecoration
            onToggled: KWin.decoration = checked ? KWin.riceDecoration : "org.kde.breeze"
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormSpinBoxDelegate {
            label: i18n("Corner radius (logical pixels)")
            description: page.stepHint
            from: 0
            to: 40
            stepSize: page.pixelStep
            Binding on value {
                value: radius.value
            }
            onValueChanged: {
                if (value !== page.snapped(value)) {
                    value = page.snapped(value);
                } else if (value !== radius.value) {
                    radius.value = value;
                }
            }
        }
    }
}
