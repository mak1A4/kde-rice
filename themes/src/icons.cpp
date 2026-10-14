#include "icons.h"
#include "trim.h"

#include <KConfigGroup>
#include <KIconLoader>
#include <KSharedConfig>
#include <KSharedDataCache>

#include <QDBusConnection>
#include <QDBusMessage>
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

bool write(const QString &mode)
{
    QDir().mkpath(overridesDir());
    QDir().mkpath(themeDir());
    // the user's pictures are the theme's one directory, as they are
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
    const QString index = u"[Icon Theme]\n"
                          "Name=kde-rice\n"
                          "Comment=Your own icons, then %1's (written by kde-rice; change it there)\n"
                          "Inherits=%2\n"
                          "Directories=overrides\n"
                          "\n"
                          "[overrides]\n"
                          "Size=64\n"
                          "MinSize=8\n"
                          "MaxSize=512\n"
                          "Type=Scalable\n"
                          "Context=Applications\n"_s.arg(inherits.first(), inherits.join(u','));
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

void removeOverride(const QString &name)
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
    return (path.endsWith(".svg"_L1) || path.endsWith(".png"_L1)) && IconTrim::trimFile(path);
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
    removeOverride(name);
    if (!QFile::rename(part, target)) {
        return false;
    }
    if (trim && (suffix == "svg"_L1 || suffix == "png"_L1)) {
        IconTrim::trimFile(target);
    }
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
