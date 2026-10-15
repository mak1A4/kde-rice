#include "riceswitch.h"
#include "applier.h"
#include "icons.h"
#include "kwin.h"
#include "panelstyle.h"
#include "session.h"
#include "theme.h"

#include <KConfigGroup>
#include <KPluginMetaData>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>

#include <unistd.h>

using namespace Qt::StringLiterals;

namespace
{

// kde-rice's KWin plugins (kwin/plugins)
const QStringList s_kwinPlugins = {u"kderice_roundcorners"_s, u"kderice_gaps"_s};
// kde-rice's programs that run in the background, and their autostart
// entries (with the icon theme's, written again at login)
const QStringList s_programs = {u"kde-rice-launcher"_s, u"kde-rice-tray"_s, u"kde-rice-gaps"_s};
const QStringList s_autostart = {u"launcher"_s, u"tray"_s, u"gaps"_s, u"icons"_s};
const auto s_loginScreen = u"/usr/lib/kde-rice/login-screen"_s;
const auto s_loginSetting = u"/etc/plasmalogin.conf.d/kde-rice.conf"_s;
const auto s_widgetPrefix = u"io.github.mak1a4.kde-rice."_s;

QString stateFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/kde-rice/off.json"_L1;
}

QString autostartFile(const QString &part)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/autostart/"_L1 + QStringLiteral(KDERICE_ID) + u'.' + part
        + ".desktop"_L1;
}

bool filesOnly()
{
    return qEnvironmentVariableIntValue("KDERICE_FILES_ONLY") == 1;
}

KSharedConfigPtr open(const QString &file)
{
    auto config = KSharedConfig::openConfig(file, KConfig::NoGlobals);
    config->reparseConfiguration();
    return config;
}

QDBusMessage call(const QString &service, const QString &path, const QString &interface, const QString &method, const QVariantList &arguments = {})
{
    auto message = QDBusMessage::createMethodCall(service, path, interface, method);
    message.setArguments(arguments);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, 10000);
}

QString evaluateInPlasma(const QString &script)
{
    const QDBusReply<QString> reply =
        call(u"org.kde.plasmashell"_s, u"/PlasmaShell"_s, u"org.kde.PlasmaShell"_s, u"evaluateScript"_s, {script});
    return reply.isValid() ? reply.value().trimmed() : QString();
}

bool plasmaRunning()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(u"org.kde.plasmashell"_s);
}

void kwinReconfigure()
{
    call(u"org.kde.KWin"_s, u"/KWin"_s, u"org.kde.KWin"_s, u"reconfigure"_s);
}

// A program of kde-rice's, by the program in its command line (pkill -x
// knows a name only to 15 characters: kde-rice-launch)
QString pattern(const QString &program)
{
    return u"^([^ ]*/)?%1( |$)"_s.arg(program);
}

