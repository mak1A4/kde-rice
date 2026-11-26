#include "panelstyle.h"

#include <KCompressionDevice>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QDirIterator>
#include <QPointF>
#include <QRectF>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

static const auto s_style = u"kde-rice"_s;
// Breeze's own, which the style is made from
static constexpr int s_breezeOpacity = 85;

static KConfigGroup settings()
{
    auto config = KSharedConfig::openConfig(u"kde-ricerc"_s);
    config->reparseConfiguration();
    return config->group(u"Panels"_s);
}

static QString currentStyle()
{
    auto plasmarc = KSharedConfig::openConfig(u"plasmarc"_s);
    plasmarc->reparseConfiguration();
    return plasmarc->group(u"Theme"_s).readEntry("name", u"default"_s);
}

static QString styleDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/plasma/desktoptheme/"_L1 + s_style;
}

static QString stateFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/kde-rice/plasma-style-before"_L1;
}

static int readOpacity()
{
    return std::clamp(settings().readEntry("Opacity", s_breezeOpacity), 0, 100);
}

int PanelStyle::savedOpacity()
{
    return readOpacity();
}

PanelStyle::PanelStyle(QObject *parent)
    : QObject(parent)
{
    connect(&m_worker, &Worker::busyChanged, this, &PanelStyle::busyChanged);
    read();
    // set elsewhere too (kde-rice --panel-opacity)
    m_watcher = KConfigWatcher::create(KSharedConfig::openConfig(u"kde-ricerc"_s));
    connect(m_watcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() == "Panels"_L1 && !busy()) {
            read();
        }
    });
}

void PanelStyle::read()
{
    const int opacity = readOpacity();
    const bool inUse = currentStyle() == s_style;
    if (opacity != m_opacity || inUse != m_inUse) {
        m_opacity = opacity;
        m_inUse = inUse;
        Q_EMIT changed();
    }
}

// Writes `contents` to `path` if that changes it. True if it did.
static bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == contents) {
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size() && file.commit();
}

