#pragma once

/// @file fetch.h
/// @brief Unified HTTP fetch API for FastLED (cross-platform)
///
/// This API provides both simple callback-based and JavaScript-like promise-based interfaces
/// for HTTP requests. Works on WASM/browser platforms with real fetch, provides stubs on embedded.
///
/// @section Simple Callback Usage
/// @code
/// #include "fl/fetch.h"
/// 
/// void setup() {
///     // Simple callback-based fetch (backward compatible)
///     fl::fetch("http://fastled.io", [](const fl::response& resp) {
///         if (resp.ok()) {
///             FL_WARN("Success: " << resp.text());
///         }
///     });
/// }
/// @endcode
///
/// @section Promise Usage 
/// @code
/// #include "fl/fetch.h"
/// 
/// void setup() {
///     // JavaScript-like fetch with promises
///     fl::fetch_get("http://fastled.io")
///         .then([](const fl::Response& resp) {
///             if (resp.ok()) {
///                 FL_WARN("Success: " << resp.text());
///             } else {
///                 FL_WARN("HTTP Error: " << resp.status() << " " << resp.status_text());
///             }
///         })
///         .catch_([](const fl::Error& err) {
///             FL_WARN("Fetch Error: " << err.message);
///         });
/// }
/// 
/// void loop() {
///     // Update all pending promises (non-blocking)
///     fl::fetch_update();
///     FastLED.show();
/// }
/// @endcode

#include "fl/namespace.h"
#include "fl/promise.h"
#include "fl/string.h"
#include "fl/vector.h"
#include "fl/map.h"
#include "fl/hash_map.h"
#include "fl/optional.h"
#include "fl/function.h"

namespace fl {

// Forward declarations
class FetchRequest;
class FetchManager;

#ifdef __EMSCRIPTEN__
// Forward declarations for WASM-specific types (defined in platforms/wasm/js_fetch.h)
class wasm_response;
class WasmFetchRequest;
class WasmFetch;
using FetchResponseCallback = fl::function<void(const wasm_response&)>;
extern WasmFetch wasm_fetch;
#endif

/// Simple HTTP response class (backward compatible interface)
class response {
public:
    response() : mStatusCode(200), mStatusText("OK") {}
    response(int status_code, const fl::string& status_text) 
        : mStatusCode(status_code), mStatusText(status_text) {}
    
    int status() const { return mStatusCode; }
    const fl::string& status_text() const { return mStatusText; }
    bool ok() const { return mStatusCode >= 200 && mStatusCode < 300; }
    const fl::string& text() const { return mBody; }
    void set_text(const fl::string& body) { mBody = body; }
    
    void set_status(int status_code) { mStatusCode = status_code; }
    void set_status_text(const fl::string& status_text) { mStatusText = status_text; }

private:
    int mStatusCode;
    fl::string mStatusText;
    fl::string mBody;
};

/// Enhanced HTTP response class (promise-based API)
class Response {
public:
    Response() : mStatus(200), mStatusText("OK") {}
    Response(int status) : mStatus(status), mStatusText(get_default_status_text(status)) {}
    Response(int status, const fl::string& status_text) : mStatus(status), mStatusText(status_text) {}
    
    /// HTTP status code (like JavaScript response.status)
    int status() const { return mStatus; }
    
    /// HTTP status text (like JavaScript response.statusText)
    const fl::string& status_text() const { return mStatusText; }
    
    /// Check if response is successful (like JavaScript response.ok)
    bool ok() const { return mStatus >= 200 && mStatus < 300; }
    
    /// Response body as text (like JavaScript response.text())
    const fl::string& text() const { return mBody; }
    
    /// Get header value (like JavaScript response.headers.get())
    fl::optional<fl::string> get_header(const fl::string& name) const {
        auto it = mHeaders.find(name);
        if (it != mHeaders.end()) {
            return fl::make_optional(it->second);
        }
        return fl::nullopt;
    }
    
    /// Get content type convenience method
    fl::optional<fl::string> get_content_type() const {
        return get_header("content-type");
    }
    
    /// Response body as text (alternative to text())
    const fl::string& get_body_text() const { return mBody; }
    
