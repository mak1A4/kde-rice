#!/bin/sh
# Builds kde-rice from this checkout and installs it as a pacman package
# (asks for the password once, for pacman):
#
#   ./install.sh
#
# Afterwards the programs that run in the background (the launcher, the
# gaps) are started again in their new build, and it says whether KWin's
# parts changed: KWin loads its plugins and the window decoration only when
# the session starts, so for those it is a new login. Plasma keeps the
# wallpaper's code until it restarts: if that changed, Plasma (the desktop
# and the panels, not the windows) is restarted.
set -e

repo=$(cd "$(dirname "$0")" && pwd)
plugins=$(qtpaths6 --query QT_INSTALL_PLUGINS 2>/dev/null || echo /usr/lib/qt6/plugins)
kwin_parts="$plugins/kwin/plugins/kderice_roundcorners.so $plugins/org.kde.kdecoration3/io.github.mak1a4.kde-rice.breeze.so"

sums() {
    # shellcheck disable=SC2086
    md5sum $kwin_parts 2> /dev/null || true
}

wallpaper=/usr/share/plasma/wallpapers/io.github.mak1a4.kde-rice.background

before=$(sums)
wallpaper_before=$(cat "$wallpaper"/contents/ui/*.qml 2> /dev/null | md5sum)

cd "$repo/packaging"
rm -f kde-rice-git-*.pkg.tar.zst
makepkg --syncdeps --force --install --noconfirm

# the background programs, as the new build
restart() {
    if pkill -x "$1"; then
        sleep 0.5
        shift
        setsid "$@" > /dev/null 2>&1 < /dev/null &
        echo "restarted $1"
    fi
}
restart kde-rice-launcher kde-rice-launcher --background
restart kde-rice-gaps kde-rice-gaps

if [ "$(cat "$wallpaper"/contents/ui/*.qml 2> /dev/null | md5sum)" != "$wallpaper_before" ] && pgrep -x plasmashell > /dev/null; then
    systemctl --user restart plasma-plasmashell.service
    echo "restarted Plasma for the new wallpaper"
fi

echo
if [ "$(sums)" != "$before" ]; then
    echo "KWin's parts changed (rounded corners, title bars): log out and back in to load them."
fi
if pgrep -x kde-rice > /dev/null; then
    echo "The hub is open in its old build: close it and open it again."
fi
echo "kde-rice installed."
