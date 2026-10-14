# kde-rice-tray: tray entries that name a theme icon, in place of
# applications' that hand the tray a picture of their own (main.cpp)
find_package(Qt6 REQUIRED COMPONENTS Core DBus)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons)

add_executable(kde-rice-tray main.cpp)
target_link_libraries(kde-rice-tray PRIVATE Qt6::Core Qt6::DBus KF6::CoreAddons)
target_compile_definitions(kde-rice-tray PRIVATE KDERICE_SOURCE_TRAY_RULES="${CMAKE_CURRENT_SOURCE_DIR}/tray-rules.json")
install(TARGETS kde-rice-tray ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES tray-rules.json DESTINATION ${KDE_INSTALL_DATADIR}/kde-rice)

configure_file(tray-autostart.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${KDERICE_ID}.tray.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${KDERICE_ID}.tray.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
