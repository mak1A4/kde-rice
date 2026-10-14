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
#include <QStandardPaths>

using namespace Qt::StringLiterals;

static const auto s_service = u"org.kde.KWin"_s;
static const auto s_path = u"/Plugins"_s;
static const auto s_interface = u"org.kde.KWin.Plugins"_s;

KPluginMetaData KWinPlugin::metaData() const
{
    if (!m_script) {
        return KPluginMetaData::findPluginById(u"kwin/plugins"_s, m_id);
    }
    const QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, u"kwin/scripts/%1/metadata.json"_s.arg(m_id));
    return file.isEmpty() ? KPluginMetaData() : KPluginMetaData::fromJsonFile(file);
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
    const KPluginMetaData data = metaData();
    return data.isValid() ? data.name() : m_id;
}

QString KWinPlugin::description() const
{
    return metaData().description();
}

bool KWinPlugin::installed() const
{
    return !m_id.isEmpty() && metaData().isValid();
}

bool KWinPlugin::enabled() const
{
    const KPluginMetaData data = metaData();
    return pluginsGroup().readEntry(m_id + "Enabled"_L1, data.isValid() && data.isEnabledByDefault());
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

    // As System Settings does: a plugin is loaded or unloaded by name; for
    // a script, KWin starts whatever kwinrc now names, and one is unloaded
    // by name.
    QDBusMessage message;
    if (!m_script) {
        message = QDBusMessage::createMethodCall(s_service, s_path, s_interface, enabled ? u"LoadPlugin"_s : u"UnloadPlugin"_s);
        message << m_id;
    } else if (enabled) {
        message = QDBusMessage::createMethodCall(s_service, u"/Scripting"_s, u"org.kde.kwin.Scripting"_s, u"start"_s);
    } else {
        message = QDBusMessage::createMethodCall(s_service, u"/Scripting"_s, u"org.kde.kwin.Scripting"_s, u"unloadScript"_s);
        message << m_id;
    }
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        if (!m_script) {
            KWin::reconfigure();
        }
        refresh();
    });
}

void KWinPlugin::refresh()
{
    Q_EMIT enabledChanged();
    if (m_id.isEmpty()) {
        setLoaded(false);
        return;
    }
    if (m_script) {
        auto message = QDBusMessage::createMethodCall(s_service, u"/Scripting"_s, u"org.kde.kwin.Scripting"_s, u"isScriptLoaded"_s);
        message << m_id;
        auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
            watcher->deleteLater();
            const QDBusPendingReply<bool> reply = *watcher;
            setLoaded(!reply.isError() && reply.value());
        });
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
