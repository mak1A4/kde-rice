// kde-rice-tray: tray entries that name a theme icon, in place of
// applications' entries that hand the tray a picture of their own.
//
// An application's tray entry (a StatusNotifierItem) either names an icon,
// which follows the icon theme, or sends a picture, which nothing can
// change: Electron applications send a picture. For the applications in
// the rules, this puts an entry of its own in the tray that names a theme
// icon, and passes everything else through to the application's entry: its
// properties, clicks, scrolling, its menu, and its signals. The
// application's own entry is hidden with Plasma's setting for it
// (disabledStatusNotifiers of the system tray), while this runs.
//
// Plasma hides by the id it knows an entry by (Electron's, which are all
// chrome_status_icon_<n>, with their program's name: plasmaId()); should
// another entry share a hidden id, it is passed on too, unchanged, picture
// and all.
//
// Rules: tray-rules.json, kde-rice's in its data directory, then the
// user's in ~/.config/kde-rice (same name: the user's), as
//   [{ "name": "Teams for Linux", "id": "Teams for Linux_status_icon_*",
//      "process": "teams-for-linux", "icon": "kde-rice-teams-tray",
//      "attentionIcon": "kde-rice-teams-tray-attention" }]
// `id` is the entry's id (* and ? as wildcards), `process` (optional) the
// name of the program that owns it, `icon` the icon to name, and
// `attentionIcon` (optional) the one while the application's own picture
// is not its first one (a count of unread messages painted into it).

#include <KSignalHandler>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <csignal>
#include <memory>

using namespace Qt::StringLiterals;

namespace
{

const auto s_item = u"org.kde.StatusNotifierItem"_s;
const auto s_menu = u"com.canonical.dbusmenu"_s;
const auto s_properties = u"org.freedesktop.DBus.Properties"_s;
const auto s_watcher = u"org.kde.StatusNotifierWatcher"_s;
const auto s_itemPath = u"/StatusNotifierItem"_s;
const auto s_menuPath = u"/MenuBar"_s;

// a picture, as the tray protocol sends one: a(iiay)
struct Pixmap {
    int width = 0;
    int height = 0;
    QByteArray data;
};

QDBusArgument &operator<<(QDBusArgument &argument, const Pixmap &pixmap)
{
    argument.beginStructure();
    argument << pixmap.width << pixmap.height << pixmap.data;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Pixmap &pixmap)
{
    argument.beginStructure();
    argument >> pixmap.width >> pixmap.height >> pixmap.data;
    argument.endStructure();
    return argument;
}

}

Q_DECLARE_METATYPE(Pixmap)

namespace
{

struct Rule {
    QString name;
    QRegularExpression id;
    QString process;
    QString icon;
    QString attentionIcon;
};

QList<Rule> loadRules()
{
    QList<Rule> rules;
    QStringList files = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, u"kde-rice/tray-rules.json"_s);
#ifdef KDERICE_SOURCE_TRAY_RULES
    if (files.isEmpty()) {
        files += QStringLiteral(KDERICE_SOURCE_TRAY_RULES);
    }
#endif
    // the system's first, the user's last: a rule of the same name replaces
    std::reverse(files.begin(), files.end());
    files += QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/kde-rice/tray-rules.json"_L1;
    for (const QString &path : std::as_const(files)) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonArray list = QJsonDocument::fromJson(file.readAll()).array();
        for (const QJsonValue &value : list) {
            const QJsonObject object = value.toObject();
            Rule rule{object[u"name"_s].toString(),
                      QRegularExpression(QRegularExpression::wildcardToRegularExpression(object[u"id"_s].toString())),
                      object[u"process"_s].toString(),
                      object[u"icon"_s].toString(),
                      object[u"attentionIcon"_s].toString()};
            if (!rule.id.isValid() || object[u"id"_s].toString().isEmpty() || rule.icon.isEmpty()) {
                continue;
            }
            rules.erase(std::remove_if(rules.begin(), rules.end(), [&rule](const Rule &other) {
                            return !rule.name.isEmpty() && other.name == rule.name;
                        }),
                        rules.end());
            rules += rule;
        }
    }
    return rules;
}

