#include "icon_cache.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <curl/curl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "constants.hpp"
#include "easyloggingpp/easylogging++.h"

namespace radiotray
{
namespace
{

const long kIconDownloadTimeoutMs = 10 * 1000;
const long kIconConnectTimeoutMs = 5 * 1000;
const curl_off_t kMaxIconSize = 4 * 1024 * 1024; // 4 MiB
const size_t kMaxIconSignatureSize = 1024;

const char* const kIconUserAgent = "Radio Tray Lite (+https://github.com/thekvs/radiotray-lite)";

std::string
trim(const std::string& str)
{
    auto begin = str.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return std::string();
    }

    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(begin, end - begin + 1);
}

std::string
to_lower(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](char c) { return std::tolower(static_cast<unsigned char>(c)); });
    return result;
}

bool
starts_with(const std::string& str, const std::string& prefix)
{
    return (str.size() >= prefix.size() and str.compare(0, prefix.size(), prefix) == 0);
}

bool
is_regular_file(const std::string& path)
{
    struct stat st;
    std::memset(&st, 0, sizeof(st));

    return (stat(path.c_str(), &st) == 0 and S_ISREG(st.st_mode));
}

bool
is_dir(const std::string& path)
{
    struct stat st;
    std::memset(&st, 0, sizeof(st));

    return (stat(path.c_str(), &st) == 0 and S_ISDIR(st.st_mode));
}

// A file is usable as an icon only when GdkPixbuf knows a loader for it: an
// empty file, an HTML error page saved by a previous download, or a truncated
// image would otherwise be treated as a valid icon and fail at display time.
bool
is_usable_file(const std::string& path)
{
    if (path.empty() or not is_regular_file(path)) {
        return false;
    }

    struct stat st;
    std::memset(&st, 0, sizeof(st));
    if (stat(path.c_str(), &st) != 0 or st.st_size == 0) {
        return false;
    }

    return (gdk_pixbuf_get_file_info(path.c_str(), nullptr, nullptr) != nullptr);
}

// Cheap check on the first bytes, used before GdkPixbuf sees the file.
bool
is_loadable_image(const std::string& path)
{
    return (gdk_pixbuf_get_file_info(path.c_str(), nullptr, nullptr) != nullptr);
}

std::string
dir_name(const std::string& path)
{
    auto pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return std::string(".");
    }

    if (pos == 0) {
        return std::string("/");
    }

    return path.substr(0, pos);
}

// mkdir -p, permissions 0755
bool
make_dirs(const std::string& dir)
{
    if (dir.empty() or dir == "/") {
        return true;
    }

    if (is_dir(dir)) {
        return true;
    }

    auto parent = dir_name(dir);
    if (parent != dir and not make_dirs(parent)) {
        return false;
    }

    auto rc = mkdir(dir.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (rc == 0 or errno == EEXIST) {
        return true;
    }

    LOG(ERROR) << "Couldn't create directory '" << dir << "': " << std::strerror(errno);
    return false;
}

// Expands "~", "~/..." and resolves relative paths against the directory of
// the bookmarks file. Absolutes paths are returned unchanged.
std::string
expand_path(const std::string& path, const std::string& base_dir)
{
    if (path.empty()) {
        return path;
    }

    if (path == "~" or starts_with(path, "~/")) {
        auto home = getenv("HOME");
        if (home != nullptr and *home != '\0') {
            return std::string(home) + path.substr(1);
        }
        return path;
    }

    if (path[0] == '/') {
        return path;
    }

    if (not base_dir.empty()) {
        auto dir = base_dir;
        if (dir.back() != '/') {
            dir += '/';
        }
        return dir + path;
    }

    return path;
}

bool
is_url(const std::string& str)
{
    auto lower = to_lower(str);

    return (starts_with(lower, "http://") or starts_with(lower, "https://") or starts_with(lower, "ftp://"));
}

// Last component of an URL, without query string nor fragment, reduced to
// characters that are safe in a file name.
std::string
url_base_name(const std::string& url)
{
    auto path = url;
    auto cut = path.find_first_of("?#");
    if (cut != std::string::npos) {
        path = path.substr(0, cut);
    }

    auto pos = path.find_last_of('/');
    if (pos != std::string::npos) {
        path = path.substr(pos + 1);
    }

    std::string name;
    for (const auto& ch : path) {
        auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) != 0 or ch == '.' or ch == '-' or ch == '_') {
            name += ch;
        } else {
            name += '_';
        }
    }

    // No leading dot: the file must not become a hidden file.
    while (not name.empty() and name.front() == '.') {
        name.erase(name.begin());
    }

    if (name.size() > 64) {
        name = name.substr(name.size() - 64);
    }

    if (name.find('.') == std::string::npos) {
        name += ".img";
    }

    return name;
}

