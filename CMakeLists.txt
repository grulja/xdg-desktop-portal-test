cmake_minimum_required(VERSION 3.0)

project(xdg-desktop-portal-test)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
set(CMAKE_AUTOMOC ON)

include(FeatureSummary)
include(GNUInstallDirs)

find_package(GLIB2)
set_package_properties(GLIB2 PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for ScreenCast portal"
)

find_package(PipeWire)
set_package_properties(PipeWire PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Required for ScreenCast portal"
)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    DBus
    Gui
    Test
)

add_subdirectory(data)
add_subdirectory(src)
