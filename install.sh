#!/bin/sh
# Builds kde-rice from this checkout and installs it as a pacman package.
# It is built as the user; everything that needs root afterwards (pacman,
# Papirus for the login screen, the login screen itself) is one sudo, so the
# password is asked for once:
#
#   ./install.sh
#
# Afterwards the programs that run in the background (the launcher, the
# gaps) are started again in their new build, and it says whether KWin's
# parts changed: KWin loads its plugins and the window decoration only when
# the session starts, so for those it is a new login. The login screen is
# brought in step with the session (Papirus, if that is the icon theme, is
# installed for everyone so that it finds it). Plasma keeps the
# wallpaper's code and the icons it has drawn until it restarts: if either
# changed, Plasma (the desktop and the panels, not the windows) is restarted.
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
# Plasma keeps the icons it has drawn by their file's name, not by what is
# in the file: when kde-rice's icons change, it is restarted too
icons_sum() {
    cat /usr/share/icons/kde-rice-whitesur/apps/scalable/*.svg 2> /dev/null | md5sum
}
icons_before=$(icons_sum)

cd "$repo/packaging"

# What it needs to build and run and is not installed yet (rarely anything):
# installed first, which is then a question for the password of its own.
# shellcheck disable=SC2016
missing=$(bash -c '. ./PKGBUILD && pacman -T "${depends[@]}" "${makedepends[@]}"' || true)
if [ -n "$missing" ]; then
    # shellcheck disable=SC2086
    sudo pacman -S --needed --asdeps --noconfirm $missing
fi

rm -f kde-rice-git-*.pkg.tar.zst
makepkg --force --noconfirm
package=$(ls "$PWD"/kde-rice-git-*.pkg.tar.zst)

# The login screen needs its icon theme installed for everyone.
papirus=
icons=$(kreadconfig6 --file kdeglobals --group Icons --key Theme)
case "$icons" in
    Papirus*) [ -d "/usr/share/icons/$icons" ] || papirus=papirus-icon-theme ;;
esac
login=
getent passwd plasmalogin > /dev/null && login=yes

# all that is root's, at once: the package, Papirus, and the login screen
# given the session's settings and kde-rice's wallpaper (sudo tells it who
# asked)
sudo sh -c '
    set -e
    pacman -U --noconfirm "$1"
    [ -z "$2" ] || pacman -S --needed --noconfirm "$2"
    [ -z "$3" ] || /usr/lib/kde-rice/login-screen
' sh "$package" "$papirus" "$login"

# kde-rice's Plasma style, if it is the one in use, made again by the new
# build
if [ "$(kreadconfig6 --file plasmarc --group Theme --key name)" = kde-rice ]; then
    kde-rice --panel-opacity "$(kreadconfig6 --file kde-ricerc --group Panels --key Opacity --default 85)"
    echo "made kde-rice's Plasma style again"
fi

# the background programs, as the new build
restart() {
    if pkill -x "$1"; then
        sleep 0.5
        shift
        setsid "$@" > /dev/null 2>&1 < /dev/null &
        echo "restarted $1"
        return 0
    fi
    return 1
}
restart kde-rice-launcher kde-rice-launcher --background
restart kde-rice-gaps kde-rice-gaps
# the tray's entries that follow the icon theme: started now if they are not
# running yet (later, by the autostart entry at login)
if ! restart kde-rice-tray kde-rice-tray; then
    setsid kde-rice-tray > /dev/null 2>&1 < /dev/null &
    echo "started kde-rice-tray"
fi

if { [ "$(cat "$wallpaper"/contents/ui/*.qml 2> /dev/null | md5sum)" != "$wallpaper_before" ] || [ "$(icons_sum)" != "$icons_before" ]; } \
    && pgrep -x plasmashell > /dev/null; then
    systemctl --user restart plasma-plasmashell.service
    echo "restarted Plasma for the new wallpaper or icons"
fi

echo
if [ "$(sums)" != "$before" ]; then
    echo "KWin's parts changed (rounded corners, title bars): log out and back in to load them."
fi
if pgrep -x kde-rice > /dev/null; then
    echo "The hub is open in its old build: close it and open it again."
fi
echo "kde-rice installed."
