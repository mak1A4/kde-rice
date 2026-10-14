#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include <functional>

// Rows of kde-rice's own that the launcher mixes into KRunner's results, such
// as "kde-rice settings" or, later, one per theme. Whoever owns such a thing
// registers it here once; the search then offers it whenever its title
// contains what was typed, above everything KRunner found.
class LocalActions : public QObject
{
    Q_OBJECT

public:
    struct Entry {
        QString id;
        QString title;
        QString subtitle;
        QString iconName;
        std::function<void()> trigger;
    };

    explicit LocalActions(QObject *parent = nullptr);

    // An entry with an id that is already there replaces it, so a part can
    // register its entries again whenever they change.
    void add(const QString &id, const QString &title, const QString &subtitle, const QString &iconName, std::function<void()> trigger);
    bool remove(const QString &id);

    // the entries whose title or subtitle contains every word of `query`, ignoring case, in the order
    // they were added; none for an empty query
    QList<Entry> matching(const QString &query) const;

    // false if there is no such entry (any more)
    bool trigger(const QString &id) const;

Q_SIGNALS:
    void changed();

private:
    QList<Entry> m_entries;
};
