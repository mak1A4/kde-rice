#include "launcher.h"
#include "localactions.h"
#include "searchmodel.h"
#include "theme.h"

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include <cstdio>

using namespace Qt::StringLiterals;

namespace
{
struct Options {
    bool background = false;
    bool takeShortcut = false;
};

// The options that matter to a running instance too: a second start hands
// its command line over to the first (KDBusService), which reads it here.
Options readOptions(const QCommandLineParser &parser)
{
    return {parser.isSet(u"background"_s), parser.isSet(u"take-shortcut"_s)};
}

QString sibling(const QString &name);

// kde-rice's own entries

void addLocalActions(LocalActions &actions, Launcher &launcher)
{
    actions.add(u"settings"_s,
                i18nc("@action entry in the launcher", "kde-rice settings"),
                i18nc("@info:usagetip", "Themes, windows, panels and effects"),
                u"preferences-desktop-theme-global"_s,
                [&launcher] {
                    launcher.startProgram(sibling(u"kde-rice"_s), {}, QStringLiteral(KDERICE_ID));
                });
}

// A kde-rice program: the one beside this executable when run from a build
// directory, else the installed one
QString sibling(const QString &name)
{
    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(name);
    return QFileInfo(path).isExecutable() ? path : name;
}

// "Theme: <title>" for every theme, choosing it with kde-rice-theme: the
// hub, if open, sees the choice and follows. Registered again whenever a
// theme comes or goes.
void addThemes(LocalActions &actions, QStringList &registered)
{
    for (const QString &id : std::as_const(registered)) {
        actions.remove(id);
    }
    registered.clear();
    const QString current = ThemeStore::choice().theme;
    const QStringList names = ThemeStore::names();
    for (const QString &name : names) {
        const QString id = "theme:"_L1 + name;
        const auto theme = ThemeStore::load(name);
        const bool dark = theme && theme->isDark();
        actions.add(id,
                    i18nc("@action launcher entry, %1 is a theme's name", "Theme: %1", ThemeStore::title(name)),
                    name == current ? (dark ? i18nc("@info", "Dark theme, in use") : i18nc("@info", "Light theme, in use"))
                                    : (dark ? i18nc("@info", "Dark theme") : i18nc("@info", "Light theme")),
                    u"preferences-desktop-color"_s,
                    [name] {
                        QProcess::startDetached(sibling(u"kde-rice-theme"_s), {u"set"_s, name});
                    });
        registered += id;
    }
}

// --query: the search without a window, its results on stdout once the
// runners are done (or after five seconds). For testing the engine.
int printResults(QApplication &app, SearchModel &results, const QString &query)
{
    auto print = [&app, &results] {
        std::fputs(qPrintable(results.dump()), stdout);
        app.quit();
    };
    QObject::connect(&results, &SearchModel::queryingChanged, &app, [&results, &app, print] {
        // a moment more for the last runners' answers to reach the model
        if (!results.isQuerying()) {
            QTimer::singleShot(300, &app, print);
        }
    });
    QTimer::singleShot(5000, &app, print);
    results.setQuery(query);
    return app.exec();
}
}

int main(int argc, char *argv[])
{
    // QApplication, not QGuiApplication: Plasma's Qt Quick style uses widgets
    // for its colours and metrics
    QApplication app(argc, argv);
    // Hidden is not quit: the process waits for the next opening. Both, as
    // something in KDE's libraries holds a QEventLoopLocker for a moment at
    // startup, and releasing it quits an application with no window shown.
    QApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setQuitLockEnabled(false);
    KLocalizedString::setApplicationDomain("kde-rice");

    KAboutData about(QStringLiteral("kde-rice-launcher"),
                     i18n("kde-rice Launcher"),
                     QStringLiteral(KDERICE_VERSION),
                     i18n("Search applications, windows, settings and more with KDE's runners"),
                     KAboutLicense::GPL_V3);
    // reversed, with the application's name, this is the D-Bus name:
    // io.github.mak1a4.kde-rice-launcher
    about.setOrganizationDomain("mak1a4.github.io");
    about.setDesktopFileName(QStringLiteral(KDERICE_ID ".launcher"));
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(u"search"_s));

    QCommandLineParser parser;
    parser.addOption({u"background"_s, i18n("Start without showing the launcher; do nothing if it is already running.")});
    parser.addOption({u"take-shortcut"_s, i18n("Take Meta+Space from whatever has it (KRunner) for the launcher.")});
    parser.addOption({u"query"_s, i18n("Print the results for <text> and quit, without a window or shortcut."), i18n("text")});
    about.setupCommandLine(&parser);
    parser.process(app);
    about.processCommandLine(&parser);

    LocalActions localActions;
    SearchModel results(&localActions);

    QStringList themeEntries;
    addThemes(localActions, themeEntries);

    if (parser.isSet(u"query"_s)) {
        Launcher launcher(&results);
        addLocalActions(localActions, launcher);
        return printResults(app, results, parser.value(u"query"_s));
    }

    // One instance. Started again, this process hands its command line to the
    // running one and exits here.
    KDBusService service(KDBusService::Unique);

    Launcher launcher(&results);
    addLocalActions(localActions, launcher);

    // themes added or removed, or another one chosen (the config directory
    // holds theme.json)
    QFileSystemWatcher themeWatcher;
    QDir().mkpath(ThemeStore::userDir());
    themeWatcher.addPaths(ThemeStore::dirs() << QFileInfo(ThemeStore::userDir()).absolutePath());
    QObject::connect(&themeWatcher, &QFileSystemWatcher::directoryChanged, &localActions, [&localActions, &themeEntries] {
        addThemes(localActions, themeEntries);
    });

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
    }

    // the window is transparent beside the box
    QQuickWindow::setDefaultAlphaBuffer(true);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.setInitialProperties({
        {u"launcher"_s, QVariant::fromValue(&launcher)},
        {u"results"_s, QVariant::fromValue(&results)},
    });
    engine.loadFromModule("io.github.mak1a4.rice.launcher", "Main");
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window) {
        return 1;
    }
    launcher.setWindow(window);

    // qdbus io.github.mak1a4.kde-rice-launcher /Launcher toggle
    QDBusConnection::sessionBus().registerObject(u"/Launcher"_s, &launcher, QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableProperties);

    launcher.registerShortcut();
    const Options options = readOptions(parser);
    if (options.takeShortcut) {
        launcher.takeShortcut();
    }

    // started again: the shortcut taken if asked, else the popup toggled,
    // except by the autostart entry
    QObject::connect(&service, &KDBusService::activateRequested, &launcher, [&launcher](const QStringList &arguments) {
        QCommandLineParser parser;
        parser.addOption({u"background"_s, QString()});
        parser.addOption({u"take-shortcut"_s, QString()});
        parser.parse(arguments);
        const Options options = readOptions(parser);
        if (options.takeShortcut) {
            launcher.takeShortcut();
        } else if (!options.background) {
            launcher.toggle();
        }
    });

    if (!options.background && !options.takeShortcut) {
        launcher.open();
    }
    return app.exec();
}
