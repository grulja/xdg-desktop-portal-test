
set(xdg_desktop_portal_test_SRCS
    desktopportal.cpp
    screencast.cpp
    screencaststream.cpp
    session.cpp
    xdg-desktop-portal-test.cpp
)

add_executable(xdg-desktop-portal-test ${xdg_desktop_portal_test_SRCS})

target_link_libraries(xdg-desktop-portal-test
    Qt5::Core
    Qt5::DBus
    Qt5::Gui
    PipeWire::PipeWire
)

install(TARGETS xdg-desktop-portal-test DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBEXECDIR})

add_subdirectory(test)
