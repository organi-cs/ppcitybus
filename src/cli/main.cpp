#include <iostream>
#include <string_view>

#include "core/version.hpp"

namespace {

void print_usage(std::ostream& out) {
    out << "usage: ppbus <command> [options]\n"
           "\n"
           "commands:\n"
           "  --version    print version\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "ppbus " << ppbus::version() << '\n';
        return 0;
    }
    print_usage(std::cerr);
    return 2;
}
