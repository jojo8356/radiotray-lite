#ifndef __TRAY_HPP_INCLUDED__
#define __TRAY_HPP_INCLUDED__

#include <functional>
#include <iostream>
#include <memory>
#include <stack>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtkmm.h>
#ifdef APPINDICATOR_USE_AYATANA_GLIB
#include <ayatana-appindicator.h>
#elif defined(APPINDICATOR_USE_AYATANA)
#include <libayatana-appindicator/app-indicator.h>
#else
#include <libappindicator/app-indicator.h>
#endif

#include "pugixml/pugixml.hpp"

#include "about.hpp"
#include "config.hpp"
#include "constants.hpp"
#include "notification.hpp"
#include "player.hpp"
#include "options.hpp"

namespace radiotray
{

class RadioTrayLite
{
public:
    RadioTrayLite() = default;
    RadioTrayLite(const RadioTrayLite&) = delete;

    ~RadioTrayLite();

    bool init(int argc, char** argv, std::shared_ptr<CmdLineOptions>& opts);
    int run();

private:
    bool initialized = false;
    Glib::RefPtr<Gtk::Application> app;
#ifdef APPINDICATOR_USE_AYATANA_GLIB
    GMenu* menu = nullptr;
    GSimpleActionGroup* actions = nullptr;
    std::vector<GMenu*> owned_submenus;
    guint action_counter = 0;
    int current_station_menu_index = -1;
    int current_broadcast_menu_index = -1;
#else
    std::shared_ptr<Gtk::Menu> menu;

    Gtk::MenuItem* current_station_menu_entry = nullptr;
    Gtk::MenuItem* current_broadcast_menu_entry = nullptr;
#endif

    AppIndicator* indicator = nullptr;

    std::string bookmarks_file;
    pugi::xml_document bookmarks_doc;

    std::shared_ptr<Player> player;
    std::shared_ptr<EventManager> em;
    std::shared_ptr<Notification> notifier;
    std::shared_ptr<Config> config;
    std::shared_ptr<CmdLineOptions> cmd_line_options;

#ifdef APPINDICATOR_USE_AYATANA_GLIB
    struct IndicatorAction
    {
        enum class Type
        {
            Station,
            Reload,
            About,
            Quit,
            CurrentStation
        };

        RadioTrayLite* radiotray = nullptr;
        Type type = Type::Station;
        Glib::ustring group_name;
        Glib::ustring station_name;
        Glib::ustring station_url;
    };

    std::vector<std::unique_ptr<IndicatorAction>> indicator_actions;
#endif

    class BookmarksWalker : public pugi::xml_tree_walker
    {
    public:
        BookmarksWalker() = delete;
        BookmarksWalker(const BookmarksWalker&) = delete;

#ifdef APPINDICATOR_USE_AYATANA_GLIB
        BookmarksWalker(RadioTrayLite& radiotray, GMenu* menu);
#else
        BookmarksWalker(RadioTrayLite& radiotray, Gtk::Menu* menu);
#endif

        bool for_each(pugi::xml_node& node) override;

    private:
        static constexpr char const* kSeparatorPrefix = "[separator-";
        static const size_t kSeparatorPrefixLength = strlen(kSeparatorPrefix);

        RadioTrayLite& radiotray;
#ifdef APPINDICATOR_USE_AYATANA_GLIB
        std::stack<GMenu*> menus;
#else
        std::stack<Gtk::Menu*> menus;
#endif
        int level = 0; // TODO: for debug, remove.
    };

    void on_quit_button();
    void on_about_button();
    void on_station_button(const Glib::ustring& group_name, const Glib::ustring& station_name, const Glib::ustring& station_url);
    void on_reload_button();
    void on_current_station_button();

    bool resume(bool resume_last_station);

    void build_menu();
    void rebuild_menu();
    void clear_menu();
    bool parse_bookmarks_file();
    void load_configuration();
    void set_current_station(bool turn_on);
    void set_current_broadcast(const Glib::ustring& info = Glib::ustring("Idle"));

    void on_station_changed_signal(const Glib::ustring& station, StationState state);
    void on_broadcast_info_changed_signal(const Glib::ustring& station, const Glib::ustring& info);

#ifdef APPINDICATOR_USE_AYATANA_GLIB
    static void on_indicator_action(GSimpleAction* action, GVariant* parameter, gpointer user_data);
    std::string add_indicator_action(IndicatorAction::Type type,
                                     const Glib::ustring& group_name = Glib::ustring(),
                                     const Glib::ustring& station_name = Glib::ustring(),
                                     const Glib::ustring& station_url = Glib::ustring());
    void append_indicator_item(GMenu* target,
                               const Glib::ustring& label,
                               IndicatorAction::Type type,
                               const Glib::ustring& group_name = Glib::ustring(),
                               const Glib::ustring& station_name = Glib::ustring(),
                               const Glib::ustring& station_url = Glib::ustring());
    void set_indicator_icon(const char* icon_name);
#endif

    void copy_default_bookmarks(const std::string& src_file);

    bool file_exists(const std::string& dir, const std::string& file);
    bool dir_exists(const std::string& dir);
};

} // namespace radiotray

#endif
