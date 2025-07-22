#pragma once

#include "fl/namespace.h"
#include "fl/string.h"
#include "fl/function.h"

#ifdef __EMSCRIPTEN__
// WASM platform: Use real JavaScript fetch implementation
#include "platforms/wasm/js_fetch.h"
namespace fl {
    // Use the response type from js_fetch.h and create alias for consistency
    using FetchCallback = fl::function<void(const response&)>;
}
#else
// Non-WASM platforms: Provide immediate error response

namespace fl {

// Simple response class for non-WASM platforms (minimal version)
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

private:
    int mStatusCode;
    fl::string mStatusText;
    fl::string mBody;
};

// Callback type for fetch responses
using FetchCallback = fl::function<void(const response&)>;

} // namespace fl

#endif // __EMSCRIPTEN__

namespace fl {

/// @brief Make an HTTP GET request (cross-platform)
/// @param url The URL to fetch
/// @param callback Function to call with the response
/// 
/// On WASM/browser platforms: Uses native JavaScript fetch() API
/// On Arduino/embedded platforms: Immediately calls callback with error response
inline void fetch(const fl::string& url, const FetchCallback& callback) {
#ifdef __EMSCRIPTEN__
    // Use the real WASM fetch implementation
    fl::fetch.get(url).response(callback);
#else
    // Immediate error response for non-WASM platforms
    fl::response error_response(501, "Not Implemented");
    error_response.set_text("HTTP fetch is only available in WASM/browser builds. Attempted URL: " + url);
    callback(error_response);
#endif
}

/// @brief Make an HTTP GET request with URL string literal (cross-platform)
/// @param url The URL to fetch (C-string)
/// @param callback Function to call with the response
inline void fetch(const char* url, const FetchCallback& callback) {
    fetch(fl::string(url), callback);
}

} // namespace fl 
