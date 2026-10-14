#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

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
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-theme-global")));

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("io.github.mak1a4.rice", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