// Small non-cryptographic hash, only used to avoid name collisions between
// icons coming from different URLs.
std::string
short_hash(const std::string& str)
{
    uint64_t hash = 14695981039346656037ULL; // FNV-1a

    for (const auto& ch : str) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(ch));
        hash *= 1099511628211ULL;
    }

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(hash));

    return std::string(buffer);
}

bool
file_looks_like_image(const std::string& path)
{
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        return false;
    }

    char buf[kMaxIconSignatureSize];
    auto bytes_read = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    if (bytes_read < 8) {
        return false;
    }

    std::string header(buf, bytes_read);

    if (header.compare(0, 8, "\x89PNG\r\n\x1a\n", 8) == 0) {
        return true; // PNG
    }

    if (header.compare(0, 3, "\xff\xd8\xff", 3) == 0) {
        return true; // JPEG
    }

    if (starts_with(header, "GIF87a") or starts_with(header, "GIF89a")) {
        return true; // GIF
    }

    if (starts_with(header, "BM")) {
        return true; // BMP
    }

    if (starts_with(header, "RIFF") and header.compare(8, 4, "WEBP") == 0) {
        return true; // WEBP
    }

    if (header.compare(0, 4, "\x00\x00\x01\x00", 4) == 0) {
        return true; // ICO
    }

    if (starts_with(header, "/* XPM */")) {
        return true; // XPM
    }

    auto lower = to_lower(header);
    auto svg_pos = lower.find("<svg");
    if (svg_pos != std::string::npos or starts_with(lower, "<?xml")) {
        return true; // SVG
    }

    return false;
}

struct DownloadSink
{
    FILE* fp = nullptr;
    size_t received = 0;
};

size_t
write_file_cb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto sink = static_cast<DownloadSink*>(userdata);
    if (sink == nullptr or sink->fp == nullptr) {
        return 0;
    }

    auto bytes = size * nmemb;

    // Servers do not always announce Content-Length: enforce the limit here.
    if (sink->received + bytes > static_cast<size_t>(kMaxIconSize)) {
        return 0; // abort the transfer
    }

    if (fwrite(ptr, 1, bytes, sink->fp) != bytes) {
        return 0;
    }

    sink->received += bytes;

    return bytes;
}

} // namespace

std::string
IconCache::default_cache_dir()
{
    std::string dir;

    auto xdg_cache_home = getenv("XDG_CACHE_HOME");
    if (xdg_cache_home != nullptr and *xdg_cache_home != '\0') {
        dir = xdg_cache_home;
    } else {
        auto home = getenv("HOME");
        if (home != nullptr and *home != '\0') {
            dir = std::string(home) + "/.cache";
        } else {
            dir = "/tmp";
        }
    }

    return dir + "/" + kAppDirName + "/icons";
}

void
IconCache::clear()
{
    resolved.clear();
}

std::string
IconCache::resolve(const std::string& icon, const std::string& icon_path, const std::string& base_dir)
{
    std::stringstream key;
    key << icon << '\n' << icon_path << '\n' << base_dir;

    auto it = resolved.find(key.str());
    if (it != resolved.end()) {
        // The cached path may have been removed by the user in the meantime.
        if (it->second.empty() or is_usable_file(it->second)) {
            return it->second;
        }
        resolved.erase(it);
    }

    auto result = do_resolve(trim(icon), trim(icon_path), base_dir);
    resolved[key.str()] = result;

    return result;
}

