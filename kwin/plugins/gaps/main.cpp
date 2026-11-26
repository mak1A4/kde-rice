// Popups, tooltips and notifications across kde-rice's gaps.
//
// kde-rice-gaps keeps a strip free along each edge of the screen, by
// exclusive zones, so that maximized and tiled windows keep a gap to the
// edges and to the panel. The rest is kept inside that area as well, where
// it has no business keeping the gap.
//
// KWin keeps popups (menus, Wayland's xdg_popup) inside it: this places
// every popup again as KWin does, by its positioner, in that area with the
// strips of kde-rice-gaps given back; what a popup cannot cover stays what
// the panel and other docks keep for themselves.
//
// Plasma's tooltips are no popups but windows Plasma places itself, by that
// area too, so a tooltip of the panel ended a gap off it: one by a panel is
// put against the panel's edge.
//
// Plasma's notifications keep a spacing of their own inside that area, so
// with the gaps they stood twice as far off the panel and the screen's edge
// as windows do: they are moved out by that spacing, all of them by the
// same, so that the corner of the stack is where a maximized window's is;
// by a floating panel, the stack is then brought to it, as far off it as
// the panel floats off the screen's edge (by a docked one it stays a gap off
// the panel, as a maximized window does).

#include "plugin.h"
#include "wayland/surface.h"
#include "wayland/xdgshell.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <QHash>
#include <QPointer>

#include <cmath>
#include <optional>

using namespace KWin;

class Gaps : public Plugin
{
    Q_OBJECT

public:
    Gaps()
    {
        // KWin's xdg-shell, which says when a popup is made and keeps its
        // positioner (KWin's popup windows do not hand it out)
        auto *shell = waylandServer() ? waylandServer()->findChild<XdgShellInterface *>() : nullptr;
        if (!shell) {
            return;
        }
        connect(shell, &XdgShellInterface::popupCreated, this, [this](XdgPopupInterface *popup) {
            SurfaceInterface *surface = popup->surface();
            m_popups.insert(surface, popup);
            connect(popup, &QObject::destroyed, this, [this, surface] {
                m_popups.remove(surface);
            });
        });
        connect(workspace(), &Workspace::windowAdded, this, &Gaps::track);
    }

private:
    void track(Window *window)
    {
        if (window->isNotification() || window->isCriticalNotification()) {
            m_notifications.insert(window, window->frameGeometry());
            connect(window, &Window::frameGeometryChanged, this, [this, window] {
                if (!m_placing) {
                    // where Plasma has put it
                    m_notifications.insert(window, window->frameGeometry());
                    placeNotifications(window->output());
                }
            });
            connect(window, &Window::closed, this, [this, window] {
                m_notifications.remove(window);
            });
            placeNotifications(window->output());
            return;
        }
        if (window->isTooltip()) {
            // Plasma moves it at every change of its contents
            connect(window, &Window::frameGeometryChanged, this, [this, window] {
                snapToPanel(window);
            });
            snapToPanel(window);
            return;
        }
        if (!m_popups.contains(window->surface())) {
            return;
        }
        // KWin places it again whenever it or its parent changes
        connect(window, &Window::frameGeometryChanged, this, [this, window] {
            place(window);
        });
        place(window);
    }

    static bool near(qreal a, qreal b)
    {
        return std::abs(a - b) < 1;
    }

    // what the strips of kde-rice-gaps on an output keep free
    static QList<StrutRect> strips(const LogicalOutput *output)
    {
        QList<StrutRect> found;
        const auto windows = workspace()->windows();
        for (const Window *strip : windows) {
            if (!strip->isDeleted() && strip->output() == output && strip->resourceClass() == QLatin1String("kde-rice-gaps")) {
                found += strip->strutRects();
            }
        }
        return found;
    }

    // The area KWin keeps popups in, without the strips of kde-rice-gaps
    // that lie along its sides.
    static RectF withoutGaps(RectF area, const LogicalOutput *output)
    {
        const auto struts = strips(output);
        for (const StrutRect &strut : struts) {
            switch (strut.area()) {
            case StrutAreaTop:
                if (near(strut.bottom(), area.top())) {
                    area.setTop(strut.top());
                }
                break;
            case StrutAreaBottom:
                if (near(strut.top(), area.bottom())) {
                    area.setBottom(strut.bottom());
                }
                break;
            case StrutAreaLeft:
                if (near(strut.right(), area.left())) {
                    area.setLeft(strut.left());
                }
                break;
            case StrutAreaRight:
                if (near(strut.left(), area.right())) {
                    area.setRight(strut.right());
                }
                break;
            default:
                break;
            }
        }
        return area;
    }

