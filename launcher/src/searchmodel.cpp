#include "searchmodel.h"
#include "localactions.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KRunner/Action>
#include <KRunner/ResultsModel>
#include <KRunner/RunnerManager>
#include <KSharedConfig>

#include <QIcon>
#include <QRegularExpression>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

// How many results KRunner keeps; it may go a little over to give every
// category a place. Eight are visible, the rest is scrolled to.
static constexpr int s_limit = 24;

// krunnerrc's [Plugins]: which runners are enabled, as KRunner itself reads it
static KConfigGroup pluginConfig()
{
    return KSharedConfig::openConfig(u"krunnerrc"_s, KConfig::NoGlobals)->group(u"Plugins"_s);
}

// KRunner's own history and launch counts, shared on purpose: what was used
// most often in KRunner ranks first here from the first day, and the other way
// round while both are in use.
static KConfigGroup stateConfig()
{
    return KSharedConfig::openConfig(u"krunnerstaterc"_s, KConfig::NoGlobals, QStandardPaths::GenericDataLocation)->group(u"PlasmaRunnerManager"_s);
}

SearchModel::SearchModel(LocalActions *localActions, QObject *parent)
    : QAbstractListModel(parent)
    , m_localActions(localActions)
    , m_source(new KRunner::ResultsModel(pluginConfig(), stateConfig(), this))
{
    m_source->setLimit(s_limit);
    // the runners listed first in System Settings > Search > KRunner, with
    // KRunner's own default when nothing was chosen there
    m_source->setFavoriteIds(pluginConfig().group(u"Favorites"_s).readEntry("plugins", QStringList{u"krunner_services"_s, u"krunner_systemsettings"_s}));

    connect(m_source, &QAbstractItemModel::rowsInserted, this, &SearchModel::sourceChanged);
    connect(m_source, &QAbstractItemModel::rowsRemoved, this, &SearchModel::sourceChanged);
    connect(m_source, &QAbstractItemModel::rowsMoved, this, &SearchModel::sourceChanged);
    connect(m_source, &QAbstractItemModel::modelReset, this, &SearchModel::sourceChanged);
    connect(m_source, &QAbstractItemModel::layoutChanged, this, &SearchModel::sourceChanged);
    connect(m_source, &QAbstractItemModel::dataChanged, this, &SearchModel::sourceChanged);
    connect(m_source, &KRunner::ResultsModel::queryingChanged, this, [this] {
        // a query that ends without changing the rows (nothing found, or the
        // same as before) is an answer too
        if (!m_source->querying()) {
            sourceChanged();
        }
        Q_EMIT queryingChanged();
    });
    connect(m_source, &KRunner::ResultsModel::queryStringChangeRequested, this, &SearchModel::queryReplaced);
    connect(m_localActions, &LocalActions::changed, this, &SearchModel::scheduleRebuild);
}

SearchModel::~SearchModel() = default;

int SearchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return row.title;
    case SubtitleRole:
        return row.subtitle;
    case CategoryRole:
        return row.category;
    case IconRole:
        return row.icon;
    case FallbackIconRole:
        return row.fallbackIcon;
    case ActionsRole: {
        QVariantList actions;
        for (const Action &action : row.actions) {
            actions.append(QVariantMap{{u"text"_s, action.text}, {u"icon"_s, action.icon}});
        }
        return actions;
    }
    case MatchEnabledRole:
        return row.enabled;
    case IdRole:
        return row.id;
    }
    return {};
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
    return {
        {TitleRole, "title"},
        {SubtitleRole, "subtitle"},
        {CategoryRole, "category"},
        {IconRole, "iconSource"},
        {FallbackIconRole, "fallbackIcon"},
        {ActionsRole, "actions"},
        {MatchEnabledRole, "matchEnabled"},
        {IdRole, "matchId"},
    };
}

QString SearchModel::query() const
{
    return m_query;
}

void SearchModel::setQuery(const QString &query)
{
    if (query == m_query) {
        return;
    }
    m_query = query;
    Q_EMIT queryChanged();

    // a new query starts at its first result, as in KRunner, and an Enter
    // still waiting was meant for the previous one
    m_selectedKey.clear();
    m_runPending = false;

    if (query.trimmed().isEmpty()) {
        m_source->setQueryString(QString());
        m_answered = true;
        setRows({}, -1);
        setFresh(true);
        return;
    }

    setFresh(false);
    m_source->setQueryString(query);
    // anything KRunner reported while taking the query was about the
    // previous one
    m_answered = false;
    // every runner may have declined at once (too few letters for them),
    // which leaves nothing to wait for
    if (!m_source->querying()) {
        sourceChanged();
    }
}

int SearchModel::currentIndex() const
{
    return m_currentIndex;
}

bool SearchModel::isFresh() const
{
    return m_fresh;
}

bool SearchModel::isQuerying() const
{
    return m_source->querying();
}

void SearchModel::select(int row)
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    m_selectedKey = m_rows.at(row).key;
    if (row != m_currentIndex) {
        m_currentIndex = row;
        Q_EMIT currentIndexChanged();
    }
}

void SearchModel::move(int delta)
{
    if (m_rows.isEmpty()) {
        return;
    }
    select(std::clamp(m_currentIndex + delta, 0, int(m_rows.size()) - 1));
}

int SearchModel::actionCount(int row) const
{
    return row >= 0 && row < m_rows.size() ? int(m_rows.at(row).actions.size()) : 0;
}

void SearchModel::run(int row, int action)
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    // a copy: running can replace the rows (a match that sets the query)
    const Row chosen = m_rows.at(row);
    if (runRow(chosen, action)) {
        Q_EMIT finished();
    }
}

