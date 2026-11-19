include(GNUInstallDirs)

install(FILES sheaf.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES sheaf.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(DIRECTORY icons/hicolor DESTINATION ${CMAKE_INSTALL_DATADIR}/icons)
install(FILES sheaf.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES sheaf.bash DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions RENAME sheaf)
