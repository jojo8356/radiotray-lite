#ifndef __GST_DIAGNOSTICS_HPP_INCLUDED__
#define __GST_DIAGNOSTICS_HPP_INCLUDED__

#include <string>

namespace radiotray
{

// When a stream can't be played because a GStreamer plugin is missing,
// GStreamer's debug string usually contains: no element "avdec_aac".
// These helpers turn that into something actionable. They are plain C++ on
// purpose: no GStreamer header is needed, so they can be unit tested.
std::string extract_missing_element(const std::string& debug);

// Debian/Ubuntu package that ships the given element, when it is known.
std::string debian_package_for_element(const std::string& element);

// Fedora package that ships the given element, when it is known.
std::string fedora_package_for_element(const std::string& element);

} // namespace radiotray

#endif