// pgrep or pkill for it: true if there was one
bool find(const QString &tool, const QString &program)
{
    QProcess process;
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(tool, {u"-u"_s, QString::number(getuid()), u"-f"_s, pattern(program)});
    return process.waitForFinished(5000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool running(const QString &program)
{
    return find(u"pgrep"_s, program);
}

// Ends it and waits for it (the tray gives the applications' tray entries
// back as it goes).
void stop(const QString &program)
{
    if (!find(u"pkill"_s, program)) {
        return;
    }
    for (int waited = 0; waited < 3000 && running(program); waited += 100) {
        QThread::msleep(100);
    }
}

// kde-rice's widgets on the desktops: where each is, and its settings
QJsonArray widgets()
{
    const QString list = evaluateInPlasma(uR"(const found = [];
        for (const d of desktops()) for (const w of d.widgets()) if (w.type.startsWith('%1')) {
            w.currentConfigGroup = ['General'];
            const config = {};
            for (const key of w.configKeys) config[key] = w.readConfig(key);
            const g = w.geometry;
            found.push({ desktop: d.id, screen: d.screen, widget: w.id, type: w.type,
                         x: g.x, y: g.y, width: g.width, height: g.height, config });
        }
        print(JSON.stringify(found));)"_s.arg(s_widgetPrefix));
    return QJsonDocument::fromJson(list.toUtf8()).array();
}

void removeWidgets(const QJsonArray &widgets)
{
    QJsonArray ids;
    for (const QJsonValue &widget : widgets) {
        ids.append(widget[u"widget"_s]);
    }
    evaluateInPlasma(u"const ids = %1; for (const d of desktops()) for (const w of d.widgets()) if (ids.includes(w.id)) w.remove();"_s.arg(
        QString::fromUtf8(QJsonDocument(ids).toJson(QJsonDocument::Compact))));
}

// back where they were: on their desktop, else the one on the same screen
void addWidgets(const QJsonArray &widgets)
{
    if (widgets.isEmpty()) {
        return;
    }
    evaluateInPlasma(uR"(for (const s of %1) {
            const d = desktopById(s.desktop) ?? desktops().find(d => d.screen === s.screen) ?? desktops()[0];
            if (!d || d.widgets().some(w => w.type === s.type)) continue;
            const w = d.addWidget(s.type, s.x, s.y, s.width, s.height);
            if (!w) continue;
            w.currentConfigGroup = ['General'];
            for (const key in s.config) w.writeConfig(key, s.config[key]);
        })"_s.arg(QString::fromUtf8(QJsonDocument(widgets).toJson(QJsonDocument::Compact))));
}

// Applets on the panels and the desktops whose icon is one of kde-rice's
// glyphs (the Show Desktop applet's kde-rice-show-desktop, set by hand),
// which only kde-rice's icon theme has: with each, the applet's own icon to
// wear while kde-rice is off.
QJsonArray riceAppletIcons()
{
    const QString list = evaluateInPlasma(uR"(const found = [];
        for (const c of [...panels(), ...desktops()]) for (const w of c.widgets()) {
            w.currentConfigGroup = ['General'];
            const icon = w.readConfig('icon');
            if (typeof icon === 'string' && icon.startsWith('kde-rice-'))
                found.push({ widget: w.id, type: w.type, icon });
        }
        print(JSON.stringify(found));)"_s);
    // their configuration's default, where it is not the plugin's icon
    static const QHash<QString, QString> defaults = {
        {u"org.kde.plasma.showdesktop"_s, u"user-desktop-symbolic"_s},
        {u"org.kde.plasma.kickoff"_s, u"start-here-kde-symbolic"_s},
        {u"org.kde.plasma.kicker"_s, u"start-here-kde-symbolic"_s},
        {u"org.kde.plasma.kickerdash"_s, u"start-here-kde-symbolic"_s},
    };
    QJsonArray applets = QJsonDocument::fromJson(list.toUtf8()).array();
    for (qsizetype i = 0; i < applets.size(); ++i) {
        QJsonObject applet = applets[i].toObject();
        const QString type = applet[u"type"_s].toString();
        QString stock = defaults.value(type, KPluginMetaData::findPluginById(u"plasma/applets"_s, type).iconName());
        if (stock.isEmpty() || stock.startsWith("kde-rice-"_L1)) {
            stock = u"plasma"_s;
        }
        applet[u"stock"_s] = stock;
        applets[i] = applet;
    }
    return applets;
}

// Each applet's icon from one to the other, where it still is the one
// (changed by hand meanwhile, it is left as it is).
void switchAppletIcons(const QJsonArray &applets, bool toStock)
{
    QJsonArray changes;
    for (const QJsonValue &applet : applets) {
        const QString rice = applet[u"icon"_s].toString();
        const QString stock = applet[u"stock"_s].toString();
        changes.append(QJsonObject{{u"widget"_s, applet[u"widget"_s]}, {u"from"_s, toStock ? rice : stock}, {u"to"_s, toStock ? stock : rice}});
    }
    if (changes.isEmpty()) {
        return;
    }
    evaluateInPlasma(uR"(const changes = %1;
        for (const c of [...panels(), ...desktops()]) for (const w of c.widgets()) for (const s of changes) {
            if (w.id !== s.widget) continue;
            w.currentConfigGroup = ['General'];
            if (w.readConfig('icon') !== s.from) continue;
            w.writeConfig('icon', s.to);
            w.reloadConfig();
        })"_s.arg(QString::fromUtf8(QJsonDocument(changes).toJson(QJsonDocument::Compact))));
}

bool writeState(const QJsonObject &state)
{
    QDir().mkpath(QFileInfo(stateFile()).absolutePath());
    QSaveFile file(stateFile());
    return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(state).toJson()) > 0 && file.commit();
}

QJsonObject readState()
{
    QFile file(stateFile());
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

// The login screen brought in step with the session, which login-screen
// takes off kde-rice's wallpaper while off.json is there. Asks for the
// password.
void updateLoginScreen(QStringList &problems)
{
    if (QProcess::execute(u"pkexec"_s, {s_loginScreen}) != 0) {
        problems += u"the login screen was left as it is (pkexec %1 did not run)"_s.arg(s_loginScreen);
    }
}

}

RiceSwitch::RiceSwitch(QObject *parent)
    : QObject(parent)
{
    connect(&m_worker, &Worker::busyChanged, this, &RiceSwitch::busyChanged);
}

bool RiceSwitch::isOn()
{
    return !QFileInfo::exists(stateFile());
}

void RiceSwitch::finished(const QStringList &problems)
{
    m_problems = problems;
    Q_EMIT changed();
}

