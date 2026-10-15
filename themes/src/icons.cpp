#include "icons.h"
#include "theme.h"
#include "trim.h"

#include <KConfigGroup>
#include <KIconLoader>
#include <KSharedConfig>
#include <KSharedDataCache>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace IconStore
{

// A picture replaced under an icon name Plasma has already drawn: Plasma
// keeps the icons it has drawn by their name, and neither a new path nor a
// change of icon theme makes it draw them again, only its restart.
static bool s_replaced = false;

static KConfigGroup settings()
{
    auto config = KSharedConfig::openConfig(u"kde-ricerc"_s);
    config->reparseConfiguration();
    return config->group(u"Icons"_s);
}

QString themeName()
{
    return u"kde-rice"_s;
}

QString themeDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/icons/"_L1 + themeName();
}

QString overridesDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/kde-rice/icons"_L1;
}

// where icon themes are, the user's first
static QStringList iconDirs()
{
    QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, u"icons"_s, QStandardPaths::LocateDirectory);
    dirs.prepend(QDir::homePath() + "/.icons"_L1);
    return dirs;
}

// every directory a theme is in (hicolor is in many: the user's and the system's)
static QStringList themePaths(const QString &theme)
{
    QStringList paths;
    for (const QString &dir : iconDirs()) {
        if (QFileInfo(dir + u'/' + theme).isDir()) {
            paths += dir + u'/' + theme;
        }
    }
    return paths;
}

static QString themePath(const QString &theme)
{
    for (const QString &dir : iconDirs()) {
        const QString path = dir + u'/' + theme;
        if (QFileInfo::exists(path + "/index.theme"_L1)) {
            return path;
        }
    }
    return {};
}

static bool exists(const QString &theme)
{
    return !themePath(theme).isEmpty();
}

QString base()
{
    const QString chosen = settings().readEntry("Base");
    if (!chosen.isEmpty() && exists(chosen)) {
        return chosen;
    }
    // kde-rice's own WhiteSur first
    for (const auto *fallback : {"kde-rice-whitesur", "WhiteSur", "Papirus", "breeze"}) {
        if (exists(QString::fromLatin1(fallback))) {
            return QString::fromLatin1(fallback);
        }
    }
    return u"hicolor"_s;
}

void setBase(const QString &theme)
{
    KConfigGroup group = settings();
    group.writeEntry("Base", theme, KConfig::Notify);
    group.sync();
}

QString baseFor(const QString &mode)
{
    QString name = base();
    // a pair's other half: WhiteSur/WhiteSur-dark, Papirus-Light/Papirus-Dark, breeze/breeze-dark
    static const QRegularExpression suffix(u"-(dark|light)$"_s, QRegularExpression::CaseInsensitiveOption);
    const QString plain = QString(name).remove(suffix);
    const bool dark = mode == "dark"_L1;
    const QStringList wanted = dark ? QStringList{plain + "-dark"_L1, plain + "-Dark"_L1} : QStringList{plain + "-light"_L1, plain + "-Light"_L1, plain};
    for (const QString &candidate : wanted) {
        if (exists(candidate)) {
            return candidate;
        }
    }
    return name;
}

static bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == contents) {
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size() && file.commit();
}

// The status icons (the tray's, a panel's) of the base, in the kde-rice
// theme's text colour: WhiteSur draws them in a grey of its own (#363636, or
// #f2f2f2 on dark), while Plasma draws the text beside them (the clock) in the
// colour scheme's, and does not colour them itself. Copies with the colour
// replaced, in the theme's own directories ("tinted/status/22" ...), which
// come before the base's; made again only when the base or the colour
// changes. Not for the names the user has a picture of their own for.
static const QString s_stamp = u".kde-rice-tint"_s;

// the text colour of the kde-rice theme chosen now
static QString textColour()
{
    const auto theme = ThemeStore::load(ThemeStore::choice().theme);
    return theme ? theme->color(u"fg"_s).name() : QString();
}

// the colour of the launcher's glyph (the user's own start-here picture):
// KDE's blue on a light panel, where the text colour is a dark one and the
// K wants its own colour; the text colour on a dark one, where that is pale
static QString launcherColour(const QString &mode, const QString &text)
{
    return mode == "dark"_L1 ? text : u"#1d99f3"_s;
}

