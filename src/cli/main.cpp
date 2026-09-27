#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "core/config.hpp"
#include "core/network.hpp"
#include "core/validate.hpp"
#include "core/version.hpp"

namespace fs = std::filesystem;

namespace {

constexpr int kExitOk = 0;
constexpr int kExitDataError = 1;
constexpr int kExitUsage = 2;

void print_usage(std::ostream& out) {
    out << "usage: ppbus <command> [options]\n"
           "\n"
           "commands:\n"
           "  validate     check the network data for likely mistakes\n"
           "  --version    print version\n"
           "\n"
           "options:\n"
           "  --data DIR   network directory with stops.csv, lines.csv and optional\n"
           "               network.ini (default: data/sample)\n";
}

struct Options {
    fs::path data_dir = "data/sample";
};

bool parse_options(const std::vector<std::string_view>& args, Options& options) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--data" && i + 1 < args.size()) {
            options.data_dir = fs::u8path(args[++i]);
        } else {
            std::cerr << "ppbus: unexpected argument '" << args[i] << "'\n";
            return false;
        }
    }
    return true;
}

ppbus::Config load_config(const fs::path& dir) {
    const fs::path ini = dir / "network.ini";
    return fs::exists(ini) ? ppbus::load_config_file(ini) : ppbus::Config{};
}

int run_validate(const Options& options) {
    const ppbus::Config config = load_config(options.data_dir);
    const ppbus::Network net = ppbus::load_network(options.data_dir, config);

    std::size_t warnings = 0;
    std::size_t notes = 0;
    for (const auto& finding : ppbus::validate_network(net, config)) {
        std::cout << ppbus::format_finding(finding) << '\n';
        ++(finding.severity == ppbus::Severity::Warning ? warnings : notes);
    }
    std::cout << net.stops.size() << " stops, " << net.stations.size() << " stations, " << net.patterns.size()
              << " line directions: " << warnings << " warnings, " << notes << " notes\n";
    return kExitOk;
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    if (args.size() == 1 && args[0] == "--version") {
        std::cout << "ppbus " << ppbus::version() << '\n';
        return kExitOk;
    }
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        print_usage(args.empty() ? std::cerr : std::cout);
        return args.empty() ? kExitUsage : kExitOk;
    }

    Options options;
    if (!parse_options({args.begin() + 1, args.end()}, options)) return kExitUsage;

    try {
        if (args[0] == "validate") return run_validate(options);
    } catch (const ppbus::DataError& e) {
        std::cerr << e.what() << '\n';
        return kExitDataError;
    } catch (const ppbus::NetworkError& e) {
        std::cerr << e.what() << '\n' << e.messages().size() << " errors\n";
        return kExitDataError;
    }

    std::cerr << "ppbus: unknown command '" << args[0] << "'\n";
    print_usage(std::cerr);
    return kExitUsage;
}
