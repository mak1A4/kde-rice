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

PanelStyle::PanelStyle(QObject *parent)
    : QObject(parent)
{
}

int PanelStyle::opacity() const
{
    return std::clamp(settings().readEntry("Opacity", s_breezeOpacity), 0, 100);
}

bool PanelStyle::inUse() const
{
    return currentStyle() == s_style;
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

bool PanelStyle::write(int percent) const
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
    return changed;
}

void PanelStyle::setOpacity(int percent)
{
    percent = std::clamp(percent, 0, 100);
    KConfigGroup group = settings();
    group.writeEntry("Opacity", percent, KConfig::Notify);
    group.sync();

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
    Q_EMIT changed();
}

void PanelStyle::restore()
{
    QFile before(stateFile());
    const QString previous = before.open(QIODevice::ReadOnly) ? QString::fromUtf8(before.readAll()).trimmed() : QString();
    QProcess::execute(u"plasma-apply-desktoptheme"_s, {previous.isEmpty() || previous == s_style ? u"default"_s : previous});
    QFile::remove(stateFile());
    Q_EMIT changed();
}
