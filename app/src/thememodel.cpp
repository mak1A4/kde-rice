#include "thememodel.h"
#include "applier.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QPointer>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrentRun>

using namespace Qt::StringLiterals;

// "-" in the choice: no background, chosen
static const auto s_none = u"-"_s;

ThemeModel::ThemeModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_settle.setSingleShot(true);
    m_settle.setInterval(300);
    connect(&m_settle, &QTimer::timeout, this, &ThemeModel::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_settle, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_settle, qOverload<>(&QTimer::start));
    QDir().mkpath(ThemeStore::userDir());
    reload();
}

void ThemeModel::reload()
{
    const QString chosenBefore = m_choice.theme;
    m_choice = ThemeStore::choice();

    beginResetModel();
    m_rows.clear();
    const QString userDir = ThemeStore::userDir();
    const QStringList names = ThemeStore::names();
    for (const QString &name : names) {
        auto theme = ThemeStore::load(name);
        if (!theme) {
            continue;
        }
        Row row;
        row.theme = *theme;
        row.own = QFileInfo(theme->path).absolutePath() == userDir;
        const QString background = chosenBackground(name);
        row.thumbnail = background.isEmpty() ? QString() : ThemeStore::thumbnail(background);
        m_rows += row;
    }
    endResetModel();
    Q_EMIT currentChanged();

    // Saving the chosen theme's file applies it again. A theme chosen
    // elsewhere (kde-rice-theme, the launcher) is applied there, and only
    // followed here.
    QFile file(ThemeStore::pathOf(m_choice.theme));
    const QByteArray contents = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    if (!m_currentFile.isNull() && m_choice.theme == chosenBefore && contents != m_currentFile) {
        apply();
    }
    m_currentFile = contents.isNull() ? QByteArray("") : contents;

    watch();
    makeThumbnails();
    ++m_revision;
    Q_EMIT revisionChanged();
}

void ThemeModel::watch()
{
    if (!m_watcher.directories().isEmpty()) {
        m_watcher.removePaths(m_watcher.directories());
    }
    if (!m_watcher.files().isEmpty()) {
        m_watcher.removePaths(m_watcher.files());
    }
    // the configuration directory too: theme.json, written by others
    QStringList paths = {ThemeStore::userDir(), QFileInfo(ThemeStore::userDir()).absolutePath()};
    const QString backgrounds = QFileInfo(ThemeStore::backgroundsDir(u"x"_s)).absolutePath();
    if (QFileInfo::exists(backgrounds)) {
        paths += backgrounds;
        const auto dirs = QDir(backgrounds).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &dir : dirs) {
            paths += dir.absoluteFilePath();
        }
    }
    // editors save by replacing the file, which a directory watch alone misses
    const auto files = QDir(ThemeStore::userDir()).entryInfoList({u"*.json"_s}, QDir::Files);
    for (const QFileInfo &file : files) {
        paths += file.absoluteFilePath();
    }
    m_watcher.addPaths(paths);
}

void ThemeModel::makeThumbnails()
{
    QStringList missing;
    for (const Row &row : std::as_const(m_rows)) {
        const QStringList all = ThemeStore::backgrounds(row.theme.name);
        for (const QString &path : all) {
            if (ThemeStore::thumbnail(path).isEmpty()) {
                missing += path;
            }
        }
    }
    if (missing.isEmpty()) {
        return;
    }
    auto watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher] {
        watcher->deleteLater();
        for (qsizetype i = 0; i < m_rows.size(); ++i) {
            const QString background = chosenBackground(m_rows[i].theme.name);
            m_rows[i].thumbnail = background.isEmpty() ? QString() : ThemeStore::thumbnail(background);
        }
        Q_EMIT dataChanged(index(0), index(m_rows.size() - 1), {ThumbnailRole});
        ++m_revision;
        Q_EMIT revisionChanged();
    });
    watcher->setFuture(QtConcurrent::run([missing] {
        for (const QString &path : missing) {
            ThemeStore::makeThumbnail(path);
        }
    }));
}

QString ThemeModel::chosenBackground(const QString &name) const
{
    if (m_choice.backgrounds.value(name) == s_none) {
        return {};
    }
    return ThemeStore::backgroundFor(name, m_choice);
}

int ThemeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant ThemeModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const Row &row = m_rows[index.row()];
    const Theme &theme = row.theme;
    switch (role) {
    case NameRole:
        return theme.name;
    case TitleRole:
    case Qt::DisplayRole:
        return theme.title;
    case DarkRole:
        return theme.isDark();
    case CurrentRole:
        return theme.name == m_choice.theme;
    case BackgroundRole:
        return theme.color(u"bg"_s);
    case SurfaceRole:
        return theme.color(u"surface"_s);
    case ForegroundRole:
        return theme.color(u"fg"_s);
    case AccentRole:
        return theme.color(u"accent"_s);
    case SwatchesRole: {
        // its red, green, yellow, blue, magenta and cyan
        QVariantList swatches;
        for (int i = 1; i <= 6; ++i) {
            swatches += theme.terminal.value(i);
        }
        return swatches;
    }
    case ThumbnailRole:
        return row.thumbnail.isEmpty() ? QUrl() : QUrl::fromLocalFile(row.thumbnail);
    case OwnRole:
        return row.own;
    }
    return {};
}

