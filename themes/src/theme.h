#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

// A theme: a file of colours, `<name>.json`,
//
//     { "colors": { "bg": "#272e33", ..., "terminal": [16 colours] },
//       "apps": { "nvim": "everforest", "btop": "..." } }
//
// The names are those of the old kde-quickshell themes: `bg` (a window),
// `view` (what an application shows in it), `surface`, `surfaceHover`,
// `surfaceActive`, `fg`, `fgDim`, `accent`, `accentFg`, `warning`, `error`,
// `desktops` (a list), `terminal` (sixteen), `terminalBackground`,
// `terminalForeground`, `terminalCursor`, `terminalSelection`. What a theme
// leaves out is Catppuccin Mocha's. `apps` names the theme where a program
// has it under a name of its own.
struct Theme {
    QString name;
    QString title;
    // the file it was read from
    QString path;
    QHash<QString, QColor> colors;
    QList<QColor> terminal;
    QList<QColor> desktops;
    QVariantMap apps;

    QColor color(const QString &name) const
    {
        return colors.value(name);
    }
    bool isDark() const;
};

// Where themes are, which one is chosen, and the backgrounds that go with
// each. The user's themes (~/.config/kde-rice/themes) come before the ones
// kde-rice installs; a file there replaces the installed one of the same
// name, whole.
namespace ThemeStore
{
QString userDir();
QStringList dirs();
QStringList names();
// "tokyo-night" -> "Tokyo Night"
QString title(const QString &name);
QString pathOf(const QString &name);
std::optional<Theme> load(const QString &name, QString *error = nullptr);

// ~/.config/kde-rice/theme.json: { "theme": name, "backgrounds": { theme: file name } }
struct Choice {
    QString theme;
    QHash<QString, QString> backgrounds;
};
Choice choice();
void saveChoice(const Choice &choice);

// A theme's backgrounds are the pictures and videos in
// ~/.local/share/kde-rice/backgrounds/<theme>/.
QString backgroundsDir(const QString &name);
QStringList backgrounds(const QString &name);
// the one chosen for the theme, else its first, else none ("")
QString backgroundFor(const QString &name, const Choice &choice);
bool isVideo(const QString &path);
// A small picture of a background (a video's frame two seconds in), made
// by ffmpeg once and kept in ~/.cache/kde-rice/thumbnails. `thumbnail` only
// looks; `makeThumbnail` makes it if needed, blocking.
QString thumbnail(const QString &background);
QString makeThumbnail(const QString &background);
}