// the name of the program a bus name belongs to
QString processOf(const QString &service)
{
    const QDBusReply<uint> pid = QDBusConnection::sessionBus().interface()->servicePid(service);
    if (!pid.isValid()) {
        return {};
    }
    return QFileInfo(QFile::symLinkTarget(u"/proc/%1/exe"_s.arg(pid.value()))).fileName();
}

// The id Plasma's system tray knows an entry by, which is what its setting
// hides: the entry's own, but for Chromium's and Electron's, which all say
// chrome_status_icon_<n>, the name of their program after an @ (the folder
// of the .asar on its command line, else /proc/<pid>/comm), as
// plasma-workspace's systemtraymodel.cpp does.
QString plasmaId(const QString &service, const QString &id)
{
    if (!id.startsWith("chrome_status_icon_"_L1)) {
        return id;
    }
    const QDBusReply<uint> pid = QDBusConnection::sessionBus().interface()->servicePid(service);
    if (!pid.isValid()) {
        return id;
    }
    const QString proc = u"/proc/%1/"_s.arg(pid.value());
    QFile cmdline(proc + "cmdline"_L1);
    if (cmdline.open(QIODevice::ReadOnly)) {
        const QList<QByteArray> arguments = cmdline.readAll().split('\0');
        for (const QByteArray &argument : arguments) {
            if (argument.endsWith(".asar")) {
                return id + u'@' + QFileInfo(QFileInfo(QString::fromUtf8(argument)).path()).fileName();
            }
        }
    }
    QFile comm(proc + "comm"_L1);
    if (comm.open(QIODevice::ReadOnly)) {
        const QString name = QString::fromUtf8(comm.readAll()).trimmed();
        if (!name.isEmpty()) {
            return id + u'@' + name;
        }
    }
    return id;
}

QVariant itemProperty(const QString &service, const QString &path, const QString &name)
{
    auto message = QDBusMessage::createMethodCall(service, path, s_properties, u"Get"_s);
    message << s_item << name;
    const QDBusReply<QDBusVariant> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 3000);
    return reply.isValid() ? reply.value().variant() : QVariant();
}

