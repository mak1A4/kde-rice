#pragma once

#include "theme.h"

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QQmlEngine>
#include <QTimer>

// The themes for the Appearance page: one row each, the chosen one marked,
// and choosing one applies it (off the GUI thread; `busy` meanwhile).
//
// The user's theme directory is watched: a theme added there is listed, and
// saving the chosen theme's file applies it again, for working on a theme
// in an editor.
class ThemeModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString current READ current NOTIFY currentChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentChanged)
    Q_PROPERTY(bool currentIsDark READ currentIsDark NOTIFY currentChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // what went wrong at the last apply, one line each
    Q_PROPERTY(QStringList problems READ problems NOTIFY busyChanged)
    // goes up when thumbnails or backgrounds change, for bindings to look again
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QString userDir READ userDir CONSTANT)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        TitleRole,
        DarkRole,
        CurrentRole,
        BackgroundRole,
        SurfaceRole,
        ForegroundRole,
        AccentRole,
        SwatchesRole,
        ThumbnailRole,
        OwnRole,
    };
    Q_ENUM(Role)

    explicit ThemeModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString current() const
    {
        return m_choice.theme;
    }
    QString currentTitle() const;
    bool currentIsDark() const;
    bool busy() const
    {
        return m_busy;
    }
    QStringList problems() const
    {
        return m_problems;
    }
    int revision() const
    {
        return m_revision;
    }
    QString userDir() const;

    // Chooses a theme, with the background chosen for it before.
    Q_INVOKABLE void choose(const QString &name);
    // Chooses one of a theme's backgrounds by file name, "" for none.
    Q_INVOKABLE void chooseBackground(const QString &name, const QString &file);
    // [{ file, path, thumbnail, video, chosen }], and a last { file: "" } for none
    Q_INVOKABLE QVariantList backgrounds(const QString &name) const;
    // The directory a theme's backgrounds go in, made if missing.
    Q_INVOKABLE QUrl backgroundsFolder(const QString &name) const;
    // A copy of a theme in the user's directory, to edit: its file.
    Q_INVOKABLE QUrl copyTheme(const QString &name);
    Q_INVOKABLE void reload();

Q_SIGNALS:
    void currentChanged();
    void busyChanged();
    void revisionChanged();

private:
    struct Row {
        Theme theme;
        QString thumbnail;
        bool own = false;
    };

    void apply();
    void watch();
    void makeThumbnails();
    QString chosenBackground(const QString &name) const;

    QList<Row> m_rows;
    ThemeStore::Choice m_choice;
    bool m_busy = false;
    bool m_again = false;
    QStringList m_problems;
    int m_revision = 0;
    QFileSystemWatcher m_watcher;
    QTimer m_settle;
    QByteArray m_currentFile;
};
