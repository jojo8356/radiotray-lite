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

To get the smallest possible binary and package (`-Os`, LTO, `--gc-sections`,
stripped binary, `.deb` recompressed with `xz -9`):

```sh
./build-deb.sh --clean --min-size
./build-appimage.sh --clean --min-size
```

For the smallest dependency set, build inside the container of the target
distribution and/or drop the AppIndicator dependency:

```sh
docker run --rm -v "$PWD:/src" -w /src debian:12 \
  bash -lc './build-deb.sh --clean --min-size'
./build-deb.sh --clean --min-size --appindicator-backend gtk-status-icon
```

See [RELEASING.md](RELEASING.md) for the complete clone → build → release
sequence (three `.deb` for Ubuntu 22.04/24.04 and Debian 12, one AppImage,
checksums, and `gh release create`).

Many stations use AAC or MP3 streams, which need extra GStreamer codecs. They
are recommended by the package, but if you install manually or use the
AppImage on a minimal system, add them.

Debian, Ubuntu, Linux Mint — the `universe` component must be enabled:

```sh
sudo apt update
sudo apt install gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav
# if apt says "no installation candidate", check that the component is enabled:
grep -r --include='*.list' --include='*.sources' -l universe /etc/apt/sources.list*
sudo add-apt-repository universe && sudo apt update
```

Other distributions:

```sh
# Fedora (RPM Fusion for freeworld codecs)
sudo dnf install gstreamer1-plugins-bad-free gstreamer1-plugins-ugly gstreamer1-plugins-bad-freeworld
# Arch / Manjaro
sudo pacman -S gst-plugins-bad gst-plugins-ugly gst-libav
# openSUSE
sudo zypper install gstreamer-plugins-bad gstreamer-plugins-ugly gstreamer-plugins-libav
```

Without the codecs the player logs the exact element that is missing and the
package providing it, for example:

```
ERROR GStreamer detail: no element "avdec_aac"
ERROR Missing GStreamer element: avdec_aac
ERROR Debian/Ubuntu: sudo apt install gstreamer1.0-libav
ERROR To list the installed plugins: gst-inspect-1.0 | grep -i aac
```

The playback then skips to the next stream of the playlist.

Check what is installed and test a stream outside the application
(`gst-inspect-1.0` and `gst-launch-1.0` come from `gstreamer1.0-tools`;
`gst-play-1.0` is not shipped by every distribution):

```sh
gst-inspect-1.0 | grep -Ei 'aacparse|faad|avdec_aac|avdec_mp3'
gst-launch-1.0 playbin uri="https://www.radioparadise.com/m3u/aac-320.m3u"
```

### Third-party repositories and the i386 warnings

Some external repositories (nodesource, docker, ...) do not provide i386
packages. apt then prints, on every `apt update`:

```
N: Le fichier configuré « main/binary-i386/Packages » ne sera pas pris en compte
```

Restrict those entries to amd64 to silence it:

```sh
# /etc/apt/sources.list.d/nodesource.list
deb [arch=amd64 signed-by=/usr/share/keyrings/nodesource.gpg] https://deb.nodesource.com/node_22.x nodistro main

# /etc/apt/sources.list.d/docker.sources (deb822)
Architectures: amd64
```

Duplicate suites between `/etc/apt/sources.list` and
`/etc/apt/sources.list.d/debian.sources` produce
"target Packages is specified multiple times": keep only one of the two files.

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

The application reads `$HOME/.config/radiotray-lite/bookmarks.xml`. When that
file does not exist yet, the default bookmarks shipped with the package (or
`data/bookmarks.xml` when running a build from the source tree) are copied
there on startup. A specific file can be used instead:

```sh
radiotray-lite --bookmarks /path/to/bookmarks.xml   # config.xml is read from
                                                   # the same directory
```

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

Only files that GdkPixbuf can decode are accepted: PNG, JPEG, GIF, BMP, WEBP,
SVG and XPM (with the corresponding loaders installed). An invalid file — an
HTML error page saved by hand, a truncated download, a format without a
loader — is detected and ignored, and a previously cached file that turns out
to be unreadable is downloaded again. To check a file by hand:

```sh
file ~/.config/radiotray-lite/icons/paradise.png
gdk-pixbuf-thumbnailer ~/.config/radiotray-lite/icons/paradise.png /tmp/out.png
``` Icons are scaled down to the menu size; use square images of at least
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
  (`~`, absolute, or relative to the config file directory), an `http(s)`/`ftp`
  URL (downloaded once and cached), or the name of a built-in image
  (`radiotray_on`, `radiotray_off`, `radiotray_connecting`). Empty by default,
  which keeps the built-in icons that change with the playback state. As soon
  as this option is set, the same icon is used whether a station plays or not:
  the icon stays constant.
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

So the icon stays constant in the panel when:

- `tray_icon` is set and `tray_icon_follow_station` is `false` (default), or
- `tray_icon` is set and no station defines an `icon`.

To keep one of the built-in images permanently, without providing a file, use
its name:

```xml
<config>
  <!-- always the "playing" icon, and it never changes -->
  <option name="tray_icon" value="radiotray_on" />
</config>
```

Note that Radio Tray's `gui_engine` option (`appindicator`, `systray`,
`chooser`) is **not** used by Radio Tray Lite: it only selected the GUI class
that drew the tray in the Python version, and never described an icon. Here the
tray backend is chosen at build time with
`-DRADIOTRAY_APPINDICATOR_BACKEND=...` (see the AppIndicator Backend section).

## Licensing

See [LICENSE.md](LICENSE.md) file for license information.
