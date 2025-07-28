#pragma once

#include "fl/string.h"
#include "fl/function.h"

// Forward declaration - fl::response is defined in fl/fetch.h
namespace fl {
    class response;
}

namespace fl {

/// Forward declaration for WASM fetch request
class WasmFetchRequest;

/// Simple fetch response callback type (now uses unified fl::response)
using FetchResponseCallback = fl::function<void(const response&)>;

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
