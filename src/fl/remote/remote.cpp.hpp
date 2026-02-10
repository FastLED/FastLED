#include "fl/remote/remote.h"

#include "fl/stl/time.h"
#include "fl/stl/cstdio.h"
#include "fl/remote/rpc/type_conversion_result.h"

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

// RPC Processing

fl::Json Remote::processRpc(const fl::Json& request) {
    // Extract optional timestamp field (0 = immediate, >0 = scheduled)
    u32 timestamp = 0;
    if (request.contains("timestamp") && request["timestamp"].is_int()) {
        timestamp = static_cast<u32>(request["timestamp"].as_int().value());
    }

    u32 receivedAt = fl::millis();

    // Execute or schedule
    if (timestamp == 0) {
        // Immediate execution - pass directly to Rpc
        fl::Json response = mRpc.handle(request);

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

    // Get JSON schema from underlying RPC
    fl::Json jsonMethods = mRpc.methods();

    if (!jsonMethods.is_array()) {
        return result;
    }

    // Convert each JSON method to MethodInfo struct
    for (fl::size i = 0; i < jsonMethods.size(); i++) {
        fl::Json method = jsonMethods[i];

        MethodInfo info;
        info.name = method["name"].as_string().value_or("");

        // Extract parameters
        if (method.contains("params") && method["params"].is_array()) {
            fl::Json params = method["params"];
            for (fl::size j = 0; j < params.size(); j++) {
                fl::Json param = params[j];
                ParamInfo paramInfo;
                paramInfo.name = param["name"].as_string().value_or("");

                // Get type from schema
                if (param.contains("schema") && param["schema"].is_object()) {
                    fl::Json schema = param["schema"];
                    paramInfo.type = schema["type"].as_string().value_or("unknown");
                } else {
                    paramInfo.type = "unknown";
                }

                info.params.push_back(fl::move(paramInfo));
            }
        }

        // Extract return type
        if (method.contains("result") && method["result"].is_object()) {
            fl::Json resultSchema = method["result"];
            if (resultSchema.contains("schema") && resultSchema["schema"].is_object()) {
                fl::Json schema = resultSchema["schema"];
                info.returnType = schema["type"].as_string().value_or("void");
            } else {
                info.returnType = "void";
            }
        } else {
            info.returnType = "void";
        }

        // Extract description
        info.description = method["description"].as_string().value_or("");

        // Extract tags
        if (method.contains("tags") && method["tags"].is_array()) {
            fl::Json tags = method["tags"];
            for (fl::size j = 0; j < tags.size(); j++) {
                fl::string tag = tags[j].as_string().value_or("");
                if (!tag.empty()) {
                    info.tags.push_back(fl::move(tag));
                }
            }
        }

        result.push_back(fl::move(info));
    }

    return result;
}

} // namespace fl
