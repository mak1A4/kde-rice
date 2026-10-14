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
};
