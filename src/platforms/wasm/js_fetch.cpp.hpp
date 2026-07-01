// IWYU pragma: private

/// @file js_fetch.cpp.hpp
/// @brief WASM HTTP fetch shim — async dispatch + thread-safe callback table.
///
/// Threading model (pthread back-end, `-pthread` + `-sPROXY_TO_PTHREAD`):
///
///   1. Multiple sketch coroutines (each on its own pthread) can call
///      `WasmFetchRequest::response()` concurrently. `mNextRequestId` and
///      `mPendingCallbacks` are protected by `mCallbacksMutex` — uncontended
///      access uses Atomics fast paths; contention is rare (callback
///      handoffs are O(microseconds)).
///   2. `js_fetch_async` is an `addToLibrary` import. With `-pthread`,
///      Emscripten auto-proxies non-thread-safe JS-library calls to the
///      browser main thread for execution, so the actual `fetch()` runs
///      where Web APIs require it.
///   3. The success/error callbacks are exported WASM functions invoked
///      from JS. They lock the same callback-map mutex briefly. After
///      committing Promise state via the user callback, they invoke
///      `ICoroutineRuntime::instance().wakeWaiters()` to unpark any
///      pthread blocked in `fl::platforms::await()`.
///
/// See issue #2452 for the JSPI -> pthread migration history.

#include "platforms/wasm/js_fetch.h"
#include "fl/net/http/fetch.h"  // Include for fl::net::http::Response definition
#include "fl/log/log.h"
#include "fl/stl/string.h"
#include "fl/stl/function.h"
#include "fl/stl/flat_map.h"
#include "fl/stl/mutex.h"
#include "fl/stl/singleton.h"
#include "fl/stl/optional.h"
#include "platforms/coroutine_runtime.h"

#include "platforms/wasm/is_wasm.h"
#ifdef FL_IS_WASM
// IWYU pragma: begin_keep
#include <emscripten.h>
#include <emscripten/val.h>
// IWYU pragma: end_keep

// Implemented in js_library.js (--js-library side file)
extern "C" void js_fetch_async(unsigned int request_id, const char* url);
#endif

namespace fl {

// FL_LINT_ALLOW_GLOBAL(WASM-only public global `fl::wasm_fetch` used from JS bindings — WASM target has no gc-sections elision benefit and this is a stable user-facing name)
WasmFetch wasm_fetch;

#ifdef FL_IS_WASM
// WASM version using JavaScript fetch API

// Internal singleton class for managing WASM fetch callbacks
class WasmFetchCallbackManager {
public:
    WasmFetchCallbackManager() : mNextRequestId(1) {}
    
    // Generate unique request ID
    u32 generateRequestId() {
        fl::unique_lock<fl::mutex> lock(mCallbacksMutex);
        return mNextRequestId++;
    }

    // Store callback for a request ID (using move semantics)
    void storeCallback(u32 request_id, FetchResponseCallback callback) {
        fl::unique_lock<fl::mutex> lock(mCallbacksMutex);
        mPendingCallbacks[request_id] = fl::move(callback);
    }

    // Retrieve and remove callback for a request ID (using move semantics)
    fl::optional<FetchResponseCallback> takeCallback(u32 request_id) {
        fl::unique_lock<fl::mutex> lock(mCallbacksMutex);
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
    fl::flat_map<u32, FetchResponseCallback> mPendingCallbacks;
    fl::mutex mCallbacksMutex;
    u32 mNextRequestId;
};

// Get singleton instance
static WasmFetchCallbackManager& getCallbackManager() {
    return fl::Singleton<WasmFetchCallbackManager>::instance();
}

// C++ callback function that JavaScript can call when fetch completes
extern "C" EMSCRIPTEN_KEEPALIVE void js_fetch_success_callback(u32 request_id, const char* content) {
    FL_WARN_F("Fetch success callback received for request %s, content length: %s", request_id, strlen(content));
    
    auto callback_opt = getCallbackManager().takeCallback(request_id);
    if (callback_opt) {
        // Create a successful response object using unified fl::response
        fl::net::http::Response response(200, "OK");
        response.set_body(fl::string(content));
        response.set_header("content-type", "text/html"); // Default content type

        (*callback_opt)(response);
    } else {
        FL_WARN_F("Warning: No pending callback found for fetch success request %s", request_id);
    }
    // Wake any pthread parked inside fl::platforms::await().
    fl::platforms::ICoroutineRuntime::instance().wakeWaiters();
}

// C++ error callback function that JavaScript can call when fetch fails
extern "C" EMSCRIPTEN_KEEPALIVE void js_fetch_error_callback(u32 request_id, const char* error_message) {
    FL_WARN_F("Fetch error callback received for request %s: %s", request_id, error_message);
    
    auto callback_opt = getCallbackManager().takeCallback(request_id);
    if (callback_opt) {
        // Create an error response object using unified fl::response
        fl::net::http::Response response(0, "Network Error");
        fl::string error_content = "Fetch Error: ";
        error_content += error_message;
        response.set_body(error_content);

        (*callback_opt)(response);
    } else {
        FL_WARN_F("Warning: No pending callback found for fetch error request %s", request_id);
    }
    // See note in js_fetch_success_callback above.
    fl::platforms::ICoroutineRuntime::instance().wakeWaiters();
}

void WasmFetchRequest::response(const FetchResponseCallback& callback) {
    FL_WARN_F("Starting JavaScript-based fetch request to: %s", mUrl);
    
    // Generate unique request ID for this request
    u32 request_id = getCallbackManager().generateRequestId();
    
    // Store the callback for when JavaScript calls back (using move semantics)
    getCallbackManager().storeCallback(request_id, FetchResponseCallback(callback));
    
    FL_WARN_F("Stored callback for request ID: %s", request_id);
    
    // Start the JavaScript fetch (non-blocking) with request ID
    js_fetch_async(request_id, mUrl.c_str());
}

#endif // FL_IS_WASM

} // namespace fl 
