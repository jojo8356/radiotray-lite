#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-deb}"
INSTALL_PREFIX="/usr"
BUILD_TYPE="Release"
INSTALL_DEPS=1
CLEAN=0
WITH_GSTREAMER_BAD=0
MIN_SIZE=0
APPINDICATOR_BACKEND="${APPINDICATOR_BACKEND:-auto}"
CONFIG_FILE="${ROOT_DIR}/build-options.conf"
SUDO=()

if [[ -f "${CONFIG_FILE}" ]]; then
    # shellcheck source=/dev/null
    source "${CONFIG_FILE}"
fi

APPINDICATOR_BACKEND="${APPINDICATOR_BACKEND,,}"

validate_appindicator_backend() {
    case "$1" in
        auto|ayatana-glib|appindicator|ayatana-gtk3|gtk-status-icon)
            ;;
        *)
            echo "Invalid APPINDICATOR_BACKEND: $1" >&2
            echo "Expected one of: auto, ayatana-glib, appindicator, ayatana-gtk3, gtk-status-icon" >&2
            exit 2
            ;;
    esac
}

validate_appindicator_backend "${APPINDICATOR_BACKEND}"

if [[ "${EUID}" -ne 0 ]]; then
    SUDO=(sudo)
fi

check_source_tree() {
    local missing=0
    local path

    for path in \
        CMakeLists.txt \
        cmake/CPackConfig.cmake.in \
        cmake/FindLibMagic.cmake \
        cmake/FindLibNotify.cmake \
        data/bookmarks.xml \
        data/radiotray-lite.desktop \
        src/CMakeLists.txt \
        src/radiotray-lite.cpp
    do
        if [[ ! -e "${ROOT_DIR}/${path}" ]]; then
            echo "Missing required source file: ${path}" >&2
            missing=1
        fi
    done

    if [[ "${missing}" -ne 0 ]]; then
        echo >&2
        echo "Run this script from a complete radiotray-lite source tree, not from a standalone build-deb.sh download." >&2
        exit 2
    fi
}

usage() {
    cat <<EOF
Usage: ./build-deb.sh [options]

Options:
  --no-install-deps   Skip apt dependency installation
  --clean             Remove the build directory before configuring
  --debug             Build with CMAKE_BUILD_TYPE=Debug
  --appindicator-backend BACKEND
                      Select backend: auto, ayatana-glib, appindicator, ayatana-gtk3, gtk-status-icon
                      Defaults to APPINDICATOR_BACKEND in build-options.conf
  --with-gstreamer-bad
                      Install and require gstreamer1.0-plugins-bad
  --min-size          Optimize for the smallest binary: -Os, LTO, gc-sections,
                      strip, and the .deb is recompressed with xz -9
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
        --min-size)
            MIN_SIZE=1
            shift
            ;;
        --appindicator-backend)
            if [[ $# -lt 2 ]]; then
                echo "--appindicator-backend requires a value" >&2
                usage >&2
                exit 2
            fi
            APPINDICATOR_BACKEND="${2,,}"
            validate_appindicator_backend "${APPINDICATOR_BACKEND}"
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

check_source_tree

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

    appindicator_dev_package() {
        local package

        case "${APPINDICATOR_BACKEND}" in
            auto)
                first_available_package libayatana-appindicator-glib-dev libayatana-appindicator3-dev libappindicator3-dev
                ;;
            ayatana-glib)
                package="libayatana-appindicator-glib-dev"
                ;;
            appindicator)
                package="libappindicator3-dev"
                ;;
            ayatana-gtk3)
                package="libayatana-appindicator3-dev"
                ;;
            gtk-status-icon)
                package=""
                ;;
        esac

        if [[ "${APPINDICATOR_BACKEND}" != "auto" ]]; then
            if [[ -z "${package}" ]]; then
                return 0
            fi

            if ! apt_has_package "${package}"; then
                echo "Selected AppIndicator backend '${APPINDICATOR_BACKEND}' requires unavailable package: ${package}" >&2
                exit 2
            fi

            printf '%s\n' "${package}"
        fi
    }

    "${SUDO[@]}" apt-get update
    "${SUDO[@]}" apt-get install -y \
        build-essential cmake pkg-config git \
        file \
        libgtkmm-3.0-dev \
        libxml2-dev \
        gstreamer1.0-plugins-base \
        gstreamer1.0-plugins-good \
        libcurl4-openssl-dev \
        libnotify-dev \
        libmagic-dev

    if [[ "${WITH_GSTREAMER_BAD}" -eq 1 ]]; then
        "${SUDO[@]}" apt-get install -y gstreamer1.0-plugins-bad
    fi

    "${SUDO[@]}" apt-get install -y \
        "$(first_available_package libgstreamermm-1.0-dev libgstreamermm-0.10-dev)"

    appindicator_package="$(appindicator_dev_package)"
    if [[ -n "${appindicator_package}" ]]; then
        "${SUDO[@]}" apt-get install -y "${appindicator_package}"
    fi
fi

if [[ "${CLEAN}" -eq 1 ]]; then
    rm -rf "${BUILD_DIR}"
fi

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DRADIOTRAY_APPINDICATOR_BACKEND="${APPINDICATOR_BACKEND}" \
    -DRADIOTRAY_REQUIRE_GSTREAMER_BAD="${WITH_GSTREAMER_BAD}" \
    -DRADIOTRAY_MIN_SIZE="${MIN_SIZE}"

cmake --build "${BUILD_DIR}" -j"$(nproc)"

if [[ "${MIN_SIZE}" -eq 1 ]]; then
    # cmake already links with -s, but a few objects may still carry symbols.
    if command -v strip >/dev/null 2>&1; then
        strip --strip-unneeded "${BUILD_DIR}/src/radiotray-lite" 2>/dev/null || true
    fi
fi

(
    cd "${BUILD_DIR}"
    cpack -G DEB
)

if [[ "${MIN_SIZE}" -eq 1 ]] && command -v dpkg-deb >/dev/null 2>&1; then
    # CPack uses the default xz level; recompress at the maximum for a smaller
    # package. Best effort: keep the CPack output if anything goes wrong.
    for deb in "${BUILD_DIR}"/packages/*.deb; do
        [[ -e "${deb}" ]] || continue

        tmp_dir="$(mktemp -d)"
        if dpkg-deb -R "${deb}" "${tmp_dir}" >/dev/null 2>&1 &&
            dpkg-deb --root-owner-group -Zxz -z9 -b "${tmp_dir}" "${deb}.xz9" >/dev/null 2>&1; then
            if [[ "$(stat -c %s "${deb}.xz9")" -lt "$(stat -c %s "${deb}")" ]]; then
                mv -f "${deb}.xz9" "${deb}"
                echo "Recompressed $(basename "${deb}") with xz -9"
            else
                rm -f "${deb}.xz9"
            fi
        else
            rm -f "${deb}.xz9"
            echo "Warning: couldn't recompress $(basename "${deb}"), keeping the CPack output" >&2
        fi
        rm -rf "${tmp_dir}"
    done
fi

echo
echo "Debian package(s):"
find "${BUILD_DIR}/packages" -maxdepth 1 -type f -name "*.deb" -exec ls -lh {} + | awk '{print $5, $9}'
