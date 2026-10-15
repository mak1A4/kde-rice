#include "panelstyle.h"
#include "riceswitch.h"

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <cstdio>

int main(int argc, char *argv[])
{
    // QApplication, not QGuiApplication: Plasma's Qt Quick style uses widgets
    // for its colours and metrics
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("kde-rice");

    KAboutData about(QStringLiteral("kde-rice"),
                     i18n("kde-rice"),
                     QStringLiteral(KDERICE_VERSION),
                     i18n("One place for everything kde-rice changes in Plasma and KWin"),
                     KAboutLicense::GPL_V3);
    about.setDesktopFileName(QStringLiteral(KDERICE_ID));
    KAboutData::setApplicationData(about);
    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    parser.addOption({QStringLiteral("page"), i18n("The page to open: appearance, icons, windows, panels or effects."), QStringLiteral("page")});
    parser.addOption({QStringLiteral("panel-opacity"), i18n("Set the panels' opacity (percent) and quit."), QStringLiteral("percent")});
    parser.addOption({QStringLiteral("restore-defaults"),
                      i18n("Turn kde-rice off: everything it changes back to what there was before it, else to Plasma's own, and quit.")});
    parser.addOption({QStringLiteral("reapply"), i18n("Turn kde-rice on again after --restore-defaults, as it was, and quit.")});
    parser.addOption({QStringLiteral("status"), i18n("Say whether kde-rice is on or off, and quit.")});
    parser.process(app);
    about.processCommandLine(&parser);

    if (parser.isSet(QStringLiteral("panel-opacity"))) {
        PanelStyle::applyOpacity(parser.value(QStringLiteral("panel-opacity")).toInt());
        return 0;
    }
    if (parser.isSet(QStringLiteral("status"))) {
        std::fputs(RiceSwitch::isOn() ? "on\n" : "off\n", stdout);
        return 0;
    }
    if (parser.isSet(QStringLiteral("restore-defaults")) || parser.isSet(QStringLiteral("reapply"))) {
        const QStringList problems = parser.isSet(QStringLiteral("restore-defaults")) ? RiceSwitch::restoreDefaults() : RiceSwitch::reapply();
        for (const QString &problem : problems) {
            std::fprintf(stderr, "%s\n", qPrintable(problem));
        }
        std::fputs(RiceSwitch::isOn() ? "kde-rice is on.\n" : "kde-rice is off: kde-rice --reapply turns it on again.\n", stdout);
        return problems.isEmpty() ? 0 : 1;
    }

    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-theme-global")));

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.rootContext()->setContextProperty(QStringLiteral("initialPage"), parser.value(QStringLiteral("page")));
    engine.loadFromModule("io.github.mak1a4.rice", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
