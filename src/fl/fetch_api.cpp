#include "fl/fetch_api.h"
#include "fl/warn.h"

#ifdef __EMSCRIPTEN__
// For WASM builds: Use the existing JavaScript fetch bridge
#include "platforms/wasm/js_fetch.h"
#endif

namespace fl {

#ifdef __EMSCRIPTEN__
// ========== WASM Implementation using existing JavaScript fetch ==========

fl::promise<Response> FetchRequest::then(fl::function<void(const Response&)> callback) {
    // Create a promise for this request
    auto promise = fl::promise<Response>::create();
    
    // Convert our request to the existing WASM fetch system
    auto wasm_request = WasmFetchRequest(mUrl);
    
    // Use the existing JavaScript fetch infrastructure
    wasm_request.response([promise, callback](const wasm_response& wasm_resp) mutable {
        // Convert WASM response to our Response type
        Response response(wasm_resp.status(), wasm_resp.status_text());
        response.set_body(wasm_resp.text());
        
        // Copy headers if available
        if (auto content_type = wasm_resp.content_type()) {
            response.set_header("content-type", *content_type);
        }
        
        // Complete the promise
        promise.complete_with_value(response);
        
        // Call the user callback if provided
        if (callback) {
            callback(response);
        }
    });
    
    // Register with fetch manager
    FetchManager::instance().register_promise(promise);
    
    return promise;
}

fl::promise<Response> FetchRequest::catch_(fl::function<void(const fl::Error&)> callback) {
    // Create a promise and execute the request
    auto promise = then([](const Response& resp) {
        // Success case is handled by then() - nothing to do here
        (void)resp;
    });
    
    // Add error handling
    promise.catch_(callback);
    
    return promise;
}

FetchManager& FetchManager::instance() {
    static FetchManager instance;
    return instance;
}

void FetchManager::register_promise(const fl::promise<Response>& promise) {
    if (promise.valid()) {
        mActivePromises.push_back(promise);
    }
}

void FetchManager::update() {
    // Update all active promises and clean up completed ones
    for (auto it = mActivePromises.begin(); it != mActivePromises.end(); ) {
        auto& promise = *it;
        if (promise.valid()) {
            promise.update();
            
            // Remove completed promises for automatic cleanup
            if (promise.is_completed()) {
                it = mActivePromises.erase(it);
            } else {
                ++it;
            }
        } else {
            // Remove invalid promises
            it = mActivePromises.erase(it);
        }
    }
}

fl::size FetchManager::active_requests() const {
    return mActivePromises.size();
}

void FetchManager::cleanup_completed_promises() {
    auto new_end = fl::remove_if(mActivePromises.begin(), mActivePromises.end(),
        [](const fl::promise<Response>& p) {
            return !p.valid() || p.is_completed();
        });
    mActivePromises.erase(new_end, mActivePromises.end());
}

#else
// ========== Non-WASM platforms: Stub implementation ==========

fl::promise<Response> FetchRequest::then(fl::function<void(const Response&)> callback) {
    FL_WARN("HTTP fetch is not supported on non-WASM platforms. URL: " << mUrl);
    
    // Create error response
    Response error_response(501, "Not Implemented");
    error_response.set_body("HTTP fetch is only available in WASM/browser builds. This platform does not support network requests.");
    
    // Create resolved promise with error response
    auto promise = fl::promise<Response>::resolve(error_response);
    
    // Call callback immediately if provided
    if (callback) {
        callback(error_response);
    }
    
    return promise;
}

fl::promise<Response> FetchRequest::catch_(fl::function<void(const fl::Error&)> callback) {
    // For non-WASM platforms, always return an error
    auto promise = fl::promise<Response>::reject(fl::Error("HTTP requests not supported on this platform"));
    
    if (callback) {
        callback(fl::Error("HTTP requests not supported on this platform"));
    }
    
    return promise;
}

FetchManager& FetchManager::instance() {
    static FetchManager instance;
    return instance;
}

void FetchManager::register_promise(const fl::promise<Response>& promise) {
    (void)promise;
    // Nothing to do on non-WASM platforms
}

void FetchManager::update() {
    // Nothing to do on non-WASM platforms
}

fl::size FetchManager::active_requests() const {
    return 0;
}

void FetchManager::cleanup_completed_promises() {
    // Nothing to do on non-WASM platforms
}

#endif // __EMSCRIPTEN__

// ========== Public API Functions ==========

FetchRequest fetch_get(const fl::string& url) {
    return FetchRequest(url, RequestOptions("GET"));
}

FetchRequest fetch_post(const fl::string& url) {
    return FetchRequest(url, RequestOptions("POST"));
}

FetchRequest fetch_put(const fl::string& url) {
    return FetchRequest(url, RequestOptions("PUT"));
}

FetchRequest fetch_delete(const fl::string& url) {
    return FetchRequest(url, RequestOptions("DELETE"));
}

FetchRequest fetch_head(const fl::string& url) {
    return FetchRequest(url, RequestOptions("HEAD"));
}

FetchRequest fetch_options(const fl::string& url) {
    return FetchRequest(url, RequestOptions("OPTIONS"));
}

FetchRequest fetch_patch(const fl::string& url) {
    return FetchRequest(url, RequestOptions("PATCH"));
}

fl::promise<Response> fetch_request(const fl::string& url, const RequestOptions& options) {
    FetchRequest req(url, options);
    return req.then([](const Response& resp) {
        // Default success handler - just complete the promise
        (void)resp;
    });
}

void fetch_update() {
    FetchManager::instance().update();
}

fl::size fetch_active_requests() {
    return FetchManager::instance().active_requests();
}

} // namespace fl 
