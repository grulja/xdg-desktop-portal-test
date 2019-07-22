configure_file(org.freedesktop.impl.portal.desktop.test.cmake.in org.freedesktop.impl.portal.desktop.test.service @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.impl.portal.desktop.test.service DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/dbus-1/services)
install(FILES test.portal DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal/portals)
