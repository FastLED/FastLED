#include "fl/remote/remote.h"

#if FASTLED_ENABLE_JSON

#include "fl/int.h"
#include "fl/json.h"
#include "fl/log.h"
#include "fl/remote/rpc/rpc.h"
#include "fl/remote/rpc/server.h"
#include "fl/remote/types.h"
#include "fl/scheduler.h"
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
        FL_DBG("Unregistered RPC function: " << name);
    }
    return removed;
}

bool Remote::has(const fl::string& name) const {
    return mRpc.has(name.c_str());
}

// Async Response Support

void Remote::sendAsyncResponse(const char* method, const fl::Json& result) {
    fl::string methodName(method);
    auto it = mAsyncRequests.find(methodName);
    if (it == mAsyncRequests.end()) {
        FL_WARN("No pending async request for method: " << method);
        return;
    }

    int requestId = it->second.requestId;
    mAsyncRequests.erase(it);

    // Build JSON-RPC response
    fl::Json response = fl::Json::object();
    response.set("jsonrpc", "2.0");
    response.set("id", requestId);
    response.set("result", result);

    // Send via response sink
    if (mResponseSink) {
        mResponseSink(response);
        FL_DBG("Sent async response for " << method << " (id=" << requestId << ")");
    }
}

void Remote::sendStreamUpdate(const char* method, const fl::Json& update) {
    fl::string methodName(method);
    auto it = mAsyncRequests.find(methodName);
    if (it == mAsyncRequests.end()) {
        FL_WARN("No pending async request for method: " << method);
        return;
    }

    int requestId = it->second.requestId;
    // Don't erase - stream is still active

    // Build JSON-RPC response with "update" marker
    fl::Json response = fl::Json::object();
    response.set("jsonrpc", "2.0");
    response.set("id", requestId);

    fl::Json resultObj = fl::Json::object();
    resultObj.set("update", update);
    response.set("result", resultObj);

    // Send via response sink
    if (mResponseSink) {
        mResponseSink(response);
        FL_DBG("Sent stream update for " << method << " (id=" << requestId << ")");
    }
}

void Remote::sendStreamFinal(const char* method, const fl::Json& result) {
    fl::string methodName(method);
    auto it = mAsyncRequests.find(methodName);
    if (it == mAsyncRequests.end()) {
        FL_WARN("No pending async request for method: " << method);
        return;
    }

    int requestId = it->second.requestId;
    mAsyncRequests.erase(it);  // Stream complete - remove request

    // Build JSON-RPC response with "stop" marker
    fl::Json response = fl::Json::object();
    response.set("jsonrpc", "2.0");
    response.set("id", requestId);

    fl::Json resultObj = fl::Json::object();
    resultObj.set("value", result);
    resultObj.set("stop", true);
    response.set("result", resultObj);

    // Send via response sink
    if (mResponseSink) {
        mResponseSink(response);
        FL_DBG("Sent stream final for " << method << " (id=" << requestId << ")");
    }
}

// RPC Processing

fl::Json Remote::processRpc(const fl::Json& request) {
    Serial.println("╔═══════════════════════════════════════════════════════════╗");
    Serial.println("║ [REMOTE] processRpc() called                              ║");
    if (request.contains("method")) {
        Serial.print("║ [REMOTE] Method: ");
        Serial.println(request["method"].as_string().value_or("unknown").c_str());
    }
    Serial.println("╚═══════════════════════════════════════════════════════════╝");
    Serial.flush();

    // Extract optional timestamp field (0 = immediate, >0 = scheduled)
    u32 timestamp = 0;
    if (request.contains("timestamp") && request["timestamp"].is_int()) {
        timestamp = static_cast<u32>(request["timestamp"].as_int().value());
    }

    u32 receivedAt = fl::millis();

    // Execute or schedule
    if (timestamp == 0) {
        // Store request ID BEFORE invoking function (needed for async functions)
        // This allows sendAsyncResponse() to find the request ID while function is running
        if (request.contains("id") && request.contains("method")) {
            fl::string methodName = request["method"].as_string().value_or("");
            int requestId = request["id"].as_int().value_or(0);
            mAsyncRequests[methodName] = {requestId, receivedAt};
            FL_DBG("Stored request ID for " << methodName.c_str() << " (id=" << requestId << ")");
        }

        // Immediate execution - pass directly to Rpc
        fl::Json response = mRpc.handle(request);

        // For async functions, response already sent via sendAsyncResponse()
        if (response.contains("__async") && response["__async"].as_bool().value_or(false)) {
            // Don't return response (ACK already sent by Rpc)
            // Return null to prevent Server from queuing it
            fl::Json nullResponse = fl::Json::object();
            nullResponse.set("__skip", true);  // Marker to skip queueing
            return nullResponse;
        }

        // For sync functions, remove request ID (not needed)
        if (request.contains("method")) {
            fl::string methodName = request["method"].as_string().value_or("");
            mAsyncRequests.erase(methodName);
        }

        // Record result if successful
        if (response.contains("result") && request.contains("method")) {
            fl::string funcName = request["method"].as_string().value_or("");
            recordResult(funcName, response["result"], 0, receivedAt, receivedAt, false);
        }

        return response;
    } else {
        // Scheduled execution - result will be pushed to ResponseSink after execution
        scheduleFunction(timestamp, receivedAt, request);
        FL_DBG("RPC: Scheduled function - result will be pushed after execution");

        // Return acknowledgment with null result and "scheduled" marker
        fl::Json response = fl::Json::object();
        if (request.contains("id")) {
            response.set("id", request["id"]);
        }
        response.set("result", fl::Json(nullptr));
        response.set("scheduled", true);  // Marker to not queue this response
        return response;
    }
}

