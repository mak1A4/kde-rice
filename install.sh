#!/bin/sh
# Builds kde-rice from this checkout and installs it as a pacman package.
# It is built as the user; everything that needs root afterwards (pacman,
# Papirus for the login screen, the login screen itself) is one sudo, so the
# password is asked for once:
#
#   ./install.sh
#   ./install.sh --force   installs, and does all that follows, even when
#                          nothing changed
#   ./install.sh --gtk     only the style sheets for GTK, from this checkout
#                          (the menus of Electron applications, libadwaita's
#                          colours): nothing is installed, no password. The
#                          theme's program is built in build/ and writes them;
#                          an application reads them when it starts.
#
# The build is incremental (makepkg keeps packaging/src/build). The package
# built is then held against the one installed, file by file: when nothing
# in it changed, nothing is installed and no password asked for. Otherwise
# only what the changed files call for follows:
#
# - the background programs (the launcher, the gaps, the tray) started
#   again in their new build, those whose program changed;
# - kde-rice's Plasma style made again, if it is in use and the hub (which
#   makes it) changed;
# - Plasma (the desktop and the panels, not the windows) restarted when it
#   keeps something that changed: the wallpaper's code, the desktop
#   widgets' (plasmoids/), the icons it has drawn, or the application style
#   it draws its menus with;
# - said, when KWin's parts changed, that it takes a new login: KWin loads
#   its plugins and the window decoration only when the session starts.
#
# - the desktop portal restarted, if KWin was restarted meanwhile (the
#   portal keeps talking to the KWin that is gone).
#
# With the package, the login screen is brought in step with the session
# (Papirus, if that is the icon theme, is installed for everyone so that it
# finds it).
#
# kde-rice's icons are a package of their own (kde-rice-whitesur-git), built
# and installed only when what they are made from changed
# (packaging/whitesur-inputs).
set -e

repo=$(cd "$(dirname "$0")" && pwd)

if [ "${1:-}" = --gtk ]; then
    [ -e "$repo/build/build.ninja" ] || cmake -S "$repo" -B "$repo/build" -G Ninja -DCMAKE_INSTALL_PREFIX=/usr > /dev/null
    cmake --build "$repo/build" --target kde-rice-theme > /dev/null
    KDERICE_TEMPLATES="$repo/themes/templates" "$repo/build/bin/kde-rice-theme" apply gtk
    echo "GTK's style sheets written: applications read them when they start."
    exit
fi
force=0
[ "${1:-}" = --force ] && force=1

scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT

# A package's files, each with its checksum, or its target for a link
# (pacman's mtree, without the times): what is built, and what is installed.
files_of() {
    grep '^\./[^.]' | sed 's/ time=[^ ]*//' | sort
}
built_files() {
    bsdtar -xOf "$1" .MTREE | gzip -dc | files_of
}
installed_files() {
    for mtree in /var/lib/pacman/local/kde-rice-git-*/mtree; do
        [ -e "$mtree" ] && gzip -dc "$mtree" | files_of
    done
}
# and what it depends on (the KWin release it is pinned to among them)
built_depends() {
    bsdtar -xOf "$1" .PKGINFO | sed -n 's/^depend = //p' | sort
}
installed_depends() {
    for desc in /var/lib/pacman/local/kde-rice-git-*/desc; do
        [ -e "$desc" ] && sed -n '/^%DEPENDS%$/,/^$/{/^%/d;/^$/d;p}' "$desc"
    done | sort
}

# whether a file whose path matches (grep's pattern) is among the changed
changed() {
    grep -q -- "$1" "$scratch/changed"
}

cd "$repo/packaging"

# the icons' package too, if what they are made from is not what is
# installed. Plasma keeps the icons it has drawn by their file's name, not
# by what is in the file: when they change, it is restarted too.
whitesur=0
[ "$(cat /usr/share/kde-rice/whitesur-inputs 2> /dev/null)" = "$(./whitesur-inputs)" ] || whitesur=1

# What it needs to build and run and is not installed yet (rarely anything):
# installed first, which is then a question for the password of its own.
# shellcheck disable=SC2016
missing=$(bash -c '. ./PKGBUILD && pacman -T "${depends[@]}" "${makedepends[@]}"' || true)
if [ -n "$missing" ]; then
    # shellcheck disable=SC2086
    sudo pacman -S --needed --asdeps --noconfirm $missing
fi

rm -f kde-rice-git-*.pkg.tar.zst kde-rice-whitesur-git-*.pkg.tar.zst
KDERICE_WHITESUR=$whitesur makepkg --force --noconfirm
package=$(ls "$PWD"/kde-rice-git-*.pkg.tar.zst)
icon_package=
[ "$whitesur" = 0 ] || icon_package=$(ls "$PWD"/kde-rice-whitesur-git-*.pkg.tar.zst)

# the files that differ from the installed package's (both sides' lines,
# by path)
built_files "$package" > "$scratch/built"
installed_files > "$scratch/installed"
comm -3 "$scratch/built" "$scratch/installed" | sed 's/^\t//; s/ .*//' | sort -u > "$scratch/changed"
depends_changed=0
[ "$(built_depends "$package")" = "$(installed_depends)" ] || depends_changed=1

if [ "$force" = 0 ] && [ ! -s "$scratch/changed" ] && [ "$depends_changed" = 0 ] && [ "$whitesur" = 0 ]; then
    echo
    echo "Nothing changed since the last install: nothing installed (./install.sh --force does it anyway)."
    exit
