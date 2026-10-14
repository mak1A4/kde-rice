import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

import io.github.mak1a4.rice

FormCard.FormCardPage {
    title: i18n("Panels")

    KWinPlugin {
        id: notifications
        pluginId: "kderice_notifications"
        script: true
    }

    FormCard.FormHeader {
        title: i18n("Notifications")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: i18n("Line notifications up with the panel")
            description: notifications.installed
                ? i18n("Popups in the bottom-right corner end at the floating panel's right edge, the panel's float gap above it, instead of Plasma's fixed distance.")
                : i18n("Not installed: install kwin/scripts/notifications.")
            enabled: notifications.installed
            checked: notifications.enabled
            onToggled: notifications.enabled = checked
        }
    }

    FormCard.FormHeader {
        title: i18n("Layout")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: i18n("Not here yet")
            description: i18n("The bar, dock and widgets, laid out the same way each time.")
        }
    }
}
