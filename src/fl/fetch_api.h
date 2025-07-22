#pragma once

/// @file fetch_api.h
/// @brief JavaScript-like fetch API for FastLED WASM builds
///
/// This API emulates the JavaScript fetch() pattern using fl::promise for ergonomic async handling.
/// It provides a simple, familiar interface for HTTP requests in WASM environments.
///
/// @section Basic Usage
/// @code
/// #include "fl/fetch_api.h"
/// 
/// void setup() {
///     // Simple GET request (matches JavaScript fetch pattern)
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
///
/// @section Advanced Usage
/// @code
/// // POST request with JSON payload (like JavaScript fetch)
/// fl::RequestOptions options;
/// options.method = "POST";
/// options.headers["Content-Type"] = "application/json";
/// options.body = R"({"name": "My Item", "quantity": 3})";
/// 
/// fl::fetch_request("https://api.example.com/items", options)
///     .then([](const fl::Response& resp) {
///         if (!resp.ok()) {
///             FL_WARN("Server error: " << resp.status());
///         } else {
///             FL_WARN("Created item: " << resp.text());
///         }
///     })
///     .catch_([](const fl::Error& err) {
///         FL_WARN("Network error: " << err.message);
///     });
/// @endcode

#include "fl/namespace.h"
#include "fl/promise.h"
#include "fl/string.h"
#include "fl/vector.h"
#include "fl/map.h"
#include "fl/optional.h"

namespace fl {

/// HTTP response class (matches JavaScript Response API)
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