// a base theme's directories of status icons, with their index entries
static QList<std::pair<QString, QHash<QString, QString>>> statusDirs(const QString &theme)
{
    QList<std::pair<QString, QHash<QString, QString>>> found;
    const QString root = themePath(theme);
    if (root.isEmpty()) {
        return found;
    }
    KConfig index(root + "/index.theme"_L1, KConfig::SimpleConfig);
    const QStringList dirs = index.group(u"Icon Theme"_s).readEntry("Directories", QStringList());
    for (const QString &dir : dirs) {
        const KConfigGroup group = index.group(dir);
        if (group.readEntry("Context") != "Status"_L1) {
            continue;
        }
        QHash<QString, QString> entries;
        for (const auto *key : {"Size", "Context", "Type", "MinSize", "MaxSize", "Threshold", "Scale"}) {
            const QString value = group.readEntry(key);
            if (!value.isEmpty()) {
                entries.insert(QString::fromLatin1(key), value);
            }
        }
        found += {dir, entries};
    }
    return found;
}

static bool tint(const QString &theme, const QString &colour, const QString &launcher, QList<std::pair<QString, QHash<QString, QString>>> &dirs)
{
    const QString tinted = themeDir() + "/tinted"_L1;
    dirs.clear();
    if (!colour.isEmpty()) {
        dirs = statusDirs(theme);
    }
    // the user's own one-colour pictures are part of it (they are coloured
    // too); their other pictures are not
    // the version of how the copies are made: a newer kde-rice makes them again
    QString stamp = u"3|"_s + theme + u'|' + colour + u'|' + launcher;
    const auto mineNow = QDir(overridesDir()).entryInfoList({u"*.svg"_s}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : mineNow) {
        QFile in(file.absoluteFilePath());
        if (in.open(QIODevice::ReadOnly) && in.read(8192).contains("ColorScheme-Text")) {
            stamp += u'|' + file.fileName() + u':' + QString::number(file.size()) + u':' + QString::number(file.lastModified().toSecsSinceEpoch());
        }
    }
    QFile current(tinted + u'/' + s_stamp);
    if (current.open(QIODevice::ReadOnly) && QString::fromUtf8(current.readAll()) == stamp) {
        return false;
    }
    current.close();
    QDir(tinted).removeRecursively();
    if (dirs.isEmpty()) {
        return true;
    }
    // the grey the base draws its glyphs in: what most of its ColorScheme-Text says
    static const QRegularExpression scheme(u"\\.ColorScheme-Text\\s*\\{\\s*color:\\s*(#[0-9a-fA-F]{6})"_s);
    QHash<QString, int> greys;
    const QString root = themePath(theme);
    for (const auto &[dir, entries] : std::as_const(dirs)) {
        const auto files = QDir(root + u'/' + dir).entryInfoList({u"*.svg"_s}, QDir::Files);
        for (const QFileInfo &file : files) {
            QFile in(file.absoluteFilePath());
            if (in.open(QIODevice::ReadOnly)) {
                const auto match = scheme.match(QString::fromUtf8(in.read(4096)));
                if (match.hasMatch()) {
                    ++greys[match.captured(1).toLower()];
                }
            }
        }
    }
    QString grey;
    for (auto it = greys.cbegin(); it != greys.cend(); ++it) {
        if (grey.isEmpty() || it.value() > greys.value(grey)) {
            grey = it.key();
        }
    }
    if (grey.isEmpty()) {
        dirs.clear();
        return true;
    }
    const QStringList own = overrides();
    for (const auto &[dir, entries] : std::as_const(dirs)) {
        QDir().mkpath(tinted + u'/' + dir);
        const auto files = QDir(root + u'/' + dir).entryInfoList({u"*.svg"_s}, QDir::Files);
        for (const QFileInfo &file : files) {
            if (own.contains(file.completeBaseName())) {
                continue;
            }
            QFile in(file.absoluteFilePath());
            if (!in.open(QIODevice::ReadOnly)) {
                continue;
            }
            QString svg = QString::fromUtf8(in.readAll());
            if (!svg.contains(grey, Qt::CaseInsensitive)) {
                continue;
            }
            svg.replace(grey, colour, Qt::CaseInsensitive);
            QFile out(tinted + u'/' + dir + u'/' + file.fileName());
            if (out.open(QIODevice::WriteOnly)) {
                out.write(svg.toUtf8());
            }
        }
    }
    // the user's own one-colour pictures (a tray glyph made in WhiteSur's
    // style, a launcher glyph): theirs, coloured too, before everything else;
    // the launcher's (start-here) in its own colour
    QDir().mkpath(tinted + "/own"_L1);
    const auto mine = QDir(overridesDir()).entryInfoList({u"*.svg"_s}, QDir::Files);
    for (const QFileInfo &file : mine) {
        QFile in(file.absoluteFilePath());
        if (!in.open(QIODevice::ReadOnly)) {
            continue;
        }
        QString svg = QString::fromUtf8(in.readAll());
        // by the text colour it says it is drawn in, whatever grey that is
        const auto declared = scheme.match(svg);
        if (!declared.hasMatch()) {
            continue;
        }
        const bool isLauncher = file.fileName().startsWith("start-here"_L1);
        svg.replace(declared.captured(1), isLauncher ? launcher : colour, Qt::CaseInsensitive);
        QFile out(tinted + "/own/"_L1 + file.fileName());
        if (out.open(QIODevice::WriteOnly)) {
            out.write(svg.toUtf8());
        }
    }
    writeFile(tinted + u'/' + s_stamp, stamp.toUtf8());
    return true;
}

