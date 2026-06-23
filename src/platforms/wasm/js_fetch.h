#pragma once

// IWYU pragma: private

#include "fl/stl/string.h"
#include "fl/stl/function.h"
#include "fl/stl/noexcept.h"

// Forward declaration - fl::net::http::Response is defined in fl/net/http/fetch.h
namespace fl {
namespace net {
namespace http {
    class Response;
}
}
    using FetchResponseCallback = fl::function<void(const net::http::Response&)>;
}

namespace fl {

/// Forward declaration for WASM fetch request
class WasmFetchRequest;

/// WASM fetch request object for fluent API
class WasmFetchRequest {
private:
    fl::string mUrl;
    
public:
    explicit WasmFetchRequest(const fl::string& url) FL_NOEXCEPT : mUrl(url) {}
    
    /// Execute the fetch request and call the response callback
    /// @param callback Function to call with the response object
    void response(const FetchResponseCallback& callback) FL_NOEXCEPT;
};

/// Internal WASM fetch object (renamed to avoid conflicts)
class WasmFetch {
public:
    /// Create a GET request
    /// @param url The URL to fetch
    /// @returns WasmFetchRequest object for chaining
    WasmFetchRequest get(const fl::string& url) FL_NOEXCEPT {
        return WasmFetchRequest(url);
    }
};

/// Internal WASM fetch object for low-level access
extern WasmFetch wasm_fetch;

} // namespace fl 
