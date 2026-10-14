// kde-rice-gaps: room kept free along every edge of the screen, so that
// maximized and tiled windows keep a gap to the edges and to the panel.
//
// KWin has no gaps of its own. What it has is the area left free by docks:
// maximized windows fill it, quick tiling divides it. So this reserves a
// strip along each edge, through four invisible layer-shell surfaces that
// take no input and only have an exclusive zone, and KWin itself fits the
// windows into what is left; nothing is moved by a script.
//
// Along a panel's edge KWin adds the strip's room to the panel's own, so
// there the strip is the panel's float margin and the gap. Plasma
// takes a floating panel down to the edge as soon as a window overlaps the
// panel's window (which is the panel and its float margin), so a maximized
// window ending a gap above the panel keeps it floating.
//
// Settings: `[Gaps] Enabled` and `Size` (logical pixels, 18) in kde-ricerc,
// read again when they change. Off, the program quits; the hub starts it
// again, and so does the autostart entry at login.

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KDBusService>
#include <KSharedConfig>

#include <LayerShellQt/Window>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRasterWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>

#include <memory>
#include <vector>

using namespace Qt::StringLiterals;

namespace
{

// How far a floating panel stands off the edge: Plasma's floating margin
// with Breeze's panel (its window is the panel and this on both sides).
constexpr int s_panelFloat = 8;

// a transparent pixel, which is all a surface needs to be mapped
class Strip : public QRasterWindow
{
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(QRect(QPoint(), size()), Qt::transparent);
    }
};

struct Panel {
    QRect screen;
    QString location;
    bool floating = false;
    bool reserves = true;
};

// Plasma's panels, from its scripting interface
std::vector<Panel> panels()
{
    auto message = QDBusMessage::createMethodCall(u"org.kde.plasmashell"_s, u"/PlasmaShell"_s, u"org.kde.PlasmaShell"_s, u"evaluateScript"_s);
    message << uR"(print(JSON.stringify(panels().map(p => { const g = screenGeometry(p.screen);
        return { x: g.x, y: g.y, width: g.width, height: g.height, location: p.location,
                 floating: p.floating, hiding: p.hiding }; }))))"_s;
    const QDBusReply<QString> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 5000);
    std::vector<Panel> found;
    if (!reply.isValid()) {
        return found;
    }
    const QJsonArray list = QJsonDocument::fromJson(reply.value().toUtf8()).array();
    for (const QJsonValue &value : list) {
        const QJsonObject object = value.toObject();
        found.push_back({
            QRect(object[u"x"_s].toInt(), object[u"y"_s].toInt(), object[u"width"_s].toInt(), object[u"height"_s].toInt()),
            object[u"location"_s].toString(),
            object[u"floating"_s].toBool(),
            // an auto-hidden panel or one windows may cover keeps no room
            object[u"hiding"_s].toString() == "none"_L1,
        });
    }
    return found;
}

class Gaps : public QObject
{
public:
    explicit Gaps(bool force)
        : m_force(force)
    {
        m_config = KSharedConfig::openConfig(u"kde-ricerc"_s);
        m_watcher = KConfigWatcher::create(m_config);
        connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
            if (group.name() == "Gaps"_L1) {
                m_settle.start();
            }
        });
        // Plasma keeps its panels here; it writes the files, it does not
        // announce changes
        const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        m_files.addPaths({config + "/plasmashellrc"_L1, config + "/plasma-org.kde.plasma.desktop-appletsrc"_L1, config + "/kde-ricerc"_L1});
        connect(&m_files, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
            // written by replacing: watched again
            if (!m_files.files().contains(path)) {
                m_files.addPath(path);
            }
            m_settle.start();
        });
        connect(qApp, &QGuiApplication::screenAdded, &m_settle, qOverload<>(&QTimer::start));
        connect(qApp, &QGuiApplication::screenRemoved, &m_settle, qOverload<>(&QTimer::start));
        m_settle.setSingleShot(true);
        m_settle.setInterval(300);
        connect(&m_settle, &QTimer::timeout, this, &Gaps::update);
        update();
    }

private:
    void update()
    {
        m_config->reparseConfiguration();
        const KConfigGroup group = m_config->group(u"Gaps"_s);
        if (!m_force && !group.readEntry("Enabled", false)) {
            QCoreApplication::quit();
            return;
        }
        const int gap = std::max(0, group.readEntry("Size", 18));
        const auto found = panels();

        m_strips.clear();
        const auto screens = QGuiApplication::screens();
        for (QScreen *screen : screens) {
            int top = gap, bottom = gap, left = gap, right = gap;
            for (const Panel &panel : found) {
                if (panel.screen != screen->geometry() || !panel.reserves) {
                    continue;
                }
                // KWin adds this to the room the panel keeps itself: what
                // lies beyond it is the float margin, then the gap
                const int reach = (panel.floating ? s_panelFloat : 0) + gap;
                if (panel.location == "top"_L1) {
                    top = std::max(top, reach);
                } else if (panel.location == "bottom"_L1) {
                    bottom = std::max(bottom, reach);
                } else if (panel.location == "left"_L1) {
                    left = std::max(left, reach);
                } else if (panel.location == "right"_L1) {
                    right = std::max(right, reach);
                }
            }
            using A = LayerShellQt::Window::Anchor;
            add(screen, LayerShellQt::Window::Anchors(A::AnchorTop | A::AnchorLeft | A::AnchorRight), top);
            add(screen, LayerShellQt::Window::Anchors(A::AnchorBottom | A::AnchorLeft | A::AnchorRight), bottom);
            add(screen, LayerShellQt::Window::Anchors(A::AnchorLeft | A::AnchorTop | A::AnchorBottom), left);
            add(screen, LayerShellQt::Window::Anchors(A::AnchorRight | A::AnchorTop | A::AnchorBottom), right);
        }
    }

    void add(QScreen *screen, LayerShellQt::Window::Anchors anchors, int zone)
    {
        auto strip = std::make_unique<Strip>();
        QSurfaceFormat format;
        format.setAlphaBufferSize(8);
        strip->setFormat(format);
        strip->setFlags(Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
        strip->setScreen(screen);
        strip->resize(1, 1);
        auto *layer = LayerShellQt::Window::get(strip.get());
        // "dock": KWin takes it for one, and it is no window to list
        layer->setScope(u"dock"_s);
        layer->setLayer(LayerShellQt::Window::LayerBackground);
        layer->setAnchors(anchors);
        layer->setExclusiveZone(zone);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        layer->setActivateOnShow(false);
        layer->setWantsToBeOnActiveScreen(false);
        layer->setScreen(screen);
        strip->show();
        m_strips.push_back(std::move(strip));
    }

    bool m_force;
    KSharedConfigPtr m_config;
    KConfigWatcher::Ptr m_watcher;
    QFileSystemWatcher m_files;
    QTimer m_settle;
    std::vector<std::unique_ptr<Strip>> m_strips;
};

}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setApplicationName(u"kde-rice-gaps"_s);
    QCoreApplication::setOrganizationDomain(u"mak1a4.github.io"_s);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({u"force"_s, u"Keep the gaps whatever kde-ricerc says (for trying them out)."_s});
    parser.process(app);

    // one at a time: started again, nothing happens
    KDBusService service(KDBusService::Unique);

    Gaps gaps(parser.isSet(u"force"_s));
    return app.exec();
}
