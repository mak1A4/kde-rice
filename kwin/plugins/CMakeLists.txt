# KWin's internal API changes with every Plasma release: these are built
# against the KWin installed here, and again after a Plasma update.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons)
find_package(KDecoration3 REQUIRED)
find_package(KWin REQUIRED)

add_subdirectory(roundcorners)
