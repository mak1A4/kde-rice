find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets DBus)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Config CoreAddons I18n)

# only needed when the application runs
include(ECMFindQmlModule)
ecm_find_qmlmodule(org.kde.kirigami REQUIRED)
ecm_find_qmlmodule(org.kde.kirigamiaddons.formcard 1.0 REQUIRED)

qt_policy(SET QTP0001 NEW)

qt_add_executable(kde-rice
    src/main.cpp
    src/configentry.cpp src/configentry.h
    src/kwin.cpp src/kwin.h
    src/kwinplugin.cpp src/kwinplugin.h
)

qt_add_qml_module(kde-rice
    URI io.github.mak1a4.rice
    QML_FILES
        qml/Main.qml
        qml/pages/AppearancePage.qml
        qml/pages/WindowsPage.qml
        qml/pages/PanelsPage.qml
        qml/pages/EffectsPage.qml
)

target_include_directories(kde-rice PRIVATE src)

target_compile_definitions(kde-rice PRIVATE
    KDERICE_ID="${KDERICE_ID}"
    KDERICE_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(kde-rice PRIVATE
    Qt6::DBus
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::I18n
    KF6::I18nQml
)

install(TARGETS kde-rice ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES ${KDERICE_ID}.desktop DESTINATION ${KDE_INSTALL_APPDIR})
