#pragma once

#include "fl/task/promise.h"
#include "fl/net/http/fetch.h"  // Includes response class  // IWYU pragma: keep
#include "fl/stl/string.h"
#include "fl/stl/url.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "platforms/is_platform.h"

// Forward declaration for hostent (platform-specific type)
struct hostent;

namespace fl {
namespace net {
namespace http {

namespace detail {
/// The HTTP/1.1 request FetchRequest sends: `options.method` (GET when
/// empty) on the request line, Host, the caller's headers, Content-Length for
/// a non-empty body, Connection: close, then the body. The caller's Host,
/// Connection, Content-Length and Transfer-Encoding are dropped: FetchRequest
/// owns those, and a second copy would duplicate or contradict the framing.
fl::string build_http_request(const RequestOptions& options, const fl::string& path,
                              const fl::string& host) FL_NO_EXCEPT;
}  // namespace detail

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
    FetchRequest(const fl::string& url, const FetchOptions& opts, fl::task::Promise<Response> promise) FL_NO_EXCEPT;

    /// @brief Destructor - closes socket if still open
    ~FetchRequest() FL_NO_EXCEPT;

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
    RequestOptions mOptions;  // method, headers and body to send

    // Parsed URL
    fl::url mParsedUrl;
    fl::string mHostname;
    int mPort;
    fl::string mPath;

    // Socket state
#ifdef FL_IS_WIN
    // Winsock's SOCKET is UINT_PTR. Holding it in an int truncated the handle
    // on 64-bit Windows and made the `< 0` failure check always false.
    using SocketHandle = fl::uptr;
#else
    using SocketHandle = int;
#endif
    SocketHandle mSocketFd;
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