// The blur behind a translucent popup is cut out along the frame's mask,
// which Plasma hands KWin as whole pixels: at a fractional scale its round
// corners are steps, and where a step lies inside the frame's own smooth
// corner the background shows through unblurred, a jagged corner. Here the
// mask's corners are square, so that the blur covers the frame's corners
// whole, and the corners plugin (kwin/plugins/roundcorners) gives Plasma's
// popup windows the frame's radius: KWin then cuts the blur along it with
// a smooth edge. Not for the panel, whose window is larger than the panel
// (the float margin), so a window's radius cannot round it: its mask stays
// Breeze's. Pictures whose mask pieces are placed in other ways (the
// opaque ones, which have no blur) are left as they are.
//
// With `opacity` ("opacity:0.6"), the frame's nine pieces in the colour
// scheme's background are drawn that see-through instead of at 85 %.
static QString squareMask(const QString &svg, bool square, const QString &opacity)
{
    static const QRegularExpression element(u"<[^>]+>"_s);
    static const QRegularExpression idPattern(uR"re(\bid="([^"]*)")re"_s);
    static const QRegularExpression number(uR"re(\b(x|y|width|height)="([-0-9.]+)")re"_s);
    static const QRegularExpression path(uR"re(\bd="[^"]*")re"_s);

    // where the mask's sides are, from its plain rectangles
    QHash<QString, QHash<QString, double>> sides;
    bool placedPlainly = true;
    for (auto it = element.globalMatch(svg); it.hasNext();) {
        const QString tag = it.next().captured();
        const QString id = idPattern.match(tag).captured(1);
        if (id == "mask-top"_L1 || id == "mask-left"_L1 || id == "mask-right"_L1 || id == "mask-bottom"_L1) {
            // the bottom one is flipped in place, which leaves its size
            placedPlainly &= !tag.contains("transform="_L1) || id == "mask-bottom"_L1;
            for (auto attribute = number.globalMatch(tag); attribute.hasNext();) {
                const auto match = attribute.next();
                sides[id][match.captured(1)] = match.captured(2).toDouble();
            }
        }
    }
    placedPlainly &= sides.size() == 4;
    const auto top = sides.value(u"mask-top"_s);
    const auto left = sides.value(u"mask-left"_s);
    const auto right = sides.value(u"mask-right"_s);
    const auto bottom = sides.value(u"mask-bottom"_s);
    // each corner piece, square, between the sides it joins
    const qreal x0 = left.value(u"x"_s);
    const qreal x1 = top.value(u"x"_s);
    const qreal x2 = x1 + top.value(u"width"_s);
    const qreal x3 = x2 + right.value(u"width"_s);
    const qreal y1 = left.value(u"y"_s);
    const qreal y0 = y1 - top.value(u"height"_s);
    const qreal y2 = y1 + left.value(u"height"_s);
    const qreal y3 = y2 + bottom.value(u"height"_s);
    const QHash<QString, QRectF> corners = {
        {u"mask-topleft"_s, QRectF(QPointF(x0, y0), QPointF(x1, y1))},
        {u"mask-topright"_s, QRectF(QPointF(x2, y0), QPointF(x3, y1))},
        {u"mask-bottomleft"_s, QRectF(QPointF(x0, y2), QPointF(x1, y3))},
        {u"mask-bottomright"_s, QRectF(QPointF(x2, y2), QPointF(x3, y3))},
    };

    QString result;
    qsizetype from = 0;
    for (auto it = element.globalMatch(svg); it.hasNext();) {
        const auto match = it.next();
        QString tag = match.captured();
        const QString id = idPattern.match(tag).captured(1);
        if (square && placedPlainly && corners.contains(id) && !tag.contains("transform="_L1) && path.match(tag).hasMatch()) {
            const QRectF r = corners.value(id);
            tag.replace(path, u"d=\"M %1,%2 H %3 V %4 H %1 Z\""_s.arg(r.left()).arg(r.top()).arg(r.right()).arg(r.bottom()));
        }
        if (!opacity.isEmpty() && tag.contains("class=\"ColorScheme-Background\""_L1)) {
            tag.replace("opacity:0.85"_L1, opacity);
        }
        result += QStringView(svg).mid(from, match.capturedStart() - from);
        result += tag;
        from = match.capturedEnd();
    }
    result += QStringView(svg).mid(from);
    return result;
}

// One state of the list highlight (hover, selected, selected+hover), as the
// nine pieces Plasma puts a frame together from, at (x, y) in the picture:
// a fill of the accent at `opacity`, round at the corners. The pieces at the
// edges are 5 high and wide, as Breeze's, so that what is laid out by them
// stays where it is; the fill keeps 1 off the top and the bottom (a rect
// that is not seen gives such a piece its size), so that two highlighted
// rows one below the other are two.
static QString highlightPieces(const QString &state, int x, int y, const QString &opacity)
{
    const int s = 5; // a piece at the edge
    const int r = 4; // the corners' radius
    const int v = s - r; // kept free above and below
    const int m = 20; // the pieces in between, stretched
    const QString fill = uR"( class="ColorScheme-ButtonFocus" style="fill:currentColor;stroke:none;opacity:%1")"_s.arg(opacity);
    const auto piece = [&](const QString &where, int px, int py, int w, int h, const QString &shape) {
        return uR"(<g id="%1-%2"><rect x="%3" y="%4" width="%5" height="%6" style="fill:#000000;stroke:none;opacity:0.001"/>%7</g>
)"_s.arg(state, where).arg(px).arg(py).arg(w).arg(h).arg(shape);
    };
    const auto rect = [&](int px, int py, int w, int h) {
        return uR"(<rect x="%1" y="%2" width="%3" height="%4"%5/>)"_s.arg(px).arg(py).arg(w).arg(h).arg(fill);
    };
    const auto path = [&](const QString &d) {
        return uR"(<path d="%1"%2/>)"_s.arg(d, fill);
    };
    const int x1 = x + s;
    const int x2 = x + s + m;
    const int y1 = y + s;
    const int y2 = y + s + m;
    QString pieces;
    pieces += piece(u"topleft"_s, x, y, s, s, path(u"M %1,%2 L %3,%2 A %4,%4 0 0 0 %5,%6 L %1,%6 Z"_s.arg(x1).arg(y + v).arg(x + r).arg(r).arg(x).arg(y1)));
    pieces += piece(u"top"_s, x1, y, m, s, rect(x1, y + v, m, r));
    pieces += piece(u"topright"_s, x2, y, s, s, path(u"M %1,%2 L %3,%2 A %4,%4 0 0 1 %5,%6 L %1,%6 Z"_s.arg(x2).arg(y + v).arg(x2 + s - r).arg(r).arg(x2 + s).arg(y1)));
    pieces += piece(u"left"_s, x, y1, s, m, rect(x, y1, s, m));
    pieces += uR"(<rect id="%1-center" x="%2" y="%3" width="%4" height="%4"%5/>
)"_s.arg(state).arg(x1).arg(y1).arg(m).arg(fill);
    pieces += piece(u"right"_s, x2, y1, s, m, rect(x2, y1, s, m));
    pieces += piece(u"bottomleft"_s, x, y2, s, s, path(u"M %1,%2 L %3,%2 L %3,%4 L %5,%4 A %6,%6 0 0 1 %1,%2 Z"_s.arg(x).arg(y2).arg(x1).arg(y2 + r).arg(x + r).arg(r)));
    pieces += piece(u"bottom"_s, x1, y2, m, s, rect(x1, y2, m, r));
    pieces += piece(u"bottomright"_s, x2, y2, s, s, path(u"M %1,%2 L %3,%2 A %4,%4 0 0 1 %5,%6 L %1,%6 Z"_s.arg(x2).arg(y2).arg(x2 + s).arg(r).arg(x2 + s - r).arg(y2 + r)));
    return pieces;
}

// Breeze's picture of what is highlighted in Plasma's lists (the launcher's,
// the system tray's: widgets/viewitem), with the three states that are a
// fill of the accent drawn anew (highlightPieces), Breeze's taken out:
//  - under the pointer, and the current entry (hover): Breeze's fill at 30 %
//    with an outline. Here a fill alone, as kde-rice's application style has
//    it (style/), and fainter, as the launcher shows several at once (the
//    category open, the entry under the pointer, the current application);
//  - clicked (selected+hover): Breeze's accent at full strength, dark under
//    the text. Here at half of it;
//  - selected: at 80 %, as Breeze's.
static QString riceHighlights(const QString &svg)
{
    // a group and all in it, or an element by itself
    static const QRegularExpression theirs(uR"re(<g\b[^>]*\bid="(?:hover|selected|selected\+hover)-[a-z]+"[^>]*>.*?</g>|<(?:path|rect)\b[^>]*\bid="(?:hover|selected|selected\+hover)-[a-z]+"[^>]*/>)re"_s,
                                           QRegularExpression::DotMatchesEverythingOption);
    QString result = svg;
    result.remove(theirs);
    const qsizetype end = result.lastIndexOf("</svg>"_L1);
    if (end < 0) {
        return svg;
    }
    // below Breeze's own, where the picture has nothing
    result.insert(end, highlightPieces(u"hover"_s, 0, 400, u"0.18"_s) + highlightPieces(u"selected"_s, 40, 400, u"0.8"_s) + highlightPieces(u"selected+hover"_s, 80, 400, u"0.5"_s));
    return result;
}

bool PanelStyle::write(int percent)
{
    const QString breeze = QStandardPaths::locate(QStandardPaths::GenericDataLocation, u"plasma/desktoptheme/default"_s, QStandardPaths::LocateDirectory);
    if (breeze.isEmpty()) {
        return false;
    }
    bool changed = false;
    changed |= writeFile(styleDir() + "/metadata.json"_L1, R"({
    "KPlugin": {
        "Description": "Breeze, with the panels as see-through as kde-rice says",
        "Id": "kde-rice",
        "License": "LGPL",
        "Name": "kde-rice"
    },
    "X-Plasma-API": "5.0"
}
)");
    // Breeze's settings for the style (adaptive transparency among them)
    QFile plasmarc(breeze + "/plasmarc"_L1);
    if (plasmarc.open(QIODevice::ReadOnly)) {
        changed |= writeFile(styleDir() + "/plasmarc"_L1, plasmarc.readAll());
    }
    // Every picture of Breeze's with a blur mask: the popups' with square
    // mask corners (squareMask), the panel's at the chosen opacity.
    const QString opacity = "opacity:"_L1 + QString::number(percent / 100.0, 'g', 3);
    QDirIterator pictures(breeze, {u"*.svgz"_s}, QDir::Files, QDirIterator::Subdirectories);
    while (pictures.hasNext()) {
        const QString path = pictures.next();
        KCompressionDevice svgz(path, KCompressionDevice::GZip);
        if (!svgz.open(QIODevice::ReadOnly)) {
            continue;
        }
        QString svg = QString::fromUtf8(svgz.readAll());
        if (!svg.contains("id=\"mask-topleft\""_L1)) {
            continue;
        }
        const QString relative = QDir(breeze).relativeFilePath(path).chopped(1);
        const bool panel = relative.endsWith("widgets/panel-background.svg"_L1) && !relative.startsWith("opaque/"_L1) && !relative.startsWith("solid/"_L1);
        svg = squareMask(svg, !relative.contains("panel-background"_L1), panel ? opacity : QString());
        changed |= writeFile(styleDir() + u'/' + relative, svg.toUtf8());
    }
    KCompressionDevice viewItem(breeze + "/widgets/viewitem.svgz"_L1, KCompressionDevice::GZip);
    if (viewItem.open(QIODevice::ReadOnly)) {
        changed |= writeFile(styleDir() + "/widgets/viewitem.svg"_L1, riceHighlights(QString::fromUtf8(viewItem.readAll())).toUtf8());
    }
    return changed;
}

