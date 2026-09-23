#include "client_bridge.h"
#include "diagnostic_log.h"
#include "guest_text.h"
#include "memory_diagnostics.h"
#include "online_awards.h"
#include "rap_diagnostics.h"
#include "toast_sound.h"
#include "windows_credentials.h"

#include <cstddef>
#include <cstring>
#include <utility>

namespace {
void shorten_for_row(std::string& text, std::size_t limit) {
    if (text.size() <= limit) return;
    std::size_t end = limit - 3;
    while (end && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) --end;
    text.resize(end);
    text += "...";
}
}

namespace dk64_ra {

ClientBridge::~ClientBridge() {
    if (client_) {
        rc_client_destroy(client_);
    }
}

bool ClientBridge::initialize(std::uint8_t* rdram, Mode mode, bool remember_signin,
                              bool sound_enabled) {
    if (!rdram) {
        return false;
    }
    if (client_) {
        rc_client_destroy(client_);
        client_ = nullptr;
    }
    rdram_ = rdram;
    frames_seen_ = 0;
    read_calls_ = 0;
    nonzero_read_calls_ = 0;
    short_read_calls_ = 0;
    mode_ = mode;
    local_tracking_requested_ = mode != Mode::DiagnosticsOffline;
    local_tracking_attempted_ = false;
    online_awards_ready_ = false;
#ifdef _WIN32
    remember_signin_ = remember_signin;
#else
    remember_signin_ = false; // No Linux token store; ask on every Online launch.
    (void)remember_signin;
#endif
    sound_enabled_ = sound_enabled;
    login_with_saved_token_ = false;
    catalog_dirty_ = false;
    rom_hash_.clear();
    ui_achievements_.clear();
    account_unlocks_.clear();
    pending_online_.clear();
    local_progress_ = LocalProgress{};
    guest_catalog_.load({}, local_progress_);
    toasts_.clear();
    active_toast_badge_url_.clear();
    active_toast_description_.clear();
    account_username_.clear();
    ui_status_ = mode == Mode::OnlineSoftcore ? "Online softcore: waiting for sign-in" :
                 mode == Mode::LocalTracking ? "Local Tracking only: checking cached set" :
                 "Diagnostics: offline";
    ++ui_revision_;
    client_ = rc_client_create(read_memory, server_call);
    if (!client_) {
        return false;
    }
    rc_client_set_userdata(client_, this);
    rc_client_set_hardcore_enabled(client_, 0);
    rc_client_set_spectator_mode_enabled(client_, mode != Mode::OnlineSoftcore);
    rc_client_set_event_handler(client_, event_handler);
    return true;
}

void ClientBridge::on_frame(std::uint8_t* rdram) {
    if (!client_ || !rdram) {
        return;
    }
    rdram_ = rdram;
    if (local_tracking_requested_ && !local_tracking_attempted_) {
        local_tracking_attempted_ = true;
        start_tracking();
    }
    if (guest_catalog_.active()) {
        std::vector<std::uint32_t> triggered;
        guest_catalog_.frame(rdram, triggered);
        for (const auto id : triggered) {
            const auto* achievement = guest_catalog_.find(id);
            if (achievement && !local_progress_.contains(id)) {
                const bool saved = local_progress_.mark(id);
                diagnostic_log("[DK64 RA] Guest local trigger %u (%s).", id,
                               saved ? "history saved" : "history SAVE FAILED");
                if (saved) queue_guest_toast(*achievement);
                catalog_dirty_ = true;
            }
        }
    } else {
        rc_client_do_frame(client_);
    }
    if (catalog_dirty_) {
        catalog_dirty_ = false;
        rebuild_ui_catalog(client_);
    }
    ++frames_seen_;
    memory_diagnostics_frame(rdram, frames_seen_);
    if (local_tracking_requested_) {
        rap_diagnostics_frame(client_, frames_seen_);
    }
    if (local_progress_.active() && frames_seen_ % 15 == 0) refresh_measured_progress();
    if (local_tracking_requested_ && frames_seen_ % 900 == 0) {
        // Aggregate counts only: never put gameplay memory or account data in a log.
        if (guest_catalog_.active()) {
            diagnostic_log("[DK64 RA] Guest diagnostic: %llu frames, %u cached core achievements.",
                           static_cast<unsigned long long>(frames_seen_),
                           static_cast<unsigned>(guest_catalog_.entries().size()));
        } else {
            diagnostic_log("[DK64 RA] Diagnostic: %llu frames, %llu RAM reads, %llu nonzero, %llu short.",
                           static_cast<unsigned long long>(frames_seen_),
                           static_cast<unsigned long long>(read_calls_),
                           static_cast<unsigned long long>(nonzero_read_calls_),
                           static_cast<unsigned long long>(short_read_calls_));
        }
    }
}

bool ClientBridge::safe_mode() const {
    return client_ && !rc_client_get_hardcore_enabled(client_) &&
           (guest_catalog_.active() || rc_client_get_spectator_mode_enabled(client_));
}

bool ClientBridge::reset_local_progress() {
    if (!local_progress_.reset()) return false;
    if (guest_catalog_.active() || (client_ && rc_client_is_game_loaded(client_)))
        rebuild_ui_catalog(client_);
    return true;
}

std::uint32_t ClientBridge::ui_count(Filter filter) const {
    std::uint32_t count = 0;
    for (const auto& entry : ui_achievements_) {
        if (filter == Filter::All ||
            (filter == Filter::Locked && !entry.unlocked) ||
            (filter == Filter::Unlocked && entry.unlocked)) {
            ++count;
        }
    }
    return count;
}

const ClientBridge::UiAchievement* ClientBridge::ui_entry(Filter filter, std::uint32_t index) const {
    if (filter != Filter::All && filter != Filter::Locked && filter != Filter::Unlocked) {
        return nullptr;
    }
    for (const auto& entry : ui_achievements_) {
        if (filter != Filter::All && (entry.unlocked != (filter == Filter::Unlocked))) {
            continue;
        }
        if (index-- == 0) {
            return &entry;
        }
    }
    return nullptr;
}

bool ClientBridge::ui_copy_entry(Filter filter, std::uint32_t index, std::uint32_t field,
                                 std::uint8_t* rdram, std::uint32_t address,
                                 std::uint32_t capacity) const {
    const auto* entry = ui_entry(filter, index);
    if (!entry) {
        return false;
    }
    std::string line;
    switch (field) {
    case 0: line = entry->title; shorten_for_row(line, 76); break;
    case 1: line = entry->description; shorten_for_row(line, 105); break;
    case 2:
        line = entry->disabled ? "Unavailable" :
               (entry->account_unlocked ? "Unlocked on RA" :
                (pending_online_.count(entry->id) ? "Online trigger: RA not confirmed" :
                 (entry->unlocked ? "Tracked locally (no RA award)" : "Locked")));
        if (!entry->measured.empty() && !entry->unlocked) line += " | " + entry->measured;
        line += " | " + std::to_string(entry->points) + " pts";
        break;
    case 3: line = entry->unlocked ? "1" : "0"; break;
    default: return false;
    }
    return write_guest_text(rdram, address, capacity, line, 0x05000000U);
}

std::uint32_t ClientBridge::ui_copy_badge(Filter filter, std::uint32_t index,
                                           std::uint8_t* rdram, std::uint32_t address,
                                           std::uint32_t capacity) {
    const auto* entry = ui_entry(filter, index);
    return entry ? badges_.copy_or_queue(entry->badge_url, rdram, address, capacity) : 0;
}

bool ClientBridge::ui_copy_summary(std::uint8_t* rdram, std::uint32_t address,
                                   std::uint32_t capacity) const {
    std::string line = account_username_.empty() ? "RA: not signed in | " + ui_status_ :
        "RA: " + account_username_ + " | " + ui_status_;
    if (!ui_achievements_.empty()) {
        std::uint32_t account_count = 0;
        for (const auto& entry : ui_achievements_) account_count += entry.account_unlocked;
        line += " | RA " + std::to_string(account_count) +
                " | tracked total " + std::to_string(ui_count(Filter::Unlocked)) +
                "/" + std::to_string(ui_count(Filter::All));
    }
    return write_guest_text(rdram, address, capacity, line, 0x05000000U);
}

bool ClientBridge::ui_take_toast(std::uint8_t* rdram, std::uint32_t address,
                                 std::uint32_t capacity) {
    if (toasts_.empty() ||
        !write_guest_text(rdram, address, capacity, toasts_.front().title, 0x05000000U)) {
        return false;
    }
    active_toast_badge_url_ = std::move(toasts_.front().badge_url);
    active_toast_description_ = std::move(toasts_.front().description);
    toasts_.pop_front();
    if (sound_enabled_) diagnostic_log("[DK64 RA] Achievement jingle %s.",
                                       play_toast_sound() ? "started" : "unavailable");
    return true;
}

bool ClientBridge::ui_copy_toast_description(std::uint8_t* rdram, std::uint32_t address,
                                              std::uint32_t capacity) const {
    return write_guest_text(rdram, address, capacity, active_toast_description_, 0x05000000U);
}

std::uint32_t ClientBridge::ui_copy_toast_badge(std::uint8_t* rdram, std::uint32_t address,
                                                 std::uint32_t capacity) {
    return badges_.copy_or_queue(active_toast_badge_url_, rdram, address, capacity);
}

void ClientBridge::queue_toast(const rc_client_achievement_t* achievement) {
    Toast toast{achievement && achievement->title ? achievement->title : "Achievement",
                achievement && achievement->description ? achievement->description : "", {}};
    char badge_url[256]{};
    if (achievement && rc_client_achievement_get_image_url(achievement,
            RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, badge_url, sizeof(badge_url)) == RC_OK) {
        toast.badge_url = badge_url;
    }
    // Limit a burst of simultaneous achievements without unbounded memory use.
    if (toasts_.size() == 16) toasts_.pop_front();
    toasts_.push_back(std::move(toast));
}

void ClientBridge::queue_guest_toast(const GuestCatalog::Entry& achievement) {
    if (toasts_.size() == 16) toasts_.pop_front();
    toasts_.push_back({achievement.title, achievement.description, achievement.badge_url});
}

void ClientBridge::rebuild_ui_catalog(rc_client_t* client) {
    ui_achievements_.clear();
    if (guest_catalog_.active()) {
        for (const auto& a : guest_catalog_.entries()) {
            const bool unlocked = local_progress_.contains(a.id);
            ui_achievements_.push_back({a.title, a.description,
                                        unlocked ? a.badge_url : a.locked_badge_url,
                                        a.id, a.points, unlocked, false, a.disabled,
                                        guest_catalog_.measured(a.id)});
        }
        ui_status_ = local_progress_.active() ?
            "Local Tracking only: cached set, no RA awards" :
            "Local Tracking only: progress storage unavailable";
        ++ui_revision_;
        return;
    }
    auto* list = rc_client_create_achievement_list(client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,
                                                    RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (list) {
        for (std::uint32_t i = 0; i < list->num_buckets; ++i) {
            const auto& bucket = list->buckets[i];
            for (std::uint32_t j = 0; j < bucket.num_achievements; ++j) {
                const auto* a = bucket.achievements[j];
                if (!a || a->id >= 101000001U) { // server-injected client warning
                    continue;
                }
                char badge_url[256]{};
                // Spectator triggers still set a->unlocked locally. Only the
                // load-time snapshot is authoritative for server account state.
                const bool account_unlocked = account_unlocks_.count(a->id) != 0;
                const bool unlocked = account_unlocked || local_progress_.contains(a->id) ||
                                      pending_online_.count(a->id) != 0;
                if (rc_client_achievement_get_image_url(a,
                        unlocked ? RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED : RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE,
                        badge_url, sizeof(badge_url)) != RC_OK) {
                    badge_url[0] = 0;
                }
                ui_achievements_.push_back({a->title ? a->title : "Untitled",
                                            a->description ? a->description : "",
                                            badge_url,
                                            a->id, a->points,
                                            unlocked, account_unlocked,
                                            a->state == RC_CLIENT_ACHIEVEMENT_STATE_DISABLED,
                                            a->measured_progress});
            }
        }
        rc_client_destroy_achievement_list(list);
    }
    ui_status_ = mode_ == Mode::OnlineSoftcore ?
        (!online_awards_ready_ ? "Online unavailable: safety gate blocked submissions" :
         local_progress_.active() ? "Online softcore beta: awards enabled" :
         "Online softcore beta: local history unavailable") :
        (local_progress_.active() ? "Local tracking active: no awards submitted" :
         "Local tracking unavailable: progress storage error");
    ++ui_revision_;
}

void ClientBridge::refresh_measured_progress() {
    bool changed = false;
    for (auto& entry : ui_achievements_) {
        if (entry.unlocked || entry.disabled) continue;
        const auto* achievement = guest_catalog_.active() ? nullptr :
            rc_client_get_achievement_info(client_, entry.id);
        const std::string measured = guest_catalog_.active() ? guest_catalog_.measured(entry.id) :
            achievement ? achievement->measured_progress : "";
        if (entry.measured != measured) {
            entry.measured = measured;
            changed = true;
        }
    }
    if (changed) ++ui_revision_;
}

std::uint32_t ClientBridge::read_memory(std::uint32_t address,
                                        std::uint8_t* buffer,
                                        std::uint32_t length,
                                        rc_client_t* client) {
    auto* bridge = static_cast<ClientBridge*>(rc_client_get_userdata(client));
    const auto count = MemoryView(bridge->rdram_).read(address, buffer, length);
    ++bridge->read_calls_;
    bridge->short_read_calls_ += (count != length);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (buffer[i]) {
            ++bridge->nonzero_read_calls_;
            break;
        }
    }
    return count;
}

void ClientBridge::server_call(const rc_api_request_t* request,
                               rc_client_server_callback_t callback,
                               void* callback_data, rc_client_t* client) {
    auto* bridge = static_cast<ClientBridge*>(rc_client_get_userdata(client));
    if (bridge->local_tracking_requested_) {
        const auto award = classify_award_request(request, bridge->online_awards_ready_, bridge->rom_hash_);
        if (award.kind == AwardRequestKind::Denied) {
            static constexpr char kDenied[] = "DK64 RA bridge denied award or leaderboard request";
            rc_api_server_response_t response{kDenied, sizeof(kDenied) - 1,
                                              RC_API_SERVER_RESPONSE_CLIENT_ERROR};
            callback(&response, callback_data);
        } else if (award.kind == AwardRequestKind::Award) {
            bridge->pending_online_.insert(award.id);
            // HttpTransport invokes the callback inline, so this stack context
            // remains valid even when rcheevos performs an immediate retry.
            AwardCallbackData data{bridge, callback, callback_data, award.id};
            bridge->transport_.enqueue(request, award_callback, &data);
        } else if (request && request->post_data &&
                   std::strncmp(request->post_data, "r=achievementsets&", 18) == 0) {
            CatalogCallbackData data{bridge, callback, callback_data};
            bridge->transport_.enqueue(request, catalog_callback, &data);
        } else {
            bridge->transport_.enqueue(request, callback, callback_data);
        }
    } else {
        static constexpr char kOffline[] = "DK64 RA bridge is offline";
        rc_api_server_response_t response{kOffline, sizeof(kOffline) - 1,
                                          RC_API_SERVER_RESPONSE_CLIENT_ERROR};
        callback(&response, callback_data);
    }
}

void ClientBridge::catalog_callback(const rc_api_server_response_t* response, void* userdata) {
    const auto* data = static_cast<CatalogCallbackData*>(userdata);
    const auto path = guest_catalog_path(local_progress_directory(), data->bridge->rom_hash_);
    if (save_guest_catalog(path, response))
        diagnostic_log("[DK64 RA] Validated DK64 set cached locally for guest tracking.");
    data->callback(response, data->callback_data);
}

void ClientBridge::award_callback(const rc_api_server_response_t* response, void* userdata) {
    const auto* data = static_cast<AwardCallbackData*>(userdata);
    auto* bridge = data->bridge;
    if (award_response_confirmed(response, data->id)) {
        bridge->account_unlocks_.insert(data->id);
        bridge->pending_online_.erase(data->id);
        bridge->catalog_dirty_ = true;
        const auto* achievement = rc_client_get_achievement_info(bridge->client_, data->id);
        bridge->queue_toast(achievement);
        diagnostic_log("[DK64 RA] RA confirmed softcore award %u.", data->id);
    } else {
        diagnostic_log("[DK64 RA] Softcore award %u not confirmed; rcheevos may retry while running.", data->id);
    }
    data->callback(response, data->callback_data);
}

void ClientBridge::start_tracking() {
    if (!verify_stored_rom(rom_hash_)) {
        ui_status_ = "Tracking unavailable: ROM mismatch";
        ++ui_revision_;
        diagnostic_log("[DK64 RA] Tracking unavailable: portable DK64.z64 did not match the supported retail hash.");
        return;
    }
    if (!remember_signin_ && !forget_saved_login()) {
        diagnostic_log("[DK64 RA] Could not remove saved sign-in; check Windows Credential Manager.");
    }
    if (mode_ == Mode::LocalTracking) {
        start_guest_tracking("Local Tracking only selected; no RA sign-in required");
        return;
    }
    if (remember_signin_) {
        std::string username;
        std::string token;
        if (read_saved_login(username, token)) {
            diagnostic_log("[DK64 RA] Supported ROM verified. Trying saved token (%s).",
                           mode_ == Mode::OnlineSoftcore ? "online softcore" : "local tracking");
            login_with_saved_token_ = true;
            rc_client_begin_login_with_token(client_, username.c_str(), token.c_str(),
                                             login_callback, this);
            clear_secret(token);
            return;
        }
    }
    prompt_password();
}

void ClientBridge::start_guest_tracking(const char* reason) {
    online_awards_ready_ = false;
    mode_ = Mode::LocalTracking;
    account_username_.clear();
    rc_client_set_spectator_mode_enabled(client_, 1);
    local_progress_ = LocalProgress{};
    if (!local_progress_.open(local_progress_directory(), "Guest", rom_hash_))
        diagnostic_log("[DK64 RA] Guest local progress store unavailable.");
    const auto path = guest_catalog_path(local_progress_directory(), rom_hash_);
    if (guest_catalog_.load(path, local_progress_)) {
        rebuild_ui_catalog(client_);
        diagnostic_log("[DK64 RA] %s; guest cached set loaded (%u core achievements).",
                       reason, static_cast<unsigned>(guest_catalog_.entries().size()));
    } else {
        ui_achievements_.clear();
        ui_status_ = "Local set unavailable: sign in online once to cache it";
        ++ui_revision_;
        diagnostic_log("[DK64 RA] %s; no valid cached DK64 set. Guest tracking inactive.", reason);
    }
}

void ClientBridge::prompt_password() {
    ui_status_ = mode_ == Mode::OnlineSoftcore ?
        "Online softcore: waiting for sign-in" : "Local tracking: waiting for sign-in";
    ++ui_revision_;
    diagnostic_log("[DK64 RA] Supported ROM verified. Waiting for sign-in (%s).",
                   mode_ == Mode::OnlineSoftcore ? "online softcore" : "local tracking");
    std::string username;
    std::string password;
    if (!prompt_for_credentials(username, password, remember_signin_)) {
        start_guest_tracking("RA sign-in unavailable or canceled");
        return;
    }
    rc_client_begin_login_with_password(client_, username.c_str(), password.c_str(),
                                        login_callback, this);
    clear_secret(password);
}

void ClientBridge::login_callback(int result, const char* error_message,
                                  rc_client_t* client, void* userdata) {
    (void)error_message; // Never echo auth/server payloads to game logs.
    auto* bridge = static_cast<ClientBridge*>(userdata);
    if (result != RC_OK) {
        if (bridge->login_with_saved_token_) {
            bridge->login_with_saved_token_ = false;
            if (result == RC_INVALID_CREDENTIALS && !forget_saved_login()) {
                diagnostic_log("[DK64 RA] Saved token was rejected but could not be removed.");
            }
            if (result == RC_INVALID_CREDENTIALS) {
                diagnostic_log("[DK64 RA] Saved token was rejected; asking for manual sign-in.");
                bridge->prompt_password();
            } else {
                diagnostic_log("[DK64 RA] Saved sign-in unavailable (code %d); using cached guest set if present.", result);
                bridge->start_guest_tracking("Saved RA sign-in unavailable");
            }
            return;
        }
        diagnostic_log("[DK64 RA] Sign-in failed (code %d); using guest fallback if cached.", result);
        bridge->start_guest_tracking("RA sign-in failed");
        return;
    }
    bridge->login_with_saved_token_ = false;
    const auto* signed_in_user = rc_client_get_user_info(client);
    bridge->account_username_ = signed_in_user && signed_in_user->username ?
        signed_in_user->username : "";
    if (bridge->remember_signin_) {
        const auto* user = rc_client_get_user_info(client);
        if (!user || !store_saved_login(user->username, user->token)) {
            diagnostic_log("[DK64 RA] Signed in, but could not save the token for next time.");
        } else {
            diagnostic_log("[DK64 RA] Login token saved in Windows Credential Manager (not the password).");
        }
    }
    diagnostic_log("[DK64 RA] Signed in; loading existing N64 set in %s mode.",
                   bridge->mode_ == Mode::OnlineSoftcore ? "online softcore" : "local spectator");
    bridge->ui_status_ = bridge->mode_ == Mode::OnlineSoftcore ?
        "Online softcore: loading DK64 set" : "Local tracking: loading DK64 set";
    ++bridge->ui_revision_;
    rc_client_begin_load_game(client, bridge->rom_hash_.c_str(), load_callback, bridge);
}

void ClientBridge::load_callback(int result, const char* error_message,
                                 rc_client_t* client, void* userdata) {
    (void)error_message;
    auto* bridge = static_cast<ClientBridge*>(userdata);
    if (result != RC_OK) {
        diagnostic_log("[DK64 RA] Achievement set load failed (code %d).", result);
        bridge->start_guest_tracking("RA set load failed");
        return;
    }
    const rc_client_game_t* game = rc_client_get_game_info(client);
    if (!game || game->id != 10075) {
        rc_client_unload_game(client);
        diagnostic_log("[DK64 RA] Resolved game is not RA game 10075; tracking aborted.");
        bridge->start_guest_tracking("RA resolved a different game ID");
        return;
    }
    rc_client_user_game_summary_t summary{};
    rc_client_get_user_game_summary(client, &summary);
    unsigned active = 0;
    unsigned disabled = 0;
    bridge->account_unlocks_.clear();
    auto* list = rc_client_create_achievement_list(client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,
                                                   RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (list) {
        for (std::uint32_t i = 0; i < list->num_buckets; ++i) {
            const auto& bucket = list->buckets[i];
            for (std::uint32_t j = 0; j < bucket.num_achievements; ++j) {
                const auto* achievement = bucket.achievements[j];
                if (achievement->id >= 101000001U) {
                    continue;
                }
                if (achievement->unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE)
                    bridge->account_unlocks_.insert(achievement->id);
                active += achievement->state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE;
                disabled += achievement->state == RC_CLIENT_ACHIEVEMENT_STATE_DISABLED;
            }
        }
        rc_client_destroy_achievement_list(list);
    }
    const auto* user = rc_client_get_user_info(client);
    if (!user || !user->username ||
        !bridge->local_progress_.open(local_progress_directory(), user->username, bridge->rom_hash_)) {
        diagnostic_log("[DK64 RA] Local progress store unavailable; triggers will not be persisted.");
    }
    bridge->online_awards_ready_ = bridge->mode_ == Mode::OnlineSoftcore &&
        !rc_client_get_hardcore_enabled(client) && !rc_client_get_spectator_mode_enabled(client);
    const auto* rap = rc_client_get_achievement_info(client, 60497);
    diagnostic_log("[DK64 RA] Game 10075 loaded: %u core, %u active across loaded subsets, %u disabled; DK Rap state %d. %s.",
                summary.num_core_achievements, active, disabled, rap ? rap->state : -1,
                bridge->online_awards_ready_ ? "ONLINE SOFTCORE BETA" : "NO AWARDS");
    bridge->rebuild_ui_catalog(client);
    rap_diagnostics_loaded(client);
}

void ClientBridge::event_handler(const rc_client_event_t* event, rc_client_t* client) {
    auto* bridge = static_cast<ClientBridge*>(rc_client_get_userdata(client));
    if (event->type == RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED && event->achievement) {
        // The RA server can insert a zero-point client warning into the set.
        // rcheevos reserves IDs >= 101000001 for these; do not present one as
        // evidence that a DK64 gameplay achievement evaluated correctly.
        if (event->achievement->id >= 101000001U) {
            diagnostic_log("[DK64 RA] Client warning (not a DK64 unlock): %s",
                        event->achievement->title);
        } else {
            const bool saved = bridge && bridge->local_progress_.mark(event->achievement->id);
            diagnostic_log("[DK64 RA] %s trigger (%s): %s",
                        bridge->mode_ == Mode::OnlineSoftcore ? "Online softcore" : "Local spectator",
                        saved ? "local history saved" : "local history SAVE FAILED", event->achievement->title);
            if (bridge) {
                if (!bridge->account_unlocks_.count(event->achievement->id)) {
                    bridge->queue_toast(event->achievement);
                }
                bridge->catalog_dirty_ = true;
            }
        }
    } else if (event->type == RC_CLIENT_EVENT_SERVER_ERROR) {
        diagnostic_log("[DK64 RA] Server error; check connection and RA award status.");
    } else if (event->type == RC_CLIENT_EVENT_DISCONNECTED) {
        diagnostic_log("[DK64 RA] Connection lost. Local catalog may be stale.");
    }
}

} // namespace dk64_ra
