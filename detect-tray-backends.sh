#!/usr/bin/env sh
set -eu

section() {
    printf '\n== %s ==\n' "$1"
}

has_cmd() {
    command -v "$1" >/dev/null 2>&1
}

print_cmd() {
    printf '+ %s\n' "$*"
}

run_optional() {
    print_cmd "$*"
    "$@" 2>&1 || true
}

pkg_config_exists() {
    has_cmd pkg-config && pkg-config --exists "$1"
}

check_pkg_config_module() {
    module="$1"
    printf '%-36s' "$module"
    if has_cmd pkg-config && pkg-config --exists "$module"; then
        version="$(pkg-config --modversion "$module" 2>/dev/null || printf unknown)"
        libs="$(pkg-config --libs "$module" 2>/dev/null || true)"
        printf 'FOUND version=%s libs=%s\n' "$version" "$libs"
    else
        printf 'missing\n'
    fi
}

print_capability() {
    name="$1"
    state="$2"
    detail="$3"
    printf '%-34s %-10s %s\n' "$name" "$state" "$detail"
}

check_deb_package() {
    package="$1"
    installed="no"
    candidate="none"

    if dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q 'install ok installed'; then
        installed="yes"
    fi

    if has_cmd apt-cache; then
        candidate="$(apt-cache policy "$package" 2>/dev/null | awk '/Candidate:/ {print $2; exit}')"
        if [ -z "$candidate" ]; then
            candidate="none"
        fi
    fi

    printf '%-40s installed=%-3s candidate=%s\n' "$package" "$installed" "$candidate"
}

check_rpm_package() {
    package="$1"
    installed="no"
    available="unknown"

    if rpm -q "$package" >/dev/null 2>&1; then
        installed="yes"
    fi

    if has_cmd dnf; then
        if dnf -q list --available "$package" >/dev/null 2>&1; then
            available="yes"
        else
            available="no"
        fi
    elif has_cmd zypper; then
        if zypper --non-interactive search --match-exact "$package" >/dev/null 2>&1; then
            available="yes"
        else
            available="no"
        fi
    fi

    printf '%-40s installed=%-3s available=%s\n' "$package" "$installed" "$available"
}

check_pacman_package() {
    package="$1"
    installed="no"
    available="no"

    if pacman -Q "$package" >/dev/null 2>&1; then
        installed="yes"
    fi

    if pacman -Si "$package" >/dev/null 2>&1; then
        available="yes"
    fi

    printf '%-40s installed=%-3s available=%s\n' "$package" "$installed" "$available"
}

section "System"
printf 'date=%s\n' "$(date -Iseconds 2>/dev/null || date)"
printf 'kernel=%s\n' "$(uname -a)"
if [ -r /etc/os-release ]; then
    sed -n 's/^\(PRETTY_NAME\|ID\|VERSION_ID\|VERSION_CODENAME\)=/\1=/p' /etc/os-release
fi

section "Desktop Session"
printf 'XDG_CURRENT_DESKTOP=%s\n' "${XDG_CURRENT_DESKTOP:-}"
printf 'XDG_SESSION_DESKTOP=%s\n' "${XDG_SESSION_DESKTOP:-}"
printf 'XDG_SESSION_TYPE=%s\n' "${XDG_SESSION_TYPE:-}"
printf 'DESKTOP_SESSION=%s\n' "${DESKTOP_SESSION:-}"
printf 'WAYLAND_DISPLAY=%s\n' "${WAYLAND_DISPLAY:-}"
printf 'DISPLAY=%s\n' "${DISPLAY:-}"

section "Tools"
for tool in pkg-config dpkg-query apt-cache rpm dnf zypper pacman; do
    if has_cmd "$tool"; then
        printf '%-16s %s\n' "$tool" "$(command -v "$tool")"
    else
        printf '%-16s missing\n' "$tool"
    fi
done

section "pkg-config Tray/Indicator Modules"
for module in \
    ayatana-appindicator-glib \
    ayatana-appindicator3-0.1 \
    ayatana-appindicator-0.1 \
    appindicator3-0.1 \
    appindicator-0.1 \
    indicator-application-0.1 \
    indicator3-0.4 \
    indicator-0.4 \
    ayatana-indicator3-0.4 \
    ayatana-indicator-0.4 \
    dbusmenu-glib-0.4 \
    dbusmenu-gtk3-0.4 \
    dbusmenu-gtk-0.4 \
    xapp \
    libxapp \
    gtk+-3.0 \
    gtkmm-3.0 \
    gdk-3.0 \
    gtk+-x11-3.0 \
    gdk-x11-3.0 \
    gtk+-2.0 \
    gtkmm-2.4 \
    gtk4 \
    gtkmm-4.0 \
    libadwaita-1 \
    dbus-1 \
    gio-2.0 \
    gio-unix-2.0 \
    Qt5Core \
    Qt5Gui \
    Qt5Widgets \
    Qt5DBus \
    Qt6Core \
    Qt6Gui \
    Qt6Widgets \
    Qt6DBus \
    KF5Notifications \
    KF6Notifications \
    KF5StatusNotifierItem \
    KF6StatusNotifierItem
