#ifndef __CONFIG_HPP_INCLUDED__
#define __CONFIG_HPP_INCLUDED__

#include <chrono>
#include <string>

#include <gtkmm.h>

#include "easyloggingpp/easylogging++.h"
#include "pugixml/pugixml.hpp"

#include "constants.hpp"

namespace radiotray
{
class Config
{
public:
    Config() = default;
    ~Config();

    void set_config_file(const std::string& name);
    void load_config();
    std::string config_dir() const;
    bool has_last_station();
    void set_last_played_station(const Glib::ustring& station);
    Glib::ustring get_last_played_station() const;
    bool has_notifications() const;

    long url_timeout_ms = kDefaultHTTPRequestTimeout;
    int buffer_size = kDefaultGStreamerBufferSize;
    int64_t buffer_duration = kDefaultBufferDuration;

    // Tray (status area) icon: empty means the built-in Radio Tray Lite icons.
    // A local path or an URL (downloaded on first use) may be given.
    std::string tray_icon;
    // When true, the icon of the playing station (bookmarks.xml 'icon'
    // attribute) replaces the tray icon while it plays.
    bool tray_icon_follow_station = false;

private:
    std::string filename;
    pugi::xml_document config;
    Glib::ustring last_station;
    bool notifications = true;

    void create_new_config();
    void save_config();
};

} // namespace radiotray

#endif
