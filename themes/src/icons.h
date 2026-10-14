#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

// kde-rice's icon theme: ~/.local/share/icons/kde-rice. It holds nothing of
// its own but the user's own pictures, one per icon name, from
// ~/.local/share/kde-rice/icons (linked in as its one directory); for every
// other name it inherits a base theme (kde-rice-whitesur by default, the
// WhiteSur kde-rice ships, see icons/CMakeLists.txt), in the variant
// for the kde-rice theme's mode (WhiteSur-dark with a dark theme), and
// after that Papirus, Breeze and hicolor.
//
// `[Icons] Base` in kde-ricerc names the base.
namespace IconStore
{
// the icon theme's name, as kdeglobals names the icon theme in use
QString themeName();
QString themeDir();
QString overridesDir();

// the base theme, and its variant for a mode ("dark" or "light")
QString base();
void setBase(const QString &theme);
QString baseFor(const QString &mode);

// Writes the icon theme for a mode. True if that changed it.
bool write(const QString &mode);
// whether kde-rice's icons are the ones in use
bool inUse();
// makes them the ones in use
void use();
// KDE's applications draw their icons again (after a change of the theme)
void reload();

struct Theme {
    QString id; // its directory's name
    QString name; // as its index.theme says
};
// the icon themes installed (not cursor themes, not kde-rice's own)
QList<Theme> installed();
// The file a theme draws an icon from: the largest it has, scalable first.
// "" if it has none of that name. Inherited themes are not looked into.
QString file(const QString &theme, const QString &name);
// every different picture a theme has for an icon name (the largest of each
// kind: a panel's, an action's, an application's ...), best first
QStringList files(const QString &theme, const QString &name);
// every icon name a theme has, sorted
QStringList names(const QString &theme);

// the user's own picture for an icon name, "" if there is none
QString overrideFor(const QString &name);
// Takes a picture (an SVG or PNG) for an icon name, with `trim` an app
// icon's SVG made to fill its canvas as kde-rice-whitesur's are (trim.h).
// False if it cannot.
bool setOverride(const QString &name, const QString &picture, bool trim = false);
// Trims an icon of the user's own (an SVG). True if that changed it.
bool trimOverride(const QString &name);
void removeOverride(const QString &name);
QStringList overrides();
}