do
    check_pkg_config_module "$module"
done

section "Backend Capability Matrix"
if pkg_config_exists ayatana-appindicator-glib; then
    print_capability "ayatana-glib AppIndicator" "native" "usable by current radiotray-lite backend ayatana-glib"
else
    print_capability "ayatana-glib AppIndicator" "missing" "needs pkg-config module ayatana-appindicator-glib"
fi

if pkg_config_exists appindicator3-0.1; then
    print_capability "legacy libappindicator GTK3" "native" "usable by current radiotray-lite backend appindicator"
else
    print_capability "legacy libappindicator GTK3" "missing" "needs pkg-config module appindicator3-0.1"
fi

if pkg_config_exists ayatana-appindicator3-0.1; then
    print_capability "Ayatana AppIndicator GTK3" "native" "usable by current radiotray-lite backend ayatana-gtk3"
else
    print_capability "Ayatana AppIndicator GTK3" "missing" "needs pkg-config module ayatana-appindicator3-0.1"
fi

if pkg_config_exists gtkmm-3.0; then
    print_capability "GTK3 StatusIcon" "native" "usable by current radiotray-lite backend gtk-status-icon"
else
    print_capability "GTK3 StatusIcon" "missing" "needs pkg-config module gtkmm-3.0 for this C++ app"
fi

if pkg_config_exists gtk+-2.0 || pkg_config_exists gtkmm-2.4; then
    print_capability "GTK2 tray/status icon" "portable" "possible only with a GTK2-specific port"
else
    print_capability "GTK2 tray/status icon" "missing" "no GTK2 development module detected"
fi

if pkg_config_exists Qt6Widgets || pkg_config_exists Qt5Widgets; then
    print_capability "Qt QSystemTrayIcon" "portable" "possible with a Qt UI/backend port"
else
    print_capability "Qt QSystemTrayIcon" "missing" "needs Qt5Widgets or Qt6Widgets"
fi

if pkg_config_exists dbus-1 || pkg_config_exists gio-unix-2.0; then
    print_capability "StatusNotifierItem DBus" "portable" "possible with a direct SNI DBus backend"
else
    print_capability "StatusNotifierItem DBus" "missing" "needs DBus/GIO development modules"
fi

if pkg_config_exists dbusmenu-glib-0.4; then
    print_capability "DBusMenu" "portable" "menu transport available for an SNI/AppIndicator-style backend"
else
    print_capability "DBusMenu" "missing" "needs dbusmenu-glib-0.4"
fi

if pkg_config_exists xapp || pkg_config_exists libxapp; then
    print_capability "XApp StatusIcon" "portable" "possible with an XApp-specific backend"
else
    print_capability "XApp StatusIcon" "missing" "needs xapp/libxapp pkg-config module"
fi

if pkg_config_exists KF6Notifications || pkg_config_exists KF5Notifications || pkg_config_exists KF6StatusNotifierItem || pkg_config_exists KF5StatusNotifierItem; then
    print_capability "KDE StatusNotifierItem" "portable" "possible with KDE Frameworks backend"
else
    print_capability "KDE StatusNotifierItem" "missing" "no KDE notification/status notifier module detected"
fi

if pkg_config_exists gtk4 || pkg_config_exists libadwaita-1; then
    print_capability "GTK4/libadwaita" "not-tray" "available, but GTK4 removed GtkStatusIcon; needs SNI/portal design"
else
    print_capability "GTK4/libadwaita" "missing" "not detected"
fi

section "pkg-config Legacy Raw List"
for module in \
    ayatana-appindicator-glib \
    ayatana-appindicator3-0.1 \
    ayatana-appindicator-0.1 \
    appindicator3-0.1 \
    appindicator-0.1
do
    check_pkg_config_module "$module"
done

if has_cmd pkg-config; then
    section "pkg-config Matching Names"
    pkg-config --list-all 2>/dev/null | grep -Ei 'appindicator|ayatana|indicator|status|tray|dbusmenu|gtk|adwaita' || true
fi

