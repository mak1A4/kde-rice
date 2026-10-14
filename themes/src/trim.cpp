#include "trim.h"

#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>

#include <cairo.h>
#include <librsvg/rsvg.h>

#include <algorithm>
#include <cmath>
#include <memory>

using namespace Qt::StringLiterals;

namespace IconTrim
{

// the size it is measured at, in pixels
static constexpr int s_size = 256;
// what counts as drawn: the faint edge of a shadow is part of the icon
static constexpr int s_alpha = 8;
// what counts as the tile: drawn (nearly) solid
static constexpr int s_solid = 200;
// how much of the canvas the tile fills, as kde-rice-whitesur's do
static constexpr double s_tile = 0.93;
// round all it draws, where there is no tile: a hair of margin
static constexpr double s_margin = 0.015;

struct Box {
    int left = 256, top = 256, right = -1, bottom = -1;
    void add(int x, int y)
    {
        left = std::min(left, x);
        right = std::max(right, x);
        top = std::min(top, y);
        bottom = std::max(bottom, y);
    }
    bool empty() const
    {
        return right < 0;
    }
    int side() const
    {
        return std::max(right - left, bottom - top) + 1;
    }
};

std::optional<QByteArray> trimmed(const QByteArray &svg)
{
    GError *error = nullptr;
    std::unique_ptr<RsvgHandle, void (*)(gpointer)> handle(
        rsvg_handle_new_from_data(reinterpret_cast<const guint8 *>(svg.constData()), gsize(svg.size()), &error), g_object_unref);
    if (!handle) {
        g_clear_error(&error);
        return std::nullopt;
    }

    // the area the file draws in, in its own units
    gboolean hasWidth = false, hasHeight = false, hasViewBox = false;
    RsvgLength width{}, height{};
    RsvgRectangle box{};
    rsvg_handle_get_intrinsic_dimensions(handle.get(), &hasWidth, &width, &hasHeight, &height, &hasViewBox, &box);
    if (!hasViewBox) {
        if (!hasWidth || !hasHeight || width.unit == RSVG_UNIT_PERCENT || height.unit == RSVG_UNIT_PERCENT) {
            return std::nullopt;
        }
        box = RsvgRectangle{0, 0, width.length, height.length};
    }
    if (box.width <= 0 || box.height <= 0) {
        return std::nullopt;
    }

    std::unique_ptr<cairo_surface_t, void (*)(cairo_surface_t *)> surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, s_size, s_size),
                                                                         cairo_surface_destroy);
    std::unique_ptr<cairo_t, void (*)(cairo_t *)> cr(cairo_create(surface.get()), cairo_destroy);
    // the view box fitted into the square, kept in proportion and centred
    const double scale = std::min(s_size / box.width, s_size / box.height);
    const double offsetX = (s_size - box.width * scale) / 2;
    const double offsetY = (s_size - box.height * scale) / 2;
    const RsvgRectangle viewport{0, 0, double(s_size), double(s_size)};
    if (!rsvg_handle_render_document(handle.get(), cr.get(), &viewport, &error)) {
        g_clear_error(&error);
        return std::nullopt;
    }
    cairo_surface_flush(surface.get());

    // the drawn part, and the solid part (the tile), in pixels
    const unsigned char *data = cairo_image_surface_get_data(surface.get());
    const int stride = cairo_image_surface_get_stride(surface.get());
    Box drawn, solid;
    for (int y = 0; y < s_size; ++y) {
        const auto *row = reinterpret_cast<const uint32_t *>(data + y * stride);
        for (int x = 0; x < s_size; ++x) {
            const int alpha = int(row[x] >> 24);
            if (alpha > s_alpha) {
                drawn.add(x, y);
            }
            if (alpha > s_solid) {
                solid.add(x, y);
            }
        }
    }
    if (drawn.empty()) {
        return std::nullopt;
    }
    // a tile when the solid part is most of what is drawn
    const bool tile = !solid.empty() && solid.side() >= drawn.side() * 0.75;
    const Box &around = tile ? solid : drawn;

    // back in the file's units, as a square round the tile's (or the
    // drawing's) centre
    const double x0 = box.x + (around.left - offsetX) / scale;
    const double y0 = box.y + (around.top - offsetY) / scale;
    const double x1 = box.x + (around.right + 1 - offsetX) / scale;
    const double y1 = box.y + (around.bottom + 1 - offsetY) / scale;
    double side = std::max(x1 - x0, y1 - y0);
    side = tile ? side / s_tile : side * (1 + 2 * s_margin);
    // filling its canvas already (or nearly): left as it is
    if (side >= std::max(box.width, box.height) * 0.98) {
        return std::nullopt;
    }
    const double cx = (x0 + x1) / 2;
    const double cy = (y0 + y1) / 2;
    const QString viewBox = u"%1 %2 %3 %4"_s.arg(cx - side / 2, 0, 'g', 6).arg(cy - side / 2, 0, 'g', 6).arg(side, 0, 'g', 6).arg(side, 0, 'g', 6);

    // the root element: its view box replaced (or given one), a square size
    QString text = QString::fromUtf8(svg);
    static const QRegularExpression root(u"<svg\\b[^>]*>"_s);
    const auto match = root.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    QString tag = match.captured();
    static const QRegularExpression viewBoxAttribute(u"\\sviewBox\\s*=\\s*(\"[^\"]*\"|'[^']*')"_s);
    static const QRegularExpression aspectAttribute(u"\\spreserveAspectRatio\\s*=\\s*(\"[^\"]*\"|'[^']*')"_s);
    tag.remove(viewBoxAttribute);
    tag.remove(aspectAttribute);
    const qsizetype end = tag.endsWith("/>"_L1) ? tag.size() - 2 : tag.size() - 1;
    tag.insert(end, u" viewBox=\"%1\" preserveAspectRatio=\"xMidYMid meet\""_s.arg(viewBox));
    text.replace(match.capturedStart(), match.capturedLength(), tag);
    return text.toUtf8();
}

bool trimFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const auto result = trimmed(file.readAll());
    file.close();
    if (!result) {
        return false;
    }
    QSaveFile out(path);
    return out.open(QIODevice::WriteOnly) && out.write(*result) == result->size() && out.commit();
}

}
