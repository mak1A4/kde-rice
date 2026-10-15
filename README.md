# kde-rice

One repository for everything that makes my KDE Plasma mine: a hub application
to switch it all on and tune it, and the Plasma and KWin parts it switches.

Plasma is customized through its own extension points, not a fork: what can
be changed from outside is changed from here, and Plasma updates stay a
`pacman -Syu` (plus a rebuild of the C++ parts, see below).

## Layout

| Directory | What | Built against |
|---|---|---|
| `app/` | The hub: a Kirigami application (QML + C++) | Qt, KDE Frameworks |
| `themes/` | Themes (`data/`), what they are written into for each program (`templates/`), the wallpaper that shows a theme's background, and the code that applies them | KDE Frameworks |
| `launcher/` | The launcher: a Spotlight-like search on Meta+Space, with KRunner's plugins underneath | KRunner, LayerShellQt |
| `icons/` | kde-rice-whitesur: WhiteSur built from its sources and shipped with kde-rice, the base of kde-rice's icon theme | bash, gtk-update-icon-cache |
| `tray/` | `kde-rice-tray`: tray entries that name a theme icon in place of applications' that send a picture | Qt D-Bus |
| `gaps/` | `kde-rice-gaps`: room kept free along the screen's edges, so maximized windows keep a gap and a floating panel keeps floating | LayerShellQt |
| `kwin/plugins/` | KWin plugins (C++), e.g. rounded corners on every window | KWin's internal API |
| `kwin/decoration/breeze/` | Breeze title bars, patched to keep their corners | KDecoration3, Breeze's sources |

Coming: `lookandfeel/` (global theme), `plasmoids/`, `kwin/scripts/`,
`kwin/effects/`, `sddm/`.

## Themes

A theme is one JSON file of colours, `<name>.json`:

```json
{
  "colors": {
    "bg": "#272e33", "view": "#1e2326", "surface": "#2e383c",
    "surfaceHover": "#374145", "surfaceActive": "#414b50",
    "fg": "#d3c6aa", "fgDim": "#9da9a0",
    "accent": "#a7c080", "accentFg": "#272e33",
    "warning": "#dbbc7f", "error": "#e67e80",
    "terminal": ["16 colours: black, red, green, yellow, blue, magenta, cyan, white, then the bright ones"],
    "terminalBackground": "#1e2326", "terminalForeground": "#d3c6aa",
    "terminalCursor": "#e69875", "terminalSelection": "#4c3743"
  },
  "apps": { "nvim": "everforest", "btop": "everforest-dark-hard" }
}
```

`bg` is a window's background, `view` what an application shows inside it
(lists, editors). Whether a theme is dark or light follows from `bg`. What a
theme leaves out is Catppuccin Mocha's. `apps` names the theme where a
program has one of its own.

- **Your themes** go in `~/.config/kde-rice/themes/`; one there replaces the
  installed theme of the same name. "Edit a Copy" on the Appearance page
  copies the current theme there and opens it: every save applies it again
  while the hub is open.
- **Backgrounds**: the pictures and videos in
  `~/.local/share/kde-rice/backgrounds/<theme>/`. The chosen one is shown on
  the desktop and the lock screen through kde-rice's wallpaper plugin; a
  theme without one gives Plasma's wallpaper back.
- **What a theme changes**: KDE's colour scheme (Qt/KDE applications, title
  bars, light or dark for everything that asks), a `-Dark`/`-Light` icon
  theme pair, GTK 4/libadwaita and btop directly; VS Code keeps its
  built-in Dark/Light Modern theme, coloured with the theme's colours
  (`workbench.colorCustomizations` between two `// kde-rice` comments in its
  settings), so no theme extensions are installed; T3 Code wears a theme
  kde-rice publishes for it (`~/.local/share/t3/userdata/themes/kde-rice.json`,
  set once with `t3 theme set`), which its open windows follow; Ghostty, WezTerm,
  tmux and Neovim through files in `~/.local/state/kde-rice/theme/`, which
  their configurations include (`config-file = ?"~/.local/state/kde-rice/theme/ghostty.conf"`
  and the like).
