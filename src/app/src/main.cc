#include <cstdio>
#include <filesystem>
#include <optional>
#include <string_view>

#include "app.h"
#include "config.h"

namespace {

constexpr const char *kUsage =
  "Map finger position on a Mac trackpad to an absolute cursor position on screen\n"
  "\n"
  "Usage: macOSu-pad [OPTIONS]\n"
  "\n"
  "Options:\n"
  "  -c, --config <PATH>  Settings file [default: ~/Library/Application "
  "Support/macOSu-pad/config.toml]\n"
  "  -h, --help           Print help\n"
  "  -V, --version        Print version\n";

}  // namespace

int main(int argc, char **argv) {
    std::optional<std::filesystem::path> config;
    for (int i = 1; i < argc; i++) {
        const std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::fputs(kUsage, stdout);
            return 0;
        }
        if (arg == "-V" || arg == "--version") {
            std::printf("macOSu-pad %s\n", MACOSU_PAD_VERSION);
            return 0;
        }
        if (arg == "-c" || arg == "--config") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: a value is required for '--config <PATH>'\n\n%s", kUsage);
                return 2;
            }
            config = argv[++i];
        } else if (arg.starts_with("--config=")) {
            config = std::string(arg.substr(9));
        } else if (arg.starts_with("-psn_")) {
            // Process serial number passed by older Finder launches.
        } else {
            std::fprintf(stderr, "error: unexpected argument '%s'\n\n%s", argv[i], kUsage);
            return 2;
        }
    }
    macosu_pad::App app(config.value_or(macosu_pad::defaultPath()));
    return app.run();
}
