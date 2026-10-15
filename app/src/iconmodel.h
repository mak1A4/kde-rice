#pragma once

#include "worker.h"

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// The Icons page: kde-rice's icon theme (its base, whether it is in use),
// the taskbar's applications with their icons, and the pictures to choose
// from for any icon name. Pictures are files here, found the way the icon
// theme finds them (the user's own first, then the base's, then Papirus's
// and Breeze's), so that a change shows at once without the hub's own icons
// having to be reloaded.
//
// Finding a picture means reading an icon theme's whole tree once, and a
// change means writing the theme and telling Plasma: both are done off the
// GUI thread (`busy` meanwhile), and what the page shows is kept here, taken
// anew after each change.
class IconModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool inUse READ inUse NOTIFY changed)
    Q_PROPERTY(QString base READ base NOTIFY changed)
    Q_PROPERTY(QString baseVariant READ baseVariant NOTIFY changed)
    Q_PROPERTY(QVariantList themes READ themes CONSTANT)
    Q_PROPERTY(bool whiteSurInstalled READ whiteSurInstalled CONSTANT)
    // [{ id, name, icon, fixed, picture, overridden, trimmed }]: the panel's
    // launchers, with the picture each is drawn with now ("" until found)
    Q_PROPERTY(QVariantList taskbar READ taskbar NOTIFY changed)
    // [{ name, picture }]: the icon names the user has a picture of their own for
    Q_PROPERTY(QVariantList overrides READ overrides NOTIFY changed)
    // app icons only hicolor or the pixmaps have, trimmed like the base's
    Q_PROPERTY(bool trimFallbacks READ trimFallbacks WRITE setTrimFallbacks NOTIFY changed)
    // goes up at every change, for pictures to be loaded again
    Q_PROPERTY(int revision MEMBER m_revision NOTIFY changed)
    // a change being written, or the pictures being found
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool searching MEMBER m_searching NOTIFY searchingChanged)

public:
    explicit IconModel(QObject *parent = nullptr);

    bool inUse() const
    {
        return m_state.inUse;
    }
    QString base() const
    {
        return m_state.base;
    }
    QString baseVariant() const
    {
        return m_state.baseVariant;
    }
    QVariantList themes() const;
    bool whiteSurInstalled() const;
    QVariantList taskbar() const
    {
        return m_state.taskbar;
    }
    QVariantList overrides() const
    {
        return m_state.overrides;
    }
    bool trimFallbacks() const
    {
        return m_state.trimFallbacks;
    }
    void setTrimFallbacks(bool trim);
    bool busy() const
    {
        return m_worker.busy();
    }

    Q_INVOKABLE void use();
    // the theme written again, for applications installed since (their
    // icons trimmed), and drawn again if that changed it
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setBase(const QString &theme);
    // the picture an icon name is drawn with now, as a URL ("" if none)
    Q_INVOKABLE QUrl current(const QString &name) const;
    Q_INVOKABLE bool overridden(const QString &name) const;
    // [{ theme, themeName, url }] for an icon name, every installed theme's:
    // answered by candidatesFound, as every theme is read for it
    Q_INVOKABLE void findCandidates(const QString &name);
    // icon names containing `text`, answered by searchFinished
    Q_INVOKABLE void search(const QString &text);
    // Takes a picture for an icon name. False if it cannot be (not a file,
    // not an SVG or PNG); true, and it is written and shown as soon as done.
    Q_INVOKABLE bool setIcon(const QString &name, const QUrl &picture);
    Q_INVOKABLE void resetIcon(const QString &name);
    // For an application whose menu entry names a picture file instead of an
    // icon: a copy of the entry in the user's applications naming an icon of
    // its own, which then can be chosen like any other. Its icon name.
    Q_INVOKABLE QString themable(const QString &desktopId);

Q_SIGNALS:
    void changed();
    void busyChanged();
    void searchingChanged();
    void searchFinished(const QString &text, const QStringList &names);
    void candidatesFound(const QString &name, const QVariantList &candidates);

private:
    // what the page shows, read all at once
    struct State {
        bool inUse = false;
        QString base;
        QString baseVariant;
        bool trimFallbacks = true;
        QVariantList taskbar;
        QVariantList overrides;
    };
    static State read(bool withPictures);
    static QString pictureFor(const QString &name, const State &state);
    // `work` done on the worker thread, then the state read anew and shown
    template<typename Work>
    void change(Work work);
    void take(const State &state);

    State m_state;
    int m_revision = 0;
    bool m_searching = false;
    Worker m_worker;
};