    // the panels on an output, without the strips of kde-rice-gaps
    static QList<Window *> panelsOn(const LogicalOutput *output)
    {
        QList<Window *> found;
        const auto windows = workspace()->windows();
        for (Window *panel : windows) {
            if (panel->isDock() && !panel->isDeleted() && panel->output() == output && panel->surface()
                && panel->resourceClass() != QLatin1String("kde-rice-gaps")) {
                found += panel;
            }
        }
        return found;
    }

    // what a panel shows of its window, which Plasma gives as the input
    // region of it (a floating panel's window is larger, its margin empty)
    static RectF shownOf(const Window *panel)
    {
        const RegionF input = panel->surface()->input();
        const RectF frame = panel->frameGeometry();
        return input.isEmpty() ? frame : input.boundingRect().translated(panel->bufferGeometry().topLeft()) & frame;
    }

    // whether a panel stands off every screen edge it lies along (a docked
    // panel touches one)
    static bool floats(const RectF &shown, const RectF &screen)
    {
        return !near(shown.left(), screen.left()) && !near(shown.top(), screen.top()) && !near(shown.right(), screen.right())
            && !near(shown.bottom(), screen.bottom());
    }

    // A tooltip by a panel, against the panel's edge. Plasma places each
    // kind of tooltip its own way, a text a gap's breadth off the panel, a
    // window's picture less (and one window does for all of them, moved and
    // then resized): each is put against the edge the panel shows, which
    // Plasma gives as the input region of its window.
    void snapToPanel(Window *window)
    {
        if (m_placing || window->isDeleted()) {
            return;
        }
        const auto struts = strips(window->output());
        if (struts.isEmpty()) {
            return;
        }
        // as far as a tooltip can be from it: Plasma's offset with the gaps
        qreal reach = 16;
        for (const StrutRect &strut : struts) {
            reach = std::max(reach, std::min(strut.width(), strut.height()) + 16);
        }
        const RectF geometry = window->frameGeometry();
        const auto panels = panelsOn(window->output());
        for (const Window *panel : panels) {
            const RectF frame = panel->frameGeometry();
            const RectF shown = shownOf(panel);
            const RectF screen = workspace()->clientArea(ScreenArea, panel);
            const bool horizontal = frame.width() >= frame.height();
            QPointF offset;
            if (horizontal && std::min(geometry.right(), shown.right()) > std::max(geometry.left(), shown.left())) {
                if (frame.center().y() > screen.center().y() && std::abs(shown.top() - geometry.bottom()) <= reach) {
                    offset.setY(shown.top() - geometry.bottom());
                } else if (frame.center().y() <= screen.center().y() && std::abs(geometry.top() - shown.bottom()) <= reach) {
                    offset.setY(shown.bottom() - geometry.top());
                }
            } else if (!horizontal && std::min(geometry.bottom(), shown.bottom()) > std::max(geometry.top(), shown.top())) {
                if (frame.center().x() > screen.center().x() && std::abs(shown.left() - geometry.right()) <= reach) {
                    offset.setX(shown.left() - geometry.right());
                } else if (frame.center().x() <= screen.center().x() && std::abs(geometry.left() - shown.right()) <= reach) {
                    offset.setX(shown.right() - geometry.left());
                }
            }
            if (std::abs(offset.x()) >= 0.5 || std::abs(offset.y()) >= 0.5) {
                m_placing = true;
                window->move(window->pos() + offset);
                m_placing = false;
                return;
            }
        }
    }