std::string
IconCache::do_resolve(const std::string& icon, const std::string& icon_path, const std::string& base_dir)
{
    if (icon.empty() and icon_path.empty()) {
        return std::string();
    }

    // A reference that is not an URL is a plain local file: nothing to download.
    if (not is_url(icon)) {
        auto path = expand_path(icon.empty() ? icon_path : icon, base_dir);

        if (is_usable_file(path)) {
            return path;
        }

        if (is_regular_file(path)) {
            LOG(WARNING) << "Icon file is not a readable image: '" << path << "'";
        } else {
            LOG(WARNING) << "Icon file not found: '" << path << "'";
        }

        return std::string();
    }

    // Remote icon. Either the bookmarks file says where to store it, or it
    // goes into the user's cache directory.
    std::string destination;
    if (icon_path.empty()) {
        destination = default_cache_dir() + "/" + short_hash(icon) + "-" + url_base_name(icon);
    } else {
        destination = expand_path(icon_path, base_dir);
        if (is_dir(destination)) {
            destination += "/" + url_base_name(icon);
        }
    }

    // Already downloaded: use the local file, no network access.
    if (is_usable_file(destination)) {
        LOG(DEBUG) << "Using cached icon '" << destination << "' for " << icon;
        return destination;
    }

    if (is_regular_file(destination)) {
        // Typically an HTML error page saved by an earlier download, or a file
        // whose format GdkPixbuf can't read (AVIF without loader, ...).
        LOG(WARNING) << "Cached icon '" << destination << "' is not a readable image, downloading it again";
    }

    LOG(INFO) << "Downloading icon " << icon << " to '" << destination << "'";

    if (not download(icon, destination)) {
        LOG(WARNING) << "Couldn't download icon " << icon;
        return std::string();
    }

    return destination;
}

bool
IconCache::download(const std::string& url, const std::string& destination)
{
    if (not make_dirs(dir_name(destination))) {
        return false;
    }

    // Download to a temporary file first so that an interrupted transfer never
    // leaves a truncated icon behind (which would then be considered valid).
    auto tmp_file = destination + ".part";

    FILE* fp = fopen(tmp_file.c_str(), "wb");
    if (fp == nullptr) {
        LOG(ERROR) << "Couldn't open '" << tmp_file << "': " << std::strerror(errno);
        return false;
    }

    auto handle = curl_easy_init();
    if (handle == nullptr) {
        fclose(fp);
        unlink(tmp_file.c_str());
        return false;
    }

    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 7);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, kIconUserAgent);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, kIconConnectTimeoutMs);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, kIconDownloadTimeoutMs);
    curl_easy_setopt(handle, CURLOPT_MAXFILESIZE_LARGE, kMaxIconSize);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_file_cb);

    DownloadSink sink;
    sink.fp = fp;
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, static_cast<void*>(&sink));

    auto rc = curl_easy_perform(handle);

    long http_status = 0;
    char* content_type = nullptr;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_status);
    curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &content_type);
    curl_easy_cleanup(handle);

    auto write_error = ferror(fp) != 0;
    fclose(fp);

    if (rc != CURLE_OK) {
        if (rc == CURLE_WRITE_ERROR and sink.received >= static_cast<size_t>(kMaxIconSize)) {
            LOG(WARNING) << "Icon is larger than " << kMaxIconSize << " bytes: " << url;
        } else {
            LOG(WARNING) << "Icon download failed (" << curl_easy_strerror(rc) << "): " << url;
        }
        unlink(tmp_file.c_str());
        return false;
    }

    if (write_error) {
        LOG(ERROR) << "Couldn't write '" << tmp_file << "'";
        unlink(tmp_file.c_str());
        return false;
    }

    if (http_status < 200 or http_status >= 300) {
        LOG(WARNING) << "Icon download returned HTTP status " << http_status << ": " << url;
        unlink(tmp_file.c_str());
        return false;
    }

    auto ctype_is_image = false;
    if (content_type != nullptr) {
        ctype_is_image = starts_with(to_lower(content_type), "image/");
    }

    // Servers answering with an HTML error page must not be cached as icons.
    if (not ctype_is_image and not file_looks_like_image(tmp_file)) {
        LOG(WARNING) << "Downloaded data from " << url << " is not an image (Content-Type: '"
                     << (content_type != nullptr ? content_type : "") << "')";
        unlink(tmp_file.c_str());
        return false;
    }

    // Last check: the file must be loadable by GdkPixbuf, otherwise it could
    // never be displayed (and it would be cached as a broken icon).
    if (not is_loadable_image(tmp_file)) {
        LOG(WARNING) << "Downloaded file from " << url << " can't be decoded as an image by GdkPixbuf";
        unlink(tmp_file.c_str());
        return false;
    }

    if (rename(tmp_file.c_str(), destination.c_str()) != 0) {
        LOG(ERROR) << "Couldn't move '" << tmp_file << "' to '" << destination << "': " << std::strerror(errno);
        unlink(tmp_file.c_str());
        return false;
    }

    return true;
}

} // namespace radiotray
