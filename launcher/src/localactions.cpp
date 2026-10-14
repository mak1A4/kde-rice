#include "localactions.h"

#include <algorithm>

LocalActions::LocalActions(QObject *parent)
    : QObject(parent)
{
}

void LocalActions::add(const QString &id, const QString &title, const QString &subtitle, const QString &iconName, std::function<void()> trigger)
{
    Entry entry{id, title, subtitle, iconName, std::move(trigger)};
    auto it = std::ranges::find(m_entries, id, &Entry::id);
    if (it != m_entries.end()) {
        *it = std::move(entry);
    } else {
        m_entries.append(std::move(entry));
    }
    Q_EMIT changed();
}

bool LocalActions::remove(const QString &id)
{
    if (m_entries.removeIf([&id](const Entry &entry) {
            return entry.id == id;
        })
        == 0) {
        return false;
    }
    Q_EMIT changed();
    return true;
}

QList<LocalActions::Entry> LocalActions::matching(const QString &query) const
{
    // every word typed somewhere in the title or the subtitle: "theme ever"
    // finds "Theme: Everforest", "theme dark" every dark theme
    const QStringList words = query.split(u' ', Qt::SkipEmptyParts);
    QList<Entry> found;
    if (words.isEmpty()) {
        return found;
    }
    for (const Entry &entry : m_entries) {
        const QString text = entry.title + u' ' + entry.subtitle;
        const bool all = std::all_of(words.cbegin(), words.cend(), [&text](const QString &word) {
            return text.contains(word, Qt::CaseInsensitive);
        });
        if (all) {
            found.append(entry);
        }
    }
    return found;
}

bool LocalActions::trigger(const QString &id) const
{
    auto it = std::ranges::find(m_entries, id, &Entry::id);
    if (it == m_entries.end() || !it->trigger) {
        return false;
    }
    // a copy: the callback may well add or remove entries
    const auto callback = it->trigger;
    callback();
    return true;
}
