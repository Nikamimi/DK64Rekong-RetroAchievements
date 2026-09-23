#pragma once

extern "C" {
#include "rc_client.h"
}

#include <string>

namespace dk64_ra {

// Synchronous WinHTTP/libcurl prototype: rc_client supports inline callbacks.
// This intentionally trades a brief game-thread pause for safe DLL lifetime
// during private development. A public release needs async I/O.
class HttpTransport {
public:
    void enqueue(const rc_api_request_t* request,
                 rc_client_server_callback_t callback, void* callback_data);

private:
    struct Request {
        std::string url;
        std::string post_data;
        std::string content_type;
        bool is_post = false;
    };
    struct Result {
        std::string body;
        int status = RC_API_SERVER_RESPONSE_CLIENT_ERROR;
    };

    static Result perform(Request& request);
};

} // namespace dk64_ra