void RiceSwitch::turnOff()
{
    m_worker.run(&RiceSwitch::restoreDefaults, [this](const QStringList &problems) {
        finished(problems);
    });
}

void RiceSwitch::turnOn()
{
    m_worker.run(&RiceSwitch::reapply, [this](const QStringList &problems) {
        finished(problems);
    });
}

QStringList RiceSwitch::restoreDefaults()
{
    QStringList problems;
    const bool tell = !filesOnly();

    // What is on now, noted first; off a second time, the first note stays
    // (it is what was on).
    auto kwinrc = open(u"kwinrc"_s);
    auto riceRc = open(u"kde-ricerc"_s);
    auto globals = open(u"kdeglobals"_s);
    QJsonObject state = readState();
    if (state.isEmpty()) {
        // a theme of kde-rice's is worn: its colour scheme, or its files
        // for the other programs written
        state[u"theme"_s] = !ThemeStore::choice().theme.isEmpty()
            && (globals->group(u"General"_s).readEntry("ColorScheme").startsWith("KDERice"_L1)
                || QFileInfo::exists(ThemeApplier::outputDir() + "/mode"_L1));
        state[u"icons"_s] = IconStore::inUse();
        state[u"plasmaStyle"_s] = PanelStyle::styleInUse();
        state[u"decoration"_s] = kwinrc->group(u"org.kde.kdecoration2"_s).readEntry("library") == KWin::riceDecoration();
        QJsonObject plugins;
        const KConfigGroup group = kwinrc->group(u"Plugins"_s);
        for (const QString &id : s_kwinPlugins) {
            const QString key = id + "Enabled"_L1;
            plugins[id] = group.hasKey(key) ? QJsonValue(group.readEntry(key, false)) : QJsonValue();
        }
        state[u"kwinPlugins"_s] = plugins;
        state[u"gaps"_s] = riceRc->group(u"Gaps"_s).readEntry("Enabled", false);
        // the launcher had Meta+Space if it was there (it takes it at start)
        state[u"launcher"_s] = running(u"kde-rice-launcher"_s);
        QJsonArray hidden;
        for (const QString &part : s_autostart) {
            if (!QFileInfo::exists(autostartFile(part))) {
                hidden.append(part);
            }
        }
        state[u"autostartHidden"_s] = hidden;
        state[u"widgets"_s] = widgets();
        state[u"appletIcons"_s] = riceAppletIcons();
        state[u"loginScreen"_s] = QFileInfo::exists(s_loginSetting);
        if (!writeState(state)) {
            return {u"%1 could not be written: nothing changed"_s.arg(stateFile())};
        }
    }

    // kde-rice's programs, stopped and kept from starting at login
    for (const QJsonValue &part : state[u"autostartHidden"_s].toArray()) {
        QSaveFile entry(autostartFile(part.toString()));
        QDir().mkpath(QFileInfo(entry.fileName()).absolutePath());
        if (entry.open(QIODevice::WriteOnly)) {
            entry.write("# kde-rice is off (kde-rice --restore-defaults): kde-rice's own entry hidden\n[Desktop Entry]\nHidden=true\n");
            entry.commit();
        }
    }
    KConfigGroup gaps = riceRc->group(u"Gaps"_s);
    gaps.writeEntry("Enabled", false, KConfig::Notify);
    riceRc->sync();
    if (tell) {
        for (const QString &program : s_programs) {
            stop(program);
        }
        // Meta+Space, free for whatever else wants it (KRunner)
        call(u"org.kde.kglobalaccel"_s, u"/kglobalaccel"_s, u"org.kde.KGlobalAccel"_s, u"unregister"_s,
             {QStringLiteral(KDERICE_ID ".launcher"), u"show"_s});
    }

    // KWin: Breeze's title bars, kde-rice's plugins off
    KConfigGroup decoration = kwinrc->group(u"org.kde.kdecoration2"_s);
    if (decoration.readEntry("library") == KWin::riceDecoration()) {
        decoration.writeEntry("library", u"org.kde.breeze"_s, KConfig::Notify);
    }
    KConfigGroup plugins = kwinrc->group(u"Plugins"_s);
    for (const QString &id : s_kwinPlugins) {
        plugins.writeEntry(id + "Enabled"_L1, false, KConfig::Notify);
    }
    kwinrc->sync();
    if (tell) {
        for (const QString &id : s_kwinPlugins) {
            call(u"org.kde.KWin"_s, u"/Plugins"_s, u"org.kde.KWin.Plugins"_s, u"UnloadPlugin"_s, {id});
        }
        kwinReconfigure();
    }

    // Plasma: the style, the widgets; the colour scheme, the icons, the
    // wallpaper and the other programs' files
    PanelStyle::restoreStyle();
    if (tell) {
        removeWidgets(widgets());
        // no applet left with an icon kde-rice's icon theme alone has
        switchAppletIcons(state[u"appletIcons"_s].toArray(), true);
    }
    ThemeApplier::Options options;
    options.filesOnly = !tell;
    problems += ThemeApplier::restore(options);

    if (tell && state[u"loginScreen"_s].toBool() && QFileInfo(s_loginScreen).isExecutable()) {
        updateLoginScreen(problems);
    }
    if (tell && plasmaRunning()) {
        IconStore::restartPlasma();
    }
    return problems;
}

