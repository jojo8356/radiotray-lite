# Compatibility

Radio Tray Lite is intentionally kept on GTK 3 for the tray build.

The current application is a tray-first GTK application built around
`Gtk::Menu` and AppIndicator. Libadwaita is a GTK 4 library, so a real
libadwaita migration also requires a GTK 3 to GTK 4 migration and a new tray
menu implementation. Keeping the tray build on GTK 3 is the compatibility path
for older and current Debian/Ubuntu releases.

## Supported build variants

- GTK: `gtkmm-3.0`
- AppIndicator: `ayatana-appindicator-glib`, `appindicator3-0.1`, or
  `ayatana-appindicator3-0.1`
- GStreamer C++ bindings: `gstreamermm-1.0` or legacy `gstreamermm-0.10`
- GStreamer runtime plugins: `gstreamer1.0-plugins-base` and
  `gstreamer1.0-plugins-good` by default; `gstreamer1.0-plugins-bad` is
  optional
- Packaging: CPack DEB

## Distribution notes

Older Ubuntu releases commonly provide the legacy AppIndicator package:

- `libappindicator3-dev`
- pkg-config module: `appindicator3-0.1`
- runtime package: `libappindicator3-1`

Newer Debian/Ubuntu releases commonly provide the Ayatana replacement:

- `libayatana-appindicator-glib-dev`
- pkg-config module: `ayatana-appindicator-glib`
- runtime package: `libayatana-appindicator-glib2`

Older Debian/Ubuntu releases commonly provide the GTK 3 Ayatana library:

- `libayatana-appindicator3-dev`
- pkg-config module: `ayatana-appindicator3-0.1`
- runtime package: `libayatana-appindicator3-1`

CMake prefers the GLib-only Ayatana library when it is available because the
GTK 3 Ayatana library emits a deprecation warning at runtime. If it is not
available, CMake falls back to the legacy GTK 3 variants and the generated
`.deb` declares the runtime dependency matching the variant used at build time.

The backend can also be selected explicitly. Edit `build-options.conf`:

```sh
APPINDICATOR_BACKEND=auto
```

Accepted values:

- `auto`: prefer `ayatana-appindicator-glib`, then `appindicator3-0.1`, then
  `ayatana-appindicator3-0.1`
- `ayatana-glib`: require `ayatana-appindicator-glib`
- `appindicator`: require `appindicator3-0.1`
- `ayatana-gtk3`: require `ayatana-appindicator3-0.1`

`./build-deb.sh` validates this value before installing dependencies, and
CMake validates it again before selecting the pkg-config module. A manually
selected backend fails the build if the matching development package is not
available.

## Build

Use the helper script:

```sh
./build-deb.sh --clean
```

If dependencies are already installed:

```sh
./build-deb.sh --no-install-deps --clean
```

To override `build-options.conf` for a single build:

```sh
./build-deb.sh --clean --appindicator-backend ayatana-glib
```

If a target distribution needs codecs or demuxers from the bad plugin set:

```sh
./build-deb.sh --clean --with-gstreamer-bad
```
