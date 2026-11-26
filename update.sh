#!/bin/sh
# Updates the system, and kde-rice with it when KWin or Dolphin changed:
#
#   ./update.sh            (arguments are passed on to paru -Syu)
#
# kde-rice-git asks for the KWin release it was built against (kwin=6.8.1,
# packaging/PKGBUILD), so a plain pacman -Syu stops at a newer KWin. Here
# the update goes through with that pin assumed met, and when the KWin
# installed afterwards is another release than the one kde-rice was built
# for, install.sh builds it again against the new one (which then pins it).
# KWin loads the new plugins at the next login.
set -e

repo=$(cd "$(dirname "$0")" && pwd)

# the pin of the kde-rice-git installed (kwin=<release>), if there is one
pin=$(LC_ALL=C pacman -Qi kde-rice-git 2> /dev/null | sed -n 's/^Depends On *: //p' | tr -s ' ' '\n' | grep '^kwin=' || true)

if command -v paru > /dev/null; then
    paru -Syu ${pin:+--assume-installed "$pin"} "$@"
else
    sudo pacman -Syu ${pin:+--assume-installed "$pin"} "$@"
fi

# the KWin release installed now, as the plugins name it (as PKGBUILD's
# _kwin_release reads it)
release=$(sed -n 's/^#define KWIN_PLUGIN_VERSION_STRING "\([^"]*\)".*/\1/p' /usr/include/kwin/config-kwin.h 2> /dev/null)

if [ -n "$pin" ] && [ "$pin" != "kwin=$release" ]; then
    echo
    echo "KWin is now $release, kde-rice was built for ${pin#kwin=}: building it again."
    exec "$repo/install.sh"
fi

# Dolphin likewise: kde-rice's build of its library is used only beside the
# Dolphin it was built for (dolphin/), so another Dolphin is built for anew
dolphin=$(readlink /usr/lib/libdolphinprivate.so.6 2> /dev/null | sed -n 's/^libdolphinprivate\.so\.//p')
if [ -n "$dolphin" ] && [ -d /usr/lib/kde-rice/dolphin ] && [ ! -e "/usr/lib/kde-rice/dolphin/$dolphin" ]; then
    echo
    echo "Dolphin is now $dolphin, kde-rice's build of its library is for another: building kde-rice again."
    exec "$repo/install.sh"
fi
