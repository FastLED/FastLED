#include "js_fetch.h"
#include "fl/warn.h"
#include "fl/str.h"
#include "fl/function.h"
#include "fl/hash_map.h"
#include "fl/mutex.h"
#include "fl/singleton.h"
#include "fl/optional.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/val.h>
#endif

namespace fl {

WasmFetch wasm_fetch;

#ifdef __EMSCRIPTEN__
// WASM version using JavaScript fetch API

// Internal singleton class for managing WASM fetch callbacks
class WasmFetchCallbackManager {
public:
    WasmFetchCallbackManager() : mNextRequestId(1) {}
    
    // Generate unique request ID
    uint32_t generateRequestId() {
        fl::lock_guard<fl::mutex> lock(mCallbacksMutex);
        return mNextRequestId++;
    }
    
    // Store callback for a request ID (using move semantics)
    void storeCallback(uint32_t request_id, FetchResponseCallback callback) {
        fl::lock_guard<fl::mutex> lock(mCallbacksMutex);
        mPendingCallbacks[request_id] = fl::move(callback);
    }
    
    // Retrieve and remove callback for a request ID (using move semantics)
    fl::optional<FetchResponseCallback> takeCallback(uint32_t request_id) {
        fl::lock_guard<fl::mutex> lock(mCallbacksMutex);
        auto it = mPendingCallbacks.find(request_id);
        if (it != mPendingCallbacks.end()) {
            // Move the callback directly from the map entry to avoid double-move
            fl::optional<FetchResponseCallback> result = fl::make_optional(fl::move(it->second));
            mPendingCallbacks.erase(it); // Use efficient iterator-based erase
            return result;
        }
        return fl::nullopt;
    }

private:
    // Thread-safe storage for pending callbacks using request IDs
    fl::hash_map<uint32_t, FetchResponseCallback, fl::Hash<uint32_t>, fl::EqualTo<uint32_t>> mPendingCallbacks;
    fl::mutex mCallbacksMutex;
    uint32_t mNextRequestId;
};

// Get singleton instance
static WasmFetchCallbackManager& getCallbackManager() {
    return fl::Singleton<WasmFetchCallbackManager>::instance();
}

// C++ callback function that JavaScript can call when fetch completes
extern "C" EMSCRIPTEN_KEEPALIVE void js_fetch_success_callback(uint32_t request_id, const char* content) {
    // NOTE: Removed FL_WARN logging to prevent stack overflow during print operations
    
    auto callback_opt = getCallbackManager().takeCallback(request_id);
    if (callback_opt) {
        // Create a successful response object
        fl::wasm_response response(200, "OK");
        response.set_text(fl::string(content));
        response.set_header("content-type", "text/html"); // Default content type
        
        (*callback_opt)(response);
    }
    // NOTE: Removed warning logging to prevent stack overflow
}

// C++ error callback function that JavaScript can call when fetch fails
extern "C" EMSCRIPTEN_KEEPALIVE void js_fetch_error_callback(uint32_t request_id, const char* error_message) {
    // NOTE: Removed FL_WARN logging to prevent stack overflow during print operations
    
    auto callback_opt = getCallbackManager().takeCallback(request_id);
    if (callback_opt) {
        // Create an error response object
        fl::wasm_response response(0, "Network Error");
        fl::string error_content = "Fetch Error: ";
        error_content += error_message;
        response.set_text(error_content);
        
        (*callback_opt)(response);
    }
    // NOTE: Removed warning logging to prevent stack overflow
}

// JavaScript function that performs the actual fetch using async await pattern
EM_JS(void, js_fetch_async, (uint32_t request_id, const char* url), {
    var urlString = UTF8ToString(url);
    console.log('🌐 JavaScript fetch starting for request', request_id, 'URL:', urlString);
    
    // Define async callback pattern for fetch
    globalThis.FastLED_fetch_callback = globalThis.FastLED_fetch_callback || async function(request_id, url) {
        try {
            const response = await fetch(url);
            console.log('🌐 Fetch response received for request', request_id, 'status:', response.status);
            
            if (!response.ok) {
                throw new Error('HTTP ' + response.status + ': ' + response.statusText);
            }
            
            const text = await response.text();
            console.log('🌐 Fetch text received for request', request_id, 'length:', text.length);
            
            // Call back into C++ success callback with request ID
            Module._js_fetch_success_callback(request_id, stringToUTF8OnStack(text));
        } catch (error) {
            console.error('🌐 Fetch error for request', request_id, ':', error.message);
            // Call back into C++ error callback with request ID  
            Module._js_fetch_error_callback(request_id, stringToUTF8OnStack(error.message));
        }
    };
    
    // Execute with Asyncify handling to fix RuntimeError: unreachable
    return Asyncify.handleAsync(async () => {
        await globalThis.FastLED_fetch_callback(request_id, urlString);
    });
});

void WasmFetchRequest::response(const FetchResponseCallback& callback) {
    // NOTE: Removed FL_WARN logging to prevent stack overflow during fetch operations
    
    // Generate unique request ID for this request
    uint32_t request_id = getCallbackManager().generateRequestId();
    
    // Store the callback for when JavaScript calls back (using move semantics)
    getCallbackManager().storeCallback(request_id, FetchResponseCallback(callback));
    
    // NOTE: Removed FL_WARN logging to prevent stack overflow during fetch operations
    
    // Start the JavaScript fetch (non-blocking) with request ID
    js_fetch_async(request_id, mUrl.c_str());
}

#else
// Non-WASM platforms: HTTP fetch is not supported

void WasmFetchRequest::response(const FetchResponseCallback& callback) {
    FL_WARN("HTTP fetch is not supported on non-WASM platforms (Arduino/embedded). URL: " << mUrl);
    
    // Return immediate error response
    fl::wasm_response error_response(501, "Not Implemented");
    error_response.set_text("HTTP fetch is only available in WASM/browser builds. This platform does not support network requests.");
    error_response.set_header("content-type", "text/plain");
    
    // Immediately call the callback with error
    callback(error_response);
}

#endif // __EMSCRIPTEN__

} // namespace fl 
