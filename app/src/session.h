#pragma once

#include <QObject>
#include <QQmlEngine>

// kde-rice's own programs, started from the hub.
class Session : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Session(QObject *parent = nullptr);

    // Starts a kde-rice program (the one beside the hub when run from a
    // build directory, else the installed one) and leaves it running.
    Q_INVOKABLE static void start(const QString &program, const QStringList &arguments = {});

    // kde-rice's widget style (style/), the name it has for Qt
    Q_INVOKABLE static QString riceStyle();
    Q_INVOKABLE static bool riceStyleInstalled();
    // The running applications told that the widget style in kdeglobals is
    // another: they take it at once, as from Plasma's own page; Plasma's
    // shell is started again for it. (KWin's menus take it at the next
    // login.)
    Q_INVOKABLE static void widgetStyleChanged();
};