bool write(const QString &mode)
{
    QDir().mkpath(overridesDir());
    QDir().mkpath(themeDir());
    // the user's pictures are the theme's first directory, as they are
    const QString link = themeDir() + "/overrides"_L1;
    bool changed = false;
    if (QFileInfo(link).symLinkTarget() != overridesDir()) {
        QFile::remove(link);
        QFile::link(overridesDir(), link);
        changed = true;
    }
    QStringList inherits = {baseFor(mode)};
    for (const auto *after : {"Papirus", "breeze", "hicolor"}) {
        const QString name = QString::fromLatin1(after);
        if (!inherits.contains(name) && exists(name)) {
            inherits += name;
        }
    }
    QList<std::pair<QString, QHash<QString, QString>>> tintedDirs;
    const QString text = textColour();
    if (tint(inherits.first(), text, launcherColour(mode, text), tintedDirs)) {
        changed = true;
        // Plasma lets go of the icons it has drawn only when it restarts
        s_replaced = true;
    }
    QStringList directories;
    QString sections;
    if (!tintedDirs.isEmpty()) {
        directories += u"tinted/own"_s;
        sections += u"\n[tinted/own]\nSize=64\nMinSize=8\nMaxSize=512\nType=Scalable\nContext=Status\n"_s;
    }
    directories += u"overrides"_s;
    sections += u"\n[overrides]\nSize=64\nMinSize=8\nMaxSize=512\nType=Scalable\nContext=Applications\n"_s;
    for (const auto &[dir, entries] : std::as_const(tintedDirs)) {
        directories += "tinted/"_L1 + dir;
        sections += u"\n[tinted/%1]\n"_s.arg(dir);
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            sections += it.key() + u'=' + it.value() + u'\n';
        }
    }
    const QString index = u"[Icon Theme]\n"
                          "Name=kde-rice\n"
                          "Comment=Your own icons, then %1's (written by kde-rice; change it there)\n"
                          "Inherits=%2\n"
                          "Directories=%3\n"_s.arg(inherits.first(), inherits.join(u','), directories.join(u','))
        + sections;
    changed |= writeFile(themeDir() + "/index.theme"_L1, index.toUtf8());
    return changed;
}

bool inUse()
{
    auto globals = KSharedConfig::openConfig(u"kdeglobals"_s, KConfig::NoGlobals);
    globals->reparseConfiguration();
    return globals->group(u"Icons"_s).readEntry("Theme") == themeName();
}

void use()
{
    QProcess::execute(u"/usr/lib/plasma-changeicons"_s, {themeName()});
    reload();
}

// Plasma (the desktop and the panels; the windows stay) restarted in two
// seconds, by a timer of systemd's under one name: changes close together
// (several icons set one after the other) are one restart, where a restart
// each would run into systemd's limit and leave Plasma stopped.
static void restartPlasma()
{
    // (a restart already waiting: systemd says so, and that one is it)
    QProcess process;
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(u"systemd-run"_s,
                  {u"--user"_s, u"--quiet"_s, u"--on-active=2"_s, u"--unit=kde-rice-plasma-restart"_s, u"--timer-property=AccuracySec=100ms"_s,
                   u"systemctl"_s, u"--user"_s, u"restart"_s, u"plasma-plasmashell.service"_s});
    process.waitForFinished(5000);
}

