#pragma once

#include <QObject>
#include <QQmlEngine>

// How Plasma's panels stand by the screen's edge: floating off it, docked
// with their popups floating (Plasma's "Applets only"), or docked with the
// popups against the panel too. Plasma keeps it per panel; kde-rice sets
// all panels alike and keeps the choice as `[Panels] Floating` in
// kde-ricerc (floating, popups, docked), so that kde-rice --reapply sets it
// again. Nothing there: what each panel has is left as it is.
class PanelLayout : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Mode, as the panels have it now (the first one's), else as chosen
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY changed)

public:
    enum Mode {
        Floating,
        FloatingPopups,
        Docked,
    };
    Q_ENUM(Mode)

    explicit PanelLayout(QObject *parent = nullptr);

    int mode() const
    {
        return m_mode;
    }
    void setMode(int mode);

    // Plasma's own: floating (kde-rice --restore-defaults), where kde-rice
    // set the panels
    static bool chosen();
    static void applyPlasmaDefault();
    // the choice in kde-ricerc, applied again (kde-rice --reapply)
    static void applyChosen();

Q_SIGNALS:
    void changed();

private:
    static void apply(Mode mode);

    int m_mode = Floating;
};
