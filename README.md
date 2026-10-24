# Radio Tray Lite

`radiotray-lite` is a lightweight clone of the original [Radio Tray](http://radiotray.sourceforge.net/) online radio streaming player rewritten in C++.

![Screenshot](images/radiotray-lite.png)

## Features

- Runs from the Linux system tray.
- Uses GTK 3 and AppIndicator/Ayatana AppIndicator.
- Plays streams through GStreamer.
- Supports PLS, M3U, ASX, RAM, and XSPF playlists.
- Can build against the newer `libayatana-appindicator-glib` backend when
  available, while keeping compatibility with older AppIndicator libraries.

## Build On Debian/Ubuntu

The recommended path is the helper script. It installs the build dependencies,
configures CMake, builds the binary, and creates a `.deb` package:

```sh
./build-deb.sh --clean
```

Run the script from a complete source checkout or source archive. Downloading
`build-deb.sh` alone is not enough because CMake also needs the top-level
`CMakeLists.txt`, the `src/`, `cmake/`, and `data/` directories.

Generated packages are written under `build-deb/packages/`.

To install the generated package:

```sh
sudo apt install ./build-deb/packages/*.deb
```

## Build AppImage In A Container

To avoid installing build libraries on the host system, build the AppImage in a
Debian container:

```sh
./build-appimage.sh --clean
```

The script uses `podman` when available, otherwise `docker`. The default image
is `debian:12` so the generated AppImage is built on a conservative base for
current Debian/Ubuntu systems. The output is written in the project root as an
`.AppImage` file.

## Uninstall

To uninstall the Debian package and remove only libraries that are no longer
needed by any other installed package:

```sh
./uninstall-radiotray-lite.sh
```

To also remove the user configuration directory:

```sh
./uninstall-radiotray-lite.sh --purge-config
```

## Manual Dependencies

If you want to install dependencies yourself, these are the important packages
on Debian/Ubuntu:

```sh
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config git \
  file \
  libgtkmm-3.0-dev \
  libxml2-dev \
  libmagic-dev \
  libcurl4-openssl-dev \
  libnotify-dev \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good
```

For the two dependencies that are commonly missing during local builds:

```sh
sudo apt install -y libmagic-dev libgtkmm-3.0-dev
```

Install one GStreamer C++ binding package:

```sh
sudo apt install -y libgstreamermm-1.0-dev
```

If your distribution does not provide `libgstreamermm-1.0-dev`, use the legacy
package instead:

```sh
sudo apt install -y libgstreamermm-0.10-dev
```

Install one AppIndicator backend:

```sh
sudo apt install -y libayatana-appindicator-glib-dev
```

If your distribution does not provide it, use one of the legacy backends:

```sh
sudo apt install -y libayatana-appindicator3-dev
# or
sudo apt install -y libappindicator3-dev
```

## AppIndicator Backend

The backend is selected in [build-options.conf](build-options.conf):

```sh
APPINDICATOR_BACKEND=auto
```

Available values:

- `auto`: prefer `ayatana-glib`, then fall back to legacy backends.
- `ayatana-glib`: require `libayatana-appindicator-glib-dev`.
- `appindicator`: require `libappindicator3-dev`.
- `ayatana-gtk3`: require `libayatana-appindicator3-dev`.
- `gtk-status-icon`: use GTK 3's deprecated `GtkStatusIcon` fallback without
  AppIndicator. This is useful on older X11 desktops where legacy AppIndicator
  libraries emit runtime warnings.

The `ayatana-glib` backend is preferred on recent systems because it avoids the
runtime deprecation warning emitted by the older GTK 3 Ayatana library.

You can override the config for one build:

```sh
./build-deb.sh --clean --appindicator-backend ayatana-glib
```

For Debian Bookworm/MATE systems that only provide `libayatana-appindicator3`,
the compatibility fallback avoids that library entirely:

```sh
./build-deb.sh --clean --appindicator-backend gtk-status-icon
```

Both `build-deb.sh` and CMake validate the selected backend and fail clearly if
the value is invalid or if the requested development package is unavailable.

## Build Options

The generated packages require `gstreamer1.0-plugins-base` and
`gstreamer1.0-plugins-good` by default. `gstreamer1.0-plugins-bad` is optional
because it can be unavailable or temporarily uninstallable on some
distributions. To build a package that requires it, use:

```sh
./build-deb.sh --clean --with-gstreamer-bad
```

If dependencies are already installed:

```sh
./build-deb.sh --no-install-deps --clean
```

For a direct CMake build:

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DRADIOTRAY_APPINDICATOR_BACKEND=auto
cmake --build build -j"$(nproc)"
```

To create a Debian package manually:

```sh
cd build
cpack -G DEB
```

See [COMPATIBILITY.md](COMPATIBILITY.md) for notes about legacy AppIndicator,
Ayatana AppIndicator, and GTK/libadwaita compatibility.

## Configuration

### Bookmarks

Copy your existing `bookmarks.xml` from [Radio Tray](http://radiotray.sourceforge.net/) (which is usually located at
`$HOME/.local/share/radiotray/bookmarks.xml`) into `$HOME/.config/radiotray-lite/` directory.

### Custom Station Icons

Stations and groups may carry their own icon. Two optional attributes are
recognized on `<bookmark>` and `<group>` elements:

- `icon`: an `http://`, `https://` or `ftp://` URL, or a local file path. Local
  paths accept `~`, absolute paths, and paths relative to the directory that
  contains `bookmarks.xml`.
- `icon_path` (also spelled `icon-path`): where a remote icon is stored on disk.

```xml
<bookmarks>
  <group name="root">
    <group name="Radio Paradise" icon="icons/paradise.png">
      <bookmark name="Main Mix"
                url="https://www.radioparadise.com/m3u/aac-320.m3u"
                icon="https://www.radioparadise.com/graphics/favicon.png"
                icon_path="~/.local/share/radiotray-lite/icons/paradise.png"/>
      <bookmark name="Rock 181"
                url="http://listen.181fm.com/181-rock_64k.aac"
                icon="https://example.org/logos/rock181.png"/>
    </group>
  </group>
</bookmarks>
```

Resolution rules:

| `icon`   | `icon_path` | Result                                                                    |
|----------|-------------|---------------------------------------------------------------------------|
| absent   | absent      | No icon, plain text menu entry.                                           |
| path     | absent      | The path is used as is; nothing is downloaded.                            |
| absent   | path        | Same as above (`icon_path` alone is treated as the icon file).            |
| URL      | path        | The file is used if it already exists, otherwise downloaded from the URL. |
| URL      | absent      | Same, but stored in `$XDG_CACHE_HOME/radiotray-lite/icons/`.              |

Remote icons are downloaded only once, when the menu is built and the file is
missing: after that the local copy is reused and no network access happens. The
download happens while the menu is being built, so the first start after adding
a new icon URL may take a moment.
Downloads are written to a `<file>.part` file and renamed when complete, are
limited to 4 MiB, and are rejected unless the server answers with a 2xx status
and an image (checked through the `Content-Type` header and the file
signature). Icons that are missing or cannot be loaded are ignored and the
entry is displayed without an image.

Any format supported by GdkPixbuf works: PNG, JPEG, GIF, BMP, WEBP, SVG and
XPM. Icons are scaled down to the menu size; use square images of at least
32x32 pixels for a sharp result.

Use *Reload Bookmarks* from the menu to retry icons that failed to download.
Note that menu icons are drawn by the panel: they are displayed with the
`gtk-status-icon` backend and, depending on the desktop, with the
`ayatana-glib` backend; AppIndicator panels that ignore icon attributes of a
D-Bus menu will only show the labels.

### Options

Configuration file is located in the same directory as bookmarks file. It has simple XML format and following options are supported:

- `last_station`: name of the last played station. Automatically updated.
- `buffer_size`: size of the internal GStreamer buffer.
- `buffer_duration`: number of seconds to buffer.
- `url_timeout`: timeout in seconds for fetching playlist files.
- `notifications`: set to `false` to disable desktop notifications. Default is
  `true`.
- `tray_icon`: icon shown in the system tray / status area. A local path
  (`~`, absolute, or relative to the config file directory) or an `http(s)`/
  `ftp` URL, which is downloaded once and cached. Empty by default, which keeps
  the built-in `radiotray_on`/`radiotray_off` icons.
- `tray_icon_follow_station`: set to `true` to display the icon of the station
  being played (the `icon` attribute of its bookmark) in the tray while it
  plays. Default is `false`.

Example:

```xml
<?xml version="1.0"?>
<config>
  <option name="last_station" value="Rock 181" />
  <option name="buffer_size" value="102400" />
  <option name="buffer_duration" value="2" />
  <option name="url_timeout" value="5" />
  <option name="notifications" value="false" />
  <option name="tray_icon" value="~/.local/share/radiotray-lite/icons/app.png" />
  <option name="tray_icon_follow_station" value="true" />
</config>
```

### Tray Icon

The icon drawn in the panel above the menu comes from `tray_icon` in
`config.xml`; when no icon is configured, the built-in images are used.

```xml
<config>
  <!-- local file, absolute path or URL (downloaded once into
       $XDG_CACHE_HOME/radiotray-lite/icons/) -->
  <option name="tray_icon" value="https://example.org/my-radio.png" />
  <!-- while a station plays, use the icon of its bookmark instead -->
  <option name="tray_icon_follow_station" value="true" />
</config>
```

Resolution order for the tray icon:

1. the playing station's `icon` attribute, when `tray_icon_follow_station` is
   `true` and the station has an icon;
2. `tray_icon` from `config.xml`;
3. the built-in `radiotray_on` / `radiotray_off` icon.

Note that Radio Tray's `gui_engine` option (`appindicator`, `systray`,
`chooser`) is **not** used by Radio Tray Lite: it only selected the GUI class
that drew the tray in the Python version, and never described an icon. Here the
tray backend is chosen at build time with
`-DRADIOTRAY_APPINDICATOR_BACKEND=...` (see the AppIndicator Backend section).

## Licensing

See [LICENSE.md](LICENSE.md) file for license information.
