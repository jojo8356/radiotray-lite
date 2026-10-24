#include "options.hpp"

namespace radiotray
{

// clang-format off
static const struct option longopts[] = {
    { "resume", no_argument, nullptr, 'r' },
    { "bookmarks", required_argument, nullptr, 'b' },
    { "help", no_argument, nullptr, 'h' },
    { nullptr, 0, nullptr, 0 }
};
// clang-format on

bool
CmdLineOptions::parse(int argc, char** argv)
{
    int opt, optidx;

    while ((opt = getopt_long(argc, argv, "b:rh", longopts, &optidx)) != -1) {
        switch (opt) {
        case 'r':
            resume = true;
            break;
        case 'b':
            bookmarks = optarg;
            break;
        case 'h':
            help = true;
            break;
        default:
            return false;
        }
    }

    return true;
}

void
CmdLineOptions::show_help()
{
    std::cout << "Online radio streaming player" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  radiotray-lite [OPTIONS...]" << std::endl << std::endl;
    std::cout << "  -h, --help    show this help and exit" << std::endl;
    std::cout << "  -r, --resume  resume last played station on startup" << std::endl;
    std::cout << "  -b, --bookmarks FILE" << std::endl;
    std::cout << "                use FILE as bookmarks.xml (config.xml is looked up" << std::endl;
    std::cout << "                in the same directory)" << std::endl;
    std::cout << std::endl;
}

} // namespace
