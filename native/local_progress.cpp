#include "local_progress.h"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
#elif defined(__linux__)
#include <pwd.h>
#include <unistd.h>
#endif

namespace dk64_ra {
namespace {
constexpr std::size_t kMaxFileSize = 64U * 1024U;
constexpr std::size_t kMaxAchievements = 4096;
constexpr char kHeader[] = "DK64RA-LOCAL-1";

std::string hex(const std::string& input) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(input.size() * 2);
    for (unsigned char byte : input) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 15]);
    }
    return result;
}

bool parse_id(const std::string& line, std::uint32_t& value) {
    if (line.empty() || line.size() > 10) return false;
    const auto parsed = std::from_chars(line.data(), line.data() + line.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == line.data() + line.size() &&
           value > 0 && value < 101000001U;
}
} // namespace

std::filesystem::path local_progress_directory() {
#ifdef _WIN32
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &folder)) ||
        !folder) return {};
    auto path = std::filesystem::path(folder) / L"Nikamimi" / L"DK64RekongRetroAchievements";
    CoTaskMemFree(folder);
    return path;
#elif defined(__linux__)
    // Keep the catalog and local history per user, apart from the game's saves.
    const char* data_home = std::getenv("XDG_DATA_HOME");
    std::filesystem::path base;
    if (data_home && *data_home) base = data_home;
    else {
        const char* home = std::getenv("HOME");
        if (!home || !*home) {
            const auto* user = getpwuid(getuid());
            home = user ? user->pw_dir : nullptr;
        }
        if (!home || !*home) return {};
        base = std::filesystem::path(home) / ".local" / "share";
    }
    return base.is_absolute() ? base / "Nikamimi" / "DK64RekongRetroAchievements" :
                                std::filesystem::path{};
#else
    return {};
#endif
}

bool LocalProgress::open(const std::filesystem::path& directory,
                         const std::string& username, const std::string& rom_hash) {
    active_ = false;
    unlocked_.clear();
    path_.clear();
    account_key_.clear();
    rom_hash_.clear();
    if (directory.empty() || username.empty() || username.size() > 64 || rom_hash.size() != 32 ||
        rom_hash.find_first_not_of("0123456789abcdef") != std::string::npos) return false;
    account_key_ = hex(username);
    rom_hash_ = rom_hash;
    path_ = directory / ("progress-" + account_key_ + "-" + rom_hash_ + ".txt");
    std::error_code error;
    const bool exists = std::filesystem::exists(path_, error);
    if (error) return false;
    if (exists) {
        const auto size = std::filesystem::file_size(path_, error);
        if (error || size > kMaxFileSize) return false;
        std::ifstream stream(path_, std::ios::binary);
        if (!stream) return false;
        std::string header, account, hash, line;
        if (!std::getline(stream, header) || header != kHeader ||
            !std::getline(stream, account) || account != account_key_ ||
            !std::getline(stream, hash) || hash != rom_hash_) return false;
        while (std::getline(stream, line)) {
            std::uint32_t id = 0;
            if (!parse_id(line, id) || !unlocked_.insert(id).second ||
                unlocked_.size() > kMaxAchievements) {
                unlocked_.clear();
                return false;
            }
        }
        if (!stream.eof()) {
            unlocked_.clear();
            return false;
        }
    }
    active_ = true;
    return true;
}

bool LocalProgress::save() const {
    if (!active_ || path_.empty()) return false;
    std::error_code error;
    std::filesystem::create_directories(path_.parent_path(), error);
    if (error) return false;
    auto temporary = path_;
    temporary += L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return false;
        stream << kHeader << '\n' << account_key_ << '\n' << rom_hash_ << '\n';
        for (auto id : unlocked_) stream << id << '\n';
        stream.flush();
        if (!stream) return false;
    }
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(), path_.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temporary, path_, error);
    return !error;
#endif
}

bool LocalProgress::mark(std::uint32_t id) {
    if (!active_ || id == 0 || id >= 101000001U) return false;
    if (contains(id)) return true;
    if (unlocked_.size() >= kMaxAchievements) return false;
    unlocked_.insert(id);
    if (save()) return true;
    unlocked_.erase(id);
    return false;
}

bool LocalProgress::reset() {
    if (!active_) return false;
    auto old = unlocked_;
    unlocked_.clear();
    if (save()) return true;
    unlocked_.swap(old);
    return false;
}

} // namespace dk64_ra
