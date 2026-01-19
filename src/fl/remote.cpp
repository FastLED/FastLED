#include "fl/remote.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/time.h"
#include "fl/stl/cstdio.h"
#include "fl/detail/rpc/type_conversion_result.h"

namespace fl {

// RpcResult Serialization

fl::Json Remote::RpcResult::to_json() const {
    fl::Json obj = fl::Json::object();
    obj.set("function", functionName);
    obj.set("result", result);
    obj.set("scheduledAt", static_cast<int64_t>(scheduledAt));
    obj.set("receivedAt", static_cast<int64_t>(receivedAt));
    obj.set("executedAt", static_cast<int64_t>(executedAt));
    obj.set("wasScheduled", wasScheduled);
    return obj;
}

// Static Helper: Print prefixed single-line JSON

void Remote::printJson(const fl::Json& json) {
    fl::string jsonStr = json.to_string();

    // Ensure single-line (replace any newlines/carriage returns with spaces)
    for (size_t i = 0; i < jsonStr.size(); ++i) {
        if (jsonStr[i] == '\n' || jsonStr[i] == '\r') {
            jsonStr[i] = ' ';
        }
    }

    // Output with prefix - combine into single string for atomic output
    constexpr const char* prefix = FASTLED_REMOTE_PREFIX;
    if (prefix[0] != '\0') {  // Only add prefix if non-empty
        fl::string output = fl::string(prefix) + jsonStr;
        fl::println(output.c_str());
    } else {
        fl::println(jsonStr.c_str());
    }
}

// Static Helper: Print JSONL stream message with type prefix

void Remote::printStream(const char* messageType, const fl::Json& data) {
    // Build pure JSONL message: RESULT: {"type":"...", ...data}
    // Example: RESULT: {"type":"config_start","driver":"PARLIO","leds":100}

    // Create a new JSON object with type field
    fl::Json output = fl::Json::object();
    output.set("type", messageType);

    // Copy all fields from data into output
    if (data.is_object()) {
        auto keys = data.keys();
        for (fl::size i = 0; i < keys.size(); i++) {
            output.set(keys[i].c_str(), data[keys[i]]);
        }
    }

    // Serialize to compact JSON
    fl::string jsonStr = output.to_string();

    // Ensure single-line
    for (size_t i = 0; i < jsonStr.size(); ++i) {
        if (jsonStr[i] == '\n' || jsonStr[i] == '\r') {
            jsonStr[i] = ' ';
        }
    }

    // Output: RESULT: <json-with-type>
    fl::print("RESULT: ");
    fl::println(jsonStr.c_str());
}

// Legacy Function Registration

void Remote::registerFunction(const fl::string& name, RpcCallback callback) {
    mLegacyCallbacks[name] = LegacyCallbackWrapper(callback);
    FL_DBG("Registered legacy RPC function: " << name);
}

void Remote::registerFunctionWithReturn(const fl::string& name, RpcCallbackWithReturn callback) {
    mLegacyCallbacks[name] = LegacyCallbackWrapper(callback);
    FL_DBG("Registered legacy RPC function with return: " << name);
}

bool Remote::unregisterFunction(const fl::string& name) {
    // Check legacy callbacks first
    auto it = mLegacyCallbacks.find(name);
    if (it != mLegacyCallbacks.end()) {
        mLegacyCallbacks.erase(it);
        FL_DBG("Unregistered legacy RPC function: " << name);
        return true;
    }
    // Note: fl::Rpc doesn't support unregistration, so typed methods cannot be removed
    return false;
}

bool Remote::hasFunction(const fl::string& name) const {
    // Check typed RPC first
    if (mRpc.has(name.c_str())) {
        return true;
    }
    // Then check legacy callbacks
    return mLegacyCallbacks.find(name) != mLegacyCallbacks.end();
}

// RPC Processing

Remote::Error Remote::processRpc(const fl::string& jsonStr) {
    fl::Json result;
    return processRpc(jsonStr, result);  // Delegate to overload, discard result
}

Remote::Error Remote::processRpc(const fl::string& jsonStr, fl::Json& outResult) {
    // Initialize output to null
    outResult = fl::Json(nullptr);

    // Parse JSON
    fl::Json doc = fl::Json::parse(jsonStr);

    if (!doc.has_value()) {
        FL_ERROR("RPC: Invalid JSON - parse failed");
        return Error::InvalidJson;
    }

    // Extract function name (required field)
    fl::string funcName = doc["function"] | fl::string("");

    if (funcName.empty()) {
        FL_ERROR("RPC: Missing 'function' field");
        return Error::MissingFunction;
    }

    // Extract timestamp with validation
    Error errorCode = Error::None;
    uint32_t timestamp = 0;

    if (doc.contains("timestamp")) {
        if (doc["timestamp"].is_int()) {
            int64_t ts = doc["timestamp"] | 0;
            if (ts < 0) {
                FL_WARN("RPC: Invalid timestamp (negative), using immediate execution");
                timestamp = 0;
                errorCode = Error::InvalidTimestamp;
            } else {
                timestamp = static_cast<uint32_t>(ts);
            }
        } else {
            FL_WARN("RPC: Invalid timestamp type, using immediate execution");
            timestamp = 0;
            errorCode = Error::InvalidTimestamp;
        }
    }
    // If timestamp field missing, default to 0 (immediate) - not an error

    // Extract arguments (optional, defaults to empty array)
    fl::Json args = doc["args"];
    if (!args.has_value() || !args.is_array()) {
        FL_DBG("RPC: No args provided, using empty array");
        args = fl::Json::array();
    }

    // Check if function exists (typed or legacy)
    if (!hasFunction(funcName)) {
        FL_WARN("RPC: Unknown function '" << funcName << "'");
        return Error::UnknownFunction;
    }

    uint32_t receivedAt = fl::millis();

    // Execute or schedule
    if (timestamp == 0) {
        // Immediate execution
        uint32_t executedAt = receivedAt;  // Immediate execution happens at receive time

        // Try typed RPC first
        if (mRpc.has(funcName.c_str())) {
            auto resultTuple = executeFunctionTyped(funcName, args);
            Error err = fl::get<0>(resultTuple);
            outResult = fl::get<1>(resultTuple);
            if (err != Error::None) {
                return err;
            }
        } else {
            // Fall back to legacy callback
            outResult = executeFunction(funcName, args);
        }

        recordResult(funcName, outResult, 0, receivedAt, executedAt, false);
    } else {
        // Scheduled execution - result will be available after execution via getResults()
        scheduleFunction(timestamp, receivedAt, funcName, args);
        FL_DBG("RPC: Scheduled function - result will be available after execution");
    }

    return errorCode;
}

// Function Execution (Legacy)

fl::Json Remote::executeFunction(const fl::string& funcName, const fl::Json& args) {
    auto it = mLegacyCallbacks.find(funcName);
    if (it != mLegacyCallbacks.end()) {
        FL_DBG("Executing legacy RPC: " << funcName);
        return it->second.execute(args);  // Call the callback wrapper
    }
    // Note: We already checked hasFunction() in processRpc(), so this should always succeed
    return fl::Json(nullptr);
}

// Function Execution (Typed via fl::Rpc)

fl::tuple<Remote::Error, fl::Json> Remote::executeFunctionTyped(const fl::string& funcName, const fl::Json& args) {
    FL_DBG("Executing typed RPC: " << funcName);

    // Build JSON-RPC request for fl::Rpc::handle()
    fl::Json request = fl::Json::object();
    request.set("method", funcName);
    request.set("params", args);
    request.set("id", 1);  // We need an ID to get the response

    // Process through fl::Rpc
    fl::Json response = mRpc.handle(request);

    // Check for errors
    if (response.contains("error")) {
        FL_WARN("RPC: Typed method error: " << response["error"]["message"].as_string().value_or("unknown"));
        return fl::make_tuple(Error::InvalidParams, fl::Json(nullptr));
    }

    // Extract result
    fl::Json result = response["result"];
    return fl::make_tuple(Error::None, result);
}

void Remote::scheduleFunction(uint32_t timestamp, uint32_t receivedAt, const fl::string& funcName, const fl::Json& args) {
    mScheduled.push({timestamp, funcName, args, receivedAt});
    FL_DBG("Scheduled RPC: " << funcName << " at " << timestamp);
}

void Remote::recordResult(const fl::string& funcName, const fl::Json& result, uint32_t scheduledAt, uint32_t receivedAt, uint32_t executedAt, bool wasScheduled) {
    mResults.push_back({funcName, result, scheduledAt, receivedAt, executedAt, wasScheduled});
}

// Update Loop

size_t Remote::tick(uint32_t currentTimeMs) {
    size_t executedCount = 0;

    // Clear previous results
    mResults.clear();

    // Process scheduled calls from stable priority queue (earliest first, FIFO for equal times)
    // priority_queue_stable ensures FIFO ordering for calls with same execution time
    while (!mScheduled.empty() && currentTimeMs >= mScheduled.top().mExecuteAt) {
        const ScheduledCall& call = mScheduled.top();

        uint32_t executedAt = currentTimeMs;  // Use the tick time, not wall clock time
        fl::Json result;

        // Try typed RPC first
        if (mRpc.has(call.mFunctionName.c_str())) {
            auto resultTuple = executeFunctionTyped(call.mFunctionName, call.mArgs);
            // Ignore error for scheduled calls - just record the result
            result = fl::get<1>(resultTuple);
        } else {
            // Fall back to legacy callback
            result = executeFunction(call.mFunctionName, call.mArgs);
        }

        // Record result with timing metadata
        recordResult(call.mFunctionName, result, call.mExecuteAt, call.mReceivedAt, executedAt, true);

        mScheduled.pop();
        executedCount++;
    }

    return executedCount;
}

fl::vector<Remote::RpcResult> Remote::getResults() const {
    return mResults;
}

void Remote::clearResults() {
    mResults.clear();
    FL_DBG("Cleared RPC results");
}

// Utility Methods

size_t Remote::pendingCount() const {
    return mScheduled.size();
}

void Remote::clearScheduled() {
    mScheduled.clear();
    FL_DBG("Cleared all scheduled RPC calls");
}

void Remote::clearFunctions() {
    mLegacyCallbacks.clear();
    // Note: fl::Rpc doesn't support clearing, so we'd need to recreate it
    // For now, just clear the legacy callbacks
    // mRpc = fl::Rpc();  // Would need to verify this works
    FL_DBG("Cleared all registered RPC functions (legacy only - typed methods remain)");
}

void Remote::clear() {
    clearScheduled();
    clearFunctions();
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON
