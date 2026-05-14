#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "net/lru_server.h"

namespace {
constexpr std::uint16_t kDefaultPort = 6379;
constexpr const char* kDefaultAddr = "0.0.0.0";
}

int main(int argc, char** argv) {
    try {
        const std::uint16_t port =
            (argc > 1) ? static_cast<std::uint16_t>(std::stoi(argv[1])) : kDefaultPort;
        std::string addr = (argc > 2) ? argv[2] : kDefaultAddr;

        std::cout << "lru-cache-sv listening on " << addr << ":" << port << '\n';

        net::lru_server<std::string, std::string> server(port, std::move(addr));
        server.listen();
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
