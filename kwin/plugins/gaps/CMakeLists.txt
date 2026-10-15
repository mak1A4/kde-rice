kcoreaddons_add_plugin(kderice_gaps INSTALL_NAMESPACE "kwin/plugins")
target_sources(kderice_gaps PRIVATE main.cpp)
target_link_libraries(kderice_gaps KWin::kwin)