if has_cmd dpkg-query; then
    section "Debian/Ubuntu Packages"
    for package in \
        libayatana-appindicator-glib-dev \
        libayatana-appindicator-glib1 \
        libayatana-appindicator-glib2 \
        libayatana-appindicator-glib0 \
        libayatana-appindicator3-dev \
        libayatana-appindicator3-1 \
        libayatana-appindicator-dev \
        libayatana-appindicator1 \
        libappindicator3-dev \
        libappindicator3-1 \
        libappindicator-dev \
        libappindicator1 \
        gir1.2-ayatanaappindicator3-0.1 \
        gir1.2-ayatanaappindicator-0.1 \
        gir1.2-appindicator3-0.1 \
        gir1.2-appindicator-0.1 \
        ayatana-indicator-application \
        libayatana-indicator3-dev \
        libayatana-indicator3-7 \
        libayatana-indicator-dev \
        libayatana-indicator7 \
        libindicator3-dev \
        libindicator3-7 \
        libindicator-dev \
        libindicator7 \
        libdbusmenu-glib-dev \
        libdbusmenu-glib4 \
        libdbusmenu-gtk3-dev \
        libdbusmenu-gtk3-4 \
        libdbusmenu-gtk-dev \
        libdbusmenu-gtk4 \
        libxapp-dev \
        libxapp1 \
        libgtkmm-3.0-dev \
        libgtkmm-3.0-1v5 \
        libgtk-3-dev \
        libgtk-3-0 \
        libgtkmm-4.0-dev \
        libgtkmm-4.0-0 \
        libgtk-4-dev \
        libgtk-4-1 \
        qtbase5-dev \
        libqt5widgets5 \
        qt6-base-dev \
        libqt6widgets6 \
        libkf5notifications-dev \
        libkf5notifications5 \
        libkf6notifications-dev \
        libkf6notifications6 \
        libadwaita-1-dev \
        libadwaita-1-0 \
        gnome-shell-extension-appindicator \
        gnome-shell-extension-kstatusnotifieritem \
        plasma-workspace \
        mate-sntray-plugin \
        xfce4-sntray-plugin \
        budgie-sntray-plugin \
        vala-sntray-plugin \
        lxpanel \
        tint2
    do
        check_deb_package "$package"
    done

    if has_cmd apt-cache; then
        section "APT Search: appindicator/status/tray"
        run_optional apt-cache search appindicator
        run_optional apt-cache search statusnotifier
        run_optional apt-cache search "status notifier"
        run_optional apt-cache search kstatusnotifier
        run_optional apt-cache search "system tray"
        run_optional apt-cache search "xapp status"
        run_optional apt-cache search "qsystemtray"
    fi
fi

if has_cmd rpm; then
    section "RPM Packages"
    for package in \
        libayatana-appindicator-devel \
        libayatana-appindicator-gtk3-devel \
        libayatana-appindicator-glib-devel \
        libappindicator-gtk3-devel \
        libappindicator-devel \
        ayatana-indicator-application \
        libdbusmenu-devel \
        libdbusmenu-gtk3-devel \
        xapps-devel \
        gtkmm30-devel \
        gtk3-devel \
        gtkmm4-devel \
        gtk4-devel \
        qt5-qtbase-devel \
        qt6-qtbase-devel \
        kf5-knotifications-devel \
        kf6-knotifications-devel \
        libadwaita-devel \
        gnome-shell-extension-appindicator
    do
        check_rpm_package "$package"
    done
fi

if has_cmd pacman; then
    section "Arch Packages"
    for package in \
        libayatana-appindicator \
        libappindicator-gtk3 \
        libdbusmenu-glib \
        libdbusmenu-gtk3 \
        xapp \
        gtkmm3 \
        gtk3 \
        gtkmm-4.0 \
        gtk4 \
        qt5-base \
        qt6-base \
        knotifications5 \
        knotifications \
        libadwaita \
        gnome-shell-extension-appindicator \
        plasma-workspace
    do
        check_pacman_package "$package"
    done
fi

section "Summary For radiotray-lite"
if pkg_config_exists ayatana-appindicator-glib; then
    printf 'radiotray-lite backend: ayatana-glib possible\n'
elif pkg_config_exists appindicator3-0.1; then
    printf 'radiotray-lite backend: appindicator possible\n'
elif pkg_config_exists ayatana-appindicator3-0.1; then
    printf 'radiotray-lite backend: ayatana-gtk3 possible\n'
elif pkg_config_exists gtkmm-3.0; then
    printf 'radiotray-lite backend: gtk-status-icon possible\n'
else
    printf 'radiotray-lite backend: none detected for the current codebase\n'
fi

section "Porting Hints"
printf 'native = already supported by this source tree.\n'
printf 'portable = enough system pieces exist, but radiotray-lite needs a new backend implementation.\n'
printf 'not-tray = GUI stack exists but does not provide a tray icon API by itself.\n'
