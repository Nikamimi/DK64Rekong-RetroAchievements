#pragma once

#ifdef __linux__
#include <curl/curl.h>

#include <cstddef>
#include <limits>
#include <string>

namespace dk64_ra {

// Initialize on the game thread, before the badge worker can make requests.
inline bool linux_curl_ready() {
    static const bool ready = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    return ready;
}

struct CurlBody {
    std::string* bytes;
    std::size_t limit;
    bool too_large = false;
};

inline std::size_t curl_bounded_write(char* bytes, std::size_t size,
                                      std::size_t count, void* userdata) {
    auto& body = *static_cast<CurlBody*>(userdata);
    if (size && count > std::numeric_limits<std::size_t>::max() / size) {
        body.too_large = true;
        return 0;
    }
    const auto length = size * count;
    if (body.bytes->size() > body.limit || length > body.limit - body.bytes->size()) {
        body.too_large = true;
        return 0;
    }
    body.bytes->append(bytes, length);
    return length;
}

inline bool curl_https_only(CURL* handle) {
#if LIBCURL_VERSION_NUM >= 0x075500
    return curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "https") == CURLE_OK;
#else
    return curl_easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS) == CURLE_OK;
#endif
}

} // namespace dk64_ra
#endif
