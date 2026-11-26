#include "session.h"
#include "icons.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

Session::Session(QObject *parent)
    : QObject(parent)
{
}

void Session::start(const QString &program, const QStringList &arguments)
{
    const QString sibling = QDir(QCoreApplication::applicationDirPath()).filePath(program);
    QProcess::startDetached(QFileInfo(sibling).isExecutable() ? sibling : program, arguments);
}

QString Session::riceStyle()
{
    return QStringLiteral("kde-rice");
}

bool Session::riceStyleInstalled()
{
    const QStringList paths = QCoreApplication::libraryPaths();
    for (const QString &path : paths) {
        if (QFileInfo::exists(path + QLatin1String("/styles/kde-rice.so"))) {
            return true;
        }
    }
    return false;
}

void Session::widgetStyleChanged()
{
    // KGlobalSettings' StyleChanged, as Plasma's Application Style page
    // sends it
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KGlobalSettings"), QStringLiteral("org.kde.KGlobalSettings"), QStringLiteral("notifyChange"));
    message.setArguments({2, 0});
    QDBusConnection::sessionBus().send(message);
    // Plasma's shell takes it only when it starts again (its launcher, its
    // popups' menus)
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.plasmashell"))) {
        IconStore::restartPlasma();
    }
}
