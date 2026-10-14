#pragma once

#include "theme.h"

#include <QHash>
#include <QString>
#include <QStringList>

// Gives a theme to everything outside kde-rice, the way Omarchy does it: the
// theme is written down as a flat list of names and values, a template per
// program is filled in with it ("{{ name }}"), each program reads its file
// from where it always does, and the running ones are told to read again.
//
//   KDE           a colour scheme, made the current one: Qt and KDE
//                 applications, title bars, GTK through KDE's own settings
//                 daemon, and whether the desktop counts as dark or light.
//                 An icon theme that comes as a -Dark/-Light pair follows.
//   GTK 4         libadwaita's colour names (~/.config/gtk-4.0/kde-rice.css)
//   Ghostty, WezTerm, tmux, Neovim
//                 files in ~/.local/state/kde-rice/theme, which the user's
//                 own configurations include
//   btop          ~/.config/btop/themes/kde-rice.theme
//   VS Code       its built-in theme coloured with the theme's colours
//                 (settings.json), no extension needed
//   background    the theme's picture or video on the desktop and the lock
//                 screen, through kde-rice's wallpaper plugin; without one,
//                 what they had before
//
// A program is only told to read again when its file changed, so applying
// the theme in use again costs nothing. Blocking (it runs programs): call it
// off the GUI thread.
namespace ThemeApplier
{
// the flat list the templates are filled in with
QHash<QString, QString> values(const Theme &theme);

struct Options {
    // write the files only, tell no program (for trying it out)
    bool filesOnly = false;
};

// What went wrong, one line each; empty if nothing did.
QStringList apply(const Theme &theme, const QString &background, const Options &options = {});

// the directory the terminal, tmux and Neovim files are written to
QString outputDir();
}
