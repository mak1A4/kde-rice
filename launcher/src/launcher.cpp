#include "launcher.h"
#include "searchmodel.h"

#include <KGlobalAccel>
#include <KLocalizedString>
#include <KWaylandExtras>
#include <LayerShellQt/Window>

#include <QAction>
#include <QCursor>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QProcess>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

#include <memory>
#include <utility>

using namespace Qt::StringLiterals;

Q_STATIC_LOGGING_CATEGORY(LAUNCHER, "kde-rice.launcher", QtInfoMsg)

const QKeySequence Launcher::defaultShortcut(Qt::META | Qt::Key_Space);

Launcher::Launcher(SearchModel *results, QObject *parent)
    : QObject(parent)
    , m_results(results)
{
    // what was run is started; the popup has done its part
    connect(m_results, &SearchModel::finished, this, &Launcher::close);
}

Launcher::~Launcher() = default;

void Launcher::setWindow(QQuickWindow *window)
{
    m_window = window;

    // Layer-shell has to be set up before the window is first shown: it
    // decides what kind of surface the window gets.
    if (QGuiApplication::platformName().startsWith(u"wayland")) {
        auto *layer = LayerShellQt::Window::get(window);
        layer->setScope(u"kde-rice-launcher"_s);
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        // the whole screen, transparent, so that a click beside the box can
        // close it; -1: nothing else moves aside for it
        layer->setAnchors({LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft
                           | LayerShellQt::Window::AnchorRight});
        layer->setExclusiveZone(-1);
        // all keys while shown, as KRunner's window has them; hidden, the
        // surface is gone and with it the claim
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
        // on the screen being worked on, chosen by KWin at each opening
        layer->setWantsToBeOnActiveScreen(true);
        m_layerShell = true;
    } else {
        qCWarning(LAUNCHER) << "Not on Wayland (platform" << QGuiApplication::platformName()
                            << "): no layer-shell, the launcher is an ordinary frameless window";
        window->setFlags(window->flags() | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    }

    // KWin may close the surface itself, when its screen goes away
    connect(window, &QWindow::visibleChanged, this, [this](bool visible) {
        if (!visible) {
            close();
        }
    });
    // Losing the keyboard (the screen locked, another surface took it) closes
    // the popup, as a click beside it does. Only once it has had it: the
    // window is not active yet in the moment it is shown.
    connect(window, &QWindow::activeChanged, this, [this] {
        if (m_window->isActive()) {
            m_wasActive = true;
        } else if (m_wasActive) {
            close();
        }
    });
}

bool Launcher::isShown() const
{
    return m_shown;
}

void Launcher::registerShortcut()
{
    m_shortcutAction = new QAction(i18nc("@action global shortcut", "Show kde-rice launcher"), this);
    // KGlobalAccel knows an action by its object name within the component.
    // The component is named after the desktop file, so that System Settings
    // > Shortcuts lists it with that name and icon.
    m_shortcutAction->setObjectName(u"show"_s);
    m_shortcutAction->setProperty("componentName", QStringLiteral(KDERICE_ID ".launcher"));
    m_shortcutAction->setProperty("componentDisplayName", i18nc("@title shortcut component", "kde-rice Launcher"));
    connect(m_shortcutAction, &QAction::triggered, this, &Launcher::toggle);

    // The default, and the shortcut itself unless one was chosen before. If
    // another component has Meta+Space (KRunner, out of the box) this one is
    // left without; --take-shortcut takes it.
    KGlobalAccel::self()->setDefaultShortcut(m_shortcutAction, {defaultShortcut});
    KGlobalAccel::self()->setShortcut(m_shortcutAction, {defaultShortcut});
}

void Launcher::takeShortcut()
{
    if (!m_shortcutAction) {
        return;
    }
    KGlobalAccel::stealShortcutSystemwide(defaultShortcut);
    KGlobalAccel::self()->setShortcut(m_shortcutAction, {defaultShortcut}, KGlobalAccel::NoAutoloading);
    qCInfo(LAUNCHER) << "Took" << defaultShortcut.toString() << "; the launcher's shortcut is now"
                     << KGlobalAccel::self()->shortcut(m_shortcutAction);
}

void Launcher::startProgram(const QString &program, const QStringList &arguments, const QString &appId)
{
    // once: with the token, or without it if none came in time
    auto started = std::make_shared<bool>(false);
    auto start = [program, arguments, started](const QString &token) {
        if (std::exchange(*started, true)) {
            return;
        }
        QProcess process;
        process.setProgram(program);
        process.setArguments(arguments);
        if (!token.isEmpty()) {
            auto environment = QProcessEnvironment::systemEnvironment();
            environment.insert(u"XDG_ACTIVATION_TOKEN"_s, token);
            process.setProcessEnvironment(environment);
        }
        if (!process.startDetached()) {
            qCWarning(LAUNCHER) << "Could not start" << program << arguments;
        }
    };
    if (!m_layerShell || !m_window || !m_window->isVisible()) {
        start(QString());
        return;
    }
    KWaylandExtras::xdgActivationToken(m_window, appId).then(this, start);
    QTimer::singleShot(500, this, [start] {
        start(QString());
    });
}

void Launcher::toggle()
{
    if (m_shown) {
        close();
    } else {
        open();
    }
}

void Launcher::open()
{
    search(QString());
}

void Launcher::search(const QString &text)
{
    if (!m_window) {
        return;
    }
    if (!m_layerShell) {
        // cover the screen with the pointer, as the layer surface covers the
        // active one
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen) {
            screen = QGuiApplication::primaryScreen();
        }
        m_window->setScreen(screen);
        m_window->setGeometry(screen->geometry());
    }
    // the query first, so that the first frame shown is already right
    Q_EMIT opened(text);
    if (!m_shown) {
        m_wasActive = false;
        setShown(true);
        m_window->show();
    }
    m_window->requestActivate();
}

void Launcher::close()
{
    if (!m_shown) {
        return;
    }
    setShown(false);
    if (m_window) {
        m_window->hide();
    }
    m_results->endSession();
}

void Launcher::setShown(bool shown)
{
    if (shown != m_shown) {
        m_shown = shown;
        Q_EMIT shownChanged();
    }
}
