# kde-rice-gaps: room kept free along the screen's edges, so maximized
# windows keep a gap to them and a floating panel keeps floating
find_package(Qt6 REQUIRED COMPONENTS Gui DBus)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Config DBusAddons)
find_package(LayerShellQt REQUIRED)

add_executable(kde-rice-gaps main.cpp)
target_link_libraries(kde-rice-gaps PRIVATE Qt6::Gui Qt6::DBus KF6::ConfigCore KF6::DBusAddons LayerShellQt::Interface)
install(TARGETS kde-rice-gaps ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

configure_file(gaps-autostart.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${KDERICE_ID}.gaps.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${KDERICE_ID}.gaps.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
