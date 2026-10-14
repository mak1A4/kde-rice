# kde-rice's own icons: WhiteSur, built from its released sources with its
# own installer under the name kde-rice-whitesur (with -light and -dark
# variants, and the KDE logo where WhiteSur has Apple's), its app icons
# trimmed to fill their canvas (kde-rice-trim-icons), and shipped with
# kde-rice, so no icon theme has to be installed besides. The user's own
# icons go on top of it (themes/src/icons.cpp).
#
# A newer release: WHITESUR_VERSION and its SHA-256.
set(WHITESUR_VERSION "2026-09-10")
set(WHITESUR_SHA256 "406c9cd59705583f1754b0eaca96cc48bafda042b88ef143f16d8ae1820ecd95")

include(FetchContent)
FetchContent_Declare(whitesur
    URL "https://github.com/vinceliuice/WhiteSur-icon-theme/archive/refs/tags/${WHITESUR_VERSION}.tar.gz"
    URL_HASH SHA256=${WHITESUR_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    # only the sources: its installer is run below
    SOURCE_SUBDIR not-built
)
FetchContent_MakeAvailable(whitesur)

find_program(BASH bash REQUIRED)
find_program(GTK_UPDATE_ICON_CACHE gtk-update-icon-cache REQUIRED)

set(themes ${CMAKE_CURRENT_BINARY_DIR}/themes)
add_custom_command(
    OUTPUT ${themes}/kde-rice-whitesur/index.theme
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${themes}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${themes}
    COMMAND ${CMAKE_COMMAND} -E env DESKTOP_SESSION=plasma ${BASH} ${whitesur_SOURCE_DIR}/install.sh -d ${themes} -n kde-rice-whitesur -p > ${CMAKE_CURRENT_BINARY_DIR}/whitesur-install.log
    # the system makes its caches of icon themes itself, when they are installed
    COMMAND find ${themes} -name icon-theme.cache -delete
    # kde-rice's own pictures in place of WhiteSur's (extra/: Steam's tray
    # glyph in the colour scheme's text colour), in every variant that has
    # its own status icons
    COMMAND sh -c "for dir in \"$1\"/*/status/32; do [ -L \"$dir\" ] || cp -f \"$2\"/*.svg \"$dir\"/; done" sh ${themes} ${CMAKE_CURRENT_SOURCE_DIR}/extra
    # the tray glyphs kde-rice-tray names (tray/<app>.svg as kde-rice-<app>-tray)
    COMMAND sh -c "for dir in \"$1\"/*/status/32; do [ -L \"$dir\" ] && continue; for f in \"$2\"/*.svg; do n=$(basename \"$f\" .svg); case $n in *-attention) b=kde-rice-\${n%-attention}-tray-attention ;; *) b=kde-rice-$n-tray ;; esac; cp -f \"$f\" \"$dir/$b.svg\"; done; done" sh ${themes} ${CMAKE_CURRENT_SOURCE_DIR}/tray
    # app icons a little larger: WhiteSur leaves a margin round each tile
    COMMAND $<TARGET_FILE:kde-rice-trim-icons> ${themes}/kde-rice-whitesur/apps ${themes}/kde-rice-whitesur-dark/apps
    DEPENDS ${whitesur_SOURCE_DIR}/install.sh kde-rice-trim-icons ${CMAKE_CURRENT_SOURCE_DIR}/extra/steam_tray_mono.svg
            ${CMAKE_CURRENT_SOURCE_DIR}/tray/outlook.svg ${CMAKE_CURRENT_SOURCE_DIR}/tray/teams.svg
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Building WhiteSur ${WHITESUR_VERSION} as kde-rice-whitesur"
    VERBATIM
)
add_custom_target(kderice_whitesur ALL DEPENDS ${themes}/kde-rice-whitesur/index.theme)

install(DIRECTORY ${themes}/ DESTINATION ${KDE_INSTALL_ICONDIR} USE_SOURCE_PERMISSIONS)

# kde-rice's tray glyphs for applications that hand the tray a picture of
# their own (themes/src/applier.cpp draws them in the mode's colour)
install(DIRECTORY tray/ DESTINATION ${KDE_INSTALL_DATADIR}/kde-rice/tray)
