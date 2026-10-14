#include "colour.h"

#include <algorithm>
#include <cmath>

namespace Colour
{

QColor between(const QColor &from, const QColor &to, double share)
{
    const auto mix = [share](int a, int b) {
        return int(std::lround(a + (b - a) * share));
    };
    return QColor(mix(from.red(), to.red()), mix(from.green(), to.green()), mix(from.blue(), to.blue()));
}

double luminance(const QColor &colour)
{
    const auto linear = [](int value) {
        const double part = value / 255.0;
        return part <= 0.04045 ? part / 12.92 : std::pow((part + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(colour.red()) + 0.7152 * linear(colour.green()) + 0.0722 * linear(colour.blue());
}

double contrast(const QColor &one, const QColor &other)
{
    const double a = luminance(one);
    const double b = luminance(other);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

QColor scaled(const QColor &colour, double by)
{
    const auto scale = [by](int value) {
        return std::min(255, int(std::lround(value * by)));
    };
    return QColor(scale(colour.red()), scale(colour.green()), scale(colour.blue()));
}

bool isDark(const QColor &colour)
{
    return 0.299 * colour.red() + 0.587 * colour.green() + 0.114 * colour.blue() < 128;
}

}
