#pragma once

#include "fl/json.h"

// Remote RPC system requires JSON support
#if FASTLED_ENABLE_JSON

#include "fl/stl/hash_map.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/stl/priority_queue.h"
#include "fl/warn.h"
#include "fl/error.h"
#include "fl/dbg.h"

namespace fl {

/**
 * @brief Remote RPC system for JSON-based function calls
 *
 * Supports both immediate and scheduled execution of registered functions.
 * Functions can optionally return JSON values for queries.
 *
 * Each RPC call contains:
 * - timestamp: When to execute (0 = immediate, >0 = scheduled at millis())
 * - function: Name of the registered function to call
 * - args: JSON array of arguments passed to the function
 *
 * Example JSON RPC format:
 * {"timestamp":0,"function":"setLed","args":[0,255,0,0]}
 * {"timestamp":5000,"function":"setBrightness","args":[128]}
 * {"function":"millis","args":[]}  // Returns: 12345
 */
class Remote {
public:
    // Callback signature (no return value): void callback(const fl::Json& args)
    using RpcCallback = fl::function<void(const fl::Json&)>;

    // Callback signature (with return value): fl::Json callback(const fl::Json& args)
    using RpcCallbackWithReturn = fl::function<fl::Json(const fl::Json&)>;

    // Error codes returned by processRpc()
    enum class Error {
        None = 0,           // Success
        InvalidJson,        // JSON parsing failed
        MissingFunction,    // "function" field missing
        UnknownFunction,    // Function not registered
        InvalidTimestamp    // Timestamp field has wrong type
    };

    /**
     * @brief Result metadata for executed RPC calls
     *
     * Contains execution timing information and return value.
     */
    struct RpcResult {
        fl::string functionName;    // Name of function that executed
        fl::Json result;            // Return value (null if no return)
        uint32_t scheduledAt;       // Timestamp when scheduled (0 for immediate)
        uint32_t receivedAt;        // Timestamp when RPC request received
        uint32_t executedAt;        // Timestamp when function executed
        bool wasScheduled;          // true if scheduled, false if immediate
    };

    Remote() = default;

    /**
     * @brief Register a function that can be called remotely (no return value)
     * @param name Function name (case-sensitive)
     * @param callback Function to invoke when RPC is received
     *
     * Example:
     *   remote.registerFunction("setLed", [](const fl::Json& args) {
     *       int index = args[0] | 0;
     *       int r = args[1] | 0;
     *       int g = args[2] | 0;
     *       int b = args[3] | 0;
     *       leds[index] = CRGB(r, g, b);
     *   });
     */
    void registerFunction(const fl::string& name, RpcCallback callback);

    /**
     * @brief Register a function that returns a JSON value
     * @param name Function name (case-sensitive)
     * @param callback Function to invoke when RPC is received
     *
     * Example (query system time):
     *   remote.registerFunctionWithReturn("millis", [](const fl::Json& args) -> fl::Json {
     *       return fl::Json(static_cast<int64_t>(millis()));
     *   });
     *
     * Call with: {"function":"millis","args":[]}
     * Returns: 12345
     */
    void registerFunctionWithReturn(const fl::string& name, RpcCallbackWithReturn callback);

    /**
     * @brief Unregister a previously registered function
     * @param name Function name to remove
     * @return true if function was found and removed, false otherwise
     */
    bool unregisterFunction(const fl::string& name);

    /**
     * @brief Check if a function is registered
     * @param name Function name to check
     * @return true if function is registered, false otherwise
     */
    bool hasFunction(const fl::string& name) const;

    /**
     * @brief Process incoming JSON RPC call
     * @param jsonStr JSON string containing RPC request
     * @return Error code indicating success or failure
     *
     * JSON format:
     * {
     *   "timestamp": 0,              // 0 = immediate, >0 = millis() time to execute
     *   "function": "functionName",  // Name of registered function
     *   "args": [arg1, arg2, ...]    // Arguments as JSON array (optional, defaults to [])
     * }
     *
     * Error handling:
     * - InvalidJson: FL_ERROR logged, returns Error::InvalidJson
     * - MissingFunction: FL_ERROR logged, returns Error::MissingFunction
     * - UnknownFunction: FL_WARN logged, returns Error::UnknownFunction
     * - InvalidTimestamp: FL_WARN logged, uses 0 (immediate), returns Error::InvalidTimestamp
     */
    Error processRpc(const fl::string& jsonStr);

