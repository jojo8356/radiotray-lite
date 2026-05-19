#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONTAINER_ENGINE="${CONTAINER_ENGINE:-}"
CONTAINER_IMAGE="${CONTAINER_IMAGE:-debian:12}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-appimage}"
APPDIR="${APPDIR:-${ROOT_DIR}/AppDir}"
APPIMAGE_NAME="${APPIMAGE_NAME:-Radio_Tray_Lite-0.2.19-x86_64.AppImage}"
APPINDICATOR_BACKEND="${APPINDICATOR_BACKEND:-auto}"
LINUXDEPLOY_URL="${LINUXDEPLOY_URL:-https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage}"

usage() {
    cat <<EOF
Usage: ./build-appimage.sh [options]

Build the AppImage in an isolated Debian container so the host system is not
polluted with build dependencies.

Options:
  --clean             Remove build-appimage/, AppDir/, and the output AppImage first
  --image IMAGE       Container image to use (default: ${CONTAINER_IMAGE})
  -h, --help          Show this help

Environment:
  CONTAINER_ENGINE    podman or docker (auto-detected by default)
  APPINDICATOR_BACKEND
                      auto, ayatana-glib, appindicator, ayatana-gtk3
EOF
}

CLEAN=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)
            CLEAN=1
            shift
            ;;
        --image)
            if [[ $# -lt 2 ]]; then
                echo "--image requires a value" >&2
                usage >&2
                exit 2
            fi
            CONTAINER_IMAGE="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "${CONTAINER_ENGINE}" ]]; then
    if command -v podman >/dev/null 2>&1; then
        CONTAINER_ENGINE=podman
    elif command -v docker >/dev/null 2>&1; then
        CONTAINER_ENGINE=docker
    else
        echo "podman or docker is required to build the AppImage in isolation." >&2
        exit 1
    fi
fi

case "${CONTAINER_ENGINE}" in
    podman|docker)
        ;;
    *)
        echo "Unsupported CONTAINER_ENGINE: ${CONTAINER_ENGINE}" >&2
        echo "Expected: podman or docker" >&2
        exit 2
        ;;
esac

if [[ "${CLEAN}" -eq 1 ]]; then
    rm -rf "${BUILD_DIR}" "${APPDIR}" "${ROOT_DIR}/${APPIMAGE_NAME}"
fi

"${CONTAINER_ENGINE}" run --rm \
    -v "${ROOT_DIR}:/src" \
    -w /src \
    -e BUILD_DIR="/src/$(basename "${BUILD_DIR}")" \
    -e APPINDICATOR_BACKEND="${APPINDICATOR_BACKEND}" \
    -e LINUXDEPLOY_URL="${LINUXDEPLOY_URL}" \
    "${CONTAINER_IMAGE}" \
    bash -lc '
set -euo pipefail

apt-get update
apt-get install -y \
    bash build-essential cmake pkg-config git ca-certificates wget file \
    desktop-file-utils patchelf \
    libgtkmm-3.0-dev \
    libmagic-dev \
    libcurl4-openssl-dev \
    libnotify-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    libgstreamermm-1.0-dev

case "${APPINDICATOR_BACKEND}" in
    auto)
        apt-get install -y libayatana-appindicator-glib-dev || \
        apt-get install -y libayatana-appindicator3-dev || \
        apt-get install -y libappindicator3-dev
        ;;
    ayatana-glib)
        apt-get install -y libayatana-appindicator-glib-dev
        ;;
    ayatana-gtk3)
        apt-get install -y libayatana-appindicator3-dev
        ;;
    appindicator)
        apt-get install -y libappindicator3-dev
        ;;
    *)
        echo "Invalid APPINDICATOR_BACKEND: ${APPINDICATOR_BACKEND}" >&2
        exit 2
        ;;
esac

rm -rf "${BUILD_DIR}" /src/AppDir /src/Radio_Tray_Lite-0.2.19-x86_64.AppImage
cmake -S /src -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DRADIOTRAY_APPINDICATOR_BACKEND="${APPINDICATOR_BACKEND}"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

mkdir -p /tmp/linuxdeploy
wget -O /tmp/linuxdeploy/linuxdeploy-x86_64.AppImage "${LINUXDEPLOY_URL}"
chmod +x /tmp/linuxdeploy/linuxdeploy-x86_64.AppImage

LINUXDEPLOY_OUTPUT_VERSION=0.2.19 \
ARCH=x86_64 \
APPIMAGE_EXTRACT_AND_RUN=1 \
/tmp/linuxdeploy/linuxdeploy-x86_64.AppImage \
    --appdir /src/AppDir \
    --executable "${BUILD_DIR}/src/radiotray-lite" \
    --desktop-file /src/data/radiotray-lite.desktop \
    --icon-file /src/data/images/radiotray-lite.png \
    --output appimage
'

echo
echo "AppImage:"
find "${ROOT_DIR}" -maxdepth 1 -type f -name "*.AppImage" -print
