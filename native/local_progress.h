#pragma once

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>

namespace dk64_ra {

// Local trigger history is independent of the RA account's server unlocks.
// It contains achievement IDs only, never tokens, RAM, or trigger definitions.
class LocalProgress {
public:
    bool open(const std::filesystem::path& directory, const std::string& username,
              const std::string& rom_hash);
    bool contains(std::uint32_t id) const { return unlocked_.count(id) != 0; }
    std::size_t count() const { return unlocked_.size(); }
    bool mark(std::uint32_t id);
    bool reset();
    bool active() const { return active_; }
    const std::filesystem::path& path() const { return path_; }

private:
    bool save() const;
    std::filesystem::path path_;
    std::string account_key_;
    std::string rom_hash_;
    std::set<std::uint32_t> unlocked_;
    bool active_ = false;
};

// Per-user application data directory. Empty when no safe absolute path exists.
std::filesystem::path local_progress_directory();

} // namespace dk64_ra
