#include "panellayout.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>

using namespace Qt::StringLiterals;

namespace
{

const QStringList s_names = {u"floating"_s, u"popups"_s, u"docked"_s};

KConfigGroup settings()
{
    auto config = KSharedConfig::openConfig(u"kde-ricerc"_s);
    config->reparseConfiguration();
    return config->group(u"Panels"_s);
}

QString evaluateInPlasma(const QString &script)
{
    auto message = QDBusMessage::createMethodCall(u"org.kde.plasmashell"_s, u"/PlasmaShell"_s, u"org.kde.PlasmaShell"_s, u"evaluateScript"_s);
    message.setArguments({script});
    const QDBusReply<QString> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 10000);
    return reply.isValid() ? reply.value().trimmed() : QString();
}

int chosenMode()
{
    return s_names.indexOf(settings().readEntry("Floating"));
}

}

PanelLayout::PanelLayout(QObject *parent)
    : QObject(parent)
{
    // the first panel's, as Plasma has it now
    const QString now = evaluateInPlasma(
        u"const p = panels()[0]; print(!p ? '' : p.floating ? 'floating' : p.floatingApplets ? 'popups' : 'docked');"_s);
    const int mode = s_names.indexOf(now);
    m_mode = mode >= 0 ? mode : std::max(chosenMode(), 0);
}

void PanelLayout::setMode(int mode)
{
    if (mode < Floating || mode > Docked) {
        return;
    }
    KConfigGroup group = settings();
    group.writeEntry("Floating", s_names[mode]);
    group.sync();
    apply(Mode(mode));
    if (mode != m_mode) {
        m_mode = mode;
        Q_EMIT changed();
    }
}

void PanelLayout::apply(Mode mode)
{
    evaluateInPlasma(u"for (const p of panels()) { p.floating = %1; p.floatingApplets = %2; }"_s.arg(mode == Floating ? "true"_L1 : "false"_L1,
                                                                                                    mode == FloatingPopups ? "true"_L1 : "false"_L1));
}

bool PanelLayout::chosen()
{
    return chosenMode() >= 0;
}

void PanelLayout::applyPlasmaDefault()
{
    apply(Floating);
}

void PanelLayout::applyChosen()
{
    const int mode = chosenMode();
    if (mode >= 0) {
        apply(Mode(mode));
    }
}
