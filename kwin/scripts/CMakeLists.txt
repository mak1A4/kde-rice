# KWin scripts: JavaScript, through KWin's stable scripting API, so not to be
# built again after a Plasma update. Each directory is a package whose name
# is its id.
install(DIRECTORY notifications/ DESTINATION ${KDE_INSTALL_DATADIR}/kwin/scripts/kderice_notifications)
