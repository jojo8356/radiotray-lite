#ifndef __ICON_CACHE_HPP_INCLUDED__
#define __ICON_CACHE_HPP_INCLUDED__

#include <map>
#include <string>

namespace radiotray
{

// Resolves the custom icon of a bookmark to a local file that GTK can load.
//
// A bookmark may reference an icon either by URL ("icon") or by local path,
// and it may tell where a downloaded icon must be stored ("icon_path").
// Rules:
//
//   icon      icon_path   behavior
//   ---------------------------------------------------------------
//   (empty)   (empty)     no icon
//   path      (empty)     use the path if the file exists
//   (empty)   path        use the path if the file exists
//   URL       (empty)     use <cache dir>/<hash>-<name>, download if missing
//   URL       path        use <path>, download from the URL if missing
//
// Remote icons are downloaded once: as soon as the destination file exists it
// is reused, no network access is performed.
class IconCache
{
public:
    IconCache() = default;
    IconCache(const IconCache&) = delete;

    std::string resolve(const std::string& icon, const std::string& icon_path, const std::string& base_dir);

    // Forgets cached results (both successes and failures), keeping the
    // downloaded files on disk. Called when the bookmarks are reloaded.
    void clear();

    // Directory used when no icon_path is given: $XDG_CACHE_HOME/.../icons
    static std::string default_cache_dir();

private:
    std::map<std::string, std::string> resolved;

    std::string do_resolve(const std::string& icon, const std::string& icon_path, const std::string& base_dir);
    bool download(const std::string& url, const std::string& destination);
};

} // namespace radiotray

#endif
