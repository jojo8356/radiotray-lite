#ifndef __URL_UTILS_HPP_INCLUDED__
#define __URL_UTILS_HPP_INCLUDED__

#include <string>

namespace radiotray
{

// True for "scheme://..." URLs and for protocol-relative "//host/path".
bool is_absolute_url(const std::string& url);

// "http://host:port" of an absolute URL, without the path. Empty when the URL
// is not absolute.
std::string url_origin(const std::string& url);

// Everything up to the last '/' of the path ("https://host/a/b" ->
// "https://host/a/"). The origin plus a slash when there is no path.
std::string url_directory(const std::string& url);

// Resolves a reference found in a playlist against the URL of that playlist,
// following RFC 3986 for the common cases: absolute URLs, protocol-relative
// "//host/path", absolute paths "/stream", relative paths "stream" and
// "./stream", and parent references "../stream". Query strings and fragments
// are preserved.
std::string resolve_url(const std::string& base, const std::string& reference);

} // namespace radiotray

#endif
