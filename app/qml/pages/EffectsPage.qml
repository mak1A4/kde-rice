import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

import io.github.mak1a4.rice

FormCard.FormCardPage {
    title: i18n("Effects")

    // kde-rice's own setting, kept while kde-rice is off (when Plasma's
    // splash screen is given back)
    ConfigEntry {
        id: splash
        file: "kde-ricerc"
        group: "Session"
        key: "Splash"
        defaultValue: true
    }

    // Plasma's: off as its Splash Screen page writes it, shown is no entry
    ConfigEntry {
        id: splashEngine
        file: "ksplashrc"
        group: "KSplash"
        key: "Engine"
        defaultValue: "KSplashQML"
    }

    ConfigEntry {
        id: splashTheme
        file: "ksplashrc"
        group: "KSplash"
        key: "Theme"
        defaultValue: ""
    }

    // Plasma's application style; no entry is Breeze
    ConfigEntry {
        id: widgetStyle
        file: "kdeglobals"
        group: "KDE"
        key: "widgetStyle"
        defaultValue: "Breeze"
    }

    FormCard.FormHeader {
        title: i18n("Applications")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18n("Highlights without an outline")
            description: Session.riceStyleInstalled()
                ? i18n("What is selected or under the pointer in a menu, a list or a file view is a fill alone: kde-rice's build of Breeze as the application style. Off is Breeze itself.")
                : i18n("Not installed: build and install style/.")
            enabled: Session.riceStyleInstalled()
            checked: widgetStyle.value === Session.riceStyle()
            onToggled: {
                widgetStyle.value = checked ? Session.riceStyle() : "Breeze";
                Session.widgetStyleChanged();
            }
        }
    }

    FormCard.FormHeader {
        title: i18n("Login")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18n("Splash screen at login")
            description: i18n("Off, the desktop is shown as soon as it is there. On is the global theme's splash screen.")
            checked: splash.value
            onToggled: {
                splash.value = checked;
                splashEngine.value = checked ? "KSplashQML" : "none";
                splashTheme.value = checked ? "" : "None";
            }
        }
    }
}
