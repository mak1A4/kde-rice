# kde-rice's widget style: Breeze as released, in the version of the Plasma
# installed here (KWin's), with fill.patch on it: what is selected or under
# the pointer in a menu, a list or a tree is a fill without Breeze's outline.
# Built under a name of its own (kde-rice) next to the installed Breeze,
# which stays as it is; it reads Breeze's settings (breezerc) and uses its
# settings page. Applications that ask the style for its name get kde-rice's:
# Dolphin, which draws its entries itself for Breeze, has this style draw
# them.
#
# The sources are fetched from download.kde.org when configuring;
# BREEZE_URL can name a local copy of the archive instead (as for the
# window decoration, kwin/decoration/breeze).
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS ColorScheme Config CoreAddons FrameworkIntegration GuiAddons IconThemes WindowSystem)
find_package(KWin REQUIRED)
include(ECMQtDeclareLoggingCategory)

set(BREEZE_URL "" CACHE STRING "Breeze's source archive (default: the installed KWin's release from download.kde.org)")
set(breeze_url "${BREEZE_URL}")
if(NOT breeze_url)
    set(breeze_url "https://download.kde.org/stable/plasma/${KWin_VERSION}/breeze-${KWin_VERSION}.tar.xz")
endif()

# A changed patch is a new source directory: FetchContent patches only what
# it has just unpacked.
file(MD5 "${CMAKE_CURRENT_SOURCE_DIR}/fill.patch" patch_hash)
string(SUBSTRING "${patch_hash}" 0 12 patch_hash)

include(FetchContent)
FetchContent_Declare(breezestyle
    SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/breeze-style-src-${patch_hash}"
    URL "${breeze_url}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PATCH_COMMAND patch -p1 -N -i "${CMAKE_CURRENT_SOURCE_DIR}/fill.patch"
    # only the sources: Breeze's own build is not wanted
    SOURCE_SUBDIR not-built
)
FetchContent_MakeAvailable(breezestyle)
if(NOT EXISTS "${breezestyle_SOURCE_DIR}/kstyle/breezestyle.cpp")
    message(FATAL_ERROR "${breezestyle_SOURCE_DIR} is not a Breeze source tree")
endif()

# Breeze is written for the casts KDE's compiler settings forbid
remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII -DQT_NO_CAST_FROM_BYTEARRAY)

set(kstyle ${breezestyle_SOURCE_DIR}/kstyle)
set(common ${breezestyle_SOURCE_DIR}/libbreezecommon)

# what Breeze's own build finds out: all of it is there (asked for above)
set(BREEZE_HAVE_QTQUICK 1)
set(BREEZE_HAVE_KSTYLE 1)
set(HAVE_QTDBUS 1)
configure_file(${kstyle}/config-breeze.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-breeze.h)

# every source of Breeze's style but its settings page (Breeze's is used)
file(GLOB style_SRCS CONFIGURE_DEPENDS ${kstyle}/*.cpp ${kstyle}/animations/*.cpp ${kstyle}/debug/*.cpp)
list(APPEND style_SRCS ${common}/breezeboxshadowrenderer.cpp)
kconfig_add_kcfg_files(style_SRCS ${breezestyle_SOURCE_DIR}/kdecoration/breezesettings.kcfgc)
kconfig_add_kcfg_files(style_SRCS ${kstyle}/breezestyleconfigdata.kcfgc)

add_library(kderice_style MODULE ${style_SRCS})
ecm_qt_declare_logging_category(kderice_style
    HEADER breeze_logging.h
    IDENTIFIER BREEZE
    CATEGORY_NAME breeze6
    DEFAULT_SEVERITY Warning
)
target_include_directories(kderice_style PRIVATE ${kstyle} ${kstyle}/animations ${kstyle}/debug ${common} ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(kderice_style PROPERTIES
    OUTPUT_NAME kde-rice
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/styles
)
target_link_libraries(kderice_style PRIVATE
    Qt6::DBus
    Qt6::Quick
    Qt6::Widgets
    KF6::ColorScheme
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::CoreAddons
    KF6::GuiAddons
    KF6::IconThemes
    KF6::Style
    KF6::WindowSystem
)
install(TARGETS kderice_style DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/styles)
# for Plasma's list of application styles
install(FILES kde-rice.themerc DESTINATION ${KDE_INSTALL_DATADIR}/kstyle/themes)
