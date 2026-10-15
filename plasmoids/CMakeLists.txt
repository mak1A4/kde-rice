# Desktop widgets (Plasma applets, QML only). What they share (common/: the
# frosted card) is installed into each one's package, as Plasma loads a
# package's QML from the package alone.
set(KDERICE_PLASMOIDS_LIST nowplaying rings)

foreach(plasmoid ${KDERICE_PLASMOIDS_LIST})
    set(dir ${KDE_INSTALL_DATADIR}/plasma/plasmoids/${KDERICE_ID}.${plasmoid})
    install(DIRECTORY ${plasmoid}/ DESTINATION ${dir})
    install(DIRECTORY common/ DESTINATION ${dir}/contents/ui/common)
endforeach()