    // Plasma's spacing, as far as the stack's nearest side lies inside
    // the area (where it lies near one), and every notification moved by it
    void placeNotifications(const LogicalOutput *output)
    {
        if (strips(output).isEmpty()) {
            return;
        }
        // Plasma's spacing is less than this; a stack farther off is not by that side
        constexpr qreal reach = 48;
        std::optional<RectF> area, screen;
        RectF stack;
        qreal left = reach + 1, top = reach + 1, right = reach + 1, bottom = reach + 1;
        for (auto it = m_notifications.cbegin(); it != m_notifications.cend(); ++it) {
            if (it.key()->isDeleted() || it.key()->output() != output) {
                continue;
            }
            if (!area) {
                area = workspace()->clientArea(MaximizeArea, it.key());
                screen = workspace()->clientArea(ScreenArea, it.key());
            }
            const RectF &placed = it.value();
            stack = stack.united(placed);
            left = std::min(left, placed.left() - area->left());
            top = std::min(top, placed.top() - area->top());
            right = std::min(right, area->right() - placed.right());
            bottom = std::min(bottom, area->bottom() - placed.bottom());
        }
        if (!area) {
            return;
        }
        const auto by = [reach](qreal distance) {
            return distance > 0 && distance <= reach ? distance : 0.0;
        };
        QPointF offset(by(right) - by(left), by(bottom) - by(top));
        // then to a panel the stack lies by, as far off it as the panel
        // floats off the screen's edge (where the stack is a gap off it); a
        // docked panel's stack stays where it is, a gap off the panel as off
        // the screen's side, not against the panel
        stack.translate(offset);
        const auto panels = panelsOn(output);
        for (const Window *panel : panels) {
            const RectF frame = panel->frameGeometry();
            const RectF shown = shownOf(panel);
            if (!floats(shown, *screen)) {
                continue;
            }
            if (frame.width() >= frame.height()) {
                if (std::min(stack.right(), shown.right()) <= std::max(stack.left(), shown.left())) {
                    continue;
                }
                if (frame.center().y() > screen->center().y() && shown.top() >= stack.bottom() && shown.top() - stack.bottom() <= reach) {
                    offset.ry() += shown.top() - (screen->bottom() - shown.bottom()) - stack.bottom();
                } else if (frame.center().y() <= screen->center().y() && stack.top() >= shown.bottom() && stack.top() - shown.bottom() <= reach) {
                    offset.ry() += shown.bottom() + (shown.top() - screen->top()) - stack.top();
                }
            } else {
                if (std::min(stack.bottom(), shown.bottom()) <= std::max(stack.top(), shown.top())) {
                    continue;
                }
                if (frame.center().x() > screen->center().x() && shown.left() >= stack.right() && shown.left() - stack.right() <= reach) {
                    offset.rx() += shown.left() - (screen->right() - shown.right()) - stack.right();
                } else if (frame.center().x() <= screen->center().x() && stack.left() >= shown.right() && stack.left() - shown.right() <= reach) {
                    offset.rx() += shown.right() + (shown.left() - screen->left()) - stack.left();
                }
            }
        }
        m_placing = true;
        for (auto it = m_notifications.cbegin(); it != m_notifications.cend(); ++it) {
            if (!it.key()->isDeleted() && it.key()->output() == output && it.key()->pos() != it.value().topLeft() + offset) {
                it.key()->move(it.value().topLeft() + offset);
            }
        }
        m_placing = false;
    }

    void place(Window *window)
    {
        const QPointer<XdgPopupInterface> popup = m_popups.value(window->surface());
        const Window *parent = window->transientFor();
        if (m_placing || !popup || !parent || parent->isFullScreen() || window->isDeleted()) {
            return;
        }
        const RectF area = workspace()->clientArea(PlacementArea, parent);
        const RectF wider = withoutGaps(area, parent->output());
        if (wider == area) {
            return;
        }
        // as XdgPopupWindow::updateRelativePlacement, in the wider area
        const QPointF parentPosition = parent->nextFramePosToClientPos(parent->pos());
        const QPointF position = popup->positioner().placement(wider.translated(-parentPosition)).translated(parentPosition).topLeft();
        if (position != window->pos()) {
            m_placing = true;
            window->move(position);
            m_placing = false;
        }
    }

    QHash<SurfaceInterface *, QPointer<XdgPopupInterface>> m_popups;
    bool m_placing = false;
    // the notifications, each where Plasma has put it
    QHash<Window *, RectF> m_notifications;
};

class GapsFactory : public PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    std::unique_ptr<Plugin> create() const override
    {
        return std::make_unique<Gaps>();
    }
};

#include "main.moc"
