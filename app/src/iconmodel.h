#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// The Icons page: kde-rice's icon theme (its base, whether it is in use),
// the taskbar's applications with their icons, and the pictures to choose
// from for any icon name. Pictures are files here, found the way the icon
// theme finds them (the user's own first, then the base's, then Papirus's
// and Breeze's), so that a change shows at once without the hub's own icons
// having to be reloaded.
class IconModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool inUse READ inUse NOTIFY changed)
    Q_PROPERTY(QString base READ base NOTIFY changed)
    Q_PROPERTY(QString baseVariant READ baseVariant NOTIFY changed)
    Q_PROPERTY(QVariantList themes READ themes CONSTANT)
    Q_PROPERTY(bool whiteSurInstalled READ whiteSurInstalled CONSTANT)
    Q_PROPERTY(QVariantList taskbar READ taskbar NOTIFY changed)
    Q_PROPERTY(QStringList overrides READ overrides NOTIFY changed)
    // goes up at every change, for pictures to be loaded again
    Q_PROPERTY(int revision MEMBER m_revision NOTIFY changed)
    Q_PROPERTY(bool searching MEMBER m_searching NOTIFY searchingChanged)

public:
    explicit IconModel(QObject *parent = nullptr);

    bool inUse() const;
    QString base() const;
    QString baseVariant() const;
    QVariantList themes() const;
    bool whiteSurInstalled() const;
    QVariantList taskbar() const;
    QStringList overrides() const;

    Q_INVOKABLE void use();
    Q_INVOKABLE void setBase(const QString &theme);
    // the picture an icon name is drawn with now, as a URL ("" if none)
    Q_INVOKABLE QUrl current(const QString &name) const;
    Q_INVOKABLE bool overridden(const QString &name) const;
    // [{ theme, themeName, url }] for an icon name, every installed theme's
    Q_INVOKABLE QVariantList candidates(const QString &name) const;
    // icon names containing `text`, answered by searchFinished
    Q_INVOKABLE void search(const QString &text);
    Q_INVOKABLE bool setIcon(const QString &name, const QUrl &picture);
    Q_INVOKABLE void resetIcon(const QString &name);
    // For an application whose menu entry names a picture file instead of an
    // icon: a copy of the entry in the user's applications naming an icon of
    // its own, which then can be chosen like any other. Its icon name.
    Q_INVOKABLE QString themable(const QString &desktopId);

Q_SIGNALS:
    void changed();
    void searchingChanged();
    void searchFinished(const QString &text, const QStringList &names);

private:
    QString mode() const;
    void bump();

    int m_revision = 0;
    bool m_searching = false;
};