void SearchModel::runCurrent(int action)
{
    // What is listed is the previous query's. KRunner's own window runs the
    // first result of the new one when it comes, and so does this.
    if (!m_fresh) {
        m_runPending = true;
        m_pendingAction = action;
        return;
    }
    run(m_currentIndex, action);
}

void SearchModel::endSession()
{
    m_source->runnerManager()->matchSessionComplete();
}

QString SearchModel::dump() const
{
    QString text;
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        const Row &row = m_rows.at(i);
        text += u"%1. %2"_s.arg(i + 1).arg(row.title);
        if (!row.subtitle.isEmpty()) {
            text += u" — "_s + row.subtitle;
        }
        text += u"  [%1, %2]"_s.arg(row.category, row.id);
        for (const Action &action : row.actions) {
            text += u" {"_s + action.text + u'}';
        }
        text += u'\n';
    }
    return text;
}

void SearchModel::sourceChanged()
{
    m_answered = true;
    scheduleRebuild();
}

// Results come runner by runner, each a handful of model signals; one rebuild
// for each burst.
void SearchModel::scheduleRebuild()
{
    if (m_rebuildQueued) {
        return;
    }
    m_rebuildQueued = true;
    QMetaObject::invokeMethod(this, &SearchModel::rebuild, Qt::QueuedConnection);
}

void SearchModel::rebuild()
{
    m_rebuildQueued = false;
    if (m_query.trimmed().isEmpty()) {
        return;
    }
    const int count = m_source->rowCount();
    // While a query runs KRunner may have nothing yet; the previous results
    // stay up until the first new ones arrive.
    if (!m_answered || (count == 0 && m_source->querying())) {
        return;
    }

    QList<Row> rows;
    const auto locals = m_localActions->matching(m_query);
    for (const LocalActions::Entry &entry : locals) {
        Row row;
        row.id = entry.id;
        row.title = entry.title;
        row.subtitle = entry.subtitle;
        row.category = i18nc("@item:intext category of kde-rice's own entries", "kde-rice");
        row.icon = entry.iconName;
        row.fallbackIcon = u"preferences-desktop-theme-global"_s;
        row.local = true;
        row.key = u"local\n"_s + entry.id;
        rows.append(std::move(row));
    }

    QList<Row> windows;
    QList<Row> answers;
    QList<Row> rest;
    for (int i = 0; i < count; ++i) {
        const KRunner::QueryMatch match = m_source->getQueryMatch(m_source->index(i, 0));
        if (!match.isValid()) {
            continue;
        }
        Row row = rowFor(match);
        (row.window ? windows : row.answer ? answers : rest).append(std::move(row));
    }
    rows << windows << answers << rest;

    int current = rows.isEmpty() ? -1 : 0;
    if (!m_selectedKey.isEmpty()) {
        const auto it = std::ranges::find(rows, m_selectedKey, &Row::key);
        if (it != rows.end()) {
            current = int(it - rows.begin());
        } else {
            m_selectedKey.clear();
        }
    }
    setRows(std::move(rows), current);
    setFresh(true);

    if (m_runPending && (!m_rows.isEmpty() || !m_source->querying())) {
        m_runPending = false;
        run(0, m_pendingAction);
    }
}

SearchModel::Row SearchModel::rowFor(const KRunner::QueryMatch &match) const
{
    static const QRegularExpression url(u"^[a-z][a-z0-9+.-]*://"_s, QRegularExpression::CaseInsensitiveOption);

    Row row;
    row.id = match.id();
    row.title = match.text();
    row.subtitle = match.subtext();
    row.category = match.matchCategory();
    row.enabled = match.isEnabled();
    row.match = match;
    // Not the id alone: the unit converter, for one, uses one id for
    // whatever it currently answers.
    row.key = row.id + u'\n' + row.title + u'\n' + row.subtitle;
    row.fallbackIcon = url.match(row.subtitle).hasMatch() ? u"internet-web-browser"_s : u"system-search"_s;
    // A runner gives an icon name or a QIcon, and the QIcon may be empty
    // (browser tabs and history without a favicon).
    if (!match.iconName().isEmpty()) {
        row.icon = match.iconName();
    } else if (const QIcon icon = match.icon(); !icon.isNull()) {
        row.icon = QVariant::fromValue(icon);
    } else {
        row.icon = row.fallbackIcon;
    }
    const auto actions = match.actions();
    for (const KRunner::Action &action : actions) {
        row.actions.append({action.text(), action.iconSource()});
    }
    // KWin's runner: an open window (or a virtual desktop) to switch to
    row.window = row.id.startsWith(u"windows_");
    // the calculator's and the unit converter's: not something to open but
    // the answer to what was typed
    row.answer = row.id.startsWith(u"calculator") || row.id.startsWith(u"unitconverter");
    return row;
}

void SearchModel::setRows(QList<Row> rows, int current)
{
    const auto oldCount = m_rows.size();
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    if (m_rows.size() != oldCount) {
        Q_EMIT countChanged();
    }
    m_currentIndex = current;
    // always: the view has just been reset and needs telling again
    Q_EMIT currentIndexChanged();
}

void SearchModel::setFresh(bool fresh)
{
    if (fresh != m_fresh) {
        m_fresh = fresh;
        Q_EMIT freshChanged();
    }
}

bool SearchModel::runRow(const Row &row, int action)
{
    if (row.local) {
        return m_localActions->trigger(row.id);
    }
    const auto actions = row.match.actions();
    const KRunner::Action chosen = action >= 0 && action < actions.size() ? actions.at(action) : KRunner::Action();
    // KDE's own run: history, launch counts, and for an informational match
    // (the calculator's) a request to replace the query instead
    return m_source->runnerManager()->run(row.match, chosen);
}