QByteArray pixmapHash(const QVariant &value)
{
    // a(iiay), as it came: its bytes are enough to tell two pictures apart
    QList<Pixmap> pixmaps;
    if (value.canConvert<QDBusArgument>()) {
        value.value<QDBusArgument>() >> pixmaps;
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    for (const Pixmap &pixmap : std::as_const(pixmaps)) {
        hash.addData(QByteArray(QByteArray::number(pixmap.width) + 'x' + QByteArray::number(pixmap.height)));
        hash.addData(pixmap.data);
    }
    return hash.result();
}

// One tray entry in place of an application's: everything asked of it is
// asked of the application's, but for its icon (when a rule names one)
// and where its menu is. On a connection of its own, so that it goes from
// the tray when it is closed.
class Proxy : public QDBusVirtualObject
{
    Q_OBJECT

public:
    Proxy(const QString &service, const QString &path, const Rule *rule, const QString &process, int number)
        : m_service(service)
        , m_id(u"kde-rice:"_s + (process.isEmpty() ? QString::number(number) : process))
        , m_path(path)
        , m_connection(QDBusConnection::connectToBus(QDBusConnection::SessionBus, u"kde-rice-tray-%1"_s.arg(number)))
    {
        if (rule) {
            m_icon = rule->icon;
            m_attentionIcon = rule->attentionIcon;
        }
        const QVariant menu = itemProperty(service, path, u"Menu"_s);
        m_menu = menu.value<QDBusObjectPath>().path();
        m_baseline = pixmapHash(itemProperty(service, path, u"IconPixmap"_s));

        auto bus = QDBusConnection::sessionBus();
        for (const auto *signal : {"NewTitle", "NewIcon", "NewAttentionIcon", "NewOverlayIcon", "NewMenu", "NewToolTip", "NewStatus"}) {
            bus.connect(service, path, s_item, QString::fromLatin1(signal), this, SLOT(forwardItemSignal(QDBusMessage)));
        }
        if (!m_menu.isEmpty()) {
            for (const auto *signal : {"LayoutUpdated", "ItemsPropertiesUpdated", "ItemActivationRequested"}) {
                bus.connect(service, m_menu, s_menu, QString::fromLatin1(signal), this, SLOT(forwardMenuSignal(QDBusMessage)));
            }
        }
        m_connection.registerVirtualObject(u"/"_s, this, QDBusConnection::SubPath);
        auto registration = QDBusMessage::createMethodCall(s_watcher, u"/StatusNotifierWatcher"_s, s_watcher, u"RegisterStatusNotifierItem"_s);
        registration << s_itemPath;
        m_connection.call(registration, QDBus::Block, 3000);
    }

    ~Proxy() override
    {
        m_connection.unregisterObject(u"/"_s, QDBusConnection::UnregisterTree);
        QDBusConnection::disconnectFromBus(m_connection.name());
    }

    QString uniqueName() const
    {
        return m_connection.baseService();
    }

    QString introspect(const QString &) const override
    {
        return {};
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        const bool menu = message.path() == s_menuPath;
        if (!menu && message.path() != s_itemPath) {
            return false;
        }
        if (menu && m_menu.isEmpty()) {
            return false;
        }
        const bool item = !menu && message.interface() == s_properties && message.arguments().value(0).toString() == s_item;
        // what is answered here
        if (item && message.member() == "Get"_L1) {
            const QVariant own = ownProperty(message.arguments().value(1).toString());
            if (own.isValid()) {
                connection.send(message.createReply(QVariant::fromValue(QDBusVariant(own))));
                return true;
            }
        }
        // everything else, the application's answer
        auto call = QDBusMessage::createMethodCall(m_service, menu ? m_menu : m_path, message.interface(), message.member());
        call.setArguments(message.arguments());
        if (message.isReplyRequired() == false) {
            QDBusConnection::sessionBus().send(call);
            return true;
        }
        auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call, 5000), this);
        const bool all = item && message.member() == "GetAll"_L1;
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, message, connection, all] {
            watcher->deleteLater();
            const QDBusMessage reply = watcher->reply();
            if (reply.type() == QDBusMessage::ErrorMessage) {
                connection.send(message.createErrorReply(reply.errorName(), reply.errorMessage()));
                return;
            }
            if (!all) {
                connection.send(message.createReply(reply.arguments()));
                return;
            }
            QVariantMap properties = qdbus_cast<QVariantMap>(reply.arguments().value(0));
            for (const auto *name : {"Id", "IconName", "IconThemePath", "IconPixmap", "Menu"}) {
                const QVariant own = ownProperty(QString::fromLatin1(name));
                if (own.isValid()) {
                    properties.insert(QString::fromLatin1(name), own);
                }
            }
            connection.send(message.createReply(QVariant::fromValue(properties)));
        });
        return true;
    }

private Q_SLOTS:
    void forwardItemSignal(const QDBusMessage &signal)
    {
        // the application's picture changed: with a count painted in, or
        // back to its first one
        if (signal.member() == "NewIcon"_L1 && !m_attentionIcon.isEmpty()) {
            m_attention = pixmapHash(itemProperty(m_service, m_path, u"IconPixmap"_s)) != m_baseline;
        }
        forward(signal, s_itemPath);
    }

    void forwardMenuSignal(const QDBusMessage &signal)
    {
        forward(signal, s_menuPath);
    }

