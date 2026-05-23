#include <cstring>

#include "tray.hpp"

namespace radiotray
{
namespace
{

#if !defined(APPINDICATOR_USE_AYATANA_GLIB) && !defined(APPINDICATOR_USE_GTK_STATUS_ICON)
void
appindicator_gtk3_log_handler(const gchar* log_domain,
                              GLogLevelFlags log_level,
                              const gchar* message,
                              gpointer user_data)
{
    static const char* const kGtk3AppIndicatorTypeWarning =
        "g_type_add_interface_static: assertion 'G_TYPE_IS_INSTANTIATABLE (instance_type)' failed";
    static const char* const kGdkToplevelUpdatesWarning =
        "gdk_window_thaw_toplevel_updates: assertion 'window->update_and_descendants_freeze_count > 0' failed";

    if ((log_level & G_LOG_LEVEL_CRITICAL) != 0 && message != nullptr &&
        (std::strstr(message, kGtk3AppIndicatorTypeWarning) != nullptr ||
         std::strstr(message, kGdkToplevelUpdatesWarning) != nullptr)) {
        return;
    }

    g_log_default_handler(log_domain, log_level, message, user_data);
}

#endif

} // namespace

RadioTrayLite::BookmarksWalker::BookmarksWalker(RadioTrayLite& radiotray,
#ifdef APPINDICATOR_USE_AYATANA_GLIB
                                                GMenu* menu
#else
                                                Gtk::Menu* menu
#endif
                                                )
    : radiotray(radiotray)
{
    menus.push(menu);
}

bool
RadioTrayLite::BookmarksWalker::for_each(pugi::xml_node& node)
{
    auto name = node.name();
    auto attr_name = node.attribute("name");
    auto attr_url = node.attribute("url");

    if (attr_name.empty() or strcasecmp(attr_name.as_string(), "root") == 0) {
        return true; // continue traversal
    }

    auto is_group = strcasecmp(name, "group") == 0;
    auto is_bookmark = strcasecmp(name, "bookmark") == 0;

    auto adjust_menu_level = [&]() {
        while (menus.size() >= static_cast<size_t>(depth())) {
            menus.pop();
        }
    };

    if (is_group) {
        adjust_menu_level();

        auto group_name = attr_name.as_string();
#ifdef APPINDICATOR_USE_AYATANA_GLIB
        auto menu_item = g_menu_item_new(group_name, nullptr);
        auto submenu = g_menu_new();

        g_menu_item_set_submenu(menu_item, G_MENU_MODEL(submenu));
        g_menu_append_item(menus.top(), menu_item);
        g_object_unref(menu_item);

        radiotray.owned_submenus.push_back(submenu);
        menus.push(submenu);
#else
        auto menu_item = Gtk::manage(new Gtk::MenuItem(group_name));
        auto submenu = Gtk::manage(new Gtk::Menu());

        menus.top()->append(*menu_item);
        menu_item->set_submenu(*submenu);
        menus.push(submenu);
#endif
        level = depth();

        LOG(DEBUG) << "Group: " << group_name << ", depth: " << depth();
    } else if (is_bookmark and (!attr_url.empty())) {
        adjust_menu_level();

        auto station_name = attr_name.as_string();
        auto station_group_name = node.parent().attribute("name").as_string();

        if (strncasecmp(station_name, kSeparatorPrefix, kSeparatorPrefixLength) == 0) {
#ifdef APPINDICATOR_USE_AYATANA_GLIB
            auto section = g_menu_new();
            g_menu_append_section(menus.top(), nullptr, G_MENU_MODEL(section));
            radiotray.owned_submenus.push_back(section);
#else
            auto separator = Gtk::manage(new Gtk::SeparatorMenuItem());
            menus.top()->append(*separator);
#endif
        } else {
            auto station_url = attr_url.as_string();
#ifdef APPINDICATOR_USE_AYATANA_GLIB
            radiotray.append_indicator_item(menus.top(),
                                            station_name,
                                            IndicatorAction::Type::Station,
                                            station_group_name,
                                            station_name,
                                            station_url);
#else
            auto sub_item = Gtk::manage(new Gtk::MenuItem(station_name));
            sub_item->signal_activate().connect(sigc::bind<Glib::ustring, Glib::ustring, Glib::ustring>(
                sigc::mem_fun(radiotray, &RadioTrayLite::on_station_button), station_group_name, station_name, station_url));
            menus.top()->append(*sub_item);
#endif
        }

        LOG(DEBUG) << "Bookmark depth: " << depth() << ", level: " << level << ", #menus: " << menus.size() << ", station: " << station_name
                   << ", group: " << station_group_name;
    }

    return true; // continue traversal
}

RadioTrayLite::~RadioTrayLite()
{
    clear_menu();

#ifdef APPINDICATOR_USE_AYATANA_GLIB
    if (actions != nullptr) {
        g_object_unref(G_OBJECT(actions));
    }

    if (menu != nullptr) {
        g_object_unref(G_OBJECT(menu));
    }
#endif

#ifndef APPINDICATOR_USE_GTK_STATUS_ICON
    if (indicator != nullptr) {
        g_object_unref(G_OBJECT(indicator));
    }
#endif

#if !defined(APPINDICATOR_USE_AYATANA_GLIB) && !defined(APPINDICATOR_USE_GTK_STATUS_ICON)
    remove_legacy_appindicator_log_handlers();
#endif
}

bool
RadioTrayLite::init(int argc, char** argv, std::shared_ptr<CmdLineOptions>& opts)
{
    cmd_line_options = opts;
    config = std::make_shared<Config>();

    load_configuration();

    // app = Gtk::Application::create(argc, argv, "github.com.thekvs.radiotray-lite");
    app = Gtk::Application::create("github.com.thekvs.radiotray-lite");
    app->register_application();
    if (app->is_remote()) {
        LOG(WARNING) << "This application is already running!";
        return false;
    }

#ifdef APPINDICATOR_USE_AYATANA_GLIB
    menu = g_menu_new();
    actions = g_simple_action_group_new();
#else
    menu = std::make_shared<Gtk::Menu>();
#endif

    player = std::make_shared<Player>();
    player->set_config(config);
    auto ok = player->init(argc, argv);
    if (not ok) {
        return false;
    }

    notifier = std::make_shared<Notification>(kAppName, config);
    ok = notifier->init();
    if (not ok) {
        return false;
    }

    em = std::make_shared<EventManager>();
    em->state_changed.connect(sigc::mem_fun(*this, &RadioTrayLite::on_station_changed_signal));
    em->broadcast_info_changed.connect(sigc::mem_fun(*this, &RadioTrayLite::on_broadcast_info_changed_signal));
    em->broadcast_info_changed.connect(sigc::mem_fun(*notifier, &Notification::on_broadcast_info_changed_signal));

    player->em = em;

#ifdef APPINDICATOR_USE_AYATANA_GLIB
    indicator = app_indicator_new_with_path(kAppName, kAppIndicatorIconOff, APP_INDICATOR_CATEGORY_APPLICATION_STATUS, kImagePath);
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_attention_icon(indicator, kAppIndicatorIconOn, kAppName);
    app_indicator_set_menu(indicator, menu);
    app_indicator_set_actions(indicator, actions);
#elif defined(APPINDICATOR_USE_GTK_STATUS_ICON)
    status_icon = Gtk::StatusIcon::create(kAppIndicatorIconOff);
    status_icon->set_tooltip_text(kAppName);
    status_icon->set_visible(true);
    status_icon->signal_activate().connect(sigc::mem_fun(*this, &RadioTrayLite::on_status_icon_activate));
    status_icon->signal_popup_menu().connect(sigc::mem_fun(*this, &RadioTrayLite::on_status_icon_popup_menu));
#else
    install_legacy_appindicator_log_handlers();
    indicator = app_indicator_new_with_path(kAppName, kAppIndicatorIconOff, APP_INDICATOR_CATEGORY_APPLICATION_STATUS, kImagePath);
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_attention_icon(indicator, kAppIndicatorIconOn);
    app_indicator_set_menu(indicator, menu->gobj());
#endif

    initialized = true;

    return true;
}

int
RadioTrayLite::run()
{
    if (not initialized) {
        LOG(ERROR) << "Application wasn't properly initialized!";
        return -1;
    }

    build_menu();

    app->hold();

    // resume the last played staion in timer callback
    sigc::slot<bool> resume_slot = sigc::bind(sigc::mem_fun(*this, &RadioTrayLite::resume), cmd_line_options->resume);
    sigc::connection conn = Glib::signal_timeout().connect(resume_slot, 200);

    auto rc = app->run();

    return rc;
}

void
RadioTrayLite::on_quit_button()
{
    LOG(DEBUG) << "'Quit' button was pressed.";

    player->stop();
    app->release();
}

void
RadioTrayLite::on_about_button()
{
    LOG(DEBUG) << "'About' button was pressed.";

    AboutDialog about;
    about.run();
}

void
RadioTrayLite::on_station_button(const Glib::ustring& group_name, const Glib::ustring& station_name, const Glib::ustring& station_url)
{
    player->stop();
    player->play(station_url, station_name);

    LOG(DEBUG) << "'" << station_url << "' "
               << "(group: " << group_name << ", station: " << station_name << ")"
               << " button was pressed.";
}

void
RadioTrayLite::on_reload_button()
{
    LOG(DEBUG) << "'Reload'"
               << " button was pressed";
    rebuild_menu();
}

void
RadioTrayLite::on_current_station_button()
{
    if (em->state == StationState::PLAYING) {
        player->pause();
    } else if (em->state == StationState::IDLE or em->state == StationState::UNKNOWN) {
        player->play();
    }
}

#ifdef APPINDICATOR_USE_GTK_STATUS_ICON
void
RadioTrayLite::on_status_icon_activate()
{
    on_current_station_button();
}

void
RadioTrayLite::on_status_icon_popup_menu(guint button, guint activate_time)
{
    if (menu) {
        menu->popup(button, activate_time);
    }
}
#endif

#ifdef APPINDICATOR_USE_AYATANA_GLIB
void
RadioTrayLite::on_indicator_action(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer user_data)
{
    auto* data = static_cast<IndicatorAction*>(user_data);
    auto* radiotray = data->radiotray;

    switch (data->type) {
    case IndicatorAction::Type::Station:
        radiotray->on_station_button(data->group_name, data->station_name, data->station_url);
        break;
    case IndicatorAction::Type::Reload:
        radiotray->on_reload_button();
        break;
    case IndicatorAction::Type::About:
        radiotray->on_about_button();
        break;
    case IndicatorAction::Type::Quit:
        radiotray->on_quit_button();
        break;
    case IndicatorAction::Type::CurrentStation:
        radiotray->on_current_station_button();
        break;
    }
}

std::string
RadioTrayLite::add_indicator_action(IndicatorAction::Type type,
                                    const Glib::ustring& group_name,
                                    const Glib::ustring& station_name,
                                    const Glib::ustring& station_url)
{
    auto action_data = std::unique_ptr<IndicatorAction>(new IndicatorAction());
    action_data->radiotray = this;
    action_data->type = type;
    action_data->group_name = group_name;
    action_data->station_name = station_name;
    action_data->station_url = station_url;

    std::stringstream name;
    name << "action" << action_counter++;

    auto simple_action = g_simple_action_new(name.str().c_str(), nullptr);
    g_signal_connect(simple_action, "activate", G_CALLBACK(&RadioTrayLite::on_indicator_action), action_data.get());
    g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(simple_action));
    g_object_unref(simple_action);

