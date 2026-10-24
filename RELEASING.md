# Building and releasing Radio Tray Lite

Everything below is copy-paste friendly. Commands are run from a source
checkout (the top directory that contains `CMakeLists.txt`).

The version lives in `CMakeLists.txt`:

```sh
grep -n 'TARGET_VERSION' CMakeLists.txt
```

Bump `TARGET_VERSION_MAJOR/MINOR/PATCH` before releasing: the `.deb` name and
the AppImage name are derived from it.

## 1. Clone

```sh
git clone https://github.com/jojo8356/radiotray-lite.git
cd radiotray-lite

# work on a branch, or on the default one
git switch arena/3f9559dc-radiotray-lite    # or: git switch master
git log --oneline -3
```

## 2. Native build (fast check)

```sh
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config git file \
  libgtkmm-3.0-dev libxml2-dev libmagic-dev libcurl4-openssl-dev libnotify-dev \
  gstreamer1.0-plugins-base gstreamer1.0-plugins-good libgstreamermm-1.0-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
  -DRADIOTRAY_MIN_SIZE=ON
cmake --build build -j"$(nproc)"
ls -lh build/src/radiotray-lite
```

## 3. The lightest `.deb`

`build-deb.sh --min-size` is the "smallest possible" mode:

- `-Os`, LTO, `-ffunction-sections -fdata-sections`, `-fno-unwind-tables`,
- link with `--gc-sections`, `--as-needed`, `-s` (stripped binary),
- `strip --strip-unneeded` on the produced binary,
- the `.deb` is recompressed with `xz -9` when that makes it smaller,
- no `gstreamer1.0-plugins-bad` requirement (do not pass `--with-gstreamer-bad`).

```sh
./build-deb.sh --clean --min-size --no-install-deps
# -> build-deb/packages/radiotray-lite_0.2.19_amd64.deb
```

`--no-install-deps` assumes step 2 already installed the build dependencies.

### Per-distribution packages (smallest dependency set)

`CPACK_DEBIAN_PACKAGE_SHLIBDEPS` computes the runtime dependencies **of the
machine that builds the package**. Building inside the container of the target
distribution therefore yields the smallest, exact dependency list (no
`>=` guesswork, no library version backported by mistake). It also avoids
linking against newer libraries than the target has.

```sh
set -e
VERSION="$(sed -n 's/^set(TARGET_VERSION_MAJOR[[:space:]]*\([0-9]*\)).*/\1/p' CMakeLists.txt).$(sed -n 's/^set(TARGET_VERSION_MINOR[[:space:]]*\([0-9]*\)).*/\1/p' CMakeLists.txt).$(sed -n 's/^set(TARGET_VERSION_PATCH[[:space:]]*\([0-9]*\)).*/\1/p' CMakeLists.txt)"
ENGINE="$(command -v podman || command -v docker)"
echo "version: ${VERSION}, engine: ${ENGINE}"

mkdir -p dist
for distro in ubuntu:22.04 ubuntu:24.04 debian:12; do
    tag="${distro/:/-}"
    "${ENGINE}" run --rm -v "$PWD:/src" -w /src "${distro}" \
        bash -lc './build-deb.sh --clean --min-size'

    deb="$(ls build-deb/packages/*.deb | head -n1)"
    cp "${deb}" "dist/$(basename "${deb%.deb}")-${tag}.deb"
    rm -rf build-deb
    echo "-> dist/$(basename "${deb%.deb}")-${tag}.deb"
done

ls -lh dist/
```

Notes:

- `auto` picks the AppIndicator backend available on the build host. For the
  smallest root package, `--appindicator-backend gtk-status-icon` drops the
  AppIndicator library dependency entirely (pure `GtkStatusIcon`); `auto` keeps
  the best panel integration. Example:

  ```sh
  ./build-deb.sh --clean --min-size --appindicator-backend gtk-status-icon
  ```

- Oldest base = widest compatibility: `debian:12` and `ubuntu:22.04` produce
  packages installable on newer releases too.

## 4. AppImage

```sh
./build-appimage.sh --clean --min-size
# -> Radio_Tray_Lite-0.2.19-x86_64.AppImage
```

The build runs in a `debian:12` container (`--image` to change it) so the host
stays clean. `--min-size` applies the same optimizations to the bundled binary.
An AppImage always embeds GTK 3, gtkmm and the GStreamer stack, so its size is
dominated by those libraries, not by `radiotray-lite` itself.

Smaller AppImage tips:

- `--appindicator-backend gtk-status-icon`: nothing extra to bundle for the
  tray, and no `libayatana-appindicator` in the payload.
- Check what is heavy in the payload:

  ```sh
  ./Radio_Tray_Lite-0.2.19-x86_64.AppImage --appimage-extract >/dev/null
  du -h -d2 squashfs-root/usr/lib | sort -h | tail -20
  ```

- Build with `--image debian:12` (default) rather than a newer base: fewer and
  older libraries to embed.

## 5. Sanity checks before publishing

```sh
VERSION=0.2.19

# dependencies and size of the packages
for f in dist/*.deb; do
    echo "== ${f} ($(du -h "${f}" | cut -f1))"
    dpkg-deb -I "${f}" | sed -n 's/^ Depends:/  Depends:/p'
done

# embedded binary is stripped and links only what it needs
tmp="$(mktemp -d)"
dpkg-deb -x dist/radiotray-lite_${VERSION}_amd64-debian-12.deb "${tmp}"
file "${tmp}/usr/bin/radiotray-lite"
ldd "${tmp}/usr/bin/radiotray-lite" | sort
rm -rf "${tmp}"

# AppImage runs
chmod +x Radio_Tray_Lite-${VERSION}-x86_64.AppImage
./Radio_Tray_Lite-${VERSION}-x86_64.AppImage --appimage-extract-and-run --help || true
```

## 6. Checksums

```sh
cd dist && sha256sum *.deb > SHA256SUMS && cd ..
sha256sum ./*.AppImage >> dist/SHA256SUMS
cat dist/SHA256SUMS
```

## 7. Tag and release

```sh
VERSION=0.2.19

git tag -a "v${VERSION}" -m "Radio Tray Lite ${VERSION}"
git push origin "v${VERSION}"

gh auth status

gh release create "v${VERSION}" \
    dist/*.deb \
    "./Radio_Tray_Lite-${VERSION}-x86_64.AppImage" \
    dist/SHA256SUMS \
    --title "Radio Tray Lite ${VERSION}" \
    --generate-notes
```

Useful variants:

```sh
# draft release, publish later from the web UI
gh release create "v${VERSION}" dist/* --draft --title "Radio Tray Lite ${VERSION}" --generate-notes

# pre-release
gh release create "v${VERSION}" dist/* --prerelease --title "Radio Tray Lite ${VERSION}"

# add/replace an asset later
gh release upload "v${VERSION}" dist/another.deb --clobber

# release notes from a file
gh release create "v${VERSION}" dist/* --notes-file CHANGELOG.md
```

The release is created from `HEAD` (or from `--target <branch>`); remember to
push the branch that contains the tag first.

## 8. Automation

`.github/workflows/linux-deb.yml` builds a `.deb` on every push for
Ubuntu 22.04/24.04 and Debian 12.

`.github/workflows/release.yml` does the whole release when a `v*` tag is
pushed: three minimal `.deb` packages, one AppImage, `SHA256SUMS`, and
`gh release create`. Locally you can trigger the same steps with
`git push origin "v${VERSION}"`.
