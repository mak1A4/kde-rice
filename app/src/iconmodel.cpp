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

// the mode of the kde-rice theme chosen now
static QString modeNow()
{
    const auto theme = ThemeStore::load(ThemeStore::choice().theme);
    return theme && !theme->isDark() ? u"light"_s : u"dark"_s;
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

// a file as a URL that changes with the file: the picture is loaded again
// when it was replaced, and kept otherwise
static QUrl pictureUrl(const QString &path)
{
    if (path.isEmpty()) {
        return {};
    }
    QUrl url = QUrl::fromLocalFile(path);
    const QFileInfo info(path);
    url.setQuery(u"m=%1-%2"_s.arg(info.lastModified().toMSecsSinceEpoch()).arg(info.size()));
    return url;
}

IconModel::IconModel(QObject *parent)
    : QObject(parent)
{
    connect(&m_worker, &Worker::busyChanged, this, &IconModel::busyChanged);
    // the names at once; their pictures as soon as the themes are read
    m_state = read(false);
    change([] {});
}

QString IconModel::pictureFor(const QString &name, const State &state)
{
    if (name.startsWith(u'/')) {
        return name;
    }
    // where the icon theme finds it: the user's picture, kde-rice's trimmed
    // copy, then the base's and the rest
    QString path = IconStore::overrideFor(name);
    if (path.isEmpty() && state.inUse) {
        path = IconStore::trimmedFor(name);
    }
    for (const QString &theme : {state.baseVariant, state.base, u"Papirus"_s, u"breeze"_s, u"hicolor"_s}) {
        if (!path.isEmpty()) {
            break;
        }
        path = IconStore::file(theme, name);
    }
    return path;
}

IconModel::State IconModel::read(bool withPictures)
{
    State state;
    state.inUse = IconStore::inUse();
    state.base = IconStore::base();
    state.baseVariant = IconStore::baseFor(modeNow());
    state.trimFallbacks = IconStore::trimsFallbacks();

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
        // a picture file, not an icon of a theme's
        const bool fixed = icon.startsWith(u'/');
        const QString picture = withPictures ? pictureFor(icon, state) : QString();
        const QString own = fixed ? QString() : IconStore::overrideFor(icon);
        state.taskbar += QVariantMap{
            {u"id"_s, id},
            {u"name"_s, desktop.readName()},
            {u"icon"_s, icon},
            {u"fixed"_s, fixed},
            {u"picture"_s, pictureUrl(picture)},
            {u"overridden"_s, !own.isEmpty()},
            {u"trimmed"_s, !fixed && own.isEmpty() && state.inUse && !IconStore::trimmedFor(icon).isEmpty()},
        };
    }
    const QStringList names = IconStore::overrides();
    for (const QString &name : names) {
        state.overrides += QVariantMap{
            {u"name"_s, name},
            {u"picture"_s, pictureUrl(withPictures ? IconStore::overrideFor(name) : QString())},
        };
    }
    return state;
}

template<typename Work>
void IconModel::change(Work work)
{
    m_worker.run(
        [work = std::move(work)] {
            work();
            return read(true);
        },
        [this](const State &state) {
            take(state);
        });
}

void IconModel::take(const State &state)
{
    m_state = state;
    ++m_revision;
    Q_EMIT changed();
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
    change([] {
        IconStore::write(modeNow());
        IconStore::use();
    });
}

void IconModel::setBase(const QString &theme)
{
    if (theme.isEmpty() || theme == m_state.base) {
        return;
    }
    IconStore::setBase(theme);
    m_state.base = theme;
    Q_EMIT changed();
    refresh();
}

void IconModel::setTrimFallbacks(bool trim)
{
    if (trim == m_state.trimFallbacks) {
        return;
    }
    IconStore::setTrimsFallbacks(trim);
    m_state.trimFallbacks = trim;
    Q_EMIT changed();
    refresh();
}

void IconModel::refresh()
{
    change([] {
        if (IconStore::write(modeNow()) && IconStore::inUse()) {
            IconStore::reload();
        }
    });
}

QUrl IconModel::current(const QString &name) const
{
    return pictureUrl(pictureFor(name, m_state));
}

bool IconModel::overridden(const QString &name) const
{
    return !IconStore::overrideFor(name).isEmpty();
}

void IconModel::findCandidates(const QString &name)
{
    auto watcher = new QFutureWatcher<QVariantList>(this);
    connect(watcher, &QFutureWatcher<QVariantList>::finished, this, [this, watcher, name] {
        watcher->deleteLater();
        Q_EMIT candidatesFound(name, watcher->result());
    });
    watcher->setFuture(QtConcurrent::run([name] {
        QVariantList list;
        const auto installed = IconStore::installed();
        for (const auto &theme : installed) {
            const QStringList paths = IconStore::files(theme.id, name);
            for (const QString &path : paths) {
                list += QVariantMap{{u"theme"_s, theme.id}, {u"themeName"_s, theme.name}, {u"url"_s, QUrl::fromLocalFile(path)}};
            }
        }
        return list;
    }));
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
    QStringList themes = {m_state.baseVariant, m_state.base};
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
    const QString path = picture.toLocalFile();
    if (!IconStore::canOverride(name, path)) {
        return false;
    }
    // an app icon (from a theme's apps, or a taskbar app's) is trimmed to
    // fill its canvas, as kde-rice-whitesur's are
    bool app = path.contains("/apps/"_L1);
    for (const QVariant &entry : std::as_const(m_state.taskbar)) {
        app |= entry.toMap().value(u"icon"_s).toString() == name;
    }
    change([name, path, app] {
        if (IconStore::setOverride(name, path, app)) {
            IconStore::reload();
        }
    });
    return true;
}

void IconModel::resetIcon(const QString &name)
{
    change([name] {
        IconStore::removeOverride(name);
        IconStore::reload();
    });
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
    change([name, picture] {
        if (IconStore::overrideFor(name).isEmpty() && QFileInfo(picture).isFile()) {
            IconStore::setOverride(name, picture);
        }
        IconStore::reload();
    });
    return name;
}
