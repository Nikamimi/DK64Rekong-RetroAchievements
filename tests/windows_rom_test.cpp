#include "windows_credentials.h"

#include <windows.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    using dk64_ra::RomVerificationStatus;

    const auto root = fs::temp_directory_path() /
        ("dk64-ra-rom-test-" + std::to_string(GetCurrentProcessId()) + "-" +
         std::to_string(GetTickCount64()));
    const auto game = root / "game";
    const auto app_data = root / "app-data";
    fs::create_directories(game);

    const auto normal_path = app_data / "DK64Recompiled" / "DK64.z64";
    if (dk64_ra::windows_stored_rom_path(game, app_data) != normal_path) return 1;

    std::string hash = "stale";
    if (dk64_ra::verify_rom_at_path(normal_path, hash) != RomVerificationStatus::NotFound ||
        !hash.empty()) return 2;

    const auto marker = game / "portable.txt";
    std::ofstream(marker).close();
    const auto portable_path = game / "DK64.z64";
    if (dk64_ra::windows_stored_rom_path(game, app_data) != portable_path ||
        dk64_ra::windows_stored_rom_path(game, {}) != portable_path) return 3;

    {
        std::ofstream rom(portable_path, std::ios::binary);
        constexpr std::array<unsigned char, 4> invalid_rom{0, 1, 2, 3};
        rom.write(reinterpret_cast<const char*>(invalid_rom.data()), invalid_rom.size());
    }
    hash = "stale";
    if (dk64_ra::verify_rom_at_path(portable_path, hash) != RomVerificationStatus::Unreadable ||
        !hash.empty()) return 4;

    {
        std::ofstream rom(portable_path, std::ios::binary | std::ios::trunc);
        constexpr std::array<unsigned char, 4> other_n64_rom{0x80, 0x37, 0x12, 0x40};
        rom.write(reinterpret_cast<const char*>(other_n64_rom.data()), other_n64_rom.size());
    }
    if (dk64_ra::verify_rom_at_path(portable_path, hash) != RomVerificationStatus::Mismatch ||
        !hash.empty()) return 5;

    const auto unicode_folder = root / L"\u732b";
    fs::create_directory(unicode_folder);
    const auto unicode_rom = unicode_folder / L"DK64.z64";
    fs::copy_file(portable_path, unicode_rom);
    if (dk64_ra::verify_rom_at_path(unicode_rom, hash) != RomVerificationStatus::Mismatch)
        return 6;

    if (const char* owned = std::getenv("DK64_RA_OWNED_ROM")) {
        if (dk64_ra::verify_rom_at_path(owned, hash) != RomVerificationStatus::Supported ||
            hash != "9ec41abf2519fc386cadd0731f6e868c") return 7;
    }

    fs::remove(unicode_rom);
    fs::remove(unicode_folder);
    fs::remove(portable_path);
    fs::remove(marker);
    fs::remove(game);
    fs::remove(root);
    std::cout << "Windows portable/nonportable paths and ROM error classification passed\n";
    return 0;
}
