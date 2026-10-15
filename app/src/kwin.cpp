#include "kwin.h"

#include <KConfigGroup>
#include <KPluginMetaData>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QTimer>

using namespace Qt::StringLiterals;

static const auto s_decorationGroup = u"org.kde.kdecoration2"_s;
static const auto s_breeze = u"org.kde.breeze"_s;

KWin::KWin(QObject *parent)
    : QObject(parent)
{
}

void KWin::reconfigure()
{
    // changes close together (a spin box stepped through its values) are
    // one reconfigure, once they rest: each is KWin reading all its settings
    static QTimer *settle = [] {
        auto timer = new QTimer(QCoreApplication::instance());
        timer->setSingleShot(true);
        timer->setInterval(250);
        QObject::connect(timer, &QTimer::timeout, [] {
            QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(u"org.kde.KWin"_s, u"/KWin"_s, u"org.kde.KWin"_s, u"reconfigure"_s));
        });
        return timer;
    }();
    settle->start();
}

QString KWin::decoration() const
{
    auto config = KSharedConfig::openConfig(u"kwinrc"_s, KConfig::NoGlobals);
    config->reparseConfiguration();
    return config->group(s_decorationGroup).readEntry("library", s_breeze);
}

void KWin::setDecoration(const QString &id)
{
    if (id == decoration()) {
        return;
    }
    auto config = KSharedConfig::openConfig(u"kwinrc"_s, KConfig::NoGlobals);
    KConfigGroup group = config->group(s_decorationGroup);
    group.writeEntry("library", id.isEmpty() ? s_breeze : id, KConfig::Notify);
    config->sync();
    reconfigure();
    Q_EMIT decorationChanged();
}

QString KWin::riceDecoration()
{
    return QStringLiteral(KDERICE_ID ".breeze");
}

bool KWin::riceDecorationInstalled()
{
    return KPluginMetaData::findPluginById(u"org.kde.kdecoration3"_s, riceDecoration()).isValid();
}
