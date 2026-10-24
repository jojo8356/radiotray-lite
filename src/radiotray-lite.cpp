#include <cstdlib>
#include <exception>

#include "tray.hpp"
#include "options.hpp"

INITIALIZE_EASYLOGGINGPP

using namespace radiotray;

namespace
{

// GTK and GStreamer report unrecoverable problems through g_error(), whose
// default handler calls abort() and leaves no message behind when the output is
// filtered or redirected. Log the message ourselves and exit cleanly instead.
void
log_handler(const gchar* log_domain, GLogLevelFlags log_level, const gchar* message, gpointer user_data)
{
    if ((log_level & G_LOG_LEVEL_ERROR) != 0 or (log_level & G_LOG_FLAG_FATAL) != 0) {
        std::cerr << (log_domain != nullptr ? log_domain : "GLib") << ": Fatal: " << (message != nullptr ? message : "") << std::endl;
        _exit(EXIT_FAILURE);
    }

    g_log_default_handler(log_domain, log_level, message, user_data);
}

} // namespace

int
main(int argc, char* argv[])
{
    el::Configurations easylogging_config;
    easylogging_config.setToDefault();
// Values are always std::string

#ifndef NDEBUG
    easylogging_config.set(el::Level::Info, el::ConfigurationType::Format, "%datetime %level %loc %msg");
    easylogging_config.set(el::Level::Error, el::ConfigurationType::Format, "%datetime %level %loc %msg");
    easylogging_config.set(el::Level::Warning, el::ConfigurationType::Format, "%datetime %level %loc %msg");
#else
    easylogging_config.set(el::Level::Info, el::ConfigurationType::Format, "%level %msg");
    easylogging_config.set(el::Level::Error, el::ConfigurationType::Format, "%level %msg");
    easylogging_config.set(el::Level::Warning, el::ConfigurationType::Format, "%level %msg");
#endif
    // do not log to a file
    easylogging_config.setGlobally(el::ConfigurationType::ToFile, "false");

    // default logger uses default configurations
    el::Loggers::reconfigureLogger("default", easylogging_config);

    // Make fatal GLib/GTK/GStreamer errors printable instead of a silent abort.
    g_log_set_default_handler(log_handler, nullptr);

    auto opts = std::make_shared<CmdLineOptions>();
    RadioTrayLite rtl;

    auto ok = opts->parse(argc, argv);
    if (not ok) {
        return EXIT_FAILURE;
    }

    if (opts->help) {
        opts->show_help();
        return EXIT_SUCCESS;
    }

    int rc = EXIT_FAILURE;

    try {
        ok = rtl.init(argc, argv, opts);
        if (not ok) {
            LOG(ERROR) << "Initialization failed";
            return EXIT_FAILURE;
        }

        rc = rtl.run();
    } catch (const Glib::Error& exc) {
        LOG(ERROR) << "GLib error: " << exc.what();
    } catch (const std::exception& exc) {
        LOG(ERROR) << "Unhandled exception: " << exc.what();
    } catch (...) {
        LOG(ERROR) << "Unhandled unknown exception";
    }

    return rc;
}