    /// Set methods (internal use)
    void set_status(int status) { mStatus = status; }
    void set_status_text(const fl::string& status_text) { mStatusText = status_text; }
    void set_body(const fl::string& body) { mBody = body; }
    void set_header(const fl::string& name, const fl::string& value) {
        mHeaders[name] = value;
    }

private:
    int mStatus;
    fl::string mStatusText;
    fl::string mBody;
    fl_map<fl::string, fl::string> mHeaders;
    
    static fl::string get_default_status_text(int status) {
        switch (status) {
            case 200: return "OK";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            default: return "Unknown";
        }
    }
};

/// Callback type for simple fetch responses (backward compatible)
using FetchCallback = fl::function<void(const response&)>;

/// Request options (matches JavaScript fetch RequestInit)
struct RequestOptions {
    fl::string method = "GET";
    fl_map<fl::string, fl::string> headers;
    fl::string body;
    int timeout_ms = 10000;  // 10 second default
    
    RequestOptions() = default;
    RequestOptions(const fl::string& method_name) : method(method_name) {}
};

/// Fetch request builder (fluent interface)
class FetchRequest {
public:
    explicit FetchRequest(const fl::string& url) : mUrl(url) {}
    FetchRequest(const fl::string& url, const RequestOptions& options) 
        : mUrl(url), mOptions(options) {}
    
    /// Set HTTP method
    FetchRequest& method(const fl::string& http_method) {
        mOptions.method = http_method;
        return *this;
    }
    
    /// Add header
    FetchRequest& header(const fl::string& name, const fl::string& value) {
        mOptions.headers[name] = value;
        return *this;
    }
    
    /// Set request body
    FetchRequest& body(const fl::string& data) {
        mOptions.body = data;
        return *this;
    }
    
    /// Set JSON body with proper content type
    FetchRequest& json(const fl::string& json_data) {
        mOptions.body = json_data;
        mOptions.headers["Content-Type"] = "application/json";
        return *this;
    }
    
    /// Set timeout in milliseconds
    FetchRequest& timeout(int timeout_ms) {
        mOptions.timeout_ms = timeout_ms;
        return *this;
    }
    
    /// Execute request and return promise (like JavaScript fetch())
    fl::promise<Response> then(fl::function<void(const Response&)> callback);
    fl::promise<Response> catch_(fl::function<void(const fl::Error&)> callback);

private:
    fl::string mUrl;
    RequestOptions mOptions;
    
    friend class FetchManager;
};

/// Internal fetch manager for promise tracking
class FetchManager {
public:
    static FetchManager& instance();
    
    void register_promise(const fl::promise<Response>& promise);
    void update();
    fl::size active_requests() const;
    void cleanup_completed_promises();

private:
    fl::vector<fl::promise<Response>> mActivePromises;
};

// ========== Simple Callback API (Backward Compatible) ==========

/// @brief Make an HTTP GET request (cross-platform, backward compatible)
/// @param url The URL to fetch
/// @param callback Function to call with the response
/// 
/// On WASM/browser platforms: Uses native JavaScript fetch() API
/// On Arduino/embedded platforms: Immediately calls callback with error response
void fetch(const fl::string& url, const FetchCallback& callback);

/// @brief Make an HTTP GET request with URL string literal (cross-platform)
/// @param url The URL to fetch (C-string)  
/// @param callback Function to call with the response
inline void fetch(const char* url, const FetchCallback& callback) {
    fetch(fl::string(url), callback);
}

// ========== Promise-Based API (JavaScript-like) ==========

/// HTTP GET request
FetchRequest fetch_get(const fl::string& url);

/// HTTP POST request
FetchRequest fetch_post(const fl::string& url);

/// HTTP PUT request
FetchRequest fetch_put(const fl::string& url);

/// HTTP DELETE request
FetchRequest fetch_delete(const fl::string& url);

/// HTTP HEAD request
FetchRequest fetch_head(const fl::string& url);

/// HTTP OPTIONS request
FetchRequest fetch_options(const fl::string& url);

/// HTTP PATCH request
FetchRequest fetch_patch(const fl::string& url);

/// Generic request with options (like fetch(url, options))
fl::promise<Response> fetch_request(const fl::string& url, const RequestOptions& options = RequestOptions());

/// Update all active promises (call in loop())
void fetch_update();

/// Get number of active requests
fl::size fetch_active_requests();

} // namespace fl 
