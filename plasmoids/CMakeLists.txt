# Desktop widgets (Plasma applets, QML only). What they share (common/: the
# frosted card) is installed into each one's package, as Plasma loads a
# package's QML from the package alone.
set(KDERICE_PLASMOIDS_LIST nowplaying rings)

foreach(plasmoid ${KDERICE_PLASMOIDS_LIST})
    set(dir ${KDE_INSTALL_DATADIR}/plasma/plasmoids/${KDERICE_ID}.${plasmoid})
    install(DIRECTORY ${plasmoid}/ DESTINATION ${dir})
    install(DIRECTORY common/ DESTINATION ${dir}/contents/ui/common)
endforeach()

# The launcher: Plasma's Application Launcher (Kickoff) as released, in the
# version of the Plasma installed here (KWin's), with kickoff/kickoff.patch
# on it: who and where written out always in its header (not the name, and
# the rest under the pointer), and the category that is open drawn as
# pressed, so that it is told from the one under the pointer; Favorites a
# heart (icons/glyphs/kde-rice-favorites.svg) for the bookmark. A widget of
# its own next to Plasma's, which stays: chosen with Show Alternatives on
# the launcher in the panel. Its QML alone, as a package (Plasma's is built
# into a plugin); what it asks of Plasma's launchers in C++
# (org.kde.plasma.private.kicker) is the installed one.
#
# The sources are fetched from download.kde.org when configuring;
# PLASMA_DESKTOP_URL can name a local copy of the archive instead.
option(KDERICE_KICKOFF "kde-rice's Application Launcher (fetches plasma-desktop's sources)" ON)
if(KDERICE_KICKOFF)
    find_package(KWin REQUIRED)
    set(PLASMA_DESKTOP_URL "" CACHE STRING "plasma-desktop's source archive (default: the installed KWin's release from download.kde.org)")
    set(plasma_desktop_url "${PLASMA_DESKTOP_URL}")
    if(NOT plasma_desktop_url)
        set(plasma_desktop_url "https://download.kde.org/stable/plasma/${KWin_VERSION}/plasma-desktop-${KWin_VERSION}.tar.xz")
    endif()

    # A changed patch is a new source directory: FetchContent patches only
    # what it has just unpacked.
    file(MD5 "${CMAKE_CURRENT_SOURCE_DIR}/kickoff/kickoff.patch" patch_hash)
    string(SUBSTRING "${patch_hash}" 0 12 patch_hash)

    include(FetchContent)
    FetchContent_Declare(plasmadesktop
        SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/plasma-desktop-src-${patch_hash}"
        URL "${plasma_desktop_url}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        PATCH_COMMAND patch -p1 -N -i "${CMAKE_CURRENT_SOURCE_DIR}/kickoff/kickoff.patch"
        # only the sources: plasma-desktop's own build is not wanted
        SOURCE_SUBDIR not-built
    )
    FetchContent_MakeAvailable(plasmadesktop)
    set(kickoff "${plasmadesktop_SOURCE_DIR}/applets/kickoff")
    if(NOT EXISTS "${kickoff}/main.qml")
        message(FATAL_ERROR "${plasmadesktop_SOURCE_DIR} is not a plasma-desktop source tree with Kickoff's QML in applets/kickoff")
    endif()

    set(dir ${KDE_INSTALL_DATADIR}/plasma/plasmoids/${KDERICE_ID}.kickoff)
    install(FILES kickoff/metadata.json DESTINATION ${dir})
    # Kickoff's two singletons, which its plugin's build declares
    install(FILES kickoff/qmldir DESTINATION ${dir}/contents/ui)
    install(DIRECTORY ${kickoff}/ DESTINATION ${dir}/contents/ui
        FILES_MATCHING PATTERN "*.qml" PATTERN "*.js"
        PATTERN "config.qml" EXCLUDE
    )
    install(FILES ${kickoff}/config.qml ${kickoff}/main.xml DESTINATION ${dir}/contents/config)
endif()
