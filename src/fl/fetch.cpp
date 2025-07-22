#include "fl/fetch.h"
#include "fl/warn.h"
#include "fl/str.h"
#include "fl/mutex.h"
#include "fl/singleton.h"
#include "fl/engine_events.h"
#include "fl/async.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/val.h>
#endif

namespace fl {

#ifdef __EMSCRIPTEN__
// ========== WASM Implementation using JavaScript fetch ==========

// Include WASM-specific implementation
#include "platforms/wasm/js_fetch.h"

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
// ========== Embedded/Stub Implementation ==========

void fetch(const fl::string& url, const FetchCallback& callback) {
    (void)url; // Unused in stub implementation
    // For embedded platforms, immediately call callback with a "not supported" response
    response resp(501, "Not Implemented");
    resp.set_text("HTTP fetch not supported on this platform");
    callback(resp);
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

#endif

// ========== Engine Events Integration ==========

/// Internal engine listener for automatic async updates
class FetchEngineListener : public EngineEvents::Listener {
public:
    FetchEngineListener() = default;
    ~FetchEngineListener() override {
        // Listener base class automatically removes itself
    }
    
    void onEndFrame() override {
        // Update all async tasks (fetch, timers, etc.) at the end of each frame
        fl::asyncrun();
    }
};

// ========== Promise-Based API Implementation ==========

FetchManager& FetchManager::instance() {
    return fl::Singleton<FetchManager>::instance();
}

void FetchManager::register_promise(const fl::promise<Response>& promise) {
    // Auto-register with async system and engine listener on first promise
    if (mActivePromises.empty()) {
        AsyncManager::instance().register_runner(this);
        
        if (!mEngineListener) {
            mEngineListener = fl::make_unique<FetchEngineListener>();
            EngineEvents::addListener(mEngineListener.get());
        }
    }
    
    mActivePromises.push_back(promise);
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
    
    // Auto-unregister from async system when no more promises
    if (mActivePromises.empty()) {
        AsyncManager::instance().unregister_runner(this);
        
        if (mEngineListener) {
            EngineEvents::removeListener(mEngineListener.get());
            mEngineListener.reset();
        }
    }
}

bool FetchManager::has_active_tasks() const {
    return !mActivePromises.empty();
}

size_t FetchManager::active_task_count() const {
    return mActivePromises.size();
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
    (void)url; // TODO: Use url parameter when implementing actual HTTP request
    (void)options; // TODO: Use options parameter when implementing actual HTTP request
    
    auto promise = fl::promise<Response>::create();
    
    // Register the promise for automatic updates
    FetchManager::instance().register_promise(promise);
    
    // TODO: Implement actual HTTP request logic here
    // For now, complete with a stub response
    promise.complete_with_value(Response(501, "Not Implemented"));
    
    return promise;
}

void fetch_update() {
    // Legacy function - use fl::asyncrun() for new code
    // This provides backwards compatibility for existing code
    fl::asyncrun();
}

fl::size fetch_active_requests() {
    return FetchManager::instance().active_requests();
}

} // namespace fl 
