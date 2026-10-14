#pragma once

#include <QObject>
#include <QQmlEngine>

// The running KWin, over D-Bus, and what of it is set in kwinrc.
class KWin : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // the window decoration's plugin id, as kwinrc names it
    Q_PROPERTY(QString decoration READ decoration WRITE setDecoration NOTIFY decorationChanged)
    // kde-rice's own Breeze (kwin/decoration/breeze)
    Q_PROPERTY(QString riceDecoration READ riceDecoration CONSTANT)
    Q_PROPERTY(bool riceDecorationInstalled READ riceDecorationInstalled CONSTANT)

public:
    explicit KWin(QObject *parent = nullptr);

    QString decoration() const;
    void setDecoration(const QString &id);
    static QString riceDecoration();
    static bool riceDecorationInstalled();

    // KWin reads kwinrc again
    Q_INVOKABLE static void reconfigure();

Q_SIGNALS:
    void decorationChanged();
};
