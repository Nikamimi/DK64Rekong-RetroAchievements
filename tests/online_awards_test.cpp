#include "online_awards.h"

#include <cstdio>
#include <string>

int main() {
    const std::string hash = "9ec41abf2519fc386cadd0731f6e868c";
    const std::string post = "r=awardachievement&u=Test&t=not-a-real-token&a=60497&h=0&m=" + hash;
    rc_api_request_t request{};
    request.post_data = post.c_str();
    auto classify = [&](bool ready, const std::string& expected) {
        return dk64_ra::classify_award_request(&request, ready, expected);
    };
    if (classify(true, hash).kind != dk64_ra::AwardRequestKind::Award || classify(true, hash).id != 60497 ||
        classify(false, hash).kind != dk64_ra::AwardRequestKind::Denied ||
        classify(true, "00000000000000000000000000000000").kind != dk64_ra::AwardRequestKind::Denied) return 1;
    for (const char* invalid : {"h=1", "a=101000001", "m=bad"}) {
        std::string modified = post;
        const auto offset = modified.find(invalid[0] == 'm' ? "m=" : invalid[0] == 'h' ? "h=0" : "a=60497");
        modified.replace(offset, invalid[0] == 'm' ? 34 : invalid[0] == 'h' ? 3 : 7, invalid);
        request.post_data = modified.c_str();
        if (classify(true, hash).kind != dk64_ra::AwardRequestKind::Denied) return 2;
    }
    request.post_data = "r=submitlbentry&u=Test&t=not-a-real-token";
    if (classify(true, hash).kind != dk64_ra::AwardRequestKind::Denied) return 3;
    request.post_data = "r=startsession&u=Test&t=not-a-real-token";
    if (classify(true, hash).kind != dk64_ra::AwardRequestKind::Other) return 4;

    const std::string success = "{\"Success\":true,\"AchievementID\":60497,\"Score\":0,\"SoftcoreScore\":1,\"AchievementsRemaining\":127}";
    const std::string failed = "{\"Success\":false,\"Error\":\"not awarded\"}";
    rc_api_server_response_t response{success.c_str(), success.size(), 200};
    if (!dk64_ra::award_response_confirmed(&response, 60497) ||
        dk64_ra::award_response_confirmed(&response, 60498)) return 5;
    response = {failed.c_str(), failed.size(), 200};
    if (dk64_ra::award_response_confirmed(&response, 60497)) return 6;
    std::puts("Online award gate and acknowledgement passed");
    return 0;
}
