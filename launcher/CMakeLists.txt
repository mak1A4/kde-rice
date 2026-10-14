find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets DBus)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Config CoreAddons DBusAddons GlobalAccel I18n Runner WindowSystem)
find_package(LayerShellQt REQUIRED)

# only needed when the application runs
include(ECMFindQmlModule)
ecm_find_qmlmodule(org.kde.kirigami REQUIRED)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(kde-rice-launcher
    src/main.cpp
    src/launcher.cpp src/launcher.h
    src/localactions.cpp src/localactions.h
    src/searchmodel.cpp src/searchmodel.h
)

qt_add_qml_module(kde-rice-launcher
    URI io.github.mak1a4.rice.launcher
    QML_FILES
        qml/Main.qml
        qml/ResultRow.qml
)

target_include_directories(kde-rice-launcher PRIVATE src)

target_compile_definitions(kde-rice-launcher PRIVATE
    KDERICE_ID="${KDERICE_ID}"
    KDERICE_VERSION="${PROJECT_VERSION}"
)

# Qt is shared here: its QML plugins are loaded when the launcher runs, not
# linked in, so there is nothing to scan for
set_target_properties(kde-rice-launcher PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)

target_link_libraries(kde-rice-launcher PRIVATE
    kderice_themes
    Qt6::DBus
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::DBusAddons
    KF6::GlobalAccel
    KF6::I18n
    KF6::I18nQml
    KF6::Runner
    KF6::WindowSystem
    LayerShellQt::Interface
)

install(TARGETS kde-rice-launcher ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# The global shortcut's component has this desktop file's name, so System
# Settings > Shortcuts shows it with its name and icon. Hidden from menus: it
# is opened by its shortcut.
configure_file(launcher.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${KDERICE_ID}.launcher.desktop COPYONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${KDERICE_ID}.launcher.desktop DESTINATION ${KDE_INSTALL_APPDIR})
# at login, hidden until the shortcut; --background so that it never opens
# the popup by itself
configure_file(launcher-autostart.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/autostart/${KDERICE_ID}.launcher.desktop COPYONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/autostart/${KDERICE_ID}.launcher.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