void reload()
{
    // what plasma-changeicons does: the shared cache of drawn icons goes,
    // and every running KDE application is told to draw its icons again
    KSharedDataCache::deleteCache(u"icon-cache"_s);
    for (int group = KIconLoader::FirstGroup; group < KIconLoader::LastGroup; ++group) {
        KIconLoader::emitChange(KIconLoader::Group(group));
    }
    // Plasma and Qt's own icons (QIcon::fromTheme, through KDE's platform
    // theme) listen for KDE's settings: "icons changed" (4), for every group
    auto signal = QDBusMessage::createSignal(u"/KGlobalSettings"_s, u"org.kde.KGlobalSettings"_s, u"notifyChange"_s);
    signal << 4 << 0;
    QDBusConnection::sessionBus().send(signal);
    // Sent when the event loop runs, which a program that ends now never
    // gets to (kde-rice-theme): a call that waits for its answer goes after
    // the signals, in order, so they are out once it returns.
    QDBusConnection::sessionBus().call(
        QDBusMessage::createMethodCall(u"org.freedesktop.DBus"_s, u"/org/freedesktop/DBus"_s, u"org.freedesktop.DBus"_s, u"GetId"_s));
    if (s_replaced) {
        s_replaced = false;
        restartPlasma();
    }
}

QList<Theme> installed()
{
    QList<Theme> found;
    QStringList seen;
    for (const QString &dir : iconDirs()) {
        const auto entries = QDir(dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &entry : entries) {
            const QString id = entry.fileName();
            if (seen.contains(id) || id == themeName() || id == "hicolor"_L1 || id == "locolor"_L1 || id == "default"_L1) {
                continue;
            }
            KConfig index(entry.absoluteFilePath() + "/index.theme"_L1, KConfig::SimpleConfig);
            const KConfigGroup group = index.group(u"Icon Theme"_s);
            // cursor themes have an index.theme too, and no directories of icons
            if (!QFileInfo::exists(index.name()) || group.readEntry("Directories").isEmpty() || group.readEntry("Hidden", false)) {
                continue;
            }
            seen += id;
            found += Theme{id, group.readEntry("Name", id)};
        }
    }
    return found;
}

// every icon of a theme: name -> its files, read once
struct Index {
    QHash<QString, QStringList> files;
};
static QHash<QString, Index> s_indexes;
static QMutex s_lock;

static int sizeOf(const QString &path)
{
    // "scalable" first, then the largest size in the path (64x64, 48, @2x)
    if (path.contains("/scalable/"_L1) || path.contains("scalable"_L1)) {
        return 100000;
    }
    static const QRegularExpression number(u"/(\\d+)(?:x\\d+)?(?:@(\\d)x)?/"_s);
    int best = 0;
    for (auto it = number.globalMatch(path); it.hasNext();) {
        const auto match = it.next();
        best = std::max(best, match.captured(1).toInt() * std::max(1, match.captured(2).toInt()));
    }
    return best;
}

static const Index &indexOf(const QString &theme)
{
    QMutexLocker locker(&s_lock);
    auto it = s_indexes.find(theme);
    if (it != s_indexes.end()) {
        return *it;
    }
    Index index;
    QStringList roots = theme == themeName() ? QStringList{overridesDir()} : themePaths(theme);
    // what applications put outside any theme, found after hicolor as KDE does
    if (theme == "hicolor"_L1) {
        roots += QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, u"pixmaps"_s, QStandardPaths::LocateDirectory);
    }
    for (const QString &root : std::as_const(roots)) {
        QDirIterator files(root, {u"*.svg"_s, u"*.svgz"_s, u"*.png"_s}, QDir::Files, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (files.hasNext()) {
            const QString path = files.next();
            // symbolic variants are another icon of their own
            index.files[QFileInfo(path).completeBaseName()] += path;
        }
    }
    {
        for (QStringList &paths : index.files) {
            std::sort(paths.begin(), paths.end(), [](const QString &a, const QString &b) {
                const bool svgA = !a.endsWith(".png"_L1), svgB = !b.endsWith(".png"_L1);
                if (svgA != svgB) {
                    return svgA;
                }
                return sizeOf(a) > sizeOf(b);
            });
        }
    }
    return *s_indexes.insert(theme, index);
}

