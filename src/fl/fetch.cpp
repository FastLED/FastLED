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

// Include WASM-specific implementation
#include "platforms/wasm/js_fetch.h"

namespace fl {

#ifdef __EMSCRIPTEN__
// ========== WASM Implementation using JavaScript fetch ==========



// Promise storage moved to FetchManager singleton

// Use existing WASM fetch infrastructure
void fetch(const fl::string& url, const FetchCallback& callback) {
    // Use the existing WASM fetch implementation - no conversion needed since both use fl::response
    wasm_fetch.get(url).response(callback);
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
    
    // Use lambda that captures the promise directly (shared_ptr is safe to copy)
    // Make the lambda mutable so we can call non-const methods on the captured promise
    wasm_request.response([promise](const response& resp) mutable {
        // Complete the promise directly - no need for double storage
        if (promise.valid()) {
            promise.complete_with_value(resp);
        }
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



// ========== Promise-Based API Implementation ==========

class FetchEngineListener : public EngineEvents::Listener {
public:
    FetchEngineListener() {
        EngineEvents::addListener(this);
    };
    ~FetchEngineListener() override {
        // Listener base class automatically removes itself
        EngineEvents::removeListener(this);
    }

    void onEndFrame() override {
        // Update all async tasks (fetch, timers, etc.) at the end of each frame
        fl::async_run();
    }
};

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

// WASM promise management methods removed - no longer needed
// Promises are now handled directly via shared_ptr capture in callbacks

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
    // Legacy function - use fl::async_run() for new code
    // This provides backwards compatibility for existing code
    fl::async_run();
}

fl::size fetch_active_requests() {
    return FetchManager::instance().active_requests();
}

// ========== Response Class Method Implementations ==========

fl::Json response::json() const {
    if (!mJsonParsed) {
        if (is_json() || mBody.find("{") != fl::string::npos || mBody.find("[") != fl::string::npos) {
            mCachedJson = parse_json_body();
        } else {
            FL_WARN("Response is not JSON: " << mBody);
            mCachedJson = fl::Json(nullptr);  // Not JSON content
        }
        mJsonParsed = true;
    }
    
    return mCachedJson.has_value() ? *mCachedJson : fl::Json(nullptr);
}

} // namespace fl 
