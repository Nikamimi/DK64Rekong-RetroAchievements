#include "online_awards.h"

#include <charconv>
#include <string_view>

namespace dk64_ra {
namespace {
std::string_view parameter(std::string_view post, std::string_view name) {
    while (!post.empty()) {
        const auto end = post.find('&');
        const auto field = post.substr(0, end);
        const auto equals = field.find('=');
        if (equals == name.size() && field.substr(0, equals) == name)
            return field.substr(equals + 1);
        if (end == std::string_view::npos) break;
        post.remove_prefix(end + 1);
    }
    return {};
}
}

AwardRequest classify_award_request(const rc_api_request_t* request,
                                    bool online_ready, std::string_view verified_hash) {
    if (!request || !request->post_data) return {};
    const std::string_view post(request->post_data);
    const auto action = parameter(post, "r");
    if (action == "submitlbentry") return {AwardRequestKind::Denied, 0};
    if (action != "awardachievement") return {};
    if (!online_ready || verified_hash.size() != 32 ||
        parameter(post, "h") != "0" || parameter(post, "m") != verified_hash) {
        return {AwardRequestKind::Denied, 0};
    }
    const auto id_text = parameter(post, "a");
    if (id_text.empty()) return {AwardRequestKind::Denied, 0};
    std::uint32_t id = 0;
    const auto parsed = std::from_chars(id_text.data(), id_text.data() + id_text.size(), id);
    if (parsed.ec != std::errc{} || parsed.ptr != id_text.data() + id_text.size() ||
        id == 0 || id >= 101000001U) return {AwardRequestKind::Denied, 0};
    return {AwardRequestKind::Award, id};
}

bool award_response_confirmed(const rc_api_server_response_t* response, std::uint32_t id) {
    if (!response || !id) return false;
    rc_api_award_achievement_response_t award{};
    const int result = rc_api_process_award_achievement_server_response(&award, response);
    const bool confirmed = result == RC_OK && award.response.succeeded &&
                           award.awarded_achievement_id == id;
    rc_api_destroy_award_achievement_response(&award);
    return confirmed;
}

} // namespace dk64_ra
