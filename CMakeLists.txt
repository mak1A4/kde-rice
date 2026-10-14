# kde-rice: one build for the hub application and everything it switches on
# in Plasma and KWin. Each part can be left out with its option.
cmake_minimum_required(VERSION 3.20)
project(kde-rice VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_MAJOR_VERSION 6)
set(QT_DEFAULT_MAJOR_VERSION 6)
set(KF_MIN_VERSION 6.20)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

option(KDERICE_APP "The hub application" ON)
option(KDERICE_LAUNCHER "The launcher, kde-rice's KRunner" ON)
option(KDERICE_THEMES "Themes and what applies them" ON)
option(KDERICE_KWIN_SCRIPTS "KWin scripts" ON)
option(KDERICE_KWIN_PLUGINS "KWin plugins (built against the installed KWin)" ON)
option(KDERICE_DECORATION "kde-rice's Breeze window decoration (fetches Breeze's sources)" ON)

# the reverse-DNS prefix every part is installed under
set(KDERICE_ID "io.github.mak1a4.kde-rice")

if(KDERICE_THEMES)
    add_subdirectory(themes)
endif()
if(KDERICE_APP)
    add_subdirectory(app)
endif()
if(KDERICE_LAUNCHER)
    add_subdirectory(launcher)
endif()
if(KDERICE_KWIN_SCRIPTS)
    add_subdirectory(kwin/scripts)
endif()
if(KDERICE_KWIN_PLUGINS)
    add_subdirectory(kwin/plugins)
endif()
if(KDERICE_DECORATION)
    add_subdirectory(kwin/decoration)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
