#include "fl/remote/remote.h"
#include "fl/stl/int.h"
#include "fl/stl/json.h"
#include "fl/log/log.h"
#include "fl/system/sketch_macros.h"  // FL_PLATFORM_HAS_LARGE_MEMORY -- gates scheduled-RPC + result-tracking
#include "fl/remote/rpc/rpc.h"
#include "fl/remote/rpc/server.h"
#include "fl/remote/types.h"
#include "fl/net/rpc_scheduler.h"
#include "fl/stl/chrono.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/function.h"
#include "fl/stl/move.h"
#include "fl/stl/optional.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/stl/vector.h"

namespace fl {

bool Remote::unbind(const fl::string& name) {
    bool removed = mRpc.unbind(name.c_str());
    if (removed) {
        FL_DBG_F("Unregistered RPC function: %s", name);
    }
    return removed;
}

bool Remote::has(const fl::string& name) const {
    return mRpc.has(name.c_str());
}

// Async Response Support
// All three sendAsync* methods touch mAsyncRequests, which Low-memory drops
// (see #3224 Tier 1B RAM-side companion). On Low-memory these methods
// become no-ops with a one-line FL_WARN; they remain in the API surface so
// callers don't break, but they're link-DCE-friendly if no caller exists.

#if FL_PLATFORM_HAS_LARGE_MEMORY
void Remote::sendAsyncResponse(const char* method, const fl::json& result) {
    fl::string methodName(method);
    auto it = mAsyncRequests.find(methodName);
    if (it == mAsyncRequests.end()) {
        FL_WARN_F("No pending async request for method: %s", method);
        return;
    }

    int requestId = it->second.requestId;
    mAsyncRequests.erase(it);

    // Build JSON-RPC response
    fl::json response = fl::json::object();
    response.set("jsonrpc", "2.0");
    response.set("id", requestId);
    response.set("result", result);

    // Send via response sink
    if (mResponseSink) {
        mResponseSink(response);
        FL_DBG_F("Sent async response for %s (id=%s)", method, requestId);
    }
}

void Remote::sendStreamUpdate(const char* method, const fl::json& update) {
    fl::string methodName(method);
    auto it = mAsyncRequests.find(methodName);
    if (it == mAsyncRequests.end()) {
        FL_WARN_F("No pending async request for method: %s", method);
        return;
    }

    int requestId = it->second.requestId;
    // Don't erase - stream is still active

    // Build JSON-RPC response with "update" marker
    fl::json response = fl::json::object();
    response.set("jsonrpc", "2.0");
    response.set("id", requestId);

    fl::json resultObj = fl::json::object();
    resultObj.set("update", update);
    response.set("result", resultObj);

    // Send via response sink
    if (mResponseSink) {
        mResponseSink(response);
        FL_DBG_F("Sent stream update for %s (id=%s)", method, requestId);
    }
}

void Remote::sendStreamFinal(const char* method, const fl::json& result) {
    fl::string methodName(method);
    auto it = mAsyncRequests.find(methodName);
    if (it == mAsyncRequests.end()) {
        FL_WARN_F("No pending async request for method: %s", method);
        return;
    }

    int requestId = it->second.requestId;
    mAsyncRequests.erase(it);  // Stream complete - remove request

    // Build JSON-RPC response with "stop" marker
    fl::json response = fl::json::object();
    response.set("jsonrpc", "2.0");
    response.set("id", requestId);

    fl::json resultObj = fl::json::object();
    resultObj.set("value", result);
    resultObj.set("stop", true);
    response.set("result", resultObj);

    // Send via response sink
    if (mResponseSink) {
        mResponseSink(response);
        FL_DBG_F("Sent stream final for %s (id=%s)", method, requestId);
    }
}
#else  // !FL_PLATFORM_HAS_LARGE_MEMORY
// Low-memory: async-tracking storage is dropped (#3224 Tier 1B RAM-side).
// Keep the public API for source-compat; they no-op on this tier.
void Remote::sendAsyncResponse(const char* /*method*/, const fl::json& /*result*/) {
    FL_WARN_LIT("sendAsyncResponse: async dispatch not supported on Low-memory targets");
}
void Remote::sendStreamUpdate(const char* /*method*/, const fl::json& /*update*/) {
    FL_WARN_LIT("sendStreamUpdate: async streams not supported on Low-memory targets");
}
void Remote::sendStreamFinal(const char* /*method*/, const fl::json& /*result*/) {
    FL_WARN_LIT("sendStreamFinal: async streams not supported on Low-memory targets");
}
#endif

// Error Reporting

void Remote::reportError(const fl::string& message) {
    fl::json params = fl::json::object();
    params.set("message", message);
    reportError(params);
}

void Remote::reportError(const fl::json& data) {
    if (!mResponseSink) {
        return;
    }
    fl::json notification = fl::json::object();
    notification.set("jsonrpc", "2.0");
    notification.set("method", "__error");
    notification.set("params", data);
    mResponseSink(notification);
}

// RPC Processing

fl::json Remote::processRpc(const fl::json& request) {
#if FL_PLATFORM_HAS_LARGE_MEMORY
    // Extract optional timestamp field (0 = immediate, >0 = scheduled).
    // Low-memory builds drop the scheduling path entirely (see #3224 Tier 1B):
    // the LPC8xx integer-only RPC contract is invoked immediate-only.
    u32 timestamp = 0;
    if (request.contains("timestamp") && request["timestamp"].is_int()) {
        timestamp = static_cast<u32>(request["timestamp"].as_int().value());
    }
#endif

    u32 receivedAt = fl::millis();

#if FL_PLATFORM_HAS_LARGE_MEMORY
    // Execute or schedule
    if (timestamp != 0) {
        // Scheduled execution - result will be pushed to ResponseSink after execution
        scheduleFunction(timestamp, receivedAt, request);
        FL_DBG_F("RPC: Scheduled function - result will be pushed after execution");

        // Return acknowledgment with null result and "scheduled" marker
        fl::json response = fl::json::object();
        if (request.contains("id")) {
            response.set("id", request["id"]);
        }
        response.set("result", fl::json(nullptr));
        response.set("scheduled", true);  // Marker to not queue this response
        return response;
    }
#endif

    // Immediate execution path (always reached on Low-memory).

#if FL_PLATFORM_HAS_LARGE_MEMORY
    // Store request ID BEFORE invoking function (needed for async functions).
    // This allows sendAsyncResponse() to find the request ID while function is
    // running. Low-memory drops the async-tracking machinery (no production
    // LowMemory sketch uses bindAsync / sendAsyncResponse today) -- see
    // #3224 Tier 1B.
    if (request.contains("id") && request.contains("method")) {
        fl::string methodName = request["method"].as_string().value_or("");
        int requestId = request["id"].as_int().value_or(0);
        mAsyncRequests[methodName] = {requestId, receivedAt};
        FL_DBG_F("Stored request ID for %s (id=%s)", methodName.c_str(), requestId);
    }
#endif

    // Immediate execution - pass directly to Rpc
    fl::json response = mRpc.handle(request);

#if FL_PLATFORM_HAS_LARGE_MEMORY
    // For async functions, response already sent via sendAsyncResponse().
    // Envelope marker names renamed in #3228 -- accept legacy `__async` for
    // one release of back-compat in case external Server forks set it.
    bool ackAlready = (response.contains("ackOnly") && response["ackOnly"].as_bool().value_or(false))
                  || (response.contains("__async") && response["__async"].as_bool().value_or(false));
    if (ackAlready) {
        // Don't return response (ACK already sent by Rpc).
        // Return a skip-envelope to prevent Server from queueing it.
        fl::json nullResponse = fl::json::object();
        nullResponse.set("noEnqueue", true);
        return nullResponse;
    }

    // For sync functions, remove request ID (not needed)
    if (request.contains("method")) {
        fl::string methodName = request["method"].as_string().value_or("");
        mAsyncRequests.erase(methodName);
    }
#endif

#if FL_PLATFORM_HAS_LARGE_MEMORY
    // Record result for the post-call results vector. LowMemory targets drop
    // mResults entirely (see #3224 Tier 1B) -- RPC callers there receive
    // results directly via the response sink, not through Remote::mResults.
    if (response.contains("result") && request.contains("method")) {
        fl::string funcName = request["method"].as_string().value_or("");
        recordResult(funcName, response["result"], 0, receivedAt, receivedAt, false);
    }
#else
    (void)receivedAt;
#endif

    return response;
}

#if FL_PLATFORM_HAS_LARGE_MEMORY
void Remote::scheduleFunction(u32 timestamp, u32 receivedAt, const fl::json& jsonRpcRequest) {
    // Make explicit copy for capture (avoid reference issues)
    fl::json requestCopy = jsonRpcRequest;
    fl::string funcName = requestCopy["method"].as_string().value_or("unknown");

    // Wrap RPC execution in lambda and schedule it
    mScheduler.schedule(timestamp, [this, requestCopy, timestamp, receivedAt, funcName]() {
        u32 executedAt = fl::millis();

        // Execute JSON-RPC request
        fl::json response = mRpc.handle(requestCopy);

        // Record result with timing metadata
        if (response.contains("result") && requestCopy.contains("method")) {
            recordResult(funcName, response["result"], timestamp, receivedAt, executedAt, true);
        }
    });

    FL_DBG_F("Scheduled RPC: %s at %s", funcName, timestamp);
}

void Remote::recordResult(const fl::string& funcName, const fl::json& result, u32 scheduledAt, u32 receivedAt, u32 executedAt, bool wasScheduled) {
    mResults.push_back({funcName, result, scheduledAt, receivedAt, executedAt, wasScheduled});
}
#endif

// Update Loop

size_t Remote::tick(u32 currentTimeMs) {
#if FL_PLATFORM_HAS_LARGE_MEMORY
    // Clear previous results
    mResults.clear();

    // Delegate to generic scheduler - tasks handle their own execution and result recording
    return mScheduler.tick(currentTimeMs);
#else
    (void)currentTimeMs;
    return 0;
#endif
}

// Utility Methods

size_t Remote::pendingCount() const {
#if FL_PLATFORM_HAS_LARGE_MEMORY
    return mScheduler.pendingCount();
#else
    return 0;
#endif
}

void Remote::clear(ClearFlags flags) {
#if FL_PLATFORM_HAS_LARGE_MEMORY
    if ((flags & ClearFlags::Results) != ClearFlags::None) {
        mResults.clear();
        FL_DBG_F("Cleared RPC results");
    }
    if ((flags & ClearFlags::Scheduled) != ClearFlags::None) {
        mScheduler.clear();
        FL_DBG_F("Cleared scheduled RPC calls");
    }
#endif
    if ((flags & ClearFlags::Functions) != ClearFlags::None) {
        mRpc.clear();
        FL_DBG_F("Cleared registered RPC functions");
    }
}

// Constructor

Remote::Remote(RequestSource source, ResponseSink sink)
    : Remote(fl::move(source), fl::move(sink), ResponseStreamSink{})
{}

Remote::Remote(RequestSource source, ResponseSink sink, ResponseStreamSink streamSink)
    : Server(fl::move(source), fl::move(sink))
{
#if FL_PLATFORM_HAS_LARGE_MEMORY
    setResponseStreamSink(fl::move(streamSink));
#else
    (void)streamSink;
#endif

    // Set request handler to processRpc
    setRequestHandler([this](const fl::json& request) {
        return processRpc(request);
    });

    // Set response sink on Rpc for async ACKs
    mRpc.setResponseSink([this](const fl::json& response) {
        // Send response directly via Server's response sink
        if (mResponseSink) {
            mResponseSink(response);
        }
    });

#if FL_PLATFORM_HAS_LARGE_MEMORY
    mRpc.setResponseStreamSink([this](fl::JsonStreamCallback writeJson) {
        if (mResponseStreamSink) {
            mResponseStreamSink(fl::move(writeJson));
        }
    });
#endif
}

// Server Coordination

size_t Remote::update(u32 currentTimeMs) {
    size_t processed = Server::pull();   // Pull requests from Server
    size_t executed = tick(currentTimeMs);  // Process scheduled tasks

#if FL_PLATFORM_HAS_LARGE_MEMORY
    // Push scheduled results as JSON-RPC responses. Gated on Low-memory
    // because mResults is dropped there (see #3224 Tier 1B).
    for (const auto& r : mResults) {
        fl::json response = fl::json::object();
        response.set("result", r.result);
        // Note: We don't have the original request ID for scheduled calls
        // This could be improved by storing the ID with RpcResult
        mOutgoingQueue.push_back(response);
    }
#endif

    size_t sent = Server::push();        // Push responses from Server
    return processed + executed + sent;
}

// Schema Methods

fl::vector<Remote::MethodInfo> Remote::methods() const {
    fl::vector<MethodInfo> result;

    // Get flat JSON schema from underlying RPC
    // Format: [["methodName", "returnType", [["param1", "type1"], ...]], ...]
    fl::json jsonMethods = mRpc.methods();

    if (!jsonMethods.is_array()) {
        return result;
    }

    // Convert each flat method array to MethodInfo struct
    for (fl::size i = 0; i < jsonMethods.size(); i++) {
        fl::json method = jsonMethods[i];

        if (!method.is_array() || method.size() < 3) {
            continue;  // Invalid method format
        }

        MethodInfo info;

        // method[0] = method name
        info.name = method[0].as_string().value_or("");

        // method[1] = return type
        info.returnType = method[1].as_string().value_or("void");

        // method[2] = params array: [["param1", "type1"], ["param2", "type2"], ...]
        if (method[2].is_array()) {
            fl::json params = method[2];
            for (fl::size j = 0; j < params.size(); j++) {
                fl::json param = params[j];
                if (param.is_array() && param.size() >= 2) {
                    ParamInfo paramInfo;
                    paramInfo.name = param[0].as_string().value_or("");
                    paramInfo.type = param[1].as_string().value_or("unknown");
                    info.params.push_back(fl::move(paramInfo));
                }
            }
        }

        // Flat schema doesn't include description/tags
        info.description = "";
        info.tags.clear();

        result.push_back(fl::move(info));
    }

    return result;
}

} // namespace fl
