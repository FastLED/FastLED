#pragma once

#include "fl/promise.h"
#include "fl/net/fetch.h"  // Includes response class
#include "fl/stl/string.h"
#include "fl/int.h"

// Forward declaration for hostent (platform-specific type)
struct hostent;

namespace fl {
namespace net {

/// @brief Non-blocking HTTP request state machine
///
/// Handles a single HTTP request using incremental state updates.
/// Designed to be pumped by fl::task::every_ms() for true async operation.
///
/// State progression:
///   DNS_LOOKUP -> CONNECTING -> SENDING -> RECEIVING -> COMPLETED/FAILED
class FetchRequest {
public:
    enum State {
        DNS_LOOKUP,      ///< Resolving hostname (brief blocking ~10-100ms)
        CONNECTING,      ///< Waiting for socket connection (non-blocking)
        SENDING,         ///< Sending HTTP request (non-blocking)
        RECEIVING,       ///< Receiving HTTP response (non-blocking)
        COMPLETED,       ///< Successfully completed
        FAILED           ///< Error occurred
    };

    /// @brief Construct a new fetch request
    /// @param url URL to fetch
    /// @param opts Fetch options (method, headers, etc.)
    /// @param promise Promise to resolve when complete
    FetchRequest(const fl::string& url, const fetch_options& opts, fl::promise<response> promise);

    /// @brief Destructor - closes socket if still open
    ~FetchRequest();

    /// @brief Pump the state machine (called by fl::task every update interval)
    ///
    /// Advances the request through its states. Safe to call repeatedly.
    /// Once done, subsequent calls are no-ops.
    void update();

    /// @brief Check if request is complete (success or failure)
    bool is_done() const { return state == COMPLETED || state == FAILED; }

    /// @brief Get current state
    State get_state() const { return state; }

private:
    State state;
    fl::promise<response> promise;

    // Parsed URL components
    fl::string hostname;
    int port;
    fl::string path;
    fl::string protocol;

    // Socket state
    int socket_fd;
    ::hostent* dns_result;  // Use global namespace to avoid conflict with fl::net::hostent

    // Send/receive buffers
    fl::string request_buffer;
    fl::string response_buffer;
    size_t bytes_sent;

    // Timeouts
    u32 state_start_time;

    // State handlers
    void handle_dns_lookup();
    void handle_connecting();
    void handle_sending();
    void handle_receiving();

    // Completion helpers
    void complete_success(const response& resp);
    void complete_error(const char* message);

    // Utilities
    response parse_http_response(const fl::string& raw);
    void close_socket();
    bool parse_url(const fl::string& url);
};

} // namespace net
} // namespace fl