private:
    // what this entry says of itself; nothing where it says what the
    // application's says
    QVariant ownProperty(const QString &name) const
    {
        // an id of its own: Plasma hides the application's by its id
        if (name == "Id"_L1) {
            return m_id;
        }
        if (name == "Menu"_L1 && !m_menu.isEmpty()) {
            return QVariant::fromValue(QDBusObjectPath(s_menuPath));
        }
        if (m_icon.isEmpty()) {
            return {};
        }
        if (name == "IconName"_L1) {
            return m_attention && !m_attentionIcon.isEmpty() ? m_attentionIcon : m_icon;
        }
        if (name == "IconThemePath"_L1) {
            return QString();
        }
        if (name == "IconPixmap"_L1) {
            return QVariant::fromValue(QList<Pixmap>());
        }
        return {};
    }

    void forward(const QDBusMessage &signal, const QString &path)
    {
        auto copy = QDBusMessage::createSignal(path, signal.interface(), signal.member());
        copy.setArguments(signal.arguments());
        m_connection.send(copy);
    }

    QString m_service;
    QString m_id;
    QString m_path;
    QString m_menu;
    QString m_icon;
    QString m_attentionIcon;
    QByteArray m_baseline;
    bool m_attention = false;
    QDBusConnection m_connection;
};

// The entries in the tray, the proxies for those the rules name, and
// Plasma's setting that hides theirs.
class Tray : public QObject
{
    Q_OBJECT

public:
    Tray()
        : m_rules(loadRules())
    {
        auto bus = QDBusConnection::sessionBus();
        bus.connect(s_watcher, u"/StatusNotifierWatcher"_s, s_watcher, u"StatusNotifierItemRegistered"_s, this, SLOT(changed()));
        bus.connect(s_watcher, u"/StatusNotifierWatcher"_s, s_watcher, u"StatusNotifierItemUnregistered"_s, this, SLOT(changed()));
        m_settle.setSingleShot(true);
        m_settle.setInterval(300);
        connect(&m_settle, &QTimer::timeout, this, &Tray::scan);
        // left over by a run that did not end well
        hide({});
        scan();
    }

    ~Tray() override
    {
        // the applications' own entries come back
        hide({});
    }

private Q_SLOTS:
    void changed()
    {
        m_settle.start();
    }

private:
    void scan()
    {
        auto message = QDBusMessage::createMethodCall(s_watcher, u"/StatusNotifierWatcher"_s, s_properties, u"Get"_s);
        message << s_watcher << u"RegisteredStatusNotifierItems"_s;
        const QDBusReply<QDBusVariant> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 3000);
        const QStringList items = reply.isValid() ? reply.value().variant().toStringList() : QStringList();

        QStringList own;
        for (const auto &proxy : std::as_const(m_proxies)) {
            own += proxy->uniqueName();
        }

        struct Entry {
            QString key;
            QString service;
            QString path;
            QString id;
            QString plasmaId;
            QString process;
            const Rule *rule = nullptr;
        };
        QList<Entry> entries;
        QSet<QString> hidden;
        for (const QString &item : items) {
            const qsizetype slash = item.indexOf(u'/');
            Entry entry;
            entry.key = item;
            entry.service = slash < 0 ? item : item.left(slash);
            entry.path = slash < 0 ? s_itemPath : item.mid(slash);
            if (own.contains(entry.service) || entry.service.startsWith("kde-rice"_L1)) {
                continue;
            }
            // a proxy of ours, under its unique name
            if (QDBusConnection::sessionBus().interface()->serviceOwner(entry.service).value() == QDBusConnection::sessionBus().baseService()) {
                continue;
            }
            entry.id = itemProperty(entry.service, entry.path, u"Id"_s).toString();
            entry.plasmaId = plasmaId(entry.service, entry.id);
            entry.process = processOf(entry.service);
            for (const Rule &rule : std::as_const(m_rules)) {
                if (rule.id.match(entry.id).hasMatch() && (rule.process.isEmpty() || rule.process == entry.process)) {
                    entry.rule = &rule;
                    hidden += entry.plasmaId;
                    break;
                }
            }
            entries += entry;
        }