QString file(const QString &theme, const QString &name)
{
    return indexOf(theme).files.value(name).value(0);
}

QStringList files(const QString &theme, const QString &name)
{
    // one per kind of directory (apps, panel, actions, status ...): the
    // first of each is its largest
    QStringList kept;
    QStringList kinds;
    const QStringList all = indexOf(theme).files.value(name);
    for (const QString &path : all) {
        const QString kind = QFileInfo(QFileInfo(path).absolutePath()).fileName();
        static const QRegularExpression sized(u"^\\d+(x\\d+)?(@\\dx)?$"_s);
        // a size directory (16, 22x22): what kind it is is the one above
        const QString key = sized.match(kind).hasMatch() ? QFileInfo(QFileInfo(QFileInfo(path).absolutePath()).absolutePath()).fileName() : kind;
        if (!kinds.contains(key)) {
            kinds += key;
            kept += path;
        }
    }
    return kept;
}

QStringList names(const QString &theme)
{
    QStringList all = indexOf(theme).files.keys();
    all.sort();
    return all;
}

QString overrideFor(const QString &name)
{
    for (const auto *suffix : {".svg", ".svgz", ".png"}) {
        const QString path = overridesDir() + u'/' + name + QString::fromLatin1(suffix);
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return {};
}

static void removeFiles(const QString &name);

// the mode the theme was last written for, from its base's variant
static QString writtenMode()
{
    KConfig index(themeDir() + "/index.theme"_L1, KConfig::SimpleConfig);
    const QString first = index.group(u"Icon Theme"_s).readEntry("Inherits").section(u',', 0, 0);
    return first.endsWith("-dark"_L1, Qt::CaseInsensitive) ? u"dark"_s : u"light"_s;
}

// the theme written again after a change of the user's pictures, for the
// coloured copies of their one-colour ones
static void rewrite()
{
    if (QFileInfo::exists(themeDir() + "/index.theme"_L1)) {
        write(writtenMode());
    }
}

void removeOverride(const QString &name)
{
    removeFiles(name);
    rewrite();
}

static void removeFiles(const QString &name)
{
    for (const auto *suffix : {".svg", ".svgz", ".png"}) {
        QFile::remove(overridesDir() + u'/' + name + QString::fromLatin1(suffix));
    }
    QMutexLocker locker(&s_lock);
    s_indexes.remove(themeName());
}

bool trimOverride(const QString &name)
{
    const QString path = overrideFor(name);
    if ((path.endsWith(".svg"_L1) || path.endsWith(".png"_L1)) && IconTrim::trimFile(path)) {
        s_replaced = true;
        return true;
    }
    return false;
}

bool setOverride(const QString &name, const QString &picture, bool trim)
{
    static const QRegularExpression safe(u"^[A-Za-z0-9._+-]+$"_s);
    const QFileInfo source(picture);
    const QString suffix = source.suffix().toLower();
    if (!safe.match(name).hasMatch() || !source.isFile() || (suffix != "svg"_L1 && suffix != "svgz"_L1 && suffix != "png"_L1)) {
        return false;
    }
    QDir().mkpath(overridesDir());
    const QString target = overridesDir() + u'/' + name + u'.' + suffix;
    const QString part = target + ".part"_L1;
    QFile::remove(part);
    // a copy: the picture stays when its theme is updated or removed
    if (!QFile::copy(source.canonicalFilePath(), part)) {
        return false;
    }
    QFile::setPermissions(part, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    s_replaced |= !overrideFor(name).isEmpty();
    removeFiles(name);
    if (!QFile::rename(part, target)) {
        return false;
    }
    if (trim && (suffix == "svg"_L1 || suffix == "png"_L1)) {
        IconTrim::trimFile(target);
    }
    rewrite();
    return true;
}

QStringList overrides()
{
    QStringList all;
    const auto files = QDir(overridesDir()).entryInfoList({u"*.svg"_s, u"*.svgz"_s, u"*.png"_s}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : files) {
        all += file.completeBaseName();
    }
    return all;
}

}
