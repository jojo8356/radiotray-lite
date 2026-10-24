#include "gst_diagnostics.hpp"

#include <vector>

namespace radiotray
{
namespace
{

struct ElementPackage
{
    const char* element;    // exact element name
    const char* prefix;     // element name prefix (nullptr when unused)
    const char* debian;     // Debian/Ubuntu package
    const char* fedora;     // Fedora package
};

// Only entries that are commonly needed for web radios are listed. The first
// matching entry wins, exact names before prefixes.
const ElementPackage kElements[] = {
    // AAC / HE-AAC
    { "faad", nullptr, "gstreamer1.0-plugins-bad", "gstreamer1-plugins-bad-free" },
    { "aacparse", nullptr, "gstreamer1.0-plugins-bad", "gstreamer1-plugins-bad-free" },
    { "avdec_aac", nullptr, "gstreamer1.0-libav", "gstreamer1-plugins-bad-freeworld" },
    { "avdec_aac_latm", nullptr, "gstreamer1.0-libav", "gstreamer1-plugins-bad-freeworld" },
    { "avmux_latm", nullptr, "gstreamer1.0-libav", "gstreamer1-plugins-bad-freeworld" },

    // MP3
    { "mpegaudioparse", nullptr, "gstreamer1.0-plugins-good", "gstreamer1-plugins-good" },
    { "mpg123audiodec", nullptr, "gstreamer1.0-plugins-ugly", "gstreamer1-plugins-ugly-free" },
    { "avdec_mp3", nullptr, "gstreamer1.0-libav", "gstreamer1-plugins-bad-freeworld" },
    { "mad", nullptr, "gstreamer1.0-plugins-ugly", "gstreamer1-plugins-ugly-free" },

    // Vorbis / Opus / FLAC
    { "vorbisdec", nullptr, "gstreamer1.0-plugins-base", "gstreamer1-plugins-base" },
    { "opusdec", nullptr, "gstreamer1.0-plugins-base", "gstreamer1-plugins-base" },
    { "flacdec", nullptr, "gstreamer1.0-plugins-good", "gstreamer1-plugins-good" },

    // Transport / containers
    { "souphttpsrc", nullptr, "gstreamer1.0-plugins-good", "gstreamer1-plugins-good" },
    { "souphttpclientsink", nullptr, "gstreamer1.0-plugins-good", "gstreamer1-plugins-good" },
    { "typefindfunctions", nullptr, "gstreamer1.0-plugins-base", "gstreamer1-plugins-base" },
    { "id3demux", nullptr, "gstreamer1.0-plugins-good", "gstreamer1-plugins-good" },
    { "hlsdemux", nullptr, "gstreamer1.0-plugins-bad", "gstreamer1-plugins-bad-free" },
    { "dashdemux", nullptr, "gstreamer1.0-plugins-bad", "gstreamer1-plugins-bad-free" },
    { "smoothstreaming", nullptr, "gstreamer1.0-plugins-bad", "gstreamer1-plugins-bad-free" },

    // Windows Media
    { "asfdemux", nullptr, "gstreamer1.0-plugins-ugly", "gstreamer1-plugins-ugly-free" },
    { "wmadec", nullptr, "gstreamer1.0-plugins-ugly", "gstreamer1-plugins-ugly-free" },

    // Audio plumbing
    { "audioconvert", nullptr, "gstreamer1.0-plugins-base", "gstreamer1-plugins-base" },
    { "audioresample", nullptr, "gstreamer1.0-plugins-base", "gstreamer1-plugins-base" },
    { "autoaudiosink", nullptr, "gstreamer1.0-plugins-good", "gstreamer1-plugins-good" },
    { "pulsesink", nullptr, "gstreamer1.0-plugins-good", "gstreamer1-plugins-good" },
    { "alsasink", nullptr, "gstreamer1.0-plugins-base", "gstreamer1-plugins-base" },

    // Catch-all families
    { nullptr, "avdec_", "gstreamer1.0-libav", "gstreamer1-plugins-bad-freeworld" },
    { nullptr, "avdemux_", "gstreamer1.0-libav", "gstreamer1-plugins-bad-freeworld" },
    { nullptr, "faad_", "gstreamer1.0-plugins-bad", "gstreamer1-plugins-bad-free" },
};

bool
starts_with(const std::string& str, const char* prefix)
{
    return str.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

const ElementPackage*
find_package(const std::string& element)
{
    if (element.empty()) {
        return nullptr;
    }

    for (const auto& entry : kElements) {
        if (entry.element != nullptr and element == entry.element) {
            return &entry;
        }
    }

    for (const auto& entry : kElements) {
        if (entry.prefix != nullptr and starts_with(element, entry.prefix)) {
            return &entry;
        }
    }

    return nullptr;
}

} // namespace

std::string
extract_missing_element(const std::string& debug)
{
    // GStreamer writes: no element "avdec_aac"
    static const std::string kMarker = "no element \"";

    auto pos = debug.find(kMarker);
    if (pos == std::string::npos) {
        return std::string();
    }

    pos += kMarker.size();

    auto end = debug.find('"', pos);
    if (end == std::string::npos) {
        return std::string();
    }

    return debug.substr(pos, end - pos);
}

std::string
debian_package_for_element(const std::string& element)
{
    auto entry = find_package(element);

    return (entry != nullptr and entry->debian != nullptr) ? std::string(entry->debian) : std::string();
}

std::string
fedora_package_for_element(const std::string& element)
{
    auto entry = find_package(element);

    return (entry != nullptr and entry->fedora != nullptr) ? std::string(entry->fedora) : std::string();
}

} // namespace radiotray
