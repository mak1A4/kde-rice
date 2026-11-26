# Dolphin's library (libdolphinprivate, where its views draw their entries)
# as released, in the version of the Dolphin installed here, with
# dolphin.patch on it: the highlight under the pointer drawn at the screen's
# pixel ratio, where Dolphin draws it at 1 and stretches it (blurred at a
# scale of 2), and in the details view the highlight starting a little
# before the icon, not 2 px off it. Only that library is built, and
# installed beside the Dolphin that is, which stays as it is: /usr/lib/kde-rice/dolphin/<version>/,
# found before /usr/lib through ld.so.conf.d (kde-rice.conf), by the link
# "current" that kde-rice-dolphin-link makes to it while Dolphin is that
# version (at install and after every pacman transaction that touches
# Dolphin, kde-rice-dolphin.hook). Another Dolphin is given its own library
# again; update.sh builds kde-rice again for it.
#
# The sources are fetched from download.kde.org when configuring;
# DOLPHIN_URL can name a local copy of the archive instead.
# the Dolphin installed (its version control plugins' interface is the
# same release)
find_package(DolphinVcs)
if(NOT DolphinVcs_FOUND)
    message(STATUS "Dolphin is not installed: kde-rice's Dolphin library is not built")
    return()
endif()
set(dolphin_version ${DolphinVcs_VERSION})

set(DOLPHIN_URL "" CACHE STRING "Dolphin's source archive (default: the installed Dolphin's release from download.kde.org)")
set(dolphin_url "${DOLPHIN_URL}")
if(NOT dolphin_url)
    set(dolphin_url "https://download.kde.org/stable/release-service/${dolphin_version}/src/dolphin-${dolphin_version}.tar.xz")
endif()

file(MD5 "${CMAKE_CURRENT_SOURCE_DIR}/dolphin.patch" patch_hash)
string(SUBSTRING "${patch_hash}" 0 12 patch_hash)

# Dolphin's own build, configured as Arch's package is (the same optional
# parts, so that the library is the same to Dolphin), for the library alone
include(ExternalProject)
ExternalProject_Add(dolphinprivate
    URL "${dolphin_url}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/dolphin-src-${dolphin_version}-${patch_hash}"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/dolphin-build-${dolphin_version}-${patch_hash}"
    PATCH_COMMAND patch -p1 -N -i "${CMAKE_CURRENT_SOURCE_DIR}/dolphin.patch"
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=/usr
        -DBUILD_TESTING=OFF
        -DCMAKE_DISABLE_FIND_PACKAGE_PackageKitQt6=ON
        -DCMAKE_DISABLE_FIND_PACKAGE_KF6DocTools=ON
        "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
        "-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}"
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target dolphinprivate
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS <BINARY_DIR>/bin/libdolphinprivate.so.${dolphin_version}
)
ExternalProject_Get_Property(dolphinprivate BINARY_DIR)

set(dir ${KDE_INSTALL_LIBDIR}/kde-rice/dolphin)
install(FILES ${BINARY_DIR}/bin/libdolphinprivate.so.${dolphin_version}
    DESTINATION ${dir}/${dolphin_version}
    RENAME libdolphinprivate.so.6
)
install(PROGRAMS kde-rice-dolphin-link DESTINATION ${KDE_INSTALL_LIBDIR}/kde-rice)
install(FILES kde-rice.conf DESTINATION ${KDE_INSTALL_SYSCONFDIR}/ld.so.conf.d)
install(FILES kde-rice-dolphin.hook DESTINATION ${KDE_INSTALL_DATADIR}/libalpm/hooks)
