
add_executable(screencasttest screencasttest.cpp ../screencaststream.cpp)
add_test(screencasttest screencasttest)

target_link_libraries(screencasttest Qt5::DBus Qt5::Gui Qt5::Test PipeWire::PipeWire GLIB2::GLIB2)