    indicator_actions.push_back(std::move(action_data));
    return "indicator." + name.str();
}

void
RadioTrayLite::append_indicator_item(GMenu* target,
                                     const Glib::ustring& label,
                                     IndicatorAction::Type type,
                                     const Glib::ustring& group_name,
                                     const Glib::ustring& station_name,
                                     const Glib::ustring& station_url)
{
    auto detailed_action = add_indicator_action(type, group_name, station_name, station_url);
    auto menu_item = g_menu_item_new(label.c_str(), detailed_action.c_str());
    g_menu_append_item(target, menu_item);
    g_object_unref(menu_item);
}

void
RadioTrayLite::set_indicator_icon(const char* icon_name)
{
    app_indicator_set_icon(indicator, icon_name, kAppName);
}
#endif

bool
RadioTrayLite::resume(bool resume_last_station)
{
    if (resume_last_station) {
        if (config->has_last_station()) {
            Glib::ustring data_url;
            try {
                std::stringstream xpath_query;
                xpath_query << "//bookmark[@name='" << config->get_last_played_station() << "']";

                pugi::xpath_node node = bookmarks_doc.select_node(xpath_query.str().c_str());
                if (not node.node().empty()) {
                    data_url = node.node().attribute("url").as_string();
                }
            } catch (pugi::xpath_exception& exc) {
                LOG(WARNING) << "XPath error: " << exc.what();
            }

            LOG(DEBUG) << "Resuming the last played station: " << config->get_last_played_station() << " (stream url: " << data_url << ")";
            player->play(data_url, config->get_last_played_station());
        }
    }

    // When we return false from the timer callback it deletes itself automatically
    // and won't be executed any more. So we have one time event here.
    return false;
}

