#include "client_bridge.h"
#include "windows_credentials.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #test); return 1; } } while (0)

namespace {
constexpr char hash[] = "9ec41abf2519fc386cadd0731f6e868c";
constexpr char catalog[] = R"({"Success":true,"GameId":10075,"Title":"Donkey Kong 64","ConsoleId":2,"ImageIconUrl":"https://media.retroachievements.org/Images/00123.png","RichPresenceGameId":10075,"RichPresencePatch":"","Sets":[{"AchievementSetId":10075,"GameId":10075,"Title":"Donkey Kong 64","Type":"core","ImageIconUrl":"https://media.retroachievements.org/Images/00123.png","Achievements":[{"ID":60497,"Title":"Test achievement","Description":"Test only","Flags":3,"Points":0,"MemAddr":"0xH000100=1","Author":"Test","BadgeName":"00123","Created":1367266583,"Modified":1376929305}],"Leaderboards":[]}]})";
bool saved = false, remember = false, cancel = false, save_ok = true, forget_ok = true;
bool reject_token = false, fail_load = false;
int reads = 0, prompts = 0, stores = 0, forgets = 0, requests = 0;
dk64_ra::RomVerificationStatus rom_status = dk64_ra::RomVerificationStatus::Supported;
std::string field(dk64_ra::ClientBridge& bridge, std::vector<std::uint8_t>& memory, unsigned id) {
    if (!bridge.ui_copy_account(id, memory.data(), 0x81000000, 1024)) return "<copy failed>";
    std::string result;
    for (unsigned i = 0; i < 1024 && memory[(0x1000000 + i) ^ 3]; ++i)
        result += static_cast<char>(memory[(0x1000000 + i) ^ 3]);
    return result;
}
}

// Link-time platform doubles: no real network, ROM or credential store access.
namespace dk64_ra {
RomVerificationStatus verify_stored_rom(std::string& result) { result = hash; return rom_status; }
bool prompt_for_credentials(std::string& user, std::string& password, bool& persist) {
    ++prompts;
    persist = !cancel && remember;
    user = cancel ? "" : "TestUser";
    password = cancel ? "" : "TestPassword";
    return !cancel;
}
bool read_saved_login(std::string& user, std::string& token) {
    ++reads;
    if (!saved) return false;
    user = "TestUser"; token = "TestToken"; return true;
}
bool store_saved_login(const char* user, const char* token) {
    ++stores;
    if (std::strcmp(user, "TestUser") || std::strcmp(token, "TestToken")) return false;
    if (save_ok) saved = true;
    return save_ok;
}
bool forget_saved_login() { ++forgets; if (forget_ok) saved = false; return forget_ok; }
void clear_secret(std::string& secret) { secret.assign(secret.size(), '\0'); secret.clear(); }
void HttpTransport::enqueue(const rc_api_request_t* request, rc_client_server_callback_t callback, void* data) {
    ++requests;
    const std::string post = request && request->post_data ? request->post_data : "";
    const char* body = R"({"Success":true,"Unlocks":[],"HardcoreUnlocks":[]})";
    int status = 200;
    if (post.find("r=login2&") == 0) {
        if (reject_token && post.find("&t=") != std::string::npos) {
            body = R"({"Success":false,"Error":"Invalid credentials","Code":"invalid_credentials","Status":401})";
            status = 401;
        } else body = R"({"Success":true,"User":"TestUser","Token":"TestToken","Score":0,"Messages":0})";
    } else if (post.find("r=achievementsets&") == 0) {
        body = fail_load ? R"({"Success":false,"Error":"Test set unavailable"})" : catalog;
    }
    const rc_api_server_response_t response{body, std::strlen(body), status};
    callback(&response, data);
}
}

int main() {
    using Bridge = dk64_ra::ClientBridge;
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("dk64-ra-account-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::vector<std::uint8_t> memory(0x1000000 + 1024);
    Bridge bridge(root);
    CHECK(bridge.initialize(memory.data(), Bridge::Mode::OnlineSoftcore, false));
    bridge.on_frame(memory.data());
    CHECK(prompts == 1 && stores == 0 && bridge.signed_in());
    CHECK(field(bridge, memory, 0) == "TestUser");
    CHECK(field(bridge, memory, 1) == "Signed in for this session");
    CHECK(!bridge.safe_mode() && bridge.ui_count(Bridge::Filter::All) == 1);
    CHECK(!bridge.ui_copy_account(99, memory.data(), 0x81000000, 1024));
    CHECK(!bridge.sign_in()); // cannot silently replace an active account

    dk64_ra::LocalProgress history;
    CHECK(history.open(root, "TestUser", hash) && history.mark(42));
    CHECK(bridge.log_out() && !bridge.signed_in() && bridge.safe_mode());
    CHECK(field(bridge, memory, 0) == "Not signed in");
    CHECK(field(bridge, memory, 2).find("Logged out") == 0);
    CHECK(bridge.mode() == Bridge::Mode::LocalTracking && bridge.ui_count(Bridge::Filter::All) == 1);
    const int after_logout = requests;
    memory[0x100] = 1;
    bridge.on_frame(memory.data()); bridge.on_frame(memory.data());
    CHECK(requests == after_logout); // logout cannot reauthenticate or submit awards
    CHECK(history.open(root, "TestUser", hash) && history.contains(42));

    cancel = true;
    CHECK(!bridge.sign_in() && bridge.safe_mode() && !bridge.signed_in());
    CHECK(field(bridge, memory, 2).find("canceled") != std::string::npos);
    cancel = false; remember = true;
    CHECK(bridge.sign_in() && saved && stores == 1);
    CHECK(field(bridge, memory, 1) == "Sign-in remembered on this PC");

    const int before_local_reads = reads, before_local_forgets = forgets;
    CHECK(bridge.initialize(memory.data(), Bridge::Mode::LocalTracking, false));
    bridge.on_frame(memory.data());
    CHECK(saved && reads == before_local_reads && forgets == before_local_forgets && !bridge.signed_in());

    const int before_token = prompts;
    CHECK(bridge.initialize(memory.data(), Bridge::Mode::OnlineSoftcore, false));
    bridge.on_frame(memory.data());
    CHECK(bridge.signed_in() && prompts == before_token && saved);
    forget_ok = false;
    CHECK(!bridge.log_out() && !bridge.signed_in() && bridge.safe_mode());
    CHECK(field(bridge, memory, 2).find("could not be removed") != std::string::npos);
    forget_ok = true;

    reject_token = true; remember = false;
    CHECK(bridge.initialize(memory.data(), Bridge::Mode::OnlineSoftcore, false));
    bridge.on_frame(memory.data());
    CHECK(prompts == before_token + 1 && bridge.signed_in() && !saved);
    CHECK(field(bridge, memory, 1) == "Signed in for this session");
    CHECK(bridge.log_out());

    remember = true; save_ok = false;
    CHECK(bridge.sign_in() && !saved);
    CHECK(field(bridge, memory, 1) == "Signed in for this session");
    CHECK(field(bridge, memory, 2).find("could not be saved") != std::string::npos);
    CHECK(bridge.log_out());
    save_ok = true; remember = false; fail_load = true;
    CHECK(bridge.sign_in() && bridge.safe_mode()); // account remains visible if set load fails
    CHECK(field(bridge, memory, 0) == "TestUser");
    CHECK(bridge.log_out());
    rom_status = dk64_ra::RomVerificationStatus::Mismatch;
    const int before_invalid = prompts;
    CHECK(!bridge.sign_in() && prompts == before_invalid && bridge.safe_mode());

    std::error_code error;
    for (const auto& entry : fs::directory_iterator(root)) fs::remove(entry.path(), error);
    fs::remove(root, error);
    std::puts("Account: opt-in token storage, saved login, cancellation, failures, logout isolation and cache fallback passed");
}