QHash<int, QByteArray> ThemeModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {TitleRole, "title"},
        {DarkRole, "dark"},
        {CurrentRole, "current"},
        {BackgroundRole, "bg"},
        {SurfaceRole, "surface"},
        {ForegroundRole, "fg"},
        {AccentRole, "accent"},
        {SwatchesRole, "swatches"},
        {ThumbnailRole, "thumbnail"},
        {OwnRole, "own"},
    };
}

QString ThemeModel::currentTitle() const
{
    return m_choice.theme.isEmpty() ? QString() : ThemeStore::title(m_choice.theme);
}

bool ThemeModel::currentIsDark() const
{
    for (const Row &row : m_rows) {
        if (row.theme.name == m_choice.theme) {
            return row.theme.isDark();
        }
    }
    return true;
}

QString ThemeModel::userDir() const
{
    return ThemeStore::userDir();
}

void ThemeModel::choose(const QString &name)
{
    if (ThemeStore::pathOf(name).isEmpty()) {
        return;
    }
    m_choice = ThemeStore::choice();
    m_choice.theme = name;
    ThemeStore::saveChoice(m_choice);
    QFile file(ThemeStore::pathOf(name));
    m_currentFile = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("");
    Q_EMIT dataChanged(index(0), index(m_rows.size() - 1), {CurrentRole});
    Q_EMIT currentChanged();
    apply();
}

void ThemeModel::chooseBackground(const QString &name, const QString &file)
{
    m_choice = ThemeStore::choice();
    m_choice.backgrounds.insert(name, file.isEmpty() ? s_none : file);
    ThemeStore::saveChoice(m_choice);
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].theme.name == name) {
            const QString background = chosenBackground(name);
            m_rows[i].thumbnail = background.isEmpty() ? QString() : ThemeStore::thumbnail(background);
            Q_EMIT dataChanged(index(i), index(i), {ThumbnailRole});
        }
    }
    ++m_revision;
    Q_EMIT revisionChanged();
    if (name == m_choice.theme) {
        apply();
    }
}

QVariantList ThemeModel::backgrounds(const QString &name) const
{
    const QString chosen = chosenBackground(name);
    QVariantList list;
    const QStringList all = ThemeStore::backgrounds(name);
    for (const QString &path : all) {
        const QString thumbnail = ThemeStore::thumbnail(path);
        list += QVariantMap{
            {u"file"_s, QFileInfo(path).fileName()},
            {u"path"_s, QUrl::fromLocalFile(path)},
            {u"thumbnail"_s, thumbnail.isEmpty() ? QUrl() : QUrl::fromLocalFile(thumbnail)},
            {u"video"_s, ThemeStore::isVideo(path)},
            {u"chosen"_s, path == chosen},
        };
    }
    list += QVariantMap{{u"file"_s, QString()}, {u"chosen"_s, chosen.isEmpty()}};
    return list;
}

QUrl ThemeModel::backgroundsFolder(const QString &name) const
{
    const QString dir = ThemeStore::backgroundsDir(name);
    QDir().mkpath(dir);
    return QUrl::fromLocalFile(dir);
}

QUrl ThemeModel::copyTheme(const QString &name)
{
    const QString from = ThemeStore::pathOf(name);
    if (from.isEmpty()) {
        return {};
    }
    // "everforest" -> "everforest-mine", "everforest-mine-2", ...
    const QString base = name.endsWith("-mine"_L1) ? name : name + "-mine"_L1;
    QString copy = base;
    for (int i = 2; !ThemeStore::pathOf(copy).isEmpty(); ++i) {
        copy = base + u'-' + QString::number(i);
    }
    const QString to = ThemeStore::userDir() + u'/' + copy + ".json"_L1;
    QDir().mkpath(ThemeStore::userDir());
    if (!QFile::copy(from, to)) {
        return {};
    }
    QFile::setPermissions(to, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    reload();
    return QUrl::fromLocalFile(to);
}

void ThemeModel::apply()
{
    // one at a time; a theme chosen meanwhile after it
    if (m_busy) {
        m_again = true;
        return;
    }
    const auto theme = ThemeStore::load(m_choice.theme);
    if (!theme) {
        return;
    }
    const QString background = chosenBackground(theme->name);
    m_busy = true;
    m_problems.clear();
    Q_EMIT busyChanged();
    auto watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher] {
        watcher->deleteLater();
        m_problems = watcher->result();
        m_busy = false;
        Q_EMIT busyChanged();
        if (m_again) {
            m_again = false;
            apply();
        }
    });
    watcher->setFuture(QtConcurrent::run([theme = *theme, background] {
        return ThemeApplier::apply(theme, background);
    }));
}
