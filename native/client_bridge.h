#pragma once

#include "memory.h"
#include "http_transport.h"
#include "badge_cache.h"
#include "local_progress.h"
#include "guest_catalog.h"

#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include "rc_client.h"
}

namespace dk64_ra {

// The native bridge supports both a spectator/local tracker and a limited
// online mode. Neither mode submits Hardcore or leaderboard results.
class ClientBridge {
public:
    enum class Mode { DiagnosticsOffline, LocalTracking, OnlineSoftcore };
    explicit ClientBridge(std::filesystem::path storage_directory = {});
    ~ClientBridge();
    ClientBridge(const ClientBridge&) = delete;
    ClientBridge& operator=(const ClientBridge&) = delete;

    bool initialize(std::uint8_t* rdram, Mode mode = Mode::LocalTracking,
                    bool sound_enabled = true);
    void on_frame(std::uint8_t* rdram);
    bool ready() const { return client_ != nullptr; }
    std::uint64_t frames_seen() const { return frames_seen_; }
    bool safe_mode() const;
    Mode mode() const { return mode_; }
    bool reset_local_progress();
    bool signed_in() const { return !account_username_.empty(); }
    bool sign_in();
    bool log_out();
    bool ui_copy_account(std::uint32_t field, std::uint8_t* rdram,
                         std::uint32_t guest_address, std::uint32_t capacity) const;

    enum class Filter : std::uint32_t { All = 0, Locked = 1, Unlocked = 2 };
    std::uint32_t ui_revision() const { return ui_revision_ + badges_.revision(); }
    std::uint32_t ui_count(Filter filter) const;
    bool ui_copy_entry(Filter filter, std::uint32_t index, std::uint32_t field,
                       std::uint8_t* rdram, std::uint32_t guest_address,
                       std::uint32_t capacity) const;
    std::uint32_t ui_copy_badge(Filter filter, std::uint32_t index,
                                std::uint8_t* rdram, std::uint32_t guest_address,
                                std::uint32_t capacity);
    bool ui_take_toast(std::uint8_t* rdram, std::uint32_t guest_address,
                       std::uint32_t capacity);
    bool ui_copy_toast_description(std::uint8_t* rdram, std::uint32_t guest_address,
                                   std::uint32_t capacity) const;
    std::uint32_t ui_copy_toast_badge(std::uint8_t* rdram, std::uint32_t guest_address,
                                      std::uint32_t capacity);
    bool ui_copy_summary(std::uint8_t* rdram, std::uint32_t guest_address,
                         std::uint32_t capacity) const;

private:
    static std::uint32_t read_memory(std::uint32_t address, std::uint8_t* buffer,
                                     std::uint32_t length, rc_client_t* client);
    static void server_call(const rc_api_request_t* request,
                            rc_client_server_callback_t callback,
                            void* callback_data, rc_client_t* client);
    struct AwardCallbackData {
        ClientBridge* bridge;
        rc_client_server_callback_t callback;
        void* callback_data;
        std::uint32_t id;
    };
    static void award_callback(const rc_api_server_response_t* response, void* callback_data);
    struct CatalogCallbackData {
        ClientBridge* bridge;
        rc_client_server_callback_t callback;
        void* callback_data;
    };
    static void catalog_callback(const rc_api_server_response_t* response, void* callback_data);
    static void login_callback(int result, const char* error_message,
                               rc_client_t* client, void* userdata);
    static void load_callback(int result, const char* error_message,
                              rc_client_t* client, void* userdata);
    static void event_handler(const rc_client_event_t* event, rc_client_t* client);
    void start_tracking();
    void start_guest_tracking(const char* reason);
    void prompt_password();
    std::filesystem::path storage_directory() const;
    void rebuild_ui_catalog(rc_client_t* client);
    void refresh_measured_progress();
    void queue_toast(const rc_client_achievement_t* achievement);
    void queue_guest_toast(const GuestCatalog::Entry& achievement);

    struct UiAchievement {
        std::string title;
        std::string description;
        std::string badge_url;
        std::uint32_t id;
        std::uint32_t points;
        bool unlocked;
        bool account_unlocked;
        bool disabled;
        std::string measured;
    };
    const UiAchievement* ui_entry(Filter filter, std::uint32_t index) const;

    rc_client_t* client_ = nullptr;
    const std::uint8_t* rdram_ = nullptr;
    std::uint64_t frames_seen_ = 0;
    HttpTransport transport_;
    std::string rom_hash_;
    std::filesystem::path storage_directory_;
    std::uint64_t read_calls_ = 0;
    std::uint64_t nonzero_read_calls_ = 0;
    std::uint64_t short_read_calls_ = 0;
    bool local_tracking_requested_ = false;
    bool local_tracking_attempted_ = false;
    Mode mode_ = Mode::DiagnosticsOffline;
    bool online_awards_ready_ = false;
    bool remember_signin_ = false;
    bool sound_enabled_ = true;
    bool login_with_saved_token_ = false;
    bool catalog_dirty_ = false;
    std::vector<UiAchievement> ui_achievements_;
    std::set<std::uint32_t> account_unlocks_;
    std::set<std::uint32_t> pending_online_;
    LocalProgress local_progress_;
    GuestCatalog guest_catalog_;
    BadgeCache badges_;
    struct Toast {
        std::string title;
        std::string description;
        std::string badge_url;
    };
    std::deque<Toast> toasts_;
    std::string active_toast_badge_url_;
    std::string active_toast_description_;
    std::string account_username_;
    std::string account_notice_;
    std::string ui_status_;
    std::uint32_t ui_revision_ = 0;
};

} // namespace dk64_ra
