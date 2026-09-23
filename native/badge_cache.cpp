#include "badge_cache.h"
#include "guest_text.h"

#include <algorithm>
#include <array>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#elif defined(__linux__)
#include "linux_curl.h"
#endif

namespace dk64_ra {
namespace {
constexpr std::string_view kBadgePrefix = "https://media.retroachievements.org/Badge/";
constexpr std::size_t kMaxBadgeBytes = 64U * 1024U;

#ifdef _WIN32
struct HttpHandle {
    HINTERNET value;
    ~HttpHandle() { if (value) WinHttpCloseHandle(value); }
    explicit operator bool() const { return value != nullptr; }
    operator HINTERNET() const { return value; }
};
#endif
} // namespace

bool BadgeCache::valid_url(std::string_view url) {
    if (url.size() <= kBadgePrefix.size() + 4 || url.size() > 128 ||
        url.substr(0, kBadgePrefix.size()) != kBadgePrefix ||
        url.substr(url.size() - 4) != ".png") {
        return false;
    }
    const auto name = url.substr(kBadgePrefix.size(), url.size() - kBadgePrefix.size() - 4);
    if (name.empty() || name.size() > 64) return false;
    return std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z') || c == '_' || c == '-';
    });
}

bool BadgeCache::valid_png(const std::vector<std::uint8_t>& data) {
    static constexpr std::array<std::uint8_t, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    if (data.size() < 33 || data.size() > kMaxBadgeBytes ||
        !std::equal(signature.begin(), signature.end(), data.begin()) ||
        data[12] != 'I' || data[13] != 'H' || data[14] != 'D' || data[15] != 'R') {
        return false;
    }
    const auto dimension = [&data](std::size_t pos) {
        return (std::uint32_t(data[pos]) << 24) | (std::uint32_t(data[pos + 1]) << 16) |
               (std::uint32_t(data[pos + 2]) << 8) | data[pos + 3];
    };
    const auto width = dimension(16), height = dimension(20);
    return width > 0 && height > 0 && width <= 256 && height <= 256;
}

BadgeCache::~BadgeCache() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        queued_.clear();
    }
    ready_.notify_one();
    if (worker_.joinable()) worker_.join();
}

std::uint32_t BadgeCache::copy_or_queue(std::string_view url, std::uint8_t* rdram,
                                        std::uint32_t guest_address, std::uint32_t capacity) {
    if (!valid_url(url) || !rdram || capacity < kMaxBadgeBytes) return 0;
#ifdef __linux__
    if (!linux_curl_ready()) return 0;
#endif
    std::lock_guard lock(mutex_);
    if (stopping_) return 0;
    auto [it, inserted] = images_.try_emplace(std::string(url));
    if (inserted) {
        queued_.push_back(it->first);
        if (!worker_.joinable()) {
            try {
                worker_ = std::thread([this] { run(); });
            } catch (...) {
                queued_.pop_back();
                it->second.state = State::Failed;
            }
        }
        ready_.notify_one();
    }
    if (it->second.state != State::Ready ||
        !write_guest_bytes(rdram, guest_address, capacity, it->second.bytes,
                           0x05000000U)) {
        return 0;
    }
    return static_cast<std::uint32_t>(it->second.bytes.size());
}

void BadgeCache::run() {
    for (;;) {
        std::string url;
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [this] { return stopping_ || !queued_.empty(); });
            if (stopping_) return;
            url = std::move(queued_.front());
            queued_.pop_front();
        }
        auto bytes = download(url);
        {
            std::lock_guard lock(mutex_);
            if (stopping_) return;
            auto it = images_.find(url);
            if (it != images_.end()) {
                it->second.state = bytes.empty() ? State::Failed : State::Ready;
                it->second.bytes = std::move(bytes);
                revision_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
}

std::vector<std::uint8_t> BadgeCache::download(std::string_view url) {
    std::vector<std::uint8_t> data;
#ifdef _WIN32
    if (!valid_url(url)) return data;
    HttpHandle session{WinHttpOpen(L"DK64RekongRA/0.1.0 (local badges)",
                                   WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session) return data;
    WinHttpSetTimeouts(session, 3000, 3000, 5000, 5000);
    HttpHandle connection{WinHttpConnect(session, L"media.retroachievements.org", 443, 0)};
    if (!connection) return data;
    const std::wstring path(url.begin() + kBadgePrefix.size() - 7, url.end()); // /Badge/...
    HttpHandle request{WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
                                          WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          WINHTTP_FLAG_SECURE)};
    if (!request) return data;
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS;
    if (!WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)) ||
        !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) return {};
    DWORD status = 0, length = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &length,
                             WINHTTP_NO_HEADER_INDEX) || status != 200) return {};
    std::array<std::uint8_t, 8192> buffer{};
    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) return {};
        if (!read) break;
        if (read > kMaxBadgeBytes - data.size()) return {};
        data.insert(data.end(), buffer.begin(), buffer.begin() + read);
    }
#elif defined(__linux__)
    if (!valid_url(url)) return data;
    CURL* handle = curl_easy_init();
    if (!handle) return data;
    std::string body;
    CurlBody bounded{&body, kMaxBadgeBytes};
    const std::string address(url);
    const bool options_ok = curl_https_only(handle) &&
        curl_easy_setopt(handle, CURLOPT_URL, address.c_str()) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 3000L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 10000L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_USERAGENT, "DK64RekongRA/0.2.0 (Linux badges)") == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_bounded_write) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &bounded) == CURLE_OK;
    long status = 0;
    if (options_ok && curl_easy_perform(handle) == CURLE_OK &&
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status) == CURLE_OK && status == 200)
        data.assign(body.begin(), body.end());
    curl_easy_cleanup(handle);
#else
    (void)url;
#endif
    if (!valid_png(data)) data.clear();
    return data;
}

} // namespace dk64_ra
