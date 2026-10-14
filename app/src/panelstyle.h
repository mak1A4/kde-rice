#pragma once

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
class PanelStyle : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int opacity READ opacity WRITE setOpacity NOTIFY changed)
    // kde-rice's Plasma style is the one in use
    Q_PROPERTY(bool inUse READ inUse NOTIFY changed)

public:
    explicit PanelStyle(QObject *parent = nullptr);

    int opacity() const;
    void setOpacity(int percent);
    bool inUse() const;

    // Back to the Plasma style there was before kde-rice's.
    Q_INVOKABLE void restore();

Q_SIGNALS:
    void changed();

private:
    bool write(int percent) const;
};