        // a proxy for every entry whose id is hidden (another entry may share
        // it); none for the others
        QSet<QString> wanted;
        for (const Entry &entry : std::as_const(entries)) {
            if (!hidden.contains(entry.plasmaId)) {
                continue;
            }
            wanted += entry.key;
            if (!m_proxies.contains(entry.key)) {
                m_proxies.insert(entry.key, std::make_shared<Proxy>(entry.service, entry.path, entry.rule, entry.process, ++m_count));
            }
        }
        for (auto it = m_proxies.begin(); it != m_proxies.end();) {
            it = wanted.contains(it.key()) ? std::next(it) : m_proxies.erase(it);
        }
        hide(hidden);
    }

    // Plasma's system trays hide the entries with these ids (and no longer
    // the ones this hid before)
    void hide(const QSet<QString> &ids)
    {
        const QString state = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/kde-rice/tray-hidden"_L1;
        QStringList before;
        QFile file(state);
        if (file.open(QIODevice::ReadOnly)) {
            before = QString::fromUtf8(file.readAll()).split(u'\n', Qt::SkipEmptyParts);
            file.close();
        }
        QStringList now(ids.cbegin(), ids.cend());
        now.sort();
        if (now == before && !before.isEmpty()) {
            return;
        }
        if (now.isEmpty() && before.isEmpty()) {
            return;
        }
        const auto quoted = [](const QStringList &list) {
            QStringList out;
            for (QString id : list) {
                id.replace(u'\\', u"\\\\"_s);
                id.replace(u'"', u"\\\""_s);
                out += u'"' + id + u'"';
            }
            return u'[' + out.join(u',') + u']';
        };
        const QString script = u"const before = %1, now = %2;"
                               "for (const p of panels()) for (const w of p.widgets()) if (w.type === 'org.kde.plasma.systemtray') {"
                               "  w.currentConfigGroup = ['General'];"
                               "  let list = (w.readConfig('disabledStatusNotifiers', []) || []).filter(id => !before.includes(id));"
                               "  for (const id of now) if (!list.includes(id)) list.push(id);"
                               "  w.writeConfig('disabledStatusNotifiers', list);"
                               "}"_s.arg(quoted(before), quoted(now));
        auto message = QDBusMessage::createMethodCall(u"org.kde.plasmashell"_s, u"/PlasmaShell"_s, u"org.kde.PlasmaShell"_s, u"evaluateScript"_s);
        message << script;
        const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 5000);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            // Plasma not there yet: again in a moment
            QTimer::singleShot(2000, this, &Tray::scan);
            return;
        }
        QDir().mkpath(QFileInfo(state).absolutePath());
        QSaveFile out(state);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(now.join(u'\n').toUtf8());
            out.commit();
        }
    }

    QList<Rule> m_rules;
    QHash<QString, std::shared_ptr<Proxy>> m_proxies;
    QTimer m_settle;
    int m_count = 0;
};

}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"kde-rice-tray"_s);
    qDBusRegisterMetaType<Pixmap>();
    qDBusRegisterMetaType<QList<Pixmap>>();

    // one at a time
    if (!QDBusConnection::sessionBus().registerService(u"io.github.mak1a4.kde-rice.tray"_s)) {
        return 0;
    }

    // ended by the session or by hand: the applications' entries back first
    KSignalHandler::self()->watchSignal(SIGTERM);
    KSignalHandler::self()->watchSignal(SIGINT);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, &app, &QCoreApplication::quit);

    auto tray = std::make_unique<Tray>();
    const int result = app.exec();
    tray.reset();
    return result;
}

#include "main.moc"