void PanelStyle::setOpacity(int percent)
{
    percent = std::clamp(percent, 0, 100);
    if (percent == m_opacity && m_inUse) {
        return;
    }
    m_opacity = percent;
    Q_EMIT changed();
    m_worker.run(
        [percent] {
            applyOpacity(percent);
        },
        [this] {
            read();
        });
}

void PanelStyle::applyOpacity(int percent)
{
    percent = std::clamp(percent, 0, 100);
    KConfigGroup group = settings();
    group.writeEntry("Opacity", percent, KConfig::Notify);
    group.sync();
    applyStyle(percent);
}

void PanelStyle::applyStyle(int percent)
{
    const bool rewritten = write(percent);
    const QString current = currentStyle();
    if (current != s_style) {
        // what there was, for going back
        QDir().mkpath(QFileInfo(stateFile()).absolutePath());
        writeFile(stateFile(), current.toUtf8());
        QProcess::startDetached(u"plasma-apply-desktoptheme"_s, {s_style});
    } else if (rewritten) {
        // Plasma reads a style's pictures again when it is applied anew,
        // and it does not apply the one it has: by way of Breeze
        QProcess::execute(u"plasma-apply-desktoptheme"_s, {u"default"_s});
        QProcess::startDetached(u"plasma-apply-desktoptheme"_s, {s_style});
    }
}

void PanelStyle::restore()
{
    m_worker.run(
        [] {
            restoreStyle();
        },
        [this] {
            read();
        });
}

bool PanelStyle::styleInUse()
{
    return currentStyle() == s_style;
}

void PanelStyle::restoreStyle()
{
    QFile before(stateFile());
    const QString previous = before.open(QIODevice::ReadOnly) ? QString::fromUtf8(before.readAll()).trimmed() : QString();
    if (styleInUse()) {
        QProcess::execute(u"plasma-apply-desktoptheme"_s, {previous.isEmpty() || previous == s_style ? u"default"_s : previous});
    }
    QFile::remove(stateFile());
}
