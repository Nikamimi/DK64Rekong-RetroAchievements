#include "guest_catalog.h"
#include "memory.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace dk64_ra {
namespace {
constexpr std::size_t kMaxCatalogBytes = 4U * 1024U * 1024U;
constexpr std::size_t kMaxAchievements = 4096;
thread_local GuestCatalog* current_catalog = nullptr;

bool valid_hash(const std::string& hash) {
    return hash.size() == 32 &&
        hash.find_first_not_of("0123456789abcdef") == std::string::npos;
}

bool parse_catalog(const rc_api_server_response_t* server,
                   rc_api_fetch_game_sets_response_t& parsed) {
    if (!server || server->http_status_code != 200 || !server->body ||
        !server->body_length || server->body_length > kMaxCatalogBytes) return false;
    const int result = rc_api_process_fetch_game_sets_server_response(&parsed, server);
    if (result != RC_OK || !parsed.response.succeeded || parsed.id != 10075 ||
        parsed.console_id != 2 || !parsed.sets || parsed.num_sets > 32) return false;
    std::size_t core = 0;
    for (std::uint32_t i = 0; i < parsed.num_sets; ++i) {
        const auto& set = parsed.sets[i];
        if (set.type != RC_ACHIEVEMENT_SET_TYPE_CORE) continue;
        if (!set.achievements || set.num_achievements > kMaxAchievements - core) return false;
        core += set.num_achievements;
    }
    return core > 0;
}
} // namespace

std::filesystem::path guest_catalog_path(const std::filesystem::path& directory,
                                         const std::string& rom_hash) {
    if (directory.empty() || !valid_hash(rom_hash)) return {};
    return directory / ("catalog-" + rom_hash + ".json");
}

bool save_guest_catalog(const std::filesystem::path& path,
                        const rc_api_server_response_t* response) {
    if (path.empty()) return false;
    rc_api_fetch_game_sets_response_t parsed{};
    const bool valid = parse_catalog(response, parsed);
    rc_api_destroy_fetch_game_sets_response(&parsed);
    if (!valid) return false;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    auto temporary = path;
    temporary += L".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file.write(response->body, static_cast<std::streamsize>(response->body_length));
        file.flush();
        if (!file) return false;
    }
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temporary, path, error);
    return !error;
#endif
}

GuestCatalog::GuestCatalog() { rc_runtime_init(&runtime_); }
GuestCatalog::~GuestCatalog() { rc_runtime_destroy(&runtime_); }

bool GuestCatalog::load(const std::filesystem::path& path, const LocalProgress& progress) {
    entries_.clear();
    rc_runtime_destroy(&runtime_);
    rc_runtime_init(&runtime_);
    if (path.empty()) return false;
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error ||
        std::filesystem::file_size(path, error) > kMaxCatalogBytes || error) return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    const std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad() || body.empty() || body.size() > kMaxCatalogBytes) return false;
    const rc_api_server_response_t server{body.c_str(), body.size(), 200};
    rc_api_fetch_game_sets_response_t parsed{};
    if (!parse_catalog(&server, parsed)) {
        rc_api_destroy_fetch_game_sets_response(&parsed);
        return false;
    }
    std::set<std::uint32_t> seen;
    for (std::uint32_t i = 0; i < parsed.num_sets; ++i) {
        const auto& set = parsed.sets[i];
        if (set.type != RC_ACHIEVEMENT_SET_TYPE_CORE) continue;
        for (std::uint32_t j = 0; j < set.num_achievements; ++j) {
            const auto& a = set.achievements[j];
            if (a.category != RC_ACHIEVEMENT_CATEGORY_CORE || a.id == 0 ||
                a.id >= 101000001U || !seen.insert(a.id).second) continue;
            const bool disabled = !a.definition || !*a.definition ||
                (!progress.contains(a.id) &&
                 rc_runtime_activate_achievement(&runtime_, a.id, a.definition, nullptr, 0) != RC_OK);
            entries_.push_back({a.id, a.points, a.title ? a.title : "Untitled",
                                a.description ? a.description : "",
                                a.badge_url ? a.badge_url : "",
                                a.badge_locked_url ? a.badge_locked_url : "", disabled});
        }
    }
    rc_api_destroy_fetch_game_sets_response(&parsed);
    return !entries_.empty();
}

std::uint32_t GuestCatalog::peek(std::uint32_t address, std::uint32_t bytes, void* userdata) {
    const auto* catalog = static_cast<const GuestCatalog*>(userdata);
    if (bytes < 1 || bytes > 4) return 0;
    std::uint8_t data[4]{};
    if (MemoryView(catalog->rdram_).read(address, data, bytes) != bytes) return 0;
    std::uint32_t value = 0;
    for (std::uint32_t i = 0; i < bytes; ++i) value |= std::uint32_t(data[i]) << (i * 8);
    return value;
}

void GuestCatalog::on_event(const rc_runtime_event_t* event) {
    if (current_catalog && event->type == RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED &&
        current_catalog->triggered_) current_catalog->triggered_->push_back(event->id);
}

void GuestCatalog::frame(const std::uint8_t* rdram, std::vector<std::uint32_t>& triggered) {
    if (!active() || !rdram) return;
    rdram_ = rdram;
    triggered_ = &triggered;
    current_catalog = this;
    rc_runtime_do_frame(&runtime_, on_event, peek, this, nullptr);
    current_catalog = nullptr;
    triggered_ = nullptr;
}

std::string GuestCatalog::measured(std::uint32_t id) const {
    char buffer[64]{};
    return rc_runtime_format_achievement_measured(&runtime_, id, buffer, sizeof(buffer)) > 0 ?
        buffer : "";
}

const GuestCatalog::Entry* GuestCatalog::find(std::uint32_t id) const {
    const auto found = std::find_if(entries_.begin(), entries_.end(),
                                    [id](const Entry& entry) { return entry.id == id; });
    return found == entries_.end() ? nullptr : &*found;
}

} // namespace dk64_ra
