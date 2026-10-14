#include "iconmodel.h"
#include "icons.h"
#include "theme.h"

#include <KConfigGroup>
#include <KDesktopFile>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrentRun>

using namespace Qt::StringLiterals;

IconModel::IconModel(QObject *parent)
    : QObject(parent)
{
}

QString IconModel::mode() const
{
    const auto theme = ThemeStore::load(ThemeStore::choice().theme);
    return theme && !theme->isDark() ? u"light"_s : u"dark"_s;
}

void IconModel::bump()
{
    ++m_revision;
    Q_EMIT changed();
}

bool IconModel::inUse() const
{
    return IconStore::inUse();
}

QString IconModel::base() const
{
    return IconStore::base();
}

QString IconModel::baseVariant() const
{
    return IconStore::baseFor(mode());
}

QVariantList IconModel::themes() const
{
    QVariantList list;
    const auto installed = IconStore::installed();
    for (const auto &theme : installed) {
        list += QVariantMap{{u"id"_s, theme.id}, {u"name"_s, theme.name}};
    }
    return list;
}

bool IconModel::whiteSurInstalled() const
{
    const auto installed = IconStore::installed();
    return std::any_of(installed.cbegin(), installed.cend(), [](const auto &theme) {
        return theme.id.startsWith("kde-rice-whitesur"_L1) || theme.id.startsWith("WhiteSur"_L1);
    });
}

void IconModel::use()
{
    IconStore::write(mode());
    IconStore::use();
    bump();
}

void IconModel::setBase(const QString &theme)
{
    IconStore::setBase(theme);
    if (IconStore::write(mode()) && IconStore::inUse()) {
        IconStore::reload();
    }
    bump();
}

QUrl IconModel::current(const QString &name) const
{
    if (name.startsWith(u'/')) {
        return QUrl::fromLocalFile(name);
    }
    QString path = IconStore::overrideFor(name);
    for (const QString &theme : {baseVariant(), base(), u"Papirus"_s, u"breeze"_s, u"hicolor"_s}) {
        if (!path.isEmpty()) {
            break;
        }
        path = IconStore::file(theme, name);
    }
    if (path.isEmpty()) {
        return {};
    }
    QUrl url = QUrl::fromLocalFile(path);
    // a new revision is a new address: the picture is loaded again
    url.setQuery(u"r=%1"_s.arg(m_revision));
    return url;
}

bool IconModel::overridden(const QString &name) const
{
    return !IconStore::overrideFor(name).isEmpty();
}

QVariantList IconModel::candidates(const QString &name) const
{
    QVariantList list;
    const auto installed = IconStore::installed();
    for (const auto &theme : installed) {
        const QStringList paths = IconStore::files(theme.id, name);
        for (const QString &path : paths) {
            list += QVariantMap{{u"theme"_s, theme.id}, {u"themeName"_s, theme.name}, {u"url"_s, QUrl::fromLocalFile(path)}};
        }
    }
    return list;
}

void IconModel::search(const QString &text)
{
    const QString needle = text.trimmed().toLower();
    if (needle.size() < 2) {
        Q_EMIT searchFinished(text, {});
        return;
    }
    m_searching = true;
    Q_EMIT searchingChanged();
    // the base's, then every other installed theme's names
    QStringList themes = {baseVariant(), base()};
    const auto installed = IconStore::installed();
    for (const auto &theme : installed) {
        themes += theme.id;
    }
    themes.removeDuplicates();
    auto watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher, text] {
        watcher->deleteLater();
        m_searching = false;
        Q_EMIT searchingChanged();
        Q_EMIT searchFinished(text, watcher->result());
    });
    watcher->setFuture(QtConcurrent::run([themes, needle] {
        QStringList found;
        for (const QString &theme : themes) {
            const QStringList names = IconStore::names(theme);
            for (const QString &name : names) {
                if (name.contains(needle, Qt::CaseInsensitive)) {
                    found += name;
                }
            }
        }
        found.removeDuplicates();
        // names that begin with it first, then the shorter
        std::sort(found.begin(), found.end(), [&needle](const QString &a, const QString &b) {
            const bool startA = a.startsWith(needle), startB = b.startsWith(needle);
            if (startA != startB) {
                return startA;
            }
            return a.size() != b.size() ? a.size() < b.size() : a < b;
        });
        return found.mid(0, 240);
    }));
}

bool IconModel::setIcon(const QString &name, const QUrl &picture)
{
    // an app icon (from a theme's apps, or a taskbar app's) is trimmed to
    // fill its canvas, as kde-rice-whitesur's are
    bool app = picture.toLocalFile().contains("/apps/"_L1);
    const QVariantList apps = taskbar();
    for (const QVariant &entry : apps) {
        app |= entry.toMap().value(u"icon"_s).toString() == name;
    }
    if (!IconStore::setOverride(name, picture.toLocalFile(), app)) {
        return false;
    }
    IconStore::reload();
    bump();
    return true;
}

void IconModel::resetIcon(const QString &name)
{
    IconStore::removeOverride(name);
    IconStore::reload();
    bump();
}

QStringList IconModel::overrides() const
{
    return IconStore::overrides();
}

// The launchers of the panel's task manager, in order, as Plasma keeps them.
static QStringList launchers()
{
    QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/plasma-org.kde.plasma.desktop-appletsrc"_L1);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    static const QRegularExpression line(u"^launchers=(.*)$"_s, QRegularExpression::MultilineOption);
    const auto match = line.match(QString::fromUtf8(file.readAll()));
    return match.hasMatch() ? match.captured(1).split(u',', Qt::SkipEmptyParts) : QStringList();
}

QVariantList IconModel::taskbar() const
{
    QVariantList list;
    const QStringList all = launchers();
    for (QString launcher : all) {
        if (launcher == "preferred://filemanager"_L1) {
            launcher = u"applications:org.kde.dolphin.desktop"_s;
        }
        if (!launcher.startsWith("applications:"_L1)) {
            continue;
        }
        const QString id = launcher.mid(13);
        const QString path = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, id);
        if (path.isEmpty()) {
            continue;
        }
        KDesktopFile desktop(path);
        const QString icon = desktop.readIcon();
        list += QVariantMap{
            {u"id"_s, id},
            {u"name"_s, desktop.readName()},
            {u"icon"_s, icon},
            // a picture file, not an icon of a theme's
            {u"fixed"_s, icon.startsWith(u'/')},
        };
    }
    return list;
}

QString IconModel::themable(const QString &desktopId)
{
    const QString path = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktopId);
    if (path.isEmpty()) {
        return {};
    }
    KDesktopFile desktop(path);
    const QString picture = desktop.readIcon();
    // "com.t3tools.T3Code.desktop" -> "com.t3tools.T3Code"
    const QString name = QString(desktopId).remove(QRegularExpression(u"\\.desktop$"_s));
    if (!picture.startsWith(u'/')) {
        return picture;
    }
    // the user's own entry: a copy, naming the icon
    const QString own = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + u'/' + desktopId;
    if (QFileInfo(path) != QFileInfo(own)) {
        QFile::remove(own);
        QFile::copy(path, own);
        QFile::setPermissions(own, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    }
    KDesktopFile copy(own);
    copy.desktopGroup().writeEntry("Icon", name);
    copy.sync();
    // Plasma reads the applications' entries again
    QProcess::startDetached(u"kbuildsycoca6"_s, {});
    // the picture it had, as its icon until another is chosen
    if (IconStore::overrideFor(name).isEmpty() && QFileInfo(picture).isFile()) {
        IconStore::setOverride(name, picture);
    }
    IconStore::reload();
    bump();
    return name;
}
