#include "about.hpp"

#include "app_paths.hpp"
#include "easyloggingpp/easylogging++.h"

namespace radiotray
{

AboutDialog::AboutDialog()
{
    this->set_program_name(kAppName);
    this->set_version(kAppVersion);

    // Images may be missing when the binary runs from a build tree: never let
    // that prevent the dialog from opening.
    auto icon = find_app_image(kAppIcon);

    if (not icon.empty()) {
        try {
            this->set_icon_from_file(icon);

            auto logo = Gdk::Pixbuf::create_from_file(icon);
            this->set_logo(logo);
        } catch (const Glib::Error& exc) {
            LOG(WARNING) << "Couldn't load '" << icon << "': " << exc.what();
        }
    }

    std::vector<Glib::ustring> authors = { kAuthor };
    this->set_authors(authors);

    this->set_license_type(Gtk::License::LICENSE_GPL_3_0);

    this->set_website(kWebSite);
    this->set_website_label("Project's website");

    char copyright[1024];
    memset(copyright, 0, sizeof(copyright));
    snprintf(copyright, sizeof(copyright) - 1, kCopyrightTmpl, kAppName, kCopyrightYear, kAuthor);
    this->set_copyright(copyright);
}

} // namespace radiotray
