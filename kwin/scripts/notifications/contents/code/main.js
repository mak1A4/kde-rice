// Notification popups lined up with a floating panel.
//
// Plasma puts its popups two grid units (36 px) inside the area the panels
// leave free. A floating panel is drawn smaller than the strip it reserves,
// so the popups line up with nothing on screen: here 28 px short of the
// panel's right edge and 44 px above it. This moves every popup by one and
// the same offset, so that the stack's corner sits on the panel's corner,
// the popups' edge in line with the panel's and the gap above the panel the
// one it floats by. Popups stay where Plasma stacks them, only shifted;
// Plasma does not read a popup's position back, so a move here sticks until
// it places the popup again, which is answered with the same shift.
//
// Only for a panel along the bottom edge, with the popups in the
// bottom-right corner (Plasma's own choice for a tray on the right); any
// other popup is left alone.

// where each popup was put by this script, to tell its own moves from Plasma's
const placed = new Map();

// The bottom panel of plasmashell on the window's screen, and how far it
// floats: half of what its window has more than the strip it reserves.
function panelFor(window) {
    const screen = window.output;
    const area = workspace.clientArea(KWin.MaximizeArea, screen, workspace.currentDesktop);
    const full = screen.geometry;
    const reserved = full.y + full.height - (area.y + area.height);
    for (const candidate of workspace.stackingOrder) {
        if (!candidate.dock || candidate.resourceClass !== "plasmashell" || candidate.output !== screen) {
            continue;
        }
        const g = candidate.frameGeometry;
        // along the bottom edge, about as wide as the screen
        if (Math.abs(g.y + g.height - (full.y + full.height)) > 1 || g.width < full.width / 2 || reserved <= 0) {
            continue;
        }
        const float = Math.max(0, Math.round((g.height - reserved) / 2));
        return {
            right: g.x + g.width - float,
            top: g.y + float,
            float: float,
            area: area,
        };
    }
    return null;
}

// Where Plasma put a popup: its own position, or, if this script moved it
// last, where it was before that.
function plasmaPosition(window) {
    const at = window.frameGeometry;
    const mine = placed.get(window.internalId.toString());
    if (mine && Math.abs(mine.x - at.x) < 0.5 && Math.abs(mine.y - at.y) < 0.5) {
        return { x: mine.fromX, y: mine.fromY };
    }
    return { x: at.x, y: at.y };
}

function isPopup(window) {
    return window.notification || window.criticalNotification;
}

// All popups shifted by one offset: from the corner of Plasma's stack (its
// right edge, the lowest popup's bottom edge, as Plasma placed them) to the
// panel's corner, one float gap above the panel.
function placeAll() {
    const popups = workspace.stackingOrder.filter(isPopup);
    if (popups.length === 0) {
        return;
    }
    const panel = panelFor(popups[0]);
    if (!panel) {
        return;
    }
    const middleX = panel.area.x + panel.area.width / 2;
    const middleY = panel.area.y + panel.area.height / 2;
    const stack = [];
    let right = -Infinity;
    let bottom = -Infinity;
    for (const window of popups) {
        const from = plasmaPosition(window);
        const g = window.frameGeometry;
        // only the bottom-right corner's
        if (from.x + g.width / 2 < middleX || from.y + g.height / 2 < middleY) {
            continue;
        }
        stack.push({ window: window, from: from });
        right = Math.max(right, from.x + g.width);
        bottom = Math.max(bottom, from.y + g.height);
    }
    const shiftX = Math.round(panel.right - right);
    const shiftY = Math.round(panel.top - panel.float - bottom);
    for (const { window, from } of stack) {
        const g = window.frameGeometry;
        const target = { x: from.x + shiftX, y: from.y + shiftY };
        placed.set(window.internalId.toString(), { x: target.x, y: target.y, fromX: from.x, fromY: from.y });
        if (Math.abs(g.x - target.x) >= 0.5 || Math.abs(g.y - target.y) >= 0.5) {
            window.frameGeometry = { x: target.x, y: target.y, width: g.width, height: g.height };
        }
    }
}

function track(window) {
    if (!isPopup(window)) {
        return;
    }
    window.frameGeometryChanged.connect(placeAll);
    window.closed.connect(() => {
        placed.delete(window.internalId.toString());
        placeAll();
    });
    placeAll();
}

workspace.windowAdded.connect(track);
for (const window of workspace.stackingOrder) {
    track(window);
}
