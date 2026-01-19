#pragma once

#include "fl/json.h"

// Remote RPC system requires JSON support
#if FASTLED_ENABLE_JSON

#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "fl/stl/allocator.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/function.h"
#include "fl/stl/priority_queue.h"
#include "fl/stl/vector.h"
#include "fl/stl/strstream.h"
#include "fl/rpc.h"

// Compile-time prefix for Remote JSON output
// Define as empty string to disable: -DFASTLED_REMOTE_PREFIX=\"\"
#ifndef FASTLED_REMOTE_PREFIX
#define FASTLED_REMOTE_PREFIX "REMOTE: "
#endif

namespace fl {

/**
 * @brief Remote RPC system for JSON-based function calls
 *
 * Supports both immediate and scheduled execution of registered functions.
 * Functions can optionally return JSON values for queries.
 *
 * UPGRADED: Now uses fl::Rpc internally for typed method registration.
 * You can register methods with auto-deduced typed signatures OR use the
 * legacy untyped JSON callback API.
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
 *
 * TYPED METHOD REGISTRATION (NEW - Recommended):
 *   remote.method("add", [](int a, int b) { return a + b; });
 *   remote.method("setLed", [](int index, int r, int g, int b) { ... });
 *
 * LEGACY UNTYPED REGISTRATION (still supported for backward compatibility):
 *   remote.registerFunction("test", [](const fl::Json& args) { ... });
 *   remote.registerFunctionWithReturn("millis", [](const fl::Json& args) -> fl::Json { ... });
 */
class Remote {
public:
    // Legacy callback signatures (for backward compatibility)
    using RpcCallback = fl::function<void(const fl::Json&)>;
    using RpcCallbackWithReturn = fl::function<fl::Json(const fl::Json&)>;

    // Error codes returned by processRpc()
    enum class Error {
        None = 0,           // Success
        InvalidJson,        // JSON parsing failed
        MissingFunction,    // "function" field missing
        UnknownFunction,    // Function not registered
        InvalidTimestamp,   // Timestamp field has wrong type
        InvalidParams       // Parameter conversion failed (typed RPC only)
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

        /**
         * @brief Serialize result to JSON object (compact single-line format)
         * @return JSON object with all result fields
         *
         * Output format:
         * {
         *   "function": "functionName",
         *   "result": <returnValue>,
         *   "scheduledAt": 1000,
         *   "receivedAt": 500,
         *   "executedAt": 1005,
         *   "wasScheduled": true
         * }
         */
        fl::Json to_json() const;
    };

    Remote() = default;

    // =========================================================================
    // TYPED Method Registration (NEW - Recommended)
    // =========================================================================

    /**
     * @brief Register a method with auto-deduced typed signature (RECOMMENDED)
     * @param name Function name (case-sensitive, supports dot notation for namespacing)
     * @param fn Callable (lambda, function pointer, or functor)
     * @return RpcHandle for direct invocation, or empty handle if registration failed
     *
     * Example:
     *   auto add = remote.method("add", [](int a, int b) { return a + b; });
     *   int result = add(2, 3);  // Direct call returns 5
     *
     *   // Void functions work too:
     *   remote.method("setLed", [&leds](int index, int r, int g, int b) {
     *       leds[index] = CRGB(r, g, b);
     *   });
     *
     *   // Namespaced methods with dot notation:
     *   remote.method("led.setBrightness", [](int b) { setBrightness(b); });
     *   remote.method("system.status", []() -> fl::string { return "ok"; });
     */
    template<typename Callable>
    auto method(const char* name, Callable&& fn)
        -> RpcHandle<typename callable_traits<typename decay<Callable>::type>::signature> {
        return mRpc.method(name, fl::forward<Callable>(fn));
    }

    /**
     * @brief Fluent builder for method registration with metadata
     * @param name Function name
     * @param fn Callable
     * @return MethodBuilder for chaining .params(), .description(), .tags(), .done()
     *
     * Example:
     *   auto setBri = remote.method_with("led.setBrightness", [](int b) { setBrightness(b); })
     *       .params({"brightness"})
     *       .description("Set LED brightness (0-255)")
     *       .tags({"led", "control"})
     *       .done();
     */
    template<typename Callable>
    auto method_with(const char* name, Callable&& fn)
        -> detail::MethodBuilder<typename callable_traits<typename decay<Callable>::type>::signature> {
        return mRpc.method_with(name, fl::forward<Callable>(fn));
    }