fi
if [ "$force" = 1 ]; then
    # all of it, as if every file were new
    cut -d' ' -f1 "$scratch/built" > "$scratch/changed"
fi
echo
echo "$(wc -l < "$scratch/changed") of kde-rice's files changed."

# The login screen needs its icon theme installed for everyone.
papirus=
icons=$(kreadconfig6 --file kdeglobals --group Icons --key Theme)
case "$icons" in
    Papirus*) [ -d "/usr/share/icons/$icons" ] || papirus=papirus-icon-theme ;;
esac
login=
getent passwd plasmalogin > /dev/null && login=yes

# KWin as it runs now: a KWin restarted meanwhile (replacing its plugins
# under it can take it down, and its wrapper starts it again) leaves the
# desktop portal talking to the one that is gone (screen sharing, file
# dialogs), which is then restarted too
kwin_before=$(pgrep -x kwin_wayland || true)

# all that is root's, at once: the package, Papirus, and the login screen
# given the session's settings and kde-rice's wallpaper (sudo tells it who
# asked)
sudo sh -c '
    set -e
    papirus=$1 login=$2
    shift 2
    pacman -U --noconfirm "$@"
    [ -z "$papirus" ] || pacman -S --needed --noconfirm "$papirus"
    [ -z "$login" ] || /usr/lib/kde-rice/login-screen
' sh "$papirus" "$login" "$package" ${icon_package:+"$icon_package"}

# the session's systemd reads the package's units again (the icon theme
# written before Plasma starts: a drop-in for its unit)
if changed '^\./usr/lib/systemd/'; then
    systemctl --user daemon-reload
fi

# kde-rice's Plasma style, if it is the one in use, made again by the new
# build of the hub, which makes it
if changed '^\./usr/bin/kde-rice$' && [ "$(kreadconfig6 --file plasmarc --group Theme --key name)" = kde-rice ]; then
    kde-rice --panel-opacity "$(kreadconfig6 --file kde-ricerc --group Panels --key Opacity --default 85)"
    echo "made kde-rice's Plasma style again"
fi

# the background programs whose program changed, as the new build. Found by
# the program in their command line: the name pkill -x knows is cut at 15
# characters (kde-rice-launch).
restart() {
    changed "^\./usr/bin/$1\$" || return 1
    if pkill -f "^([^ ]*/)?$1( |\$)"; then
        sleep 0.5
        shift
        setsid "$@" > /dev/null 2>&1 < /dev/null &
        echo "restarted $1"
        return 0
    fi
    return 1
}
# (one that is not running stays so, and the rest goes on)
restart kde-rice-launcher kde-rice-launcher --background || true
restart kde-rice-gaps kde-rice-gaps || true
restart kde-rice-tray kde-rice-tray || true
# the tray's entries that follow the icon theme: started now if they are not
# running yet (later, by the autostart entry at login), unless kde-rice is
# off (kde-rice --restore-defaults)
off="${XDG_STATE_HOME:-$HOME/.local/state}/kde-rice/off.json"
if ! pgrep -f '^([^ ]*/)?kde-rice-tray( |$)' > /dev/null && [ ! -e "$off" ]; then
    setsid kde-rice-tray > /dev/null 2>&1 < /dev/null &
    echo "started kde-rice-tray"
fi

# Copies of kde-rice's widgets in the home directory (left from trying one
# out) would be loaded in place of the installed ones.
copies=0
if rm -rv "$HOME"/.local/share/plasma/plasmoids/io.github.mak1a4.kde-rice.* 2> /dev/null | grep -q .; then
    copies=1
    echo "removed kde-rice's widgets from ~/.local/share/plasma/plasmoids"
fi

# what Plasma keeps once loaded: the wallpaper's and the widgets' code, the
# icons, the application style (its menus)
if { changed '^\./usr/share/plasma/wallpapers/io\.github\.mak1a4\.kde-rice\.' \
    || changed '^\./usr/share/plasma/plasmoids/io\.github\.mak1a4\.kde-rice\.' \
    || changed '/plugins/styles/kde-rice\.so$' \
    || [ "$whitesur" = 1 ] || [ "$copies" = 1 ]; } \
    && pgrep -x plasmashell > /dev/null; then
    systemctl --user restart plasma-plasmashell.service
    echo "restarted Plasma for the new wallpaper, widgets, icons or style"
fi

# (a restart of KWin's own may take a moment to show)
if [ -n "$kwin_before" ]; then
    sleep 1
    kwin_after=$(pgrep -x kwin_wayland || true)
    if [ "$kwin_after" != "$kwin_before" ]; then
        systemctl --user restart xdg-desktop-portal.service
        echo "KWin was restarted meanwhile: restarted the desktop portal for it"
    fi
fi

echo
if changed '/plugins/kwin/plugins/kderice_' || changed '/plugins/org\.kde\.kdecoration3/io\.github\.mak1a4\.kde-rice\.'; then
    echo "KWin's parts changed (rounded corners, title bars, popups across the gaps): log out and back in to load them."
fi
if changed '/plugins/styles/kde-rice\.so$'; then
    echo "The application style changed: applications take it when they start again (KWin's menus at the next login)."
fi
if changed '^\./usr/lib/kde-rice/dolphin/'; then
    echo "Dolphin's library changed: Dolphin takes it when it starts again."
fi
if changed '^\./usr/bin/kde-rice$' && pgrep -x kde-rice > /dev/null; then
    echo "The hub is open in its old build: close it and open it again."
fi
if [ -e "$off" ]; then
    echo "kde-rice is off: kde-rice --reapply turns it on again."
fi
echo "kde-rice installed."