    /**
     * @brief Process incoming JSON RPC call and return the result
     * @param jsonStr JSON string containing RPC request
     * @param outResult Output parameter for return value (if function returns one)
     * @return Error code indicating success or failure
     *
     * For immediate execution (timestamp=0):
     *   - If function returns a value, outResult will contain the JSON result
     *   - If function has no return value, outResult will be set to null
     *
     * For scheduled execution (timestamp>0):
     *   - outResult will be set to null (use getResults() later to retrieve)
     *
     * Example:
     *   fl::Json result;
     *   auto err = remote.processRpc(R"({"function":"millis"})", result);
     *   if (err == fl::Remote::Error::None && result.has_value()) {
     *       int64_t time = result.as_int() | 0;
     *   }
     */
    Error processRpc(const fl::string& jsonStr, fl::Json& outResult);

    /**
     * @brief Update scheduled RPC calls (call from main loop)
     * @param currentTimeMs Current time in milliseconds (typically millis())
     * @return Number of scheduled functions executed this update
     *
     * Call this regularly from your main loop to process scheduled RPCs.
     * Functions scheduled at or before currentTimeMs will be executed.
     * Return values from scheduled functions are stored and can be retrieved
     * via getResults().
     */
    size_t update(uint32_t currentTimeMs);

    /**
     * @brief Get results from recently executed functions (both immediate and scheduled)
     * @return Vector of RpcResult objects containing execution metadata and return values
     *
     * After calling processRpc() or update(), this returns all functions that executed
     * and returned values. The vector is cleared on each update() call.
     *
     * Each RpcResult contains:
     * - functionName: Name of the function that executed
     * - result: JSON return value (null if function had no return)
     * - scheduledAt: Timestamp when function was scheduled (0 for immediate)
     * - receivedAt: Timestamp when RPC request was received
     * - executedAt: Timestamp when function actually executed
     * - wasScheduled: true if function was scheduled, false if immediate
     *
     * Example:
     *   remote.update(millis());
     *   auto results = remote.getResults();
     *   for (const auto& r : results) {
     *       if (r.wasScheduled) {
     *           int delay = r.executedAt - r.receivedAt;
     *           FL_DBG("Scheduled function " << r.functionName << " executed after " << delay << "ms");
     *       }
     *       if (r.result.has_value()) {
     *           FL_DBG("Result: " << r.result.to_string());
     *       }
     *   }
     */
    fl::vector<RpcResult> getResults() const;

    /**
     * @brief Clear the results vector
     *
     * Useful if you want to explicitly clear results after processing them.
     */
    void clearResults();

    /**
     * @brief Get number of pending scheduled calls
     * @return Count of calls waiting to be executed
     */
    size_t pendingCount() const;

    /**
     * @brief Clear all scheduled calls
     *
     * Does not affect registered functions, only pending scheduled executions.
     */
    void clearScheduled();

    /**
     * @brief Clear all registered functions
     *
     * Does not affect pending scheduled calls.
     */
    void clearFunctions();

    /**
     * @brief Clear everything (scheduled calls + registered functions)
     */
    void clear();

private:
    struct ScheduledCall {
        uint32_t mExecuteAt;        // millis() timestamp when to execute
        fl::string mFunctionName;   // Name of function to call
        fl::Json mArgs;             // Arguments to pass
        uint32_t mReceivedAt;       // Timestamp when RPC was received

        // Comparison operator for stable priority queue (earlier times have higher priority)
        bool operator<(const ScheduledCall& other) const {
            return mExecuteAt > other.mExecuteAt;  // Inverted: smaller time = higher priority
        }
    };

    // Internal callback wrapper that handles both void and returning functions
    struct CallbackWrapper {
        RpcCallback voidCallback;
        RpcCallbackWithReturn returningCallback;
        bool hasReturn;

        CallbackWrapper() : hasReturn(false) {}
        CallbackWrapper(RpcCallback cb) : voidCallback(cb), hasReturn(false) {}
        CallbackWrapper(RpcCallbackWithReturn cb) : returningCallback(cb), hasReturn(true) {}

        fl::Json execute(const fl::Json& args) {
            if (hasReturn) {
                return returningCallback(args);
            } else {
                voidCallback(args);
                return fl::Json(nullptr);  // Return null for void functions
            }
        }
    };

    fl::Json executeFunction(const fl::string& funcName, const fl::Json& args);
    void scheduleFunction(uint32_t timestamp, uint32_t receivedAt, const fl::string& funcName, const fl::Json& args);
    void recordResult(const fl::string& funcName, const fl::Json& result, uint32_t scheduledAt, uint32_t receivedAt, uint32_t executedAt, bool wasScheduled);

    fl::HashMap<fl::string, CallbackWrapper> mCallbacks;
    fl::priority_queue_stable<ScheduledCall> mScheduled;  // Stable min-heap ordered by execution time (FIFO for equal times)
    fl::vector<RpcResult> mResults;  // Results from executed functions (cleared on update())
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON
