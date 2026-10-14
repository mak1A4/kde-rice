#include "session.h"

#include <QCoreApplication>
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
