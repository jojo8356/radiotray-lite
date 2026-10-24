#include "app_paths.hpp"

#include <cstdlib>
#include <cstring>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "constants.hpp"
#include "easyloggingpp/easylogging++.h"

namespace radiotray
{
namespace
{

bool
is_dir(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    struct stat st;
    std::memset(&st, 0, sizeof(st));

    return (stat(path.c_str(), &st) == 0 and S_ISDIR(st.st_mode));
}

bool
is_regular_file(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    struct stat st;
    std::memset(&st, 0, sizeof(st));

    return (stat(path.c_str(), &st) == 0 and S_ISREG(st.st_mode));
}

std::vector<std::string>
image_dir_candidates()
{
    std::vector<std::string> candidates;

    // Installed location (CMAKE_INSTALL_PREFIX at build time).
    candidates.push_back(kImagePath);

    auto exe_dir = executable_dir();
    if (not exe_dir.empty()) {
        // build/src/radiotray-lite -> <repo>/data/images
        candidates.push_back(exe_dir + "../../data/images/");
        // AppImage / install layout next to the binary
        candidates.push_back(exe_dir + "../share/radiotray-lite/images/");
        candidates.push_back(exe_dir + "images/");
    }

    // Typical installation prefixes, useful when the binary was configured
    // with a prefix it was not actually installed to.
    candidates.push_back("/usr/share/radiotray-lite/images/");
    candidates.push_back("/usr/local/share/radiotray-lite/images/");

    return candidates;
}

} // namespace

std::string
executable_dir()
{
    char buffer[4096];
    auto len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len <= 0) {
        return std::string();
    }

    buffer[len] = '\0';

    std::string path(buffer);
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return std::string();
    }

    return path.substr(0, pos + 1);
}

std::string
expand_user_path(const std::string& path)
{
    if (path == "~" or path.compare(0, 2, "~/") == 0) {
        auto home = getenv("HOME");
        if (home != nullptr and *home != '\0') {
            return std::string(home) + path.substr(1);
        }
    }

    return path;
}

std::string
app_image_dir()
{
    static std::string cached;
    static bool searched = false;

    if (searched) {
        return cached;
    }

    searched = true;

    for (const auto& candidate : image_dir_candidates()) {
        if (is_dir(candidate)) {
            cached = candidate;
            LOG(INFO) << "Using images from: " << cached;
            return cached;
        }
    }

    LOG(WARNING) << "Application images not found, expected in '" << kImagePath << "'";

    return std::string();
}

std::string
find_app_image(const std::string& name)
{
    if (name.empty()) {
        return std::string();
    }

    for (const auto& candidate : image_dir_candidates()) {
        auto path = candidate + name;
        if (is_regular_file(path)) {
            return path;
        }
    }

    return std::string();
}

} // namespace radiotray