    /**
     * @brief Bind a typed callable for a registered method
     * @param name Function name
     * @return Typed function if found and signature matches, empty function otherwise
     *
     * Example:
     *   auto addFn = remote.bind<int(int, int)>("add");
     *   if (addFn) {
     *       int result = addFn(5, 7);  // Returns 12
     *   }
     */
    template<class Sig>
    RpcFn<Sig> bind(const char* name) const {
        return mRpc.bind<Sig>(name);
    }

    /**
     * @brief Try to bind a typed callable (returns optional)
     * @param name Function name
     * @return Optional containing function if found and signature matches
     */
    template<class Sig>
    fl::optional<RpcFn<Sig>> try_bind(const char* name) const {
        return mRpc.try_bind<Sig>(name);
    }

    /**
     * @brief Direct typed invocation without binding
     * @param name Function name
     * @param args Arguments to pass
     * @return Return value (undefined behavior if method not found or signature mismatch)
     */
    template<typename R, typename... Args>
    R call(const char* name, Args&&... args) {
        return mRpc.call<R>(name, fl::forward<Args>(args)...);
    }

    /**
     * @brief Safe direct invocation with optional return
     * @param name Function name
     * @param args Arguments to pass
     * @return Optional containing result, or nullopt if method not found
     */
    template<typename R, typename... Args>
    fl::optional<R> try_call(const char* name, Args&&... args) {
        return mRpc.try_call<R>(name, fl::forward<Args>(args)...);
    }

    // =========================================================================
    // Legacy Untyped Registration (for backward compatibility)
    // =========================================================================

    /**
     * @brief Register a function that can be called remotely (no return value)
     * @param name Function name (case-sensitive)
     * @param callback Function to invoke when RPC is received
     *
     * @deprecated Use method() with typed lambdas instead for better type safety
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
     * @deprecated Use method() with typed lambdas instead for better type safety
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
     * @brief Check if a function is registered (typed or legacy)
     * @param name Function name to check
     * @return true if function is registered, false otherwise
     */
    bool hasFunction(const fl::string& name) const;

    // =========================================================================
    // RPC Processing
    // =========================================================================

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
     * - InvalidParams: For typed methods, returns Error::InvalidParams if args don't match signature
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
    size_t tick(uint32_t currentTimeMs);

    /**
     * @brief Get results from recently executed functions (both immediate and scheduled)
     * @return Vector of RpcResult objects containing execution metadata and return values
     *
     * After calling processRpc() or tick(), this returns all functions that executed
     * and returned values. The vector is cleared on each tick() call.
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
     *   remote.tick(millis());
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
     * @brief Clear all registered functions (typed and legacy)
     *
     * Does not affect pending scheduled calls.
     */
    void clearFunctions();

    /**
     * @brief Clear everything (scheduled calls + registered functions)
     */
    void clear();

    // =========================================================================
    // Schema and Discovery (NEW - from fl::Rpc)
    // =========================================================================

    /**
     * @brief Enable built-in rpc.discover method
     * @param title API title for OpenRPC schema
     * @param version API version for OpenRPC schema
     *
     * When enabled, clients can call {"method":"rpc.discover"} to get
     * the full OpenRPC schema for all registered typed methods.
     */
    void enableDiscover(const char* title = "RPC API", const char* version = "1.0.0") {
        mRpc.enableDiscover(title, version);
    }

    /**
     * @brief Returns array of method schemas (typed methods only)
     * @return JSON array of OpenRPC method schemas
     */
    Json methods() const {
        return mRpc.methods();
    }

    /**
     * @brief Returns full OpenRPC document (typed methods only)
     * @param title API title
     * @param version API version
     * @return OpenRPC schema document
     */
    Json schema(const char* title = "RPC API", const char* version = "1.0.0") const {
        return mRpc.schema(title, version);
    }

