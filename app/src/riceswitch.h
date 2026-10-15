#pragma once

#include "worker.h"

#include <QObject>
#include <QQmlEngine>
#include <QStringList>

// kde-rice as a whole, off and on again: off leaves a session that looks
// like Plasma as it comes (or as the user had it before kde-rice), on puts
// back what off took away.
//
// Off, everything kde-rice changes outside itself goes back to what there
// was before it, where kde-rice noted that, else to Plasma's own:
//   - the colour scheme, the icon theme and the Plasma style (the ones noted
//     in ~/.local/state/kde-rice, else Breeze Light, Breeze and Breeze's);
//   - the window decoration (Breeze) and KWin's plugins of kde-rice's
//     (unloaded, and off in kwinrc);
//   - the gaps, the launcher and the tray: stopped, and kept from starting
//     at login (entries of the user's own in ~/.config/autostart that hide
//     kde-rice's), the launcher's Meta+Space given up;
//   - the desktop's and the lock screen's wallpaper, and kde-rice's widgets
//     taken off the desktop (noted, where they were and how set up);
//   - applets wearing one of kde-rice's glyphs as their icon (Show Desktop's
//     kde-rice-show-desktop), which only kde-rice's icon theme has: their
//     own icon instead, and the glyph again when on;
//   - the files written for other programs, and T3 Code's theme
//     (ThemeApplier::restore);
//   - the login screen, through login-screen (pkexec: asks for the
//     password).
// What was on is noted in ~/.local/state/kde-rice/off.json, which is also
// what says kde-rice is off (login-screen and install.sh read it too). The
// choices of kde-rice's own (the theme, the opacity, the gap ...) are left
// as they are, for on.
//
// Plasma is restarted at the end of either (the desktop and the panels, not
// the windows), so that it lets go of every icon and picture of kde-rice's.
//
// KDERICE_FILES_ONLY=1 leaves kde-rice's programs and the login screen as
// they are and tells no running program (for trying it out).
class RiceSwitch : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool on READ isOn NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // what went wrong the last time, one line each
    Q_PROPERTY(QStringList problems READ problems NOTIFY changed)

public:
    explicit RiceSwitch(QObject *parent = nullptr);

    static bool isOn();
    bool busy() const
    {
        return m_worker.busy();
    }
    QStringList problems() const
    {
        return m_problems;
    }

    Q_INVOKABLE void turnOff();
    Q_INVOKABLE void turnOn();

    // the same, waiting for it (the command line); what went wrong
    static QStringList restoreDefaults();
    static QStringList reapply();

Q_SIGNALS:
    void changed();
    void busyChanged();

private:
    void finished(const QStringList &problems);

    QStringList m_problems;
    Worker m_worker;
};
