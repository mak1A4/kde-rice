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
    # app icons a little larger: WhiteSur leaves a margin round each tile
    COMMAND $<TARGET_FILE:kde-rice-trim-icons> ${themes}/kde-rice-whitesur/apps ${themes}/kde-rice-whitesur-dark/apps
    DEPENDS ${whitesur_SOURCE_DIR}/install.sh kde-rice-trim-icons
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Building WhiteSur ${WHITESUR_VERSION} as kde-rice-whitesur"
    VERBATIM
)
add_custom_target(kderice_whitesur ALL DEPENDS ${themes}/kde-rice-whitesur/index.theme)

install(DIRECTORY ${themes}/ DESTINATION ${KDE_INSTALL_ICONDIR} USE_SOURCE_PERMISSIONS)
