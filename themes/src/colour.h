#pragma once

#include <QColor>

// Colour arithmetic for writing a theme down for other programs. Colours are
// taken as opaque #rrggbb throughout.
namespace Colour
{
// `share` of the way from one colour to another, channel by channel
QColor between(const QColor &from, const QColor &to, double share);
// how light a colour is to the eye, 0 to 1 (WCAG's relative luminance)
double luminance(const QColor &colour);
// how far apart two colours are for reading one on the other (WCAG's
// contrast ratio: 1 is none, 4.5 is what body text wants)
double contrast(const QColor &one, const QColor &other);
// each channel multiplied, as Qt's lighter() and darker() do for all but
// the lightest colours
QColor scaled(const QColor &colour, double by);
// what KDE and the old shell call dark: perceived brightness below half
bool isDark(const QColor &colour);
}
