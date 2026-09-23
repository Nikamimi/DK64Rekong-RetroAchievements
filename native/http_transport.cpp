#include "http_transport.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#elif defined(__linux__)
#include "linux_curl.h"
#endif

namespace dk64_ra {
namespace {
constexpr std::size_t kMaxResponse = 8U * 1024U * 1024U;
constexpr std::size_t kMaxPost = 1024U * 1024U;

void clear_secret(std::string& value) {
    if (!value.empty()) {
#ifdef _WIN32
        SecureZeroMemory(value.data(), value.size());
#else
        std::fill(value.begin(), value.end(), '\0');
#endif
        value.clear();
    }
}
} // namespace

void HttpTransport::enqueue(const rc_api_request_t* request,
                            rc_client_server_callback_t callback,
                            void* callback_data) {
    Request item;
    item.url = request && request->url ? request->url : "";
    item.is_post = request && request->post_data;
    if (item.is_post) {
        item.post_data = request->post_data;
    }
    item.content_type = request && request->content_type ? request->content_type : "";
    Result result = perform(item);
    clear_secret(item.post_data);
    rc_api_server_response_t response{result.body.c_str(), result.body.size(), result.status};
    callback(&response, callback_data);
    clear_secret(result.body);
}

HttpTransport::Result HttpTransport::perform(Request& request) {
    Result result;
    result.body = "RetroAchievements request rejected";
    // Never let an API request send a password/token to an unapproved host,
    // plaintext HTTP endpoint, redirect target, or nonstandard port.
    static constexpr std::string_view kOrigin = "https://retroachievements.org/";
    if (request.url.size() > 8192 || request.post_data.size() > kMaxPost ||
        !std::all_of(request.url.begin(), request.url.end(),
                     [](unsigned char c) { return c >= 0x21 && c <= 0x7e; }) ||
        request.url.compare(0, kOrigin.size(), kOrigin) != 0 ||
        request.url.find_first_of("\\#") != std::string::npos ||
        (request.is_post && request.content_type != "application/x-www-form-urlencoded")) {
        return result;
    }
#ifdef _WIN32
    const std::wstring url(request.url.begin(), request.url.end());
    std::array<wchar_t, 256> host{};
    std::array<wchar_t, 4096> path{};
    std::array<wchar_t, 4096> extra{};
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host.data();
    parts.dwHostNameLength = static_cast<DWORD>(host.size());
    parts.lpszUrlPath = path.data();
    parts.dwUrlPathLength = static_cast<DWORD>(path.size());
    parts.lpszExtraInfo = extra.data();
    parts.dwExtraInfoLength = static_cast<DWORD>(extra.size());
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts) ||
        parts.nScheme != INTERNET_SCHEME_HTTPS || parts.nPort != 443 ||
        std::wstring(parts.lpszHostName, parts.dwHostNameLength) != L"retroachievements.org") {
        return result;
    }

    struct Handle {
        HINTERNET value;
        ~Handle() { if (value) WinHttpCloseHandle(value); }
        operator HINTERNET() const { return value; }
    };
    Handle session{WinHttpOpen(L"DK64RekongRA/0.2.0 (Windows)",
                               WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session) {
        result.body = "Could not start Windows HTTPS session";
        return result;
    }
    WinHttpSetTimeouts(session, 5000, 5000, 10000, 10000);
    Handle connection{WinHttpConnect(session, L"retroachievements.org", 443, 0)};
    if (!connection) {
        result.body = "Could not connect to RetroAchievements";
        return result;
    }
    std::wstring resource(parts.lpszUrlPath, parts.dwUrlPathLength);
    resource.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (resource.empty()) {
        resource = L"/";
    }
    Handle http_request{WinHttpOpenRequest(connection, request.is_post ? L"POST" : L"GET",
                                       resource.c_str(), nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
    if (!http_request) {
        result.body = "Could not create HTTPS request";
        return result;
    }
    DWORD disabled_features = WINHTTP_DISABLE_REDIRECTS;
    if (!WinHttpSetOption(http_request, WINHTTP_OPTION_DISABLE_FEATURE,
                          &disabled_features, sizeof(disabled_features))) {
        result.body = "Could not disable HTTPS redirects";
        return result;
    }
    const wchar_t* headers = request.is_post ? L"Content-Type: application/x-www-form-urlencoded\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS;
    void* data = request.is_post ? static_cast<void*>(request.post_data.data()) : WINHTTP_NO_REQUEST_DATA;
    DWORD data_size = request.is_post ? static_cast<DWORD>(request.post_data.size()) : 0;
    if (!WinHttpSendRequest(http_request, headers,
                            request.is_post ? static_cast<DWORD>(-1L) : 0,
                            data, data_size, data_size, 0) ||
        !WinHttpReceiveResponse(http_request, nullptr)) {
        result.body = "RetroAchievements HTTPS request failed";
        result.status = RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
        return result;
    }
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    if (!WinHttpQueryHeaders(http_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                             WINHTTP_NO_HEADER_INDEX)) {
        result.body = "RetroAchievements response had no HTTP status";
        return result;
    }
    result.status = static_cast<int>(status);
    result.body.clear();
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(http_request, &available)) {
            result.body = "RetroAchievements response read failed";
            result.status = RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
            return result;
        }
        if (!available) {
            break;
        }
        if (available > kMaxResponse - result.body.size()) {
            result.body = "RetroAchievements response too large";
            result.status = RC_API_SERVER_RESPONSE_CLIENT_ERROR;
            return result;
        }
        std::array<char, 65536> buffer{};
        while (available) {
            DWORD read = 0;
            const DWORD wanted = std::min(available, static_cast<DWORD>(buffer.size()));
            if (!WinHttpReadData(http_request, buffer.data(), wanted, &read) || !read) {
                result.body = "RetroAchievements response read failed";
                result.status = RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
                return result;
            }
            result.body.append(buffer.data(), read);
            available -= read;
        }
    }
#elif defined(__linux__)
    if (!linux_curl_ready()) return result;
    CURL* handle = curl_easy_init();
    if (!handle) return result;
    result.body.clear();
    CurlBody body{&result.body, kMaxResponse};
    curl_slist* headers = nullptr;
    if (request.is_post) headers = curl_slist_append(nullptr,
                                                     "Content-Type: application/x-www-form-urlencoded");
    const bool options_ok = (!request.is_post || headers) && curl_https_only(handle) &&
        curl_easy_setopt(handle, CURLOPT_URL, request.url.c_str()) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 5000L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 20000L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_USERAGENT, "DK64RekongRA/0.2.0 (Linux)") == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_bounded_write) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &body) == CURLE_OK &&
        (!request.is_post ||
         (curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers) == CURLE_OK &&
          curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request.post_data.data()) == CURLE_OK &&
          curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
                           static_cast<long>(request.post_data.size())) == CURLE_OK));
    if (options_ok) {
        const auto code = curl_easy_perform(handle);
        if (code == CURLE_OK) {
            long status = 0;
            if (curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status) == CURLE_OK)
                result.status = static_cast<int>(status);
        } else {
            result.status = body.too_large ? RC_API_SERVER_RESPONSE_CLIENT_ERROR :
                                             RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
        }
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(handle);
    if (!options_ok || result.status == RC_API_SERVER_RESPONSE_CLIENT_ERROR ||
        result.status == RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR)
        result.body = body.too_large ? "RetroAchievements response too large" :
                                       "RetroAchievements HTTPS request failed";
#else
    result.body = "HTTPS transport unavailable on this platform";
#endif
    return result;
}

} // namespace dk64_ra