    /**
     * @brief Returns number of registered methods (typed + legacy)
     */
    fl::size count() const {
        return mRpc.count() + mLegacyCallbacks.size();
    }

    /**
     * @brief Returns list of unique tags used across all typed methods
     */
    fl::vector<fl::string> tags() const {
        return mRpc.tags();
    }

    // =========================================================================
    // Static Output Helpers
    // =========================================================================

    /**
     * @brief Print JSON to output with FASTLED_REMOTE_PREFIX (single-line format)
     * @param json JSON value to output
     *
     * Outputs JSON in compact single-line format with configured prefix.
     * Useful for structured responses that need to be filtered on host side.
     *
     * Example:
     *   fl::Json response = fl::Json::object();
     *   response.set("status", "ok");
     *   fl::Remote::printJson(response);
     *   // Output: "REMOTE: {"status":"ok"}"
     *
     * To filter on host side:
     *   grep "^REMOTE: " /dev/ttyUSB0 | sed 's/^REMOTE: //' | jq .
     */
    static void printJson(const fl::Json& json);

    /**
     * @brief Print JSONL stream message with type field embedded
     * @param messageType Type of message (e.g., "config_start", "test_result")
     * @param data JSON object containing message data
     *
     * Outputs pure JSONL format: RESULT: {"type":"...", ...data}
     * This is the recommended format for streaming progress updates.
     *
     * Example:
     *   fl::Json data = fl::Json::object();
     *   data.set("driver", "PARLIO");
     *   data.set("leds", 100);
     *   fl::Remote::printStream("config_start", data);
     *   // Output: RESULT: {"type":"config_start","driver":"PARLIO","leds":100}
     *
     * To filter on host side:
     *   grep "^RESULT: " /dev/ttyUSB0 | sed 's/^RESULT: //' | jq 'select(.type == "config_start")'
     *   # Or with Python:
     *   # line.startswith("RESULT: ") and json.loads(line[8:])
     */
    static void printStream(const char* messageType, const fl::Json& data);

protected:
    struct ScheduledCall {
        uint32_t mExecuteAt;        // millis() timestamp when to execute
        fl::string mFunctionName;   // Name of function to call
        fl::Json mArgs;             // Arguments to pass
        uint32_t mReceivedAt;       // Timestamp when RPC was received

        // Comparison operator for stable priority queue (earlier times have higher priority)
        // priority_queue_stable uses fl::greater by default, so use natural comparison
        bool operator<(const ScheduledCall& other) const {
            return mExecuteAt < other.mExecuteAt;  // Natural: smaller time = higher priority
        }
    };

    // Legacy callback wrapper that handles both void and returning functions
    struct LegacyCallbackWrapper {
        RpcCallback voidCallback;
        RpcCallbackWithReturn returningCallback;
        bool hasReturn;

        LegacyCallbackWrapper() : hasReturn(false) {}
        LegacyCallbackWrapper(RpcCallback cb) : voidCallback(cb), hasReturn(false) {}
        LegacyCallbackWrapper(RpcCallbackWithReturn cb) : returningCallback(cb), hasReturn(true) {}

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
    fl::tuple<Error, fl::Json> executeFunctionTyped(const fl::string& funcName, const fl::Json& args);
    void scheduleFunction(uint32_t timestamp, uint32_t receivedAt, const fl::string& funcName, const fl::Json& args);
    void recordResult(const fl::string& funcName, const fl::Json& result, uint32_t scheduledAt, uint32_t receivedAt, uint32_t executedAt, bool wasScheduled);

    // Typed RPC registry (NEW)
    fl::Rpc mRpc;

    // Legacy untyped callbacks (for backward compatibility)
    fl::unordered_map<fl::string, LegacyCallbackWrapper> mLegacyCallbacks;

    fl::priority_queue_stable<ScheduledCall> mScheduled;  // Uses default fl::greater for min-heap: earlier times = higher priority (FIFO for equal times)
    fl::vector<RpcResult> mResults;  // Results from executed functions (cleared on tick())
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON
