#include "guest_catalog.h"
#include "memory.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
constexpr char kHash[] = "9ec41abf2519fc386cadd0731f6e868c";
constexpr char kCatalog[] =
    "{\"Success\":true,\"GameId\":10075,\"Title\":\"Donkey Kong 64\",\"ConsoleId\":2,"
    "\"ImageIconUrl\":\"https://media.retroachievements.org/Images/00123.png\","
    "\"RichPresenceGameId\":10075,\"RichPresencePatch\":\"\",\"Sets\":[{"
    "\"AchievementSetId\":10075,\"GameId\":10075,\"Title\":\"Donkey Kong 64\","
    "\"Type\":\"core\",\"ImageIconUrl\":\"https://media.retroachievements.org/Images/00123.png\","
    "\"Achievements\":[{\"ID\":60497,\"Title\":\"DK Rap\","
    "\"Description\":\"Listen to the DK Rap\",\"Flags\":3,\"Points\":0,"
    "\"MemAddr\":\"0xH000100=1\",\"Author\":\"Test\",\"BadgeName\":\"00123\","
    "\"Created\":1367266583,\"Modified\":1376929305}],"
    "\"Leaderboards\":[]}]}";

bool check(bool value, const char* message) {
    if (!value) std::fprintf(stderr, "%s\n", message);
    return value;
}
} // namespace

int main() {
    namespace fs = std::filesystem;
    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root = fs::temp_directory_path() / ("dk64-ra-guest-test-" + suffix);
    const auto path = dk64_ra::guest_catalog_path(root, kHash);
    if (!check(!path.empty() && dk64_ra::guest_catalog_path(root, "../bad").empty(),
               "Unsafe catalog path accepted")) return 1;
    dk64_ra::LocalProgress progress;
    if (!check(progress.open(root, "Guest", kHash), "Guest history did not open")) return 2;
    dk64_ra::GuestCatalog first_run;
    if (!check(!first_run.load(path, progress), "Missing cache presented an active guest set")) return 2;
    const rc_api_server_response_t server{kCatalog, sizeof(kCatalog) - 1, 200};
    if (!check(dk64_ra::save_guest_catalog(path, &server), "Valid catalog was not cached")) return 3;
    dk64_ra::GuestCatalog guest;
    if (!check(guest.load(path, progress) && guest.entries().size() == 1 &&
               guest.entries()[0].id == 60497 && guest.entries()[0].description == "Listen to the DK Rap",
               "Guest catalog did not load without an RA account")) return 4;
    std::vector<std::uint8_t> memory(dk64_ra::kPhysicalRdramSize);
    std::vector<std::uint32_t> triggered;
    guest.frame(memory.data(), triggered);
    memory[0x100] = 1;
    guest.frame(memory.data(), triggered);
    guest.frame(memory.data(), triggered);
    if (!check(triggered.size() == 1 && triggered.front() == 60497 &&
               progress.mark(triggered.front()), "Guest trigger was not tracked")) return 5;
    dk64_ra::LocalProgress reloaded;
    dk64_ra::GuestCatalog replay;
    if (!check(reloaded.open(root, "Guest", kHash) && reloaded.contains(60497) &&
               replay.load(path, reloaded), "Guest history did not persist")) return 6;
    triggered.clear();
    replay.frame(memory.data(), triggered);
    if (!check(triggered.empty(), "Persisted trigger replayed after restart")) return 7;

    const std::string wrong = std::string(kCatalog).replace(std::string(kCatalog).find("10075"), 5, "10076");
    const rc_api_server_response_t invalid{wrong.c_str(), wrong.size(), 200};
    if (!check(!dk64_ra::save_guest_catalog(path, &invalid),
               "Wrong game overwrote a valid cached set")) return 8;
    const auto corrupt_path = root / "catalog-broken.json";
    { std::ofstream file(corrupt_path); file << "invalid"; }
    dk64_ra::GuestCatalog corrupt;
    if (!check(!corrupt.load(corrupt_path, progress), "Corrupt catalog loaded")) return 9;

    std::error_code error;
    fs::remove(path, error);
    fs::remove(progress.path(), error);
    fs::remove(corrupt_path, error);
    fs::remove(root, error);
    std::puts("Guest catalog validation, no-login evaluation, persistence and fail-closed checks passed");
    return 0;
}
