// Rounds the corners of application windows with KWin's own border radius
// (the one a decoration gives its window), also where nobody asks for it:
// maximized windows, and windows that draw themselves with square corners.
//
// KWin clips a window's surface to the radius, not its decoration: a title
// bar KWin draws keeps the corners the decoration gives it.
//
// With kde-rice's Breeze as the decoration (kwin/decoration/breeze), a window that draws
// its own title bar and has square corners is given that decoration without a
// title bar instead: the corners, and Breeze's shadow and outline in place of
// the window's own shadow, which the corners would cut off.
//
// Windows tiled together have no corners where they meet, and no shadow
// there either, so that they look like one: this works out which sides of
// a tiled window lie against another tiled window (same screen, same
// desktop, edge on edge) and squares those corners. kde-rice's Breeze reads
// the same sides from the window (`kdeRiceJoinedEdges`) for its title bar,
// outline and shadow.
//
// The radius is `Radius` in the group `KDERiceRoundCorners` of kwinrc
// (logical pixels, 0 for none), read again when KWin is told to reconfigure.
// Not there, it is Breeze's own, 5. kde-rice's Breeze (kwin/decoration/breeze) reads
// the same key, so decorated windows and the others have the same corners.

#include "main.h"
#include "plugin.h"
#include "scene/borderradius.h"
#include "wayland/surface.h"
#include "window.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KDecoration3/Decoration>
#include <KSharedConfig>

#include <QPointer>
#include <QTimer>

#include <cmath>
#include <optional>

using namespace KWin;

// read by kde-rice's Breeze, on the window its decoration belongs to
static const char s_noTitleBar[] = "kdeRiceNoTitleBar";
// the sides joined to another window, as kde-rice's Breeze reads them
static const char s_joined[] = "kdeRiceJoinedEdges";
enum JoinedEdge {
    JoinedLeft = 1,
    JoinedTop = 2,
    JoinedRight = 4,
    JoinedBottom = 8,
};
static const QString s_ownDecoration = QStringLiteral("io.github.mak1a4.kde-rice.breeze");

class RoundCorners : public Plugin
{
    Q_OBJECT

public:
    RoundCorners()
    {
        readConfig();
        connect(workspace(), &Workspace::configChanged, this, [this] {
            readConfig();
            applyAll();
        });
        connect(workspace(), &Workspace::windowAdded, this, &RoundCorners::track);
        // which windows are joined is looked at once the moves are done
        m_join.setSingleShot(true);
        m_join.setInterval(0);
        connect(&m_join, &QTimer::timeout, this, &RoundCorners::joinAll);
        connect(workspace(), &Workspace::windowRemoved, &m_join, qOverload<>(&QTimer::start));
        connect(workspace(), &Workspace::currentDesktopChanged, &m_join, qOverload<>(&QTimer::start));
        connect(workspace(), &Workspace::currentActivityChanged, &m_join, qOverload<>(&QTimer::start));
        const auto windows = workspace()->windows();
        for (Window *window : windows) {
            track(window);
        }
    }

    ~RoundCorners() override
    {
        const auto windows = workspace()->windows();
        for (Window *window : windows) {
            if (wanted(window)) {
                disconnect(window, nullptr, this, nullptr);
                setFramed(window, false);
                window->setProperty(s_joined, QVariant());
                window->setBorderRadius(own(window));
            }
        }
    }

private:
    // Applications' windows only. A layer surface whose namespace KWin does
    // not know is a normal window too, so the type alone does not say it.
    static bool wanted(const Window *window)
    {
        if (!window->inherits("KWin::XdgToplevelWindow") && !window->inherits("KWin::X11Window")) {
            return false;
        }
        return window->isClient() && !window->isDeleted() && (window->isNormalWindow() || window->isDialog());
    }

    // what KWin gives the window without this plugin
    static BorderRadius own(const Window *window)
    {
        return window->isDecorated() ? BorderRadius::from(window->decoration()->borderRadius()) : BorderRadius();
    }