QStringList RiceSwitch::reapply()
{
    const QJsonObject state = readState();
    if (state.isEmpty()) {
        return {};
    }
    QStringList problems;
    const bool tell = !filesOnly();
    // on from here: what follows notes what there is before kde-rice anew
    // (the colour scheme, the icons, the Plasma style)
    QFile::remove(stateFile());

    // the autostart entries of kde-rice's own, unhidden (the ones this hid)
    for (const QJsonValue &part : state[u"autostartHidden"_s].toArray()) {
        QFile entry(autostartFile(part.toString()));
        if (entry.open(QIODevice::ReadOnly) && entry.readAll().startsWith("# kde-rice is off")) {
            entry.remove();
        }
    }

    // KWin as it was
    auto kwinrc = open(u"kwinrc"_s);
    if (state[u"decoration"_s].toBool() && KWin::riceDecorationInstalled()) {
        kwinrc->group(u"org.kde.kdecoration2"_s).writeEntry("library", KWin::riceDecoration(), KConfig::Notify);
    }
    KConfigGroup plugins = kwinrc->group(u"Plugins"_s);
    const QJsonObject before = state[u"kwinPlugins"_s].toObject();
    QStringList load;
    for (const QString &id : s_kwinPlugins) {
        const QJsonValue value = before.value(id);
        if (value.isBool()) {
            plugins.writeEntry(id + "Enabled"_L1, value.toBool(), KConfig::Notify);
        } else {
            plugins.deleteEntry(id + "Enabled"_L1, KConfig::Notify);
        }
        const KPluginMetaData data = KPluginMetaData::findPluginById(u"kwin/plugins"_s, id);
        if (data.isValid() && (value.isBool() ? value.toBool() : data.isEnabledByDefault())) {
            load += id;
        }
    }
    kwinrc->sync();
    if (tell) {
        for (const QString &id : std::as_const(load)) {
            call(u"org.kde.KWin"_s, u"/Plugins"_s, u"org.kde.KWin.Plugins"_s, u"LoadPlugin"_s, {id});
        }
        kwinReconfigure();
    }

    // the theme: kde-rice's icons first, so that the theme writes them for
    // its mode
    const ThemeStore::Choice choice = ThemeStore::choice();
    const auto theme = ThemeStore::load(choice.theme);
    if (state[u"icons"_s].toBool()) {
        IconStore::write(theme && !theme->isDark() ? u"light"_s : u"dark"_s);
        IconStore::use();
    }
    if (state[u"theme"_s].toBool() && theme) {
        const QString background = choice.backgrounds.value(choice.theme) == "-"_L1 ? QString() : ThemeStore::backgroundFor(choice.theme, choice);
        ThemeApplier::Options options;
        options.filesOnly = !tell;
        problems += ThemeApplier::apply(*theme, background, options);
    } else if (state[u"theme"_s].toBool()) {
        problems += u"the theme chosen (%1) could not be read: it was not applied again"_s.arg(choice.theme);
    }
    if (state[u"plasmaStyle"_s].toBool()) {
        PanelStyle::applyOpacity(PanelStyle::savedOpacity());
    }
    if (tell) {
        addWidgets(state[u"widgets"_s].toArray());
        switchAppletIcons(state[u"appletIcons"_s].toArray(), false);
    }

    // kde-rice's programs
    auto riceRc = open(u"kde-ricerc"_s);
    riceRc->group(u"Gaps"_s).writeEntry("Enabled", state[u"gaps"_s].toBool(), KConfig::Notify);
    riceRc->sync();
    if (tell) {
        Session::start(u"kde-rice-launcher"_s, state[u"launcher"_s].toBool() ? QStringList{u"--background"_s, u"--take-shortcut"_s} : QStringList{u"--background"_s});
        Session::start(u"kde-rice-tray"_s);
        if (state[u"gaps"_s].toBool()) {
            Session::start(u"kde-rice-gaps"_s);
        }
    }

    if (tell && state[u"loginScreen"_s].toBool() && QFileInfo(s_loginScreen).isExecutable()) {
        updateLoginScreen(problems);
    }
    if (tell && plasmaRunning()) {
        IconStore::restartPlasma();
    }
    return problems;
}
