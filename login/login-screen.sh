#!/bin/sh
# Brings the login screen (Plasma Login Manager) in step with the session of
# the user who runs it, through pkexec or sudo:
#
#   pkexec /usr/lib/kde-rice/login-screen      (the hub's "Update Login Screen")
#   sudo /usr/lib/kde-rice/login-screen        (install.sh)
#
# The login screen runs as a user of its own and reads nothing in a home
# directory. So:
#   - /var/lib/kde-rice/background is made, the user's and readable by all:
#     kde-rice puts a copy of the theme's background there at every change
#     of theme, without root;
#   - the login screen is set to kde-rice's wallpaper, which shows that copy
#     (/etc/plasmalogin.conf.d/kde-rice.conf; a wallpaper chosen in System
#     Settings goes to /etc/plasmalogin.conf and counts for more);
#   - the user's kdeglobals (colours, fonts, icon theme), plasmarc, kxkbrc,
#     kcminputrc, kwinoutputconfig.json (display scaling and arrangement) and
#     fontconfig/fonts.conf are copied to the login screen's user, as KDE's
#     own "Apply Plasma Settings" does. Its icon theme has to be installed
#     for everyone (in /usr/share/icons) to be found there.
set -e

plugin=io.github.mak1a4.kde-rice.background
greeter=plasmalogin
shared=/var/lib/kde-rice
setting=/etc/plasmalogin.conf.d/kde-rice.conf
synced="kxkbrc kdeglobals plasmarc kcminputrc kwinoutputconfig.json fontconfig/fonts.conf"

if [ "$(id -u)" != 0 ]; then
    echo "login-screen: needs root: run it through pkexec or sudo" >&2
    exit 1
fi
if ! getent passwd "$greeter" > /dev/null; then
    echo "login-screen: there is no Plasma Login Manager here"
    exit 0
fi

# the user who asked: pkexec and sudo both say who
uid=${PKEXEC_UID:-${SUDO_UID:?run it through pkexec or sudo}}
user=$(id -nu "$uid")
config="$(getent passwd "$user" | cut -d: -f6)/.config"
home=$(getent passwd "$greeter" | cut -d: -f6)

# the user's files are read as the user, the greeter's written as the greeter
as_user() { runuser -u "$user" -- "$@"; }
as_greeter() { runuser -u "$greeter" -- "$@"; }

install -d -m 755 "$shared"
install -d -m 755 -o "$user" "$shared/background"
# the background in use now, at once (kde-rice copies it there itself at
# each change of theme from then on)
userdata="$(getent passwd "$user" | cut -d: -f6)/.local/share/kde-rice/background"
for file in "$userdata"/*; do
    [ -e "$file" ] || continue
    name=$(basename "$file")
    as_user cp --dereference "$file" "$shared/background/$name.part"
    chmod 644 "$shared/background/$name.part"
    mv -f "$shared/background/$name.part" "$shared/background/$name"
done

install -d -m 755 "$(dirname "$setting")"
printf '[Greeter]\nWallpaperPluginId=%s\n' "$plugin" > "$setting"
chmod 644 "$setting"

# the greeter keeps colours in a cache it only fills when it has none
as_greeter rm -rf "$home/.cache"
as_greeter mkdir -p "$home/.config/fontconfig"
for file in $synced; do
    if as_user test -r "$config/$file"; then
        as_user cat "$config/$file" | as_greeter tee "$home/.config/$file" > /dev/null
    else
        as_greeter rm -f "$home/.config/$file"
    fi
done

icons=$(as_user kreadconfig6 --file kdeglobals --group Icons --key Theme 2> /dev/null || true)
if [ -n "$icons" ] && [ ! -d "/usr/share/icons/$icons" ]; then
    echo "login-screen: the icon theme $icons is not installed for everyone: the login screen shows none of its icons" >&2
fi
echo "login screen updated for $user"
