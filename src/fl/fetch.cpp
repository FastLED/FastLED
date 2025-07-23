#include "fl/fetch.h"
#include "fl/warn.h"
#include "fl/str.h"
#include "fl/mutex.h"
#include "fl/singleton.h"
#include "fl/engine_events.h"
#include "fl/async.h"
#include "fl/hash_map.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/val.h>
#endif

namespace fl {

#ifdef __EMSCRIPTEN__
// ========== WASM Implementation using JavaScript fetch ==========

// Include WASM-specific implementation
#include "platforms/wasm/js_fetch.h"

// Promise storage moved to FetchManager singleton

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

// Internal helper to execute a fetch request and return a promise
fl::promise<response> execute_fetch_request(const fl::string& url, const fetch_options& request) {
    // Create a promise for this request
    auto promise = fl::promise<response>::create();
    
    // Register with fetch manager to ensure it's tracked
    FetchManager::instance().register_promise(promise);
    
    // Get the actual URL to use (use request URL if provided, otherwise use parameter URL)
    fl::string fetch_url = request.url().empty() ? url : request.url();
    
    // Convert our request to the existing WASM fetch system
    auto wasm_request = WasmFetchRequest(fetch_url);
    
    // Generate a unique ID and store the promise safely using the singleton
    fl::string request_id = FetchManager::instance().register_pending_promise(promise);
    
    // Use lambda that captures only the request ID, not the promise
    wasm_request.response([request_id](const wasm_response& wasm_resp) {
        // Retrieve the promise from storage
        fl::promise<response> stored_promise = FetchManager::instance().retrieve_pending_promise(request_id);
        if (!stored_promise.valid()) {
            // Promise not found - this shouldn't happen but handle gracefully
            return;
        }
        
        // Convert WASM response to our response type
        response response(wasm_resp.status(), wasm_resp.status_text());
        response.set_body(wasm_resp.text());
        
        // Copy headers if available
        if (auto content_type = wasm_resp.content_type()) {
            response.set_header("content-type", *content_type);
        }
        
        // Complete the promise
        stored_promise.complete_with_value(response);
    });
    
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

// Internal helper to execute a fetch request and return a promise
fl::promise<response> execute_fetch_request(const fl::string& url, const fetch_options& request) {
    (void)request; // Unused in stub implementation
    FL_WARN("HTTP fetch is not supported on non-WASM platforms. URL: " << url);
    
    // Create error response
    response error_response(501, "Not Implemented");
    error_response.set_body("HTTP fetch is only available in WASM/browser builds. This platform does not support network requests.");
    
    // Create resolved promise with error response
    auto promise = fl::promise<response>::resolve(error_response);
    
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

void FetchManager::register_promise(const fl::promise<response>& promise) {
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
    fl::vector<fl::promise<response>> active_promises;
    for (const auto& promise : mActivePromises) {
        if (promise.valid() && !promise.is_completed()) {
            active_promises.push_back(promise);
        }
    }
    mActivePromises = fl::move(active_promises);
}

fl::string FetchManager::register_pending_promise(const fl::promise<response>& promise) {
    fl::lock_guard<fl::mutex> lock(mPromisesMutex);
    fl::string request_id = fl::string("req_");
    request_id.append(mNextRequestId++);
    mPendingPromises[request_id] = promise;
    return request_id;
}

fl::promise<response> FetchManager::retrieve_pending_promise(const fl::string& request_id) {
    fl::lock_guard<fl::mutex> lock(mPromisesMutex);
    auto it = mPendingPromises.find(request_id);
    if (it != mPendingPromises.end()) {
        fl::promise<response> stored_promise = it->second;
        mPendingPromises.erase(it);
        return stored_promise;
    }
    // Return an invalid promise if not found
    return fl::promise<response>();
}

// ========== Public API Functions ==========

fl::promise<response> fetch_get(const fl::string& url, const fetch_options& request) {
    // Create a new request with GET method
    fetch_options get_request(url, RequestOptions("GET"));
    
    // Apply any additional options from the provided request
    const auto& opts = request.options();
    get_request.timeout(opts.timeout_ms);
    for (const auto& header : opts.headers) {
        get_request.header(header.first, header.second);
    }
    if (!opts.body.empty()) {
        get_request.body(opts.body);
    }
    
    return execute_fetch_request(url, get_request);
}

fl::promise<response> fetch_post(const fl::string& url, const fetch_options& request) {
    // Create a new request with POST method
    fetch_options post_request(url, RequestOptions("POST"));
    
    // Apply any additional options from the provided request
    const auto& opts = request.options();
    post_request.timeout(opts.timeout_ms);
    for (const auto& header : opts.headers) {
        post_request.header(header.first, header.second);
    }
    if (!opts.body.empty()) {
        post_request.body(opts.body);
    }
    
    return execute_fetch_request(url, post_request);
}

fl::promise<response> fetch_put(const fl::string& url, const fetch_options& request) {
    // Create a new request with PUT method
    fetch_options put_request(url, RequestOptions("PUT"));
    
    // Apply any additional options from the provided request
    const auto& opts = request.options();
    put_request.timeout(opts.timeout_ms);
    for (const auto& header : opts.headers) {
        put_request.header(header.first, header.second);
    }
    if (!opts.body.empty()) {
        put_request.body(opts.body);
    }
    
    return execute_fetch_request(url, put_request);
}

fl::promise<response> fetch_delete(const fl::string& url, const fetch_options& request) {
    // Create a new request with DELETE method
    fetch_options delete_request(url, RequestOptions("DELETE"));
    
    // Apply any additional options from the provided request
    const auto& opts = request.options();
    delete_request.timeout(opts.timeout_ms);
    for (const auto& header : opts.headers) {
        delete_request.header(header.first, header.second);
    }
    if (!opts.body.empty()) {
        delete_request.body(opts.body);
    }
    
    return execute_fetch_request(url, delete_request);
}

fl::promise<response> fetch_head(const fl::string& url, const fetch_options& request) {
    // Create a new request with HEAD method
    fetch_options head_request(url, RequestOptions("HEAD"));
    
    // Apply any additional options from the provided request
    const auto& opts = request.options();
    head_request.timeout(opts.timeout_ms);
    for (const auto& header : opts.headers) {
        head_request.header(header.first, header.second);
    }
    if (!opts.body.empty()) {
        head_request.body(opts.body);
    }
    
    return execute_fetch_request(url, head_request);
}

fl::promise<response> fetch_http_options(const fl::string& url, const fetch_options& request) {
    // Create a new request with OPTIONS method
    fetch_options options_request(url, RequestOptions("OPTIONS"));
    
    // Apply any additional options from the provided request
    const auto& opts = request.options();
    options_request.timeout(opts.timeout_ms);
    for (const auto& header : opts.headers) {
        options_request.header(header.first, header.second);
    }
    if (!opts.body.empty()) {
        options_request.body(opts.body);
    }
    
    return execute_fetch_request(url, options_request);
}

fl::promise<response> fetch_patch(const fl::string& url, const fetch_options& request) {
    // Create a new request with PATCH method
    fetch_options patch_request(url, RequestOptions("PATCH"));
    
    // Apply any additional options from the provided request
    const auto& opts = request.options();
    patch_request.timeout(opts.timeout_ms);
    for (const auto& header : opts.headers) {
        patch_request.header(header.first, header.second);
    }
    if (!opts.body.empty()) {
        patch_request.body(opts.body);
    }
    
    return execute_fetch_request(url, patch_request);
}

fl::promise<response> fetch_request(const fl::string& url, const RequestOptions& options) {
    // Create a fetch_options with the provided options
    fetch_options request(url, options);
    
    // Use the helper function to execute the request
    return execute_fetch_request(url, request);
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
