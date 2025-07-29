#pragma once

/// @file fetch.h
/// @brief Unified HTTP fetch API for FastLED (cross-platform)
///
/// This API provides both simple callback-based and JavaScript-like promise-based interfaces
/// for HTTP requests. Works on WASM/browser platforms with real fetch, provides stubs on embedded.
///
/// **WASM Optimization:** On WASM platforms, `delay()` automatically pumps all async tasks 
/// (fetch, timers, etc.) in 1ms intervals, making delay time useful for processing async operations.
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
///         .then([](const fl::response& resp) {
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
///     // Fetch promises are automatically updated through FastLED's engine events!
///     // On WASM platforms, delay() also pumps all async tasks automatically.
///     // No manual updates needed - just use normal FastLED loop
///     FastLED.show();
///     delay(16); // delay() automatically pumps all async tasks on WASM
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
#include "fl/ptr.h"
#include "fl/async.h"
#include "fl/mutex.h"
#include "fl/warn.h"
#include "fl/json.h"  // Add JSON support for response.json() method

namespace fl {

// Forward declarations
class fetch_options;
class FetchManager;

#ifdef __EMSCRIPTEN__
// Forward declarations for WASM-specific types (defined in platforms/wasm/js_fetch.h)
class WasmFetchRequest;
class WasmFetch;
using FetchResponseCallback = fl::function<void(const response&)>;
extern WasmFetch wasm_fetch;
#endif

/// HTTP response class (unified interface)
class response {
public:
    response() : mStatusCode(200), mStatusText("OK") {}
    response(int status_code) : mStatusCode(status_code), mStatusText(get_default_status_text(status_code)) {}
    response(int status_code, const fl::string& status_text) 
        : mStatusCode(status_code), mStatusText(status_text) {}
    
    /// HTTP status code (like JavaScript response.status)
    int status() const { return mStatusCode; }
    
    /// HTTP status text (like JavaScript response.statusText)
    const fl::string& status_text() const { return mStatusText; }
    
    /// Check if response is successful (like JavaScript response.ok)
    bool ok() const { return mStatusCode >= 200 && mStatusCode < 300; }
    
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
    
    /// Response body parsed as JSON (JavaScript-like API)
    /// @return fl::Json object for safe, ergonomic access
    /// @note Automatically parses JSON on first call, caches result
    /// @note Returns null JSON object for non-JSON or malformed content
    fl::Json json() const;
    
    /// Check if response appears to contain JSON content
    /// @return true if Content-Type header indicates JSON or body contains JSON markers
    bool is_json() const {
        auto content_type = get_content_type();
        if (content_type.has_value()) {
            fl::string ct = *content_type;
            // Check for various JSON content types (case-insensitive)
            return ct.find("json") != fl::string::npos;
        }
        return false;
    }
    
    /// Set methods (internal use)
    void set_status(int status_code) { mStatusCode = status_code; }
    void set_status_text(const fl::string& status_text) { mStatusText = status_text; }
    void set_text(const fl::string& body) { mBody = body; }  // Backward compatibility
    void set_body(const fl::string& body) { mBody = body; }
    void set_header(const fl::string& name, const fl::string& value) {
        mHeaders[name] = value;
    }

private:
    int mStatusCode;
    fl::string mStatusText;
    fl::string mBody;
    fl_map<fl::string, fl::string> mHeaders;
    
    // JSON parsing cache
    mutable fl::optional<fl::Json> mCachedJson;  // Lazy-loaded JSON cache
    mutable bool mJsonParsed = false;            // Track parsing attempts
    
    /// Parse JSON from response body with error handling
    fl::Json parse_json_body() const {
        fl::Json parsed = fl::Json::parse(mBody);
        if (parsed.is_null() && (!mBody.empty())) {
            // If parsing failed but we have content, return null JSON
            // This allows safe chaining: resp.json()["key"] | default
            return fl::Json(nullptr);
        }
        return parsed;
    }
    
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

/// Fetch options builder (fluent interface)
class fetch_options {
public:
    explicit fetch_options(const fl::string& url) : mUrl(url) {}
    fetch_options(const fl::string& url, const RequestOptions& options) 
        : mUrl(url), mOptions(options) {}
    
    /// Set HTTP method
    fetch_options& method(const fl::string& http_method) {
        mOptions.method = http_method;
        return *this;
    }
    
    /// Add header
    fetch_options& header(const fl::string& name, const fl::string& value) {
        mOptions.headers[name] = value;
        return *this;
    }
    
    /// Set request body
    fetch_options& body(const fl::string& data) {
        mOptions.body = data;
        return *this;
    }
    
    /// Set JSON body with proper content type
    fetch_options& json(const fl::string& json_data) {
        mOptions.body = json_data;
        mOptions.headers["Content-Type"] = "application/json";
        return *this;
    }
    
    /// Set timeout in milliseconds
    fetch_options& timeout(int timeout_ms) {
        mOptions.timeout_ms = timeout_ms;
        return *this;
    }
    
    /// Get the URL for this request
    const fl::string& url() const { return mUrl; }
    
    /// Get the options for this request  
    const RequestOptions& options() const { return mOptions; }

private:
    fl::string mUrl;
    RequestOptions mOptions;
    
    friend class FetchManager;
};

class FetchEngineListener;

/// Internal fetch manager for promise tracking
class FetchManager : public async_runner {
public:
    static FetchManager& instance();
    
    void register_promise(const fl::promise<response>& promise);
    
    // async_runner interface
    void update() override;
    bool has_active_tasks() const override;
    size_t active_task_count() const override;
    
    // Legacy API
    fl::size active_requests() const;
    void cleanup_completed_promises();
    
private:
    fl::vector<fl::promise<response>> mActivePromises;
    fl::unique_ptr<FetchEngineListener> mEngineListener;
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
fl::promise<response> fetch_get(const fl::string& url, const fetch_options& request = fetch_options(""));

/// HTTP POST request
fl::promise<response> fetch_post(const fl::string& url, const fetch_options& request = fetch_options(""));

/// HTTP PUT request
fl::promise<response> fetch_put(const fl::string& url, const fetch_options& request = fetch_options(""));

/// HTTP DELETE request
fl::promise<response> fetch_delete(const fl::string& url, const fetch_options& request = fetch_options(""));

/// HTTP HEAD request
fl::promise<response> fetch_head(const fl::string& url, const fetch_options& request = fetch_options(""));

/// HTTP OPTIONS request
fl::promise<response> fetch_http_options(const fl::string& url, const fetch_options& request = fetch_options(""));

/// HTTP PATCH request
fl::promise<response> fetch_patch(const fl::string& url, const fetch_options& request = fetch_options(""));

/// Generic request with options (like fetch(url, options))
fl::promise<response> fetch_request(const fl::string& url, const RequestOptions& options = RequestOptions());

/// Legacy manual update for fetch promises (use fl::async_run() for new code)
/// @deprecated Use fl::async_run() instead - this calls async_run() internally
void fetch_update();

/// Get number of active requests
fl::size fetch_active_requests();

/// Internal helper to execute a fetch request and return a promise
fl::promise<response> execute_fetch_request(const fl::string& url, const fetch_options& request);

} // namespace fl 
