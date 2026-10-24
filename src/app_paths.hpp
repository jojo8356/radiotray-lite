#ifndef __APP_PATHS_HPP_INCLUDED__
#define __APP_PATHS_HPP_INCLUDED__

#include <string>

namespace radiotray
{

// Directory of the running binary, with a trailing slash (empty on failure).
std::string executable_dir();

// Expands a leading "~" / "~/" using $HOME. Other paths are returned as is.
std::string expand_user_path(const std::string& path);

// First directory that contains the application images. It is
// <install prefix>/share/radiotray-lite/images/ for an installed binary, but
// also handles running straight from a build tree (build/src/radiotray-lite)
// or from an AppImage layout. Empty when nothing is found.
std::string app_image_dir();

// Absolute path of an image shipped with the application (for example
// kAppIconOn), or an empty string when it can't be found.
std::string find_app_image(const std::string& name);

} // namespace radiotray

#endif
