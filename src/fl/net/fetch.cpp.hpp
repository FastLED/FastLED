#include "fl/net/fetch.h"
#include "fl/singleton.h"
#include "fl/engine_events.h"
#include "fl/async.h"
#include "fl/stl/unique_ptr.h"  // For make_unique
#include "fl/task.h"  // For fl::task::every_ms
#include "fl/scheduler.h"  // For fl::Scheduler

// IWYU pragma: begin_keep
#ifdef FL_IS_WASM
#include <emscripten.h>
#include <emscripten/val.h>
#endif
// IWYU pragma: end_keep

// Include WASM-specific implementation
// IWYU pragma: begin_keep
#include "platforms/wasm/js_fetch.h"  // ok platform headers
// IWYU pragma: end_keep

// Native networking includes
#if defined(FASTLED_HAS_NETWORKING) && !defined(FL_IS_WASM)
#include "fl/stl/cstdlib.h"  // for atoi
#include "fl/int.h"  // for u16

#ifdef FL_IS_WIN
    #include "platforms/win/socket_win.h"  // ok platform headers  // IWYU pragma: keep
#else
    #include "platforms/posix/socket_posix.h"  // ok platform headers  // IWYU pragma: keep
    #include <fcntl.h>  // ok platform headers (for O_NONBLOCK flag)  // IWYU pragma: keep
#endif
#endif

