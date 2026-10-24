#include "url_utils.hpp"

#include <cctype>
#include <vector>

namespace radiotray
{
namespace
{

// Index of the ':' that ends the scheme, or npos.
size_t scheme_end(const std::string& url)
{
    for (size_t i = 0; i < url.size(); i++) {
        auto c = url[i];

        if (c == ':') {
            return i;
        }

        auto is_scheme_char = (std::isalnum(static_cast<unsigned char>(c)) != 0) or c == '+' or c == '-' or c == '.';
        if (not is_scheme_char) {
            return std::string::npos;
        }
    }

    return std::string::npos;
}

bool has_scheme(const std::string& url)
{
    auto pos = scheme_end(url);

    // "http://x" -> scheme at 4, followed by "//"
    return (pos != std::string::npos and url.compare(pos, 3, "://") == 0);
}

// Removes "./" segments and collapses "x/../".
std::string normalize_path(const std::string& path)
{
    std::vector<std::string> segments;
    std::string segment;
    std::string result;
    bool trailing_slash = false;

    for (size_t i = 0; i <= path.size(); i++) {
        if (i == path.size() or path[i] == '/') {
            if (segment == "..") {
                if (not segments.empty()) {
                    segments.pop_back();
                }
            } else if (segment != "." and not segment.empty()) {
                segments.push_back(segment);
            }

            if (i == path.size()) {
                trailing_slash = (not path.empty() and path.back() == '/');
            }

            segment.clear();
        } else {
            segment += path[i];
        }
    }

    for (const auto& s : segments) {
        result += '/' + s;
    }

    if (result.empty()) {
        result = "/";
    }

    if (trailing_slash and result.back() != '/') {
        result += '/';
    }

    return result;
}

} // namespace

bool
is_absolute_url(const std::string& url)
{
    return has_scheme(url) or url.compare(0, 2, "//") == 0;
}

std::string
url_origin(const std::string& url)
{
    if (not has_scheme(url)) {
        return std::string();
    }

    auto start = url.find("://") + 3;
    auto end = url.find_first_of("/?#", start);

    return url.substr(0, end == std::string::npos ? url.size() : end);
}

std::string
url_directory(const std::string& url)
{
    auto origin = url_origin(url);
    if (origin.empty()) {
        return std::string();
    }

    auto path_start = origin.size();
    auto end = url.find_first_of("?#", path_start);
    auto path = url.substr(path_start, end == std::string::npos ? std::string::npos : end - path_start);

    auto slash = path.find_last_of('/');

    return origin + (slash == std::string::npos ? std::string("/") : path.substr(0, slash + 1));
}

std::string
resolve_url(const std::string& base, const std::string& reference)
{
    if (reference.empty()) {
        return base;
    }

    // Already usable as is.
    if (has_scheme(reference)) {
        return reference;
    }

    auto origin = url_origin(base);
    if (origin.empty()) {
        return reference; // no usable base, leave the reference untouched
    }

    auto scheme = base.substr(0, base.find(':') + 1);

    if (reference.compare(0, 2, "//") == 0) {
        return scheme + reference;
    }

    // Split the reference into path and suffix (query / fragment).
    auto suffix_pos = reference.find_first_of("?#");
    auto ref_path = reference.substr(0, suffix_pos);
    auto suffix = (suffix_pos == std::string::npos) ? std::string() : reference.substr(suffix_pos);

    if (not ref_path.empty() and ref_path.front() == '/') {
        return origin + normalize_path(ref_path) + suffix;
    }

    auto dir = url_directory(base);
    auto dir_path = dir.substr(origin.size());

    if (ref_path.empty()) {
        // Only a query or a fragment: applies to the base document.
        auto base_path = base.substr(origin.size());
        auto base_suffix = base_path.find_first_of("?#");
        if (base_suffix != std::string::npos) {
            base_path = base_path.substr(0, base_suffix);
        }
        return origin + base_path + suffix;
    }

    return origin + normalize_path(dir_path + ref_path) + suffix;
}

} // namespace radiotray
