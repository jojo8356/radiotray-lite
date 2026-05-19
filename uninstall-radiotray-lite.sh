#!/usr/bin/env bash
set -euo pipefail

SUDO=()
YES=0
PURGE_CONFIG=0

usage() {
    cat <<EOF
Usage: ./uninstall-radiotray-lite.sh [options]

Uninstall radiotray-lite and remove only packages that apt considers orphaned.
This keeps libraries that are still required by another installed package.

Options:
  --yes              Do not ask apt for confirmation
  --purge-config     Also remove ~/.config/radiotray-lite
  -h, --help         Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --yes|-y)
            YES=1
            shift
            ;;
        --purge-config)
            PURGE_CONFIG=1
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

if ! command -v apt-get >/dev/null 2>&1; then
    echo "This script requires apt-get." >&2
    exit 1
fi

if [[ "${EUID}" -ne 0 ]]; then
    SUDO=(sudo)
fi

CONFIG_HOME="${HOME}"
if [[ "${EUID}" -eq 0 && -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
    CONFIG_HOME="$(getent passwd "${SUDO_USER}" | cut -d: -f6)"
fi

APT_YES=()
if [[ "${YES}" -eq 1 ]]; then
    APT_YES=(-y)
fi

"${SUDO[@]}" apt-get purge "${APT_YES[@]}" radiotray-lite
"${SUDO[@]}" apt-get autoremove --purge "${APT_YES[@]}"

if [[ "${PURGE_CONFIG}" -eq 1 ]]; then
    rm -rf "${CONFIG_HOME}/.config/radiotray-lite"
fi