- **From a terminal**: `kde-rice-theme list | current | set <theme> [<background>|--none] | apply`.

## Launcher

`kde-rice-launcher` replaces KRunner's window: a search in the middle of the
screen on **Meta+Space**, with KDE's runner plugins underneath (applications,
windows, calculator, units, files, settings, web shortcuts, as enabled in
System Settings > Search). Open windows come first, then answers, then the
rest; kde-rice's own entries ("Theme: …", "kde-rice settings") above all.

It starts at login and stays running, so it opens at once. Meta+Space was
KRunner's here: `kde-rice-launcher --take-shortcut` takes it over once.
Enter runs, Shift+Enter a result's first action, Tab walks its actions,
Escape clears and then closes. Also over D-Bus:
`qdbus6 io.github.mak1a4.kde-rice-launcher /Launcher toggle`.

## Gaps

KWin has no gaps around maximized windows. `kde-rice-gaps` reserves an
invisible, click-through strip along each screen edge (layer-shell surfaces
with only an exclusive zone), and KWin fits maximized and quick-tiled windows
into what is left; nothing moves windows around. Along a floating panel the
strip is its float margin plus the gap, so maximized windows end a gap above
the panel and Plasma keeps it floating (it un-floats a panel whose window a
window overlaps). Switched on, with the gap's size, on the hub's Windows page
(`[Gaps] Enabled`, `Size` in `kde-ricerc`); it runs at login when on.

## Icons

kde-rice has an icon theme of its own, `kde-rice` in `~/.local/share/icons`:
a base theme (kde-rice-whitesur by default, in its light or dark variant
with the kde-rice theme; then Papirus, Breeze, hicolor) with your own picture for any
icon name on top, kept in `~/.local/share/kde-rice/icons/<name>.svg|png`.
The Icons page in the hub picks the base, lists the taskbar's applications
and finds any icon by name; for each, the same name from every installed
theme, the pictures of another name, or a file of your own. An application
whose menu entry names a picture file instead of an icon (T3 Code) is given a
copy of its entry naming an icon of its own, which then can be changed too.
The base's status icons (the tray's) are coloured in the kde-rice theme's
text colour, the colour Plasma draws the clock in: copies in
`~/.local/share/icons/kde-rice/tinted`, made again when the theme or the base
changes (Plasma is restarted then, as it keeps the icons it has drawn). So
are your own one-colour pictures (those declaring a `ColorScheme-Text`
colour) and kde-rice's own glyphs (`icons/glyphs`: the launcher's K, in
KDE's blue on a light panel; the weather applet's cloud before it is set
up), which come below yours and above the base's.
Teams and Outlook (the Electron ones), which hand the tray a picture of their
own, get kde-rice's glyphs (`icons/tray`) in the same colour, through their
config.json.

### Tray entries that follow the theme

Some applications (Electron ones: Teams, Outlook) hand the tray a picture
of their own instead of naming an icon, so no icon theme reaches them.
`kde-rice-tray` (started at login) puts an entry of its own in the tray for
each of them that names a theme icon, and passes everything else through to
the application's entry: clicks, scrolling, its menu, tooltip and status. The
application's own entry is hidden with Plasma's `disabledStatusNotifiers`
while it runs, and comes back when it stops. While an application's own
picture is not its first one (an unread count painted in), the rule's
`attentionIcon` is shown.

Which applications, by rules in `tray-rules.json` (kde-rice's in
`/usr/share/kde-rice`, yours in `~/.config/kde-rice`, a rule of the same name
replacing kde-rice's):

```json
[{ "name": "Teams for Linux", "id": "Teams for Linux_status_icon_*",
   "process": "teams-for-linux", "icon": "kde-rice-teams-tray",
   "attentionIcon": "kde-rice-teams-tray-attention" }]
```

`id` is the entry's id (wildcards), `process` the program that owns it (for
the many Electron entries that are all `chrome_status_icon_1`), `icon` any
icon name. A glyph of kde-rice's own goes in `icons/tray/<app>.svg` (and
`<app>-attention.svg`), drawn in `.ColorScheme-Text`: it is installed as
`kde-rice-<app>-tray` and coloured with the theme like the tray's other icons.