namespace fl {

#ifdef FL_IS_WASM
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



#elif defined(FASTLED_HAS_NETWORKING)
// ========== Native Socket Implementation (POSIX/Windows) ==========

#ifdef FL_IS_WIN
    #define SOCKET_ERROR_WOULD_BLOCK WSAEWOULDBLOCK
    #define SOCKET_ERROR_IN_PROGRESS WSAEINPROGRESS
#else
    #define SOCKET_ERROR_WOULD_BLOCK EWOULDBLOCK
    #define SOCKET_ERROR_IN_PROGRESS EINPROGRESS
#endif

namespace {

// Old blocking implementation removed - now using FetchRequest with fl::task

/*
// Helper: Perform synchronous HTTP request (DEPRECATED - now async with fl::task)
response perform_http_request(const fl::string& url, const fetch_options& request) {
    ParsedURL parsed = parse_url(url);

    if (!parsed.valid) {
        response resp(400, "Bad Request");
        resp.set_body("Invalid URL");
        return resp;
    }

    if (parsed.protocol == "https") {
        response resp(501, "Not Implemented");
        resp.set_body("HTTPS not supported on this platform");
        return resp;
    }

    // Create socket (initialization handled by socket wrapper)
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        response resp(500, "Internal Server Error");
        resp.set_body("Failed to create socket");
        return resp;
    }

    // Resolve hostname
    FL_WARN("[FETCH] Resolving hostname: " << parsed.host);
    struct hostent* server = gethostbyname(parsed.host.c_str());
    if (server == nullptr) {
        close(sock);
        response resp(500, "Internal Server Error");
        resp.set_body("Failed to resolve hostname");
        return resp;
    }

    // Set socket to non-blocking mode (allows async integration)
#ifdef FL_IS_WIN
    u_long mode = 1;  // 1 = non-blocking
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    // Connect to server
    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<u16>(parsed.port));
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, static_cast<size_t>(server->h_length));

    // Non-blocking connect may return EINPROGRESS/EWOULDBLOCK immediately
    int conn_result = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (conn_result < 0) {
#ifdef FL_IS_WIN
        int err = WSAGetLastError();
#else
        int err = errno;
#endif
        if (err != SOCKET_ERROR_IN_PROGRESS && err != SOCKET_ERROR_WOULD_BLOCK) {
            close(sock);
            response resp(500, "Internal Server Error");
            resp.set_body("Failed to connect to server");
            return resp;
        }
        // Connection in progress, wait for it to complete
        fd_set write_fds;
        struct timeval timeout;
        timeout.tv_sec = 5;  // 5 second timeout
        timeout.tv_usec = 0;

        FD_ZERO(&write_fds);
        FD_SET(sock, &write_fds);

        // Wait for connection with async pumping
        FL_WARN("[FETCH] Waiting for connection to " << parsed.host << ":" << parsed.port);
        while (true) {
            timeout.tv_sec = 0;
            timeout.tv_usec = 10000;  // 10ms
            int sel_result = select(sock + 1, nullptr, &write_fds, nullptr, &timeout);
            if (sel_result > 0) {
                // Check if connection succeeded
                int sock_err = 0;
                socklen_t len = sizeof(sock_err);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&sock_err, &len);
                if (sock_err != 0) {
                    close(sock);
                    response resp(500, "Internal Server Error");
                    resp.set_body("Failed to connect to server");
                    return resp;
                }
                break;  // Connection successful
            } else if (sel_result < 0) {
                close(sock);
                response resp(500, "Internal Server Error");
                resp.set_body("select() failed during connection");
                return resp;
            }
            // Timeout or no data yet - pump async system to allow server to process
            async_run();
        }
    }

    // Build HTTP request
    const auto& opts = request.options();
    fl::string method = opts.method.empty() ? "GET" : opts.method;

    fl::string http_request;
    http_request += method + " " + parsed.path + " HTTP/1.0\r\n";
    http_request += "Host: " + parsed.host + "\r\n";
    http_request += "Connection: close\r\n";

    // Add custom headers
    for (const auto& header : opts.headers) {
        http_request += header.first + ": " + header.second + "\r\n";
    }

    // Add body if present
    if (!opts.body.empty()) {
        http_request += "Content-Length: " + fl::to_string(opts.body.size()) + "\r\n";
        http_request += "\r\n";
        http_request += opts.body;
    } else {
        http_request += "\r\n";
    }

    // Send request
    if (send(sock, http_request.c_str(), static_cast<int>(http_request.size()), 0) < 0) {
        close(sock);
        response resp(500, "Internal Server Error");
        resp.set_body("Failed to send request");
        return resp;
    }

    // Read response (non-blocking with async pumping)
    FL_WARN("[FETCH] Waiting for HTTP response...");
    fl::string response_data;
    char buffer[4096];
    int retries = 0;
    const int max_retries = 5000;  // 5000 * 10ms = 50 seconds timeout

    while (retries < max_retries) {
        int bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes > 0) {
            // Got data - append and reset retry counter
            response_data.append(buffer, static_cast<size_t>(bytes));
            retries = 0;  // Reset on successful read
        } else if (bytes == 0) {
            // Connection closed by server
            break;
        } else {
            // Error or would block
#ifdef FL_IS_WIN
            int err = WSAGetLastError();
#else
            int err = errno;
#endif
            if (err == SOCKET_ERROR_WOULD_BLOCK) {
                // No data available yet - pump async system to allow server to process
                async_run();
                retries++;

                // Small delay to avoid busy-waiting
#ifdef FL_IS_WIN
                Sleep(10);  // 10ms
#else
                usleep(10000);  // 10ms
#endif
            } else {
                // Real error
                close(sock);
                response resp(500, "Internal Server Error");
                resp.set_body("Failed to read response");
                return resp;
            }
        }
    }

    close(sock);

    if (retries >= max_retries) {
        response resp(500, "Internal Server Error");
        resp.set_body("Timeout reading response");
        return resp;
    }

    // Parse HTTP response
    size_t header_end = response_data.find("\r\n\r\n");
    if (header_end == fl::string::npos) {
        response resp(500, "Internal Server Error");
        resp.set_body("Invalid HTTP response");
        return resp;
    }

    fl::string header_section = response_data.substr(0, header_end);
    fl::string body = response_data.substr(header_end + 4);

    // Parse status line
    size_t first_line_end = header_section.find("\r\n");
    fl::string status_line = header_section.substr(0, first_line_end);

    // Extract status code (format: "HTTP/1.1 200 OK")
    size_t code_start = status_line.find(' ');
    if (code_start == fl::string::npos) {
        response resp(500, "Internal Server Error");
        resp.set_body("Invalid status line");
        return resp;
    }

    size_t code_end = status_line.find(' ', code_start + 1);
    fl::string code_str = status_line.substr(code_start + 1, code_end - code_start - 1);
    int status_code = atoi(code_str.c_str());

    fl::string status_text = (code_end != fl::string::npos)
        ? status_line.substr(code_end + 1)
        : "";

    // Create response object
    response resp(status_code, status_text);
    resp.set_body(body);

    // Parse headers
    size_t header_start = first_line_end + 2;
    while (header_start < header_section.size()) {
        size_t line_end = header_section.find("\r\n", header_start);
        if (line_end == fl::string::npos) break;

        fl::string header_line = header_section.substr(header_start, line_end - header_start);
        size_t colon = header_line.find(':');
        if (colon != fl::string::npos) {
            fl::string name = header_line.substr(0, colon);
            fl::string value = header_line.substr(colon + 1);
            // Trim leading space from value
            if (!value.empty() && value[0] == ' ') {
                value = value.substr(1);
            }
            // Normalize header name to lowercase for case-insensitive lookup (RFC 2616)
            for (size_t i = 0; i < name.size(); ++i) {
                if (name[i] >= 'A' && name[i] <= 'Z') {
                    name[i] = name[i] + ('a' - 'A');
                }
            }
            resp.set_header(name, value);
        }

        header_start = line_end + 2;
    }

    return resp;
}
*/

} // anonymous namespace

void fetch(const fl::string& url, const FetchCallback& callback) {
    // Use async execute_fetch_request and attach callback to promise
    execute_fetch_request(url, fetch_options(url))
        .then([callback](const response& resp) {
            callback(resp);
        })
        .catch_([callback](const Error& err) {
            // On error, return 500 response
            response resp(500, "Internal Server Error");
            resp.set_body(err.message);
            callback(resp);
        });
}

// Internal helper to execute a fetch request and return a promise
fl::promise<response> execute_fetch_request(const fl::string& url, const fetch_options& request) {
    // Create promise for this request
    auto promise = fl::promise<response>::create();

    // Register promise with FetchManager for tracking
    FetchManager::instance().register_promise(promise);

    // Create shared FetchRequest (managed by task lambda)
    auto fetch_req = fl::make_shared<fl::net::FetchRequest>(url, request, promise);

    // Create self-canceling task (stored in shared_ptr for lambda capture)
    auto task_ptr = fl::make_shared<fl::task>();

    *task_ptr = fl::task::every_ms(1)  // Update every 1ms
        .then([fetch_req, task_ptr]() {
            // Pump the fetch state machine
            fetch_req->update();

            // Self-cancel when done
            if (fetch_req->is_done()) {
                task_ptr->cancel();
            }
        })
        .catch_([promise](const fl::Error& e) mutable {
            // Task error - reject promise if not already completed
            if (promise.valid() && !promise.is_completed()) {
                promise.complete_with_error(e);
            }
        });

    // Add to scheduler
    fl::Scheduler::instance().add_task(*task_ptr);

    return promise;
}

#else
// ========== True Stub Implementation (no networking) ==========

void fetch(const fl::string& url, const FetchCallback& callback) {
    (void)url;
    response resp(501, "Not Implemented");
    resp.set_text("HTTP fetch not supported on this platform");
    callback(resp);
}

fl::promise<response> execute_fetch_request(const fl::string& url, const fetch_options& request) {
    (void)request;
    FL_WARN("HTTP fetch is not supported on this platform. URL: " << url);
    response error_response(501, "Not Implemented");
    error_response.set_body("HTTP fetch is not available on this platform.");
    return fl::promise<response>::resolve(error_response);
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