void
RadioTrayLite::build_menu()
{
    auto bookmarks_parsed = parse_bookmarks_file();
    if (bookmarks_parsed) {
#ifdef APPINDICATOR_USE_AYATANA_GLIB
        BookmarksWalker walker(*this, menu);
#else
        BookmarksWalker walker(*this, &(*menu));
#endif
        bookmarks_doc.traverse(walker);
    } else {
        // TODO: notify about parsing errors
    }

    Glib::ustring name;

#ifdef APPINDICATOR_USE_AYATANA_GLIB
    auto section = g_menu_new();
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(section));
    owned_submenus.push_back(section);

    name = "Reload Bookmarks";
    append_indicator_item(menu, name, IndicatorAction::Type::Reload);

    name = "About";
    append_indicator_item(menu, name, IndicatorAction::Type::About);

    name = "Quit";
    append_indicator_item(menu, name, IndicatorAction::Type::Quit);
#else
    auto separator_item = Gtk::manage(new Gtk::SeparatorMenuItem());
    menu->append(*separator_item);

    name = "Reload Bookmarks";
    auto menu_item = Gtk::manage(new Gtk::MenuItem(name));
    menu_item->signal_activate().connect(sigc::mem_fun(*this, &RadioTrayLite::on_reload_button));
    menu->append(*menu_item);

    name = "About";
    menu_item = Gtk::manage(new Gtk::MenuItem(name));
    menu_item->signal_activate().connect(sigc::mem_fun(*this, &RadioTrayLite::on_about_button));
    menu->append(*menu_item);

    name = "Quit";
    menu_item = Gtk::manage(new Gtk::MenuItem(name));
    menu_item->signal_activate().connect(sigc::mem_fun(*this, &RadioTrayLite::on_quit_button));
    menu->append(*menu_item);

    separator_item = Gtk::manage(new Gtk::SeparatorMenuItem());
    menu->prepend(*separator_item);
