#pragma once

#include <filesystem>
#include <string>

namespace dk64_ra {

enum class RomVerificationStatus {
    Supported,
    NotFound,
    Unreadable,
    Mismatch,
    LocationUnavailable,
};

// Platform-specific ROM verification and sign-in. Only Windows optionally
// persists a token in Credential Manager; passwords and ROMs are never stored here.
RomVerificationStatus verify_stored_rom(std::string& hash);
// Verifies a selected file without using it as the game's active ROM. The client
// must call verify_stored_rom, which resolves the game's own storage directory.
RomVerificationStatus verify_rom_at_path(const std::filesystem::path& path, std::string& hash);
#ifdef _WIN32
// Purely selects the game's portable or per-user storage path; testable without
// changing any user's game directory or save files.
std::filesystem::path windows_stored_rom_path(
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& local_app_data_directory);
#endif
// The sign-in window returns the user's token-saving choice. Passwords are
// never persisted by the OS dialog, even when its Save checkbox is checked.
bool prompt_for_credentials(std::string& username, std::string& password, bool& remember_signin);
bool read_saved_login(std::string& username, std::string& token);
bool store_saved_login(const char* username, const char* token);
bool forget_saved_login();
void clear_secret(std::string& secret);

} // namespace dk64_ra
