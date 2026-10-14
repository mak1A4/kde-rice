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
| `kwin/plugins/` | KWin plugins (C++), e.g. rounded corners on every window | KWin's internal API |
| `kwin/decoration/breeze/` | Breeze title bars, patched to keep their corners | KDecoration3, Breeze's sources |

Coming: `lookandfeel/` (global theme), `plasmoids/`, `kwin/scripts/`,
`kwin/effects/`, `themes/` (colours, Plasma style, icons, cursors), `sddm/`.

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

Or as a package: `cd packaging && makepkg -si`.

Options: `-DKDERICE_APP=OFF`, `-DKDERICE_KWIN_PLUGINS=OFF`,
`-DKDERICE_DECORATION=OFF`. The decoration fetches Breeze's sources for the
installed Plasma release from download.kde.org; `-DBREEZE_URL=` points it at
a local archive instead.

**After every Plasma update**, build and install again: KWin's plugin API
is not stable between releases, and a plugin built for the previous KWin is
not loaded. A plugin already loaded is replaced by its new build only at the
next login.
