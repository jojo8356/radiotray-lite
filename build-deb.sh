#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-deb}"
INSTALL_PREFIX="/usr"
BUILD_TYPE="Release"
INSTALL_DEPS=1
CLEAN=0
WITH_GSTREAMER_BAD=0
SUDO=()

if [[ "${EUID}" -ne 0 ]]; then
    SUDO=(sudo)
fi

usage() {
    cat <<EOF
Usage: ./build-deb.sh [options]

Options:
  --no-install-deps   Skip apt dependency installation
  --clean             Remove the build directory before configuring
  --debug             Build with CMAKE_BUILD_TYPE=Debug
  --with-gstreamer-bad
                      Install and require gstreamer1.0-plugins-bad
  -h, --help          Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-install-deps)
            INSTALL_DEPS=0
            shift
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --with-gstreamer-bad)
            WITH_GSTREAMER_BAD=1
            shift
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

if [[ "${INSTALL_DEPS}" -eq 1 ]]; then
    apt_has_package() {
        apt-cache show "$1" >/dev/null 2>&1
    }

    first_available_package() {
        local package

        for package in "$@"; do
            if apt_has_package "${package}"; then
                printf '%s\n' "${package}"
                return 0
            fi
        done

        return 1
    }

    "${SUDO[@]}" apt-get update
    "${SUDO[@]}" apt-get install -y \
        build-essential cmake pkg-config git \
        file \
        libgtkmm-3.0-dev \
        gstreamer1.0-plugins-base \
        gstreamer1.0-plugins-good \
        libcurl4-openssl-dev \
        libnotify-dev \
        libmagic-dev

    if [[ "${WITH_GSTREAMER_BAD}" -eq 1 ]]; then
        "${SUDO[@]}" apt-get install -y gstreamer1.0-plugins-bad
    fi

    "${SUDO[@]}" apt-get install -y \
        "$(first_available_package libgstreamermm-1.0-dev libgstreamermm-0.10-dev)" \
        "$(first_available_package libayatana-appindicator3-dev libappindicator3-dev)"
fi

if [[ "${CLEAN}" -eq 1 ]]; then
    rm -rf "${BUILD_DIR}"
fi

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DRADIOTRAY_REQUIRE_GSTREAMER_BAD="${WITH_GSTREAMER_BAD}"

cmake --build "${BUILD_DIR}" -j"$(nproc)"

(
    cd "${BUILD_DIR}"
    cpack -G DEB
)

echo
echo "Debian package(s):"
find "${BUILD_DIR}/packages" -maxdepth 1 -type f -name "*.deb" -print
