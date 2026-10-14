# The login screen kept in step with the session (login-screen.sh, as root)
install(PROGRAMS login-screen.sh DESTINATION ${KDE_INSTALL_LIBDIR}/kde-rice RENAME login-screen)
install(FILES io.github.mak1a4.kde-rice.login-screen.policy DESTINATION ${KDE_INSTALL_DATADIR}/polkit-1/actions)
