#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/val.h>

#include "js_fetch.h"
#include "fl/warn.h"
#include "fl/str.h"
#include "fl/function.h"

namespace fl {

Fetch fetch;

// Global storage for pending callbacks
static FetchResponseCallback* g_pending_callback = nullptr;

// C++ callback function that JavaScript can call when fetch completes
extern "C" EMSCRIPTEN_KEEPALIVE void js_fetch_success_callback(const char* content) {
    FL_WARN("Fetch success callback received, content length: " << strlen(content));
    
    if (g_pending_callback) {
        fl::string response_content(content);
        (*g_pending_callback)(response_content);
        delete g_pending_callback;
        g_pending_callback = nullptr;
    } else {
        FL_WARN("Warning: No pending callback found for fetch success");
    }
}

// C++ error callback function that JavaScript can call when fetch fails
extern "C" EMSCRIPTEN_KEEPALIVE void js_fetch_error_callback(const char* error_message) {
    FL_WARN("Fetch error callback received: " << error_message);
    
    if (g_pending_callback) {
        fl::string error_content = "Fetch Error: ";
        error_content += error_message;
        (*g_pending_callback)(error_content);
        delete g_pending_callback;
        g_pending_callback = nullptr;
    } else {
        FL_WARN("Warning: No pending callback found for fetch error");
    }
}

// JavaScript function that performs the actual fetch and calls back to C++
EM_JS(void, js_fetch_async, (const char* url), {
    var urlString = UTF8ToString(url);
    console.log('🌐 JavaScript fetch starting for URL:', urlString);
    
    // Use native JavaScript fetch API
    fetch(urlString)
        .then(response => {
            console.log('🌐 Fetch response received, status:', response.status);
            if (!response.ok) {
                throw new Error('HTTP ' + response.status + ': ' + response.statusText);
            }
            return response.text();
        })
        .then(text => {
            console.log('🌐 Fetch text received, length:', text.length);
            // Call back into C++ success callback
            Module._js_fetch_success_callback(stringToUTF8OnStack(text));
        })
        .catch(error => {
            console.error('🌐 Fetch error:', error.message);
            // Call back into C++ error callback
            Module._js_fetch_error_callback(stringToUTF8OnStack(error.message));
        });
});

void FetchRequest::response(const FetchResponseCallback& callback) {
    FL_WARN("Starting JavaScript-based fetch request to: " << mUrl);
    
    // Clean up any existing callback
    if (g_pending_callback) {
        delete g_pending_callback;
    }
    
    // Store the callback for when JavaScript calls back
    g_pending_callback = new FetchResponseCallback(callback);
    
    // Start the JavaScript fetch (non-blocking)
    js_fetch_async(mUrl.c_str());
}

} // namespace fl

#endif // __EMSCRIPTEN__ 
