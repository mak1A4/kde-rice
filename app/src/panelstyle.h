#pragma once

#include "worker.h"

#include <KConfigWatcher>

#include <QObject>
#include <QQmlEngine>

// How see-through Plasma's panels are. Plasma's own setting knows only
// opaque, translucent and adaptive; how translucent is the Plasma style's,
// which draws the panel's background at 85 %. So kde-rice makes a Plasma
// style of its own, "kde-rice": the panel's background of Breeze's style at
// the opacity chosen here, and everything else Breeze's (a style takes
// what it does not have from the default one, and with no colours of its
// own it follows the colour scheme, the kde-rice theme's).
//
// `[Panels] Opacity` in kde-ricerc, in percent. The panel itself has to be
// translucent or adaptive in its settings.
//
// Writing the style reads every picture of Breeze's and applies the style
// anew, seconds of work: done off the GUI thread (`busy` meanwhile).
class PanelStyle : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int opacity READ opacity WRITE setOpacity NOTIFY changed)
    // kde-rice's Plasma style is the one in use
    Q_PROPERTY(bool inUse READ inUse NOTIFY changed)
    // the style being written and applied
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit PanelStyle(QObject *parent = nullptr);

    int opacity() const
    {
        return m_opacity;
    }
    void setOpacity(int percent);
    bool inUse() const
    {
        return m_inUse;
    }
    bool busy() const
    {
        return m_worker.busy();
    }

    // Back to the Plasma style there was before kde-rice's.
    Q_INVOKABLE void restore();

    // Sets the opacity and writes and applies the style, waiting for it
    // (the command line).
    static void applyOpacity(int percent);

Q_SIGNALS:
    void changed();
    void busyChanged();

private:
    static bool write(int percent);
    static void applyStyle(int percent);
    void read();

    int m_opacity = 85;
    bool m_inUse = false;
    Worker m_worker;
    KConfigWatcher::Ptr m_watcher;
};
