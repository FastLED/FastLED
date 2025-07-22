#include "fl/fetch.h"
#include "fl/warn.h"

#ifdef __EMSCRIPTEN__
// Include WASM-specific implementation
#include "platforms/wasm/js_fetch.h"
#endif

namespace fl {

#ifdef __EMSCRIPTEN__
// ========== WASM Implementation using existing js_fetch.h ==========

// Use existing WASM fetch infrastructure
void fetch(const fl::string& url, const FetchCallback& callback) {
    // Use the existing WASM fetch implementation, convert response types
    wasm_fetch.get(url).response([callback](const wasm_response& wasm_resp) {
        // Convert wasm_response to simple response
        response simple_resp(wasm_resp.status(), wasm_resp.status_text());
        simple_resp.set_text(wasm_resp.text());
        
        // Call the callback
        callback(simple_resp);
    });
}

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

#else
// ========== Non-WASM platforms: Stub implementation ==========

void fetch(const fl::string& url, const FetchCallback& callback) {
    // Immediate error response for non-WASM platforms
    response error_response(501, "Not Implemented");
    error_response.set_text("HTTP fetch is only available in WASM/browser builds. Attempted URL: " + url);
    callback(error_response);
}

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

#endif // __EMSCRIPTEN__

// ========== Common Implementation (both WASM and non-WASM) ==========

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
    // Update all active promises first
    for (auto& promise : mActivePromises) {
        if (promise.valid()) {
            promise.update();
        }
    }
    
    // Then clean up completed/invalid promises in a separate pass
    cleanup_completed_promises();
}

fl::size FetchManager::active_requests() const {
    return mActivePromises.size();
}

void FetchManager::cleanup_completed_promises() {
    // Rebuild vector without completed promises
    fl::vector<fl::promise<Response>> active_promises;
    for (const auto& promise : mActivePromises) {
        if (promise.valid() && !promise.is_completed()) {
            active_promises.push_back(promise);
        }
    }
    mActivePromises = fl::move(active_promises);
}

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
