IBus with Wayland support
=========================

Dependencies
------------

* gtk+ master
* wayland master
* weston master from https://github.com/openismus/weston

Build/Install
-------------

* Build Wayland/Weston according to http://wayland.freedesktop.org/building.html
* Build ibus
   * ./autogen.sh --prefix=$WLD
   * make && make install
* Make sure it shows 'Build wayland support    yes' after running autogen.sh

Run
---

* Launch Weston
   * dbus-launch weston
* Start IBus inside of Weston
   * $WLD/bin/ibus-daemon &
   * $WLD/libexec/ibus-ui-wayland &
   * $WLD/bin/ibus engine libpinyin
* Try out with editor example
   * clients/editor

State
-----

* The Wayland support in bus is mostly complete
   * IBusPreeditFocusMode is not checked yet
   * The input state serials should be proxyed to the engines
     (needs some new API there)
* There should be support added to disable wayland in build system/ibus-daemon
* The UI is more a demo it is just a copy from the gtk3 UI (since it was not
  possible to make gdk-x11 optional in the gtk3 vala code). So it should be
  merged into the gtk3 code for upstream IBus.

See also https://code.google.com/p/ibus/issues/detail?id=1617