#endif

    set_current_broadcast();

    auto turn_on = not(em->state == StationState::PLAYING);
    set_current_station(turn_on);
}

void
RadioTrayLite::rebuild_menu()
{
    clear_menu();
    build_menu();
}

void
RadioTrayLite::clear_menu()
{
#ifdef APPINDICATOR_USE_AYATANA_GLIB
    if (menu) {
        g_menu_remove_all(menu);
    }

    if (actions != nullptr) {
        auto old_actions = actions;
        actions = g_simple_action_group_new();
        if (indicator != nullptr) {
            app_indicator_set_actions(indicator, actions);
        }
        g_object_unref(G_OBJECT(old_actions));
    }

    for (auto* submenu : owned_submenus) {
        g_object_unref(G_OBJECT(submenu));
    }
    owned_submenus.clear();
    indicator_actions.clear();
    action_counter = 0;
    current_station_menu_index = -1;
    current_broadcast_menu_index = -1;
#else
    if (menu) {
        for (auto& e : menu->get_children()) {
            menu->remove(*e);
            delete e;
        }
    }

    current_station_menu_entry = nullptr;
    current_broadcast_menu_entry = nullptr;
#endif
}

bool
RadioTrayLite::parse_bookmarks_file()
{
    bool status = false;

    if (not bookmarks_file.empty()) {
        pugi::xml_parse_result result = bookmarks_doc.load_file(bookmarks_file.c_str());
        if (result) {
            status = true;
        } else {
            LOG(ERROR) << "XML parser failed: " << result.description();
        }
    } else {
        LOG(WARNING) << "Bookmarks file not specified!";
    }

    return status;
}

void
RadioTrayLite::load_configuration()
{
    std::string user_config_dir, dist_config_dir;
    bool has_user_bookmarks = false;

    auto home = getenv("HOME");
    if (home != nullptr) {
        user_config_dir = std::string(home) + "/.config/" + kAppDirName + "/";
    }

    dist_config_dir = std::string(INSTALL_PREFIX "/share/") + kAppDirName + "/";

    if (file_exists(user_config_dir, kBookmarksFileName)) {
        bookmarks_file = user_config_dir + kBookmarksFileName;
        has_user_bookmarks = true;
    }

    if (not has_user_bookmarks) {
        if (file_exists(dist_config_dir, kBookmarksFileName)) {
            bookmarks_file = dist_config_dir + kBookmarksFileName;
            copy_default_bookmarks(bookmarks_file);
        } else {
            LOG(WARNING) << "Distribution's bookmarks file doesn't exist in '" << dist_config_dir << "'";
        }
    }

    if (dir_exists(user_config_dir)) {
        config->set_config_file(user_config_dir + kConfigFileName);
        if (file_exists(user_config_dir, kConfigFileName)) {
            config->load_config();
        }
    }
}