From a terminal: `kde-rice-theme icons use | base <theme> | set <name> <file>
| reset <name> | find <name>`.

kde-rice-whitesur is WhiteSur, built by kde-rice from WhiteSur's released
sources with WhiteSur's own installer (with KDE's logo for Apple's) and
installed with kde-rice: no icon theme package is needed besides. The
release is pinned, with its checksum, in `icons/CMakeLists.txt`.

## Login screen

Plasma Login Manager runs as a user of its own and reads nothing in a home
directory. `login/login-screen.sh` (installed as
`/usr/lib/kde-rice/login-screen`, run as root by `install.sh` and by "Update
Login Screen" on the Appearance page) makes `/var/lib/kde-rice/background`
the user's, sets the login screen to kde-rice's wallpaper, and copies the
session's colours, fonts, icon theme, keyboard layout and display scaling
there. kde-rice then copies the theme's background into that directory at
every change of theme, without root. The icon theme has to be installed for
everyone; `install.sh` installs Papirus if that is the one in use.

## Panel transparency

Plasma's panel settings know only opaque, translucent and adaptive; how
translucent is the Plasma style's (Breeze draws the panel at 85 %). The
Panels page has a slider for it: kde-rice makes a Plasma style of its own,
`kde-rice` in `~/.local/share/plasma/desktoptheme/`, which is Breeze with the
panel's background at the chosen opacity (everything else, and the colours,
Breeze's and the colour scheme's), and applies it. "Back to the Plasma style
before" undoes that. From a terminal: `kde-rice --panel-opacity <percent>`.

The same style smooths the corners of Plasma's popups and tooltips. Their
blur is cut out along a mask Plasma gives KWin in whole pixels: at a
fractional scale the round corners come out as steps, and where a step lies
inside the frame's own corner the background shows through unblurred. The
style makes those masks square, and the corners plugin gives Plasma's popup
windows the frame's radius (6 px), along which KWin's blur has a smooth
edge.

## How the hub changes Plasma

The hub does what System Settings does: it writes the configuration files
(`kwinrc`, `kdeglobals`, …) with KConfig and tells KWin and Plasma over D-Bus
to read them again, load a plugin, or run a script. The QML types it builds on
are in `app/src`:

- `ConfigEntry`: one entry of a KConfig file, read and written from QML
- `KWinPlugin`: a KWin plugin switched on and off, now and for the next login
- `KWin` (singleton): reconfigure, and the window decoration

## Build

```sh
cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
build/bin/kde-rice                # the hub runs from the build directory
sudo cmake --install build        # the KWin parts only work installed
```

Or as a package, the way to install it here: **`./install.sh`**. It builds
the pacman package from this checkout, installs it (asking for the password
once), starts the launcher and the gaps again in their new build, and says
when KWin's parts changed and need a new login. The icons are a package of
their own (`kde-rice-whitesur-git`), made and installed again only when what
they are built from changed (`packaging/whitesur-inputs`).

Options: `-DKDERICE_APP=OFF`, `-DKDERICE_THEMES=OFF`, `-DKDERICE_LAUNCHER=OFF`,
`-DKDERICE_ICONS=OFF`, `-DKDERICE_TRAY=OFF`, `-DKDERICE_GAPS=OFF`, `-DKDERICE_KWIN_PLUGINS=OFF`,
`-DKDERICE_DECORATION=OFF`. The decoration fetches Breeze's sources for the
installed Plasma release from download.kde.org; `-DBREEZE_URL=` points it at
a local archive instead.

**After every Plasma update**, run `./install.sh` again: KWin's plugin API
is not stable between releases, and a plugin built for the previous KWin is
not loaded. A plugin already loaded is replaced by its new build only at the
next login.
