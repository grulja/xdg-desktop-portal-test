# xdg-desktop-portal-test

A test backend implementation for [xdg-desktop-portal](http://github.com/flatpak/xdg-desktop-portal)

## Building xdg-desktop-portal-test

### Dependencies:
 - xdg-desktop-portal (runtime dependency)
 - Qt 5 (build dependency)
 - PipeWire (build dependency)

### Build instructions:
```
$ mkdir build && cd build
$ cmake .. [your_options]
$ make -jX
$ make install
```

### Run instructions:
The backend will be started automatically when XDG_CURRENT_DESKTOP is set to
"KDE" or "gnome", but it actually doesn't require any desktop to be running.
You just need to make sure DBus session is available and no other backend is
available, as the other one would be loaded instead. The test is installed
to your $PREFIX/$LIBDIR/xdp/screencasttest and can be executed from there.
