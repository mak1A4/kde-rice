kcoreaddons_add_plugin(kderice_roundcorners INSTALL_NAMESPACE "kwin/plugins")
target_sources(kderice_roundcorners PRIVATE main.cpp)
target_link_libraries(kderice_roundcorners KWin::kwin KDecoration3::KDecoration)
