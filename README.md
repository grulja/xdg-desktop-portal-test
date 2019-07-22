# xdg-desktop-portal-test

A test backend implementation for [xdg-desktop-portal](http://github.com/flatpak/xdg-desktop-portal)

## Building xdg-desktop-portal-test

### Dependencies:
 - xdg-desktop-portal (runtime dependency)
 - Qt 5 (build dependency)

### Build instructions:
```
$ mkdir build && cd build
$ cmake .. [your_options]
$ make -jX
$ make install
```
