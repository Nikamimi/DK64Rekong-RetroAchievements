#include "local_progress.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
constexpr char kHash[] = "9ec41abf2519fc386cadd0731f6e868c";

bool check(bool value, const char* message) {
    if (!value) std::fprintf(stderr, "%s\n", message);
    return value;
}
} // namespace

int main() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root = fs::temp_directory_path() / ("dk64-ra-progress-test-" + suffix);
    dk64_ra::LocalProgress first, reload, another, other_rom;
    if (!check(first.open(root, "Player", kHash), "New account did not open") ||
        !check(first.mark(60497), "Local trigger did not save") ||
        !check(first.contains(60497) && first.count() == 1, "Local trigger missing") ||
        !check(!first.mark(101000001U), "Synthetic server warning was stored") ||
        !check(reload.open(root, "Player", kHash) && reload.contains(60497),
               "Progress did not persist over restart") ||
        !check(another.open(root, "Another", kHash) && !another.contains(60497),
               "Progress leaked across accounts") ||
        !check(other_rom.open(root, "Player", "00000000000000000000000000000000") &&
               !other_rom.contains(60497), "Progress leaked across ROMs") ||
        !check(reload.reset() && !reload.contains(60497), "Reset failed")) return 1;

    dk64_ra::LocalProgress after_reset;
    if (!check(after_reset.open(root, "Player", kHash) && !after_reset.contains(60497),
               "Reset did not persist")) return 1;
    {
        std::ofstream broken(after_reset.path(), std::ios::binary | std::ios::trunc);
        broken << "broken file\n";
    }
    dk64_ra::LocalProgress corrupt;
    if (!check(!corrupt.open(root, "Player", kHash) && !corrupt.mark(60497) && !corrupt.reset(),
               "Corrupt file was overwritten")) return 1;

    std::error_code error;
    fs::remove(first.path(), error);
    fs::remove(another.path(), error);
    fs::remove(other_rom.path(), error);
    fs::remove(root, error);
    std::puts("Local progress account/ROM isolation, persistence, reset, and corruption passed");
    return 0;
}
