import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

import io.github.mak1a4.rice

FormCard.FormCardPage {
    title: i18n("Windows")

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
            enabled: gaps.value
            from: 0
            to: 60
            value: gapSize.value
            onValueChanged: if (value !== gapSize.value) gapSize.value = value
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
            from: 0
            to: 40
            value: radius.value
            onValueChanged: if (value !== radius.value) radius.value = value
        }
    }
}
