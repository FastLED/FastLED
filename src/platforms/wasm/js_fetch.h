#pragma once

#include "fl/string.h"
#include "fl/function.h"
#include "fl/optional.h"
#include "fl/vector.h"
#include "fl/hash_map.h"

namespace fl {

/// Forward declaration for WASM fetch request
class WasmFetchRequest;

/// Simple HTTP response for WASM fetch API
class wasm_response {
public:
    /// Default constructor
    wasm_response() : mStatusCode(200), mStatusText("OK") {}
    
    /// Constructor with status code and status text
    wasm_response(int status_code, const fl::string& status_text) 
        : mStatusCode(status_code), mStatusText(status_text) {}
    
    /// Status code access
    int status() const { return mStatusCode; }
    void set_status(int status_code) { mStatusCode = status_code; }
    
    /// Status text access
    const fl::string& status_text() const { return mStatusText; }
    void set_status_text(const fl::string& status_text) { mStatusText = status_text; }
    
    /// Success check (2xx status codes)
    bool ok() const { return mStatusCode >= 200 && mStatusCode < 300; }
    
    /// Body content access
    const fl::string& text() const { return mBody; }
    void set_text(const fl::string& body) { mBody = body; }
    
    /// Headers access
    void set_header(const fl::string& name, const fl::string& value) {
        mHeaders[name] = value;
    }
    
    fl::optional<fl::string> get_header(const fl::string& name) const {
        auto it = mHeaders.find(name);
        if (it != mHeaders.end()) {
            return fl::make_optional(it->second);
        }
        return fl::nullopt;
    }
    
    /// Content type convenience
    fl::optional<fl::string> content_type() const {
        return get_header("content-type");
    }
    
private:
    int mStatusCode;
    fl::string mStatusText;
    fl::string mBody;
    fl::hash_map<fl::string, fl::string> mHeaders;
};

/// Simple fetch response callback type
using FetchResponseCallback = fl::function<void(const wasm_response&)>;

/// WASM fetch request object for fluent API
class WasmFetchRequest {
private:
    fl::string mUrl;
    
public:
    explicit WasmFetchRequest(const fl::string& url) : mUrl(url) {}
    
    /// Execute the fetch request and call the response callback
    /// @param callback Function to call with the response object
    void response(const FetchResponseCallback& callback);
};

/// Internal WASM fetch object (renamed to avoid conflicts)
class WasmFetch {
public:
    /// Create a GET request
    /// @param url The URL to fetch
    /// @returns WasmFetchRequest object for chaining
    WasmFetchRequest get(const fl::string& url) {
        return WasmFetchRequest(url);
    }
};

/// Internal WASM fetch object for low-level access
extern WasmFetch wasm_fetch;

} // namespace fl 
