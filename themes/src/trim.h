#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

// App icons drawn smaller than their canvas (macOS-style themes leave a
// margin round the tile for its shadow) made to fill it: the SVG's view box
// tightened, as a square, round what it draws, shadow included, with a
// hair of margin. The drawing itself is not touched.
//
// What is drawn is measured by librsvg, which draws SVG filters (the
// shadows of many themes are filters, which Qt's SVG renderer leaves out).
namespace IconTrim
{
// the trimmed SVG; nothing if it cannot be read, or already fills its canvas
std::optional<QByteArray> trimmed(const QByteArray &svg, double margin = 0.015);
// Trims an SVG file in place. True if it changed.
bool trimFile(const QString &path);
}
