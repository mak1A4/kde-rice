# kde-rice's Breeze window decoration: Breeze as released, in the version of
# the KWin installed here, with rounded.patch on it (the corners stay on
# maximized and tiled windows; the radius is the one of
# kwin/plugins/roundcorners). Built under a name of its own next to the
# installed Breeze; it reads Breeze's settings (breezerc) and uses its
# settings page.
#
# The sources are fetched from download.kde.org when configuring;
# BREEZE_URL can name a local copy of the archive instead.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS ColorScheme Config CoreAddons GuiAddons I18n IconThemes)
find_package(KDecoration3 REQUIRED)
find_package(KWin REQUIRED)

# Not kept in the cache unless given: after a Plasma update, the next
# configure fetches the new Breeze.
set(BREEZE_URL "" CACHE STRING "Breeze's source archive (default: the installed KWin's release from download.kde.org)")
set(breeze_url "${BREEZE_URL}")
if(NOT breeze_url)
    set(breeze_url "https://download.kde.org/stable/plasma/${KWin_VERSION}/breeze-${KWin_VERSION}.tar.xz")
endif()

# A changed patch is a new source directory: FetchContent patches only what
# it has just unpacked.
file(MD5 "${CMAKE_CURRENT_SOURCE_DIR}/rounded.patch" patch_hash)
string(SUBSTRING "${patch_hash}" 0 12 patch_hash)

include(FetchContent)
FetchContent_Declare(breeze
    SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/breeze-src-${patch_hash}"
    URL "${breeze_url}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PATCH_COMMAND patch -p1 -N -i "${CMAKE_CURRENT_SOURCE_DIR}/rounded.patch"
    # only the sources: Breeze's own build is not wanted
    SOURCE_SUBDIR not-built
)
FetchContent_MakeAvailable(breeze)
if(NOT EXISTS "${breeze_SOURCE_DIR}/kdecoration/breezedecoration.cpp")
    message(FATAL_ERROR "${breeze_SOURCE_DIR} is not a Breeze source tree")
endif()

# Breeze's own metadata would give the plugin Breeze's id
configure_file(breeze.json "${breeze_SOURCE_DIR}/kdecoration/breeze.json" COPYONLY)

# Breeze is written for the casts KDE's compiler settings forbid
remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII -DQT_NO_CAST_FROM_BYTEARRAY)

set(decoration ${breeze_SOURCE_DIR}/kdecoration)
set(common ${breeze_SOURCE_DIR}/libbreezecommon)

set(breezedecoration_SRCS
    ${decoration}/breezebutton.cpp
    ${decoration}/breezedecoration.cpp
    ${decoration}/breezesettingsprovider.cpp
    ${decoration}/breezeexceptionlist.cpp
    ${common}/breezeboxshadowrenderer.cpp
)
kconfig_add_kcfg_files(breezedecoration_SRCS ${decoration}/breezesettings.kcfgc)

add_library(kderice_breeze MODULE ${breezedecoration_SRCS})
target_compile_definitions(kderice_breeze PRIVATE TRANSLATION_DOMAIN="breeze_kwin_deco")
target_include_directories(kderice_breeze PRIVATE ${decoration} ${common})
set_target_properties(kderice_breeze PROPERTIES
    OUTPUT_NAME ${KDERICE_ID}.breeze
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/org.kde.kdecoration3
)
target_link_libraries(kderice_breeze PRIVATE
    Qt6::DBus
    Qt6::Widgets
    KF6::ColorScheme
    KF6::ConfigGui
    KF6::CoreAddons
    KF6::GuiAddons
    KF6::I18n
    KF6::IconThemes
    KDecoration3::KDecoration
)
install(TARGETS kderice_breeze DESTINATION ${KDE_INSTALL_PLUGINDIR}/org.kde.kdecoration3)
