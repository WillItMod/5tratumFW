#include "display_ota_compatibility.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>

int main(int argc, char **argv) {
    if (argc == 2 && std::strcmp(argv[1], "--deadline") == 0) {
        constexpr int64_t started = 1234567;
        std::cout << FiveTratumDisplayOta::prefixReadDeadlineExpired(started, started) << ' '
                  << FiveTratumDisplayOta::prefixReadDeadlineExpired(started, started + 14999999) << ' '
                  << FiveTratumDisplayOta::prefixReadDeadlineExpired(started, started + 15000000) << ' '
                  << FiveTratumDisplayOta::prefixReadDeadlineExpired(started, started + 15000001) << ' '
                  << FiveTratumDisplayOta::prefixReadDeadlineExpired(started, started - 1) << ' '
                  << FiveTratumDisplayOta::prefixReadDeadlineExpired(-1, 0) << ' '
                  << FiveTratumDisplayOta::prefixReadDeadlineExpired(0, std::numeric_limits<int64_t>::max()) << '\n';
        return 0;
    }
    if (argc != 4) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 3;
    const std::vector<uint8_t> image((std::istreambuf_iterator<char>(input)),
                                    std::istreambuf_iterator<char>());
    std::cout << FiveTratumDisplayOta::requiresProfileSupport(argv[2], argv[3]) << ' '
              << FiveTratumDisplayOta::compatibleImagePrefix(image.data(), image.size()) << ' '
              << FiveTratumDisplayOta::requiresProfileSupport(nullptr, nullptr) << ' '
              << FiveTratumDisplayOta::compatibleImagePrefix(nullptr, image.size()) << '\n';
}
