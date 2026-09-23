#include "http_transport.h"

#include <cstdlib>
#include <cstdio>
#include <thread>

struct State {
    bool complete = false;
    int status = 0;
    std::size_t body_size = 0;
    std::thread::id callback_thread;
};

static void completed(const rc_api_server_response_t* response, void* userdata) {
    auto* state = static_cast<State*>(userdata);
    state->complete = true;
    state->status = response->http_status_code;
    state->body_size = response->body_length;
    state->callback_thread = std::this_thread::get_id();
}

int main() {
    dk64_ra::HttpTransport transport;
    State state;
    rc_api_request_t request{};
    request.url = "http://retroachievements.org/dorequest.php";
    request.post_data = "p=not-a-credential";
    request.content_type = "application/x-www-form-urlencoded";
    transport.enqueue(&request, completed, &state);

    if (!state.complete || state.status != RC_API_SERVER_RESPONSE_CLIENT_ERROR ||
        state.callback_thread != std::this_thread::get_id()) {
        std::fputs("Insecure URL was not rejected on the calling thread\n", stderr);
        return 1;
    }
    state = {};
    request.url = "https://retroachievements.org.evil.invalid/dorequest.php";
    transport.enqueue(&request, completed, &state);
    if (!state.complete || state.status != RC_API_SERVER_RESPONSE_CLIENT_ERROR ||
        state.callback_thread != std::this_thread::get_id()) {
        std::fputs("Lookalike host was not rejected\n", stderr);
        return 1;
    }
    state = {};
    request.url = "https://retroachievements.org/dorequest.php#fragment";
    transport.enqueue(&request, completed, &state);
    if (!state.complete || state.status != RC_API_SERVER_RESPONSE_CLIENT_ERROR) return 1;
    state = {};
    request.url = "https://retroachievements.org/dorequest.php";
    request.content_type = "application/json";
    transport.enqueue(&request, completed, &state);
    if (!state.complete || state.status != RC_API_SERVER_RESPONSE_CLIENT_ERROR) return 1;
    request.content_type = "application/x-www-form-urlencoded";
    std::puts("HTTP transport rejects plaintext, lookalike hosts, fragments and non-form POSTs");

    // Manual opt-in smoke test, never run by CTest or require a user account.
    // This GET sends no credentials and exercises the platform HTTPS transport.
    if (std::getenv("DK64_RA_LIVE_CHECK")) {
        state = {};
        request.url = "https://retroachievements.org/dorequest.php";
        request.post_data = nullptr;
        request.content_type = nullptr;
        transport.enqueue(&request, completed, &state);
        // 422 is the API's expected missing-parameters response to this empty GET.
        if (!state.complete || state.status != 422 || state.body_size == 0) {
            std::fprintf(stderr, "Unauthenticated RA API GET failed: HTTP %d, %zu bytes\n",
                         state.status, state.body_size);
            return 1;
        }
        std::puts("Unauthenticated RA API reached over HTTPS (expected HTTP 422)");
    }
    return 0;
}