void
RadioTrayLite::set_current_station(bool turn_on)
{
    if ((not player->has_station()) and config->has_last_station()) {
        try {
            std::stringstream xpath_query;
            xpath_query << "//bookmark[@name='" << config->get_last_played_station() << "']";

            pugi::xpath_node node = bookmarks_doc.select_node(xpath_query.str().c_str());
            if (not node.node().empty()) {
                auto data_url = node.node().attribute("url").as_string();
                player->init_streams(data_url, config->get_last_played_station());
            }
        } catch (pugi::xpath_exception& exc) {
            LOG(WARNING) << "XPath error: " << exc.what();
        }
    }

    if (player->has_station()) {
        auto mk_menu_entry = [](Glib::ustring name, bool turn_on) {
            std::stringstream ss;
            if (turn_on) {
                ss << R"(Turn On ")" << name << R"(")";
            } else {
                ss << R"(Turn Off ")" << name << R"(")";
            }

            return Glib::ustring(ss.str());
        };

#ifdef APPINDICATOR_USE_AYATANA_GLIB
        auto label = mk_menu_entry(player->get_station(), turn_on);
        if (current_station_menu_index < 0) {
            auto detailed_action = add_indicator_action(IndicatorAction::Type::CurrentStation);
            auto menu_item = g_menu_item_new(label.c_str(), detailed_action.c_str());
            g_menu_insert_item(menu, 0, menu_item);
            g_object_unref(menu_item);
            current_station_menu_index = 0;
            if (current_broadcast_menu_index >= 0) {
                current_broadcast_menu_index++;
            }
        } else {
            auto detailed_action = add_indicator_action(IndicatorAction::Type::CurrentStation);
            auto menu_item = g_menu_item_new(label.c_str(), detailed_action.c_str());
            g_menu_remove(menu, current_station_menu_index);
            g_menu_insert_item(menu, current_station_menu_index, menu_item);
            g_object_unref(menu_item);
        }
#else
        if (current_station_menu_entry == nullptr) {
            current_station_menu_entry = Gtk::manage(new Gtk::MenuItem(mk_menu_entry(player->get_station(), turn_on)));
            current_station_menu_entry->signal_activate().connect(sigc::mem_fun(*this, &RadioTrayLite::on_current_station_button));
            menu->prepend(*current_station_menu_entry);
        } else {
            current_station_menu_entry->set_label(mk_menu_entry(player->get_station(), turn_on));
        }
#endif
    }

#ifndef APPINDICATOR_USE_AYATANA_GLIB
    menu->show_all();
#endif
}

void
RadioTrayLite::set_current_broadcast(const Glib::ustring& info)
{
    auto split = [](const Glib::ustring& info, size_t size) {
        if (info.size() <= size) {
            return info;
        }

        Glib::ustring result;

        size_t chunk = 0;
        for (const auto& ch : info) {
            if (std::isspace(ch) != 0 and chunk >= size) {
                result += "\n";
                chunk = 0;
            } else {
                result += ch;
            }
            chunk++;
        }

        return result;
    };

    auto label = split(info, 30);

#ifdef APPINDICATOR_USE_AYATANA_GLIB
    auto menu_item = g_menu_item_new(label.c_str(), nullptr);
    if (current_broadcast_menu_index < 0) {
        g_menu_insert_item(menu, 0, menu_item);
        current_broadcast_menu_index = 0;
        if (current_station_menu_index >= 0) {
            current_station_menu_index++;
        }
    } else {
        g_menu_remove(menu, current_broadcast_menu_index);
        g_menu_insert_item(menu, current_broadcast_menu_index, menu_item);
    }
    g_object_unref(menu_item);
#else
    if (current_broadcast_menu_entry == nullptr) {
        current_broadcast_menu_entry = Gtk::manage(new Gtk::MenuItem(split(info, 30)));
        menu->prepend(*current_broadcast_menu_entry);
    } else {
        current_broadcast_menu_entry->set_label(split(info, 30));
    }

    current_broadcast_menu_entry->set_sensitive(false);
#endif
}

