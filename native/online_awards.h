#pragma once

#include <cstdint>
#include <string_view>

extern "C" {
#include "rc_api_request.h"
#include "rc_api_runtime.h"
}

namespace dk64_ra {

enum class AwardRequestKind { Other, Award, Denied };

struct AwardRequest {
    AwardRequestKind kind = AwardRequestKind::Other;
    std::uint32_t id = 0;
};

// Check the official rcheevos POST before it leaves the process. In particular,
// this bridge never sends a Hardcore or leaderboard result.
AwardRequest classify_award_request(const rc_api_request_t* request,
                                    bool online_ready, std::string_view verified_hash);
bool award_response_confirmed(const rc_api_server_response_t* response, std::uint32_t id);

} // namespace dk64_ra
