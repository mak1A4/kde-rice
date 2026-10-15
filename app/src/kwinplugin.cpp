#include "kwinplugin.h"
#include "kwin.h"

#include <KConfigGroup>
#include <KPluginMetaData>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

using namespace Qt::StringLiterals;

static const auto s_service = u"org.kde.KWin"_s;
static const auto s_path = u"/Plugins"_s;
static const auto s_interface = u"org.kde.KWin.Plugins"_s;

static KPluginMetaData metaData(const QString &id)
{
    return KPluginMetaData::findPluginById(u"kwin/plugins"_s, id);
}

static KConfigGroup pluginsGroup()
{
    auto config = KSharedConfig::openConfig(u"kwinrc"_s, KConfig::NoGlobals);
    config->reparseConfiguration();
    return config->group(u"Plugins"_s);
}

KWinPlugin::KWinPlugin(QObject *parent)
    : QObject(parent)
{
}

void KWinPlugin::componentComplete()
{
    connect(this, &KWinPlugin::pluginIdChanged, this, &KWinPlugin::refresh);
    refresh();
}

QString KWinPlugin::name() const
{
    return m_data.isValid() ? m_data.name() : m_id;
}

QString KWinPlugin::description() const
{
    return m_data.description();
}

bool KWinPlugin::installed() const
{
    return !m_id.isEmpty() && m_data.isValid();
}

bool KWinPlugin::enabled() const
{
    return pluginsGroup().readEntry(m_id + "Enabled"_L1, m_data.isValid() && m_data.isEnabledByDefault());
}

void KWinPlugin::setEnabled(bool enabled)
{
    if (m_id.isEmpty() || enabled == this->enabled()) {
        return;
    }
    KConfigGroup group = pluginsGroup();
    group.writeEntry(m_id + "Enabled"_L1, enabled, KConfig::Notify);
    group.sync();
    Q_EMIT enabledChanged();

    auto message = QDBusMessage::createMethodCall(s_service, s_path, s_interface, enabled ? u"LoadPlugin"_s : u"UnloadPlugin"_s);
    message << m_id;
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        KWin::reconfigure();
        refresh();
    });
}

void KWinPlugin::refresh()
{
    m_data = m_id.isEmpty() ? KPluginMetaData() : metaData(m_id);
    Q_EMIT enabledChanged();
    if (m_id.isEmpty()) {
        setLoaded(false);
        return;
    }
    auto message = QDBusMessage::createMethodCall(s_service, s_path, u"org.freedesktop.DBus.Properties"_s, u"Get"_s);
    message << s_interface << u"LoadedPlugins"_s;
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<QDBusVariant> reply = *watcher;
        setLoaded(!reply.isError() && reply.value().variant().toStringList().contains(m_id));
    });
}

void KWinPlugin::setLoaded(bool loaded)
{
    if (m_loaded != loaded) {
        m_loaded = loaded;
        Q_EMIT loadedChanged();
    }
}
