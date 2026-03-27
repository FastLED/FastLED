#pragma once

#include "fl/task/promise.h"
#include "fl/net/http/fetch.h"  // Includes response class  // IWYU pragma: keep
#include "fl/stl/string.h"
#include "fl/stl/url.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

// Forward declaration for hostent (platform-specific type)
struct hostent;

namespace fl {
namespace net {
namespace http {

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
    FetchRequest(const fl::string& url, const FetchOptions& opts, fl::task::Promise<Response> promise);

    /// @brief Destructor - closes socket if still open
    ~FetchRequest() FL_NOEXCEPT;

    /// @brief Pump the state machine (called by fl::task every update interval)
    ///
    /// Advances the request through its states. Safe to call repeatedly.
    /// Once done, subsequent calls are no-ops.
    void update();

    /// @brief Check if request is complete (success or failure)
    bool is_done() const { return mState == COMPLETED || mState == FAILED; }

    /// @brief Get current state
    State get_state() const { return mState; }

private:
    State mState;
    fl::task::Promise<Response> mPromise;

    // Parsed URL
    fl::url mParsedUrl;
    fl::string mHostname;
    int mPort;
    fl::string mPath;

    // Socket state
    int mSocketFd;
    ::hostent* mDnsResult;  // Use global namespace to avoid conflict

    // Send/receive buffers
    fl::string mRequestBuffer;
    fl::string mResponseBuffer;
    size_t mBytesSent;

    // Timeouts
    u32 mStateStartTime;

    // State handlers
    void handle_dns_lookup();
    void handle_connecting();
    void handle_sending();
    void handle_receiving();

    // Completion helpers
    void complete_success(const Response& resp);
    void complete_error(const char* message);

    // Utilities
    Response parse_http_response(const fl::string& raw);
    void close_socket();
};

} // namespace http
} // namespace net
} // namespace fl