void
RadioTrayLite::on_station_changed_signal(const Glib::ustring& station, StationState state)
{
    LOG(DEBUG) << "Station changed: " << station << " state: " << get_station_state_desc(state)
               << " e.m. state: " << get_station_state_desc(em->state);

    if (state == em->state) {
        return;
    }

    config->set_last_played_station(station);

    auto turn_on = not(state == StationState::PLAYING);
    set_current_station(turn_on);

    if (state == StationState::IDLE) {
        set_current_broadcast();
    }

    if (state == StationState::PLAYING) {
#ifdef APPINDICATOR_USE_AYATANA_GLIB
        set_indicator_icon(kAppIndicatorIconOn);
#elif defined(APPINDICATOR_USE_GTK_STATUS_ICON)
        status_icon->set(kAppIndicatorIconOn);
#else
        app_indicator_set_icon(indicator, kAppIndicatorIconOn);
#endif
        set_current_broadcast(station);
    } else {
#ifdef APPINDICATOR_USE_AYATANA_GLIB
        set_indicator_icon(kAppIndicatorIconOff);
#elif defined(APPINDICATOR_USE_GTK_STATUS_ICON)
        status_icon->set(kAppIndicatorIconOff);
#else
        app_indicator_set_icon(indicator, kAppIndicatorIconOff);
#endif
    }
}

void
RadioTrayLite::on_broadcast_info_changed_signal(const Glib::ustring& /*station*/, const Glib::ustring& info)
{
    set_current_broadcast(info);

    LOG(DEBUG) << info;
}

#if !defined(APPINDICATOR_USE_AYATANA_GLIB) && !defined(APPINDICATOR_USE_GTK_STATUS_ICON)
void
RadioTrayLite::install_legacy_appindicator_log_handlers()
{
    auto flags = GLogLevelFlags(G_LOG_LEVEL_CRITICAL | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION);

    if (glib_gobject_log_handler_id == 0) {
        glib_gobject_log_handler_id = g_log_set_handler("GLib-GObject", flags, appindicator_gtk3_log_handler, nullptr);
    }

    if (gdk_log_handler_id == 0) {
        gdk_log_handler_id = g_log_set_handler("Gdk", flags, appindicator_gtk3_log_handler, nullptr);
    }
}

void
RadioTrayLite::remove_legacy_appindicator_log_handlers()
{
    if (glib_gobject_log_handler_id != 0) {
        g_log_remove_handler("GLib-GObject", glib_gobject_log_handler_id);
        glib_gobject_log_handler_id = 0;
    }

    if (gdk_log_handler_id != 0) {
        g_log_remove_handler("Gdk", gdk_log_handler_id);
        gdk_log_handler_id = 0;
    }
}
#endif

void
RadioTrayLite::copy_default_bookmarks(const std::string& src_file)
{
    auto home = getenv("HOME");
    if (home == nullptr) {
        return;
    }

    auto copy_file = [](const std::string& dst_dir, const std::string& src_file) {
        auto dst_file = dst_dir + kBookmarksFileName;

        std::ifstream src(src_file, std::ios::binary);
        std::ofstream dst(dst_file, std::ios::binary);

        dst << src.rdbuf();
    };

    std::string path = home;
    path.append("/.config/").append(kAppDirName).append("/");

    if (not dir_exists(path)) {
        auto rc = mkdir(path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        if (rc == 0) {
            copy_file(path, src_file);
        } else {
            LOG(WARNING) << "Couldn't create '" << path << "': " << strerror(errno);
        }
    } else {
        copy_file(path, src_file);
    }
}

bool
RadioTrayLite::file_exists(const std::string& dir, const std::string& file)
{
    if (dir.empty() or file.empty()) {
        return false;
    }

    auto full_name = dir + file;

    struct stat st;
    std::memset(&st, 0, sizeof(st));
    auto rc = stat(full_name.c_str(), &st);

    return (rc == 0 and S_ISREG(st.st_mode));
};

bool
RadioTrayLite::dir_exists(const std::string& dir)
{
    if (dir.empty()) {
        return false;
    }

    struct stat st;
    std::memset(&st, 0, sizeof(st));
    auto rc = stat(dir.c_str(), &st);

    return (rc == 0 and S_ISDIR(st.st_mode));
};

} // namespace radiotray
