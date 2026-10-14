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

#include <optional>

using namespace KWin;

// read by kde-rice's Breeze, on the window its decoration belongs to
static const char s_noTitleBar[] = "kdeRiceNoTitleBar";
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
        return BorderRadius(m_radius);
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