    // A window that draws its own shadow has it in its buffer, outside the
    // frame, and the radius would cut it off along with the corners.
    static bool drawsOutsideFrame(const Window *window)
    {
        const RectF frame = window->frameGeometry();
        const RectF buffer = window->bufferGeometry();
        return buffer.left() < frame.left() - 0.5 || buffer.top() < frame.top() - 0.5
            || buffer.right() > frame.right() + 0.5 || buffer.bottom() > frame.bottom() + 0.5;
    }

    // Whether the window fills its corners. It says so itself, with the part
    // of its surface it calls opaque; nothing, if it calls no part so.
    static std::optional<bool> squareCorners(const Window *window)
    {
        const SurfaceInterface *surface = window->surface();
        if (!surface) {
            return std::nullopt;
        }
        const RegionF opaque = surface->opaque();
        const RectF buffer = window->bufferGeometry();
        const RectF client = window->clientGeometry().translated(-buffer.x(), -buffer.y());
        if (opaque.isEmpty() || client.width() < 8 || client.height() < 8) {
            return std::nullopt;
        }
        // Looked at a little inside each corner: Chromium leaves a pixel
        // around its window out of the region, and a window with corners of
        // its own leaves out more than that.
        const qreal inset = 2;
        const QPointF corners[] = {
            QPointF(client.left() + inset, client.top() + inset),
            QPointF(client.right() - inset - 1, client.top() + inset),
            QPointF(client.left() + inset, client.bottom() - inset - 1),
            QPointF(client.right() - inset - 1, client.bottom() - inset - 1),
        };
        for (const QPointF &corner : corners) {
            if (opaque.contains(RectF(corner, QSizeF(1, 1)))) {
                return true;
            }
        }
        return false;
    }

    static bool framed(const Window *window)
    {
        return window->property(s_noTitleBar).toBool();
    }

    static void setFramed(Window *window, bool framed)
    {
        if (RoundCorners::framed(window) != framed) {
            window->setProperty(s_noTitleBar, framed);
            window->setDecorationPolicy(framed ? DecorationPolicy::Server : DecorationPolicy::PreferredByClient);
        }
    }

    // The decoration without a title bar, for a window with a title bar of
    // its own and square corners; taken away again when it has corners (or
    // the decoration is no longer kde-rice's).
    void frame(Window *window) const
    {
        if (!window->inherits("KWin::XdgToplevelWindow")) {
            return;
        }
        if (!m_ownDecoration) {
            setFramed(window, false);
            return;
        }
        if (window->isFullScreen()) {
            return;
        }
        const bool isFramed = framed(window);
        // the application asked for KWin's title bar
        if (!isFramed && (window->decoration() || window->nextDecoration())) {
            return;
        }
        const std::optional<bool> square = squareCorners(window);
        if (square.has_value()) {
            setFramed(window, *square);
        }
    }

    BorderRadius radiusFor(const Window *window) const
    {
        const BorderRadius given = own(window);
        if (!given.isNull() || window->isFullScreen() || drawsOutsideFrame(window)) {
            return given;
        }
        // none where the window is joined to another
        const int joined = window->property(s_joined).toInt();
        const auto corner = [this, joined](int one, int other) {
            return joined & (one | other) ? 0.0 : m_radius;
        };
        return BorderRadius(corner(JoinedLeft, JoinedTop), corner(JoinedRight, JoinedTop), corner(JoinedRight, JoinedBottom), corner(JoinedLeft, JoinedBottom));
    }

    // tiled (quick tiling or a tile of KWin's layout) and to be seen now
    static bool joinable(const Window *window)
    {
        return wanted(window) && !window->isMinimized() && !window->isFullScreen() && window->isOnCurrentDesktop() && window->isOnCurrentActivity()
            && (window->quickTileMode() != QuickTileMode(QuickTileFlag::None) || window->tile());
    }

