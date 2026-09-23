#pragma once

#include "local_progress.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

extern "C" {
#include "rc_api_runtime.h"
#include "rc_runtime.h"
}

namespace dk64_ra {

// The authenticated achievement-set response is cached only on the player's
// machine. No definitions, tokens, or ROM bytes are shipped with the mod.
std::filesystem::path guest_catalog_path(const std::filesystem::path& directory,
                                         const std::string& rom_hash);
bool save_guest_catalog(const std::filesystem::path& path,
                        const rc_api_server_response_t* response);

class GuestCatalog {
public:
    struct Entry {
        std::uint32_t id;
        std::uint32_t points;
        std::string title;
        std::string description;
        std::string badge_url;
        std::string locked_badge_url;
        bool disabled;
    };

    GuestCatalog();
    ~GuestCatalog();
    GuestCatalog(const GuestCatalog&) = delete;
    GuestCatalog& operator=(const GuestCatalog&) = delete;

    bool load(const std::filesystem::path& path, const LocalProgress& progress);
    void frame(const std::uint8_t* rdram, std::vector<std::uint32_t>& triggered);
    std::string measured(std::uint32_t id) const;
    const Entry* find(std::uint32_t id) const;
    const std::vector<Entry>& entries() const { return entries_; }
    bool active() const { return !entries_.empty(); }

private:
    static std::uint32_t peek(std::uint32_t address, std::uint32_t bytes, void* userdata);
    static void on_event(const rc_runtime_event_t* event);
    rc_runtime_t runtime_{};
    const std::uint8_t* rdram_ = nullptr;
    std::vector<Entry> entries_;
    std::vector<std::uint32_t>* triggered_ = nullptr;
};

} // namespace dk64_ra