void Remote::scheduleFunction(u32 timestamp, u32 receivedAt, const fl::Json& jsonRpcRequest) {
    // Make explicit copy for capture (avoid reference issues)
    fl::Json requestCopy = jsonRpcRequest;
    fl::string funcName = requestCopy["method"].as_string().value_or("unknown");

    // Wrap RPC execution in lambda and schedule it
    mScheduler.schedule(timestamp, [this, requestCopy, timestamp, receivedAt, funcName]() {
        u32 executedAt = fl::millis();

        // Execute JSON-RPC request
        fl::Json response = mRpc.handle(requestCopy);

        // Record result with timing metadata
        if (response.contains("result") && requestCopy.contains("method")) {
            recordResult(funcName, response["result"], timestamp, receivedAt, executedAt, true);
        }
    });

    FL_DBG("Scheduled RPC: " << funcName << " at " << timestamp);
}

void Remote::recordResult(const fl::string& funcName, const fl::Json& result, u32 scheduledAt, u32 receivedAt, u32 executedAt, bool wasScheduled) {
    mResults.push_back({funcName, result, scheduledAt, receivedAt, executedAt, wasScheduled});
}

// Update Loop

size_t Remote::tick(u32 currentTimeMs) {
    // Clear previous results
    mResults.clear();

    // Delegate to generic scheduler - tasks handle their own execution and result recording
    return mScheduler.tick(currentTimeMs);
}

// Utility Methods

size_t Remote::pendingCount() const {
    return mScheduler.pendingCount();
}

void Remote::clear(ClearFlags flags) {
    if ((flags & ClearFlags::Results) != ClearFlags::None) {
        mResults.clear();
        FL_DBG("Cleared RPC results");
    }
    if ((flags & ClearFlags::Scheduled) != ClearFlags::None) {
        mScheduler.clear();
        FL_DBG("Cleared scheduled RPC calls");
    }
    if ((flags & ClearFlags::Functions) != ClearFlags::None) {
        mRpc.clear();
        FL_DBG("Cleared registered RPC functions");
    }
}

// Constructor

Remote::Remote(RequestSource source, ResponseSink sink)
    : Server(fl::move(source), fl::move(sink))
{
    // Set request handler to processRpc
    setRequestHandler([this](const fl::Json& request) {
        return processRpc(request);
    });

    // Set response sink on Rpc for async ACKs
    mRpc.setResponseSink([this](const fl::Json& response) {
        // Send response directly via Server's response sink
        if (mResponseSink) {
            mResponseSink(response);
        }
    });
}

// Server Coordination

size_t Remote::update(u32 currentTimeMs) {
    size_t processed = Server::pull();   // Pull requests from Server
    size_t executed = tick(currentTimeMs);  // Process scheduled tasks

    // Push scheduled results as JSON-RPC responses
    for (const auto& r : mResults) {
        fl::Json response = fl::Json::object();
        response.set("result", r.result);
        // Note: We don't have the original request ID for scheduled calls
        // This could be improved by storing the ID with RpcResult
        mOutgoingQueue.push_back(response);
    }

    size_t sent = Server::push();        // Push responses from Server
    return processed + executed + sent;
}

// Schema Methods

fl::vector<Remote::MethodInfo> Remote::methods() const {
    fl::vector<MethodInfo> result;

    // Get flat JSON schema from underlying RPC
    // Format: [["methodName", "returnType", [["param1", "type1"], ...]], ...]
    fl::Json jsonMethods = mRpc.methods();

    if (!jsonMethods.is_array()) {
        return result;
    }

    // Convert each flat method array to MethodInfo struct
    for (fl::size i = 0; i < jsonMethods.size(); i++) {
        fl::Json method = jsonMethods[i];

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
            fl::Json params = method[2];
            for (fl::size j = 0; j < params.size(); j++) {
                fl::Json param = params[j];
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

#endif // FASTLED_ENABLE_JSON
