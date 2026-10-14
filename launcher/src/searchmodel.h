#pragma once

#include <KRunner/QueryMatch>

#include <QAbstractListModel>
#include <QQmlEngine>

namespace KRunner
{
class ResultsModel;
}
class LocalActions;

// What the launcher lists for the typed query: kde-rice's own entries, then
// KRunner's results, as one flat list of rows for the view.
//
// The search is KDE's: KRunner::ResultsModel, the model Plasma's own KRunner
// window shows, which runs the runner plugins enabled in System Settings >
// Search > KRunner (krunnerrc) in this process. This model is a copy of its
// rows taken whenever it changes, for three reasons:
// - KRunner empties its list for a moment after every key; the copy keeps the
//   previous results until the first new ones are there, so the list does not
//   collapse and regrow while typing (`fresh`).
// - KRunner orders by category and use. Open windows go first here whatever it
//   thinks (switching to what is running comes before starting anything), then
//   the calculator's and the unit converter's answers.
// - kde-rice's own entries (LocalActions) are rows like the others.
//
// The selection lives here too, so that a row chosen by hand is found again,
// by id, title and subtitle together, when the results are replaced.
class SearchModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("made by the launcher")

    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE select NOTIFY currentIndexChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // false from a change of the query until KRunner has answered it; until
    // then the rows are still the previous query's
    Q_PROPERTY(bool fresh READ isFresh NOTIFY freshChanged)
    Q_PROPERTY(bool querying READ isQuerying NOTIFY queryingChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        SubtitleRole,
        CategoryRole,
        // an icon name or a QIcon, never an empty QIcon
        IconRole,
        // for an icon name the theme does not have
        FallbackIconRole,
        // [{text, icon}] for KRunner's extra actions of the match
        ActionsRole,
        MatchEnabledRole,
        IdRole,
    };
    Q_ENUM(Roles)

    explicit SearchModel(LocalActions *localActions, QObject *parent = nullptr);
    ~SearchModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const;
    void setQuery(const QString &query);
    int currentIndex() const;
    bool isFresh() const;
    bool isQuerying() const;

    // the selection, chosen by hand: kept across refreshes of the results
    Q_INVOKABLE void select(int row);
    Q_INVOKABLE void move(int delta);
    // how many extra actions the row has, for Tab
    Q_INVOKABLE int actionCount(int row) const;

    // Runs a row, or its action `action` (>= 0; out of range means the row
    // itself). Emits finished() if the launcher can go.
    Q_INVOKABLE void run(int row, int action = -1);
    // Runs the selected row, as Enter does. Before the results of the current
    // query are in, it runs the first one once they are.
    Q_INVOKABLE void runCurrent(int action = -1);

    // the runners loaded once and kept; tell them a search session is over
    // so they can drop what they cache for one
    void endSession();

    // the rows as text, for --query
    QString dump() const;

Q_SIGNALS:
    void queryChanged();
    void currentIndexChanged();
    void countChanged();
    void freshChanged();
    void queryingChanged();
    // something was run and the launcher can close
    void finished();
    // a match wants the query replaced (the calculator offers its answer so)
    void queryReplaced(const QString &query);

private:
    struct Action {
        QString text;
        QString icon;
    };
    struct Row {
        QString key;
        QString id;
        QString title;
        QString subtitle;
        QString category;
        QVariant icon;
        QString fallbackIcon;
        QList<Action> actions;
        bool enabled = true;
        bool local = false;
        bool window = false;
        bool answer = false;
        KRunner::QueryMatch match;
    };

    void sourceChanged();
    void scheduleRebuild();
    void rebuild();
    void setRows(QList<Row> rows, int current);
    void setFresh(bool fresh);
    bool runRow(const Row &row, int action);
    Row rowFor(const KRunner::QueryMatch &match) const;

    LocalActions *const m_localActions;
    KRunner::ResultsModel *const m_source;
    QList<Row> m_rows;
    QString m_query;
    int m_currentIndex = -1;
    // the selected row's key if chosen by hand, else empty (the first row)
    QString m_selectedKey;
    bool m_fresh = true;
    bool m_answered = true;
    bool m_rebuildQueued = false;
    // Enter before the results were in
    bool m_runPending = false;
    int m_pendingAction = -1;
};
