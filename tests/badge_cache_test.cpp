#include "badge_cache.h"
#include "rc_client.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

int main() {
    using dk64_ra::BadgeCache;
    if (!BadgeCache::valid_url("https://media.retroachievements.org/Badge/12345_lock.png") ||
        BadgeCache::valid_url("http://media.retroachievements.org/Badge/12345.png") ||
        BadgeCache::valid_url("https://media.retroachievements.org.evil.test/Badge/12345.png") ||
        BadgeCache::valid_url("https://media.retroachievements.org/Badge/../private.png") ||
        BadgeCache::valid_url("https://media.retroachievements.org/Badge/12345.png?key=secret")) {
        std::fputs("Badge URL host/path allowlist failed\n", stderr);
        return 1;
    }
    std::vector<std::uint8_t> png(33, 0);
    const std::uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    for (unsigned i = 0; i < 8; ++i) png[i] = signature[i];
    png[12] = 'I'; png[13] = 'H'; png[14] = 'D'; png[15] = 'R';
    png[19] = 96; png[23] = 96;
    if (!BadgeCache::valid_png(png)) return 1;
    png[19] = 0;
    if (BadgeCache::valid_png(png)) return 1;
    png[19] = 96;
    png[16] = 1; // 16,777,312 pixels is not a badge.
    if (BadgeCache::valid_png(png)) return 1;
    BadgeCache cache;
    if (cache.copy_or_queue("https://elsewhere.test/Badge/123.png", nullptr, 0, 0)) return 1;
    rc_client_achievement_t achievement{};
    std::memcpy(achievement.badge_name, "12345", 6);
    char locked[256]{}, unlocked[256]{};
    if (rc_client_achievement_get_image_url(&achievement, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE,
                                            locked, sizeof(locked)) != RC_OK ||
        rc_client_achievement_get_image_url(&achievement, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED,
                                            unlocked, sizeof(unlocked)) != RC_OK ||
        std::string(locked) != "https://media.retroachievements.org/Badge/12345_lock.png" ||
        std::string(unlocked) != "https://media.retroachievements.org/Badge/12345.png") {
        std::fputs("Official locked/unlocked badge URL selection failed\n", stderr);
        return 1;
    }
    std::puts("Badge allowlist and PNG bounds passed");
    return 0;
}
