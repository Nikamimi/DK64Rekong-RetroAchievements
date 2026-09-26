#include "local_progress.h"
#include "windows_credentials.h"

#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <string>

int main() {
    namespace fs = std::filesystem;
    const fs::path data = fs::temp_directory_path() / "dk64-ra-linux-data-test";
    setenv("XDG_DATA_HOME", data.c_str(), 1);
    if (dk64_ra::local_progress_directory() !=
        data / "Nikamimi" / "DK64RekongRetroAchievements") return 1;
    setenv("XDG_DATA_HOME", "relative/path", 1);
    if (!dk64_ra::local_progress_directory().empty()) return 2;
    unsetenv("XDG_DATA_HOME");
    setenv("HOME", data.c_str(), 1);
    if (dk64_ra::local_progress_directory() !=
        data / ".local" / "share" / "Nikamimi" / "DK64RekongRetroAchievements") return 3;

    std::string hash = "not a ROM hash";
    setenv("APP_FOLDER_PATH", data.c_str(), 1);
    if (dk64_ra::verify_stored_rom(hash) != dk64_ra::RomVerificationStatus::NotFound ||
        !hash.empty()) return 4;
    std::string username = "stale", token = "stale";
    if (dk64_ra::read_saved_login(username, token) || !username.empty() || !token.empty() ||
        dk64_ra::store_saved_login("user", "secret") || !dk64_ra::forget_saved_login()) return 5;
    unsetenv("DISPLAY");
    unsetenv("WAYLAND_DISPLAY");
    std::string password = "stale";
    bool remember_signin = true;
    if (dk64_ra::prompt_for_credentials(username, password, remember_signin) ||
        !username.empty() || !password.empty() || remember_signin) return 6;

    // Optional read-only check with an owned, normalized ROM in the game's app
    // folder; no ROM is copied or needed for regular CTest runs.
    if (const char* install = std::getenv("DK64_RA_OWNED_INSTALL")) {
        setenv("APP_FOLDER_PATH", install, 1);
        if (dk64_ra::verify_stored_rom(hash) != dk64_ra::RomVerificationStatus::Supported ||
            hash != "9ec41abf2519fc386cadd0731f6e868c") return 7;
    }
    std::puts("Linux paths, fail-closed ROM verification, and no-token fallback passed");
    return 0;
}
