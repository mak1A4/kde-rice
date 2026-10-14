import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

import io.github.mak1a4.rice

FormCard.FormCardPage {
    title: i18n("Panels")

    FormCard.FormHeader {
        title: i18n("Transparency")
    }

    FormCard.FormCard {
        FormCard.AbstractFormDelegate {
            background: null
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: i18n("Panel opacity")
                    }
                    QQC2.Label {
                        text: i18nc("percent", "%1 %", opacity.value)
                        font.features: { "tnum": 1 }
                    }
                }

                QQC2.Slider {
                    id: opacity

                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    stepSize: 5
                    value: PanelStyle.opacity
                    // applied once the slider rests: each change is the
                    // Plasma style applied anew
                    onMoved: settle.restart()
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                    text: i18n("Through kde-rice's Plasma style: Breeze, with the panel's background as see-through as set here (Breeze's own is 85 %). The panel must be Translucent or Adaptive in its settings.")
                }
            }

            Timer {
                id: settle
                interval: 400
                onTriggered: PanelStyle.opacity = opacity.value
            }
        }

        FormCard.FormDelegateSeparator {
            visible: PanelStyle.inUse
        }

        FormCard.FormButtonDelegate {
            visible: PanelStyle.inUse
            text: i18n("Back to the Plasma style before")
            icon.name: "edit-undo"
            onClicked: PanelStyle.restore()
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
