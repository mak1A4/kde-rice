# Themes: the files of colours (data/), the templates they are written into
# for each program (templates/), the Plasma wallpaper that shows a theme's
# background (wallpaper/), and the code that applies them (src/), which the
# hub, the launcher and kde-rice-theme share.
find_package(Qt6 REQUIRED COMPONENTS Core Gui DBus)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Config)

add_library(kderice_themes STATIC
    src/applier.cpp src/applier.h
    src/colour.cpp src/colour.h
    src/theme.cpp src/theme.h
)
target_include_directories(kderice_themes PUBLIC src)
target_link_libraries(kderice_themes PUBLIC Qt6::Core Qt6::Gui PRIVATE Qt6::DBus KF6::ConfigCore)
# found when nothing is installed, for running from the build directory
target_compile_definitions(kderice_themes PRIVATE
    KDERICE_SOURCE_THEMES="${CMAKE_CURRENT_SOURCE_DIR}/data"
    KDERICE_SOURCE_TEMPLATES="${CMAKE_CURRENT_SOURCE_DIR}/templates"
)
set_target_properties(kderice_themes PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(kde-rice-theme cli/main.cpp)
target_link_libraries(kde-rice-theme PRIVATE kderice_themes)
install(TARGETS kde-rice-theme ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(DIRECTORY data/ DESTINATION ${KDE_INSTALL_DATADIR}/kde-rice/themes)
install(DIRECTORY templates/ DESTINATION ${KDE_INSTALL_DATADIR}/kde-rice/templates)
install(DIRECTORY wallpaper/ DESTINATION ${KDE_INSTALL_DATADIR}/plasma/wallpapers/${KDERICE_ID}.background)