    // The sides on which tiled windows lie against each other, edge on
    // edge (within a pixel) and along more than a pixel, on every window.
    void joinAll()
    {
        const auto windows = workspace()->windows();
        QList<Window *> tiled;
        for (Window *window : windows) {
            if (joinable(window)) {
                tiled += window;
            }
        }
        const auto near = [](qreal a, qreal b) {
            return std::abs(a - b) <= 1;
        };
        const auto along = [](qreal from, qreal to, qreal otherFrom, qreal otherTo) {
            return std::min(to, otherTo) - std::max(from, otherFrom) > 1;
        };
        for (Window *window : windows) {
            if (!wanted(window)) {
                continue;
            }
            int joined = 0;
            if (tiled.contains(window)) {
                const RectF g = window->frameGeometry();
                for (const Window *other : std::as_const(tiled)) {
                    if (other == window || other->output() != window->output()) {
                        continue;
                    }
                    const RectF o = other->frameGeometry();
                    if (along(g.top(), g.bottom(), o.top(), o.bottom())) {
                        joined |= near(g.right(), o.left()) ? JoinedRight : 0;
                        joined |= near(g.left(), o.right()) ? JoinedLeft : 0;
                    }
                    if (along(g.left(), g.right(), o.left(), o.right())) {
                        joined |= near(g.bottom(), o.top()) ? JoinedBottom : 0;
                        joined |= near(g.top(), o.bottom()) ? JoinedTop : 0;
                    }
                }
            }
            if (window->property(s_joined).toInt() != joined) {
                window->setProperty(s_joined, joined);
                apply(window);
            }
        }
    }

    void readConfig()
    {
        m_radius = std::max(0.0, kwinApp()->config()->group(QStringLiteral("KDERiceRoundCorners")).readEntry("Radius", 5.0));
        m_ownDecoration = kwinApp()->config()->group(QStringLiteral("org.kde.kdecoration2")).readEntry("library") == s_ownDecoration;
    }

    void track(Window *window)
    {
        if (!wanted(window)) {
            return;
        }
        const auto update = [this, window] {
            apply(window);
        };
        // KWin sets the radius again whenever the decoration changes
        connect(window, &Window::borderRadiusChanged, this, update);
        connect(window, &Window::fullScreenChanged, this, update);
        connect(window, &Window::frameGeometryChanged, this, update);
        connect(window, &Window::bufferGeometryChanged, this, update);
        // and which windows are joined, looked at again
        const auto join = [this] {
            m_join.start();
        };
        connect(window, &Window::frameGeometryChanged, this, join);
        connect(window, &Window::minimizedChanged, this, join);
        connect(window, &Window::desktopsChanged, this, join);
        connect(window, &Window::quickTileModeChanged, this, join);
        connect(window, &Window::fullScreenChanged, this, join);
        connect(window, &Window::tileChanged, this, join);
        m_join.start();
        if (const SurfaceInterface *surface = window->surface()) {
            connect(surface, &SurfaceInterface::opaqueChanged, this, update);
        }
        apply(window);
    }

    void apply(Window *window)
    {
        if (window->isDeleted()) {
            return;
        }
        // Looked at once what led here is done with: a window's region and
        // its size change one after the other.
        QTimer::singleShot(0, this, [this, window = QPointer<Window>(window)] {
            if (window && !window->isDeleted()) {
                frame(window);
            }
        });
        const BorderRadius radius = radiusFor(window);
        if (window->borderRadius() != radius) {
            window->setBorderRadius(radius);
        }
    }

    void applyAll()
    {
        const auto windows = workspace()->windows();
        for (Window *window : windows) {
            if (wanted(window)) {
                apply(window);
            }
        }
    }

    qreal m_radius = 5;
    bool m_ownDecoration = false;
    QTimer m_join;
};

class RoundCornersFactory : public PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    std::unique_ptr<Plugin> create() const override
    {
        return std::make_unique<RoundCorners>();
    }
};

#include "main.moc"
