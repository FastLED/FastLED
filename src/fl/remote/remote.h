#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "fl/stl/allocator.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/function.h"
#include "fl/stl/vector.h"
#include "fl/stl/strstream.h"
#include "fl/remote/rpc/rpc.h"
#include "fl/remote/rpc/server.h"
#include "fl/remote/types.h"
#include "fl/scheduler.h"

namespace fl {

/**
 * @brief JSON-RPC server with scheduling support
 *
 * Extends Server with JSON-RPC method dispatch and time-based scheduling.
 * Supports immediate and scheduled execution via "timestamp" field in requests.
 *
 * Architecture:
 *   - Server: JSON-RPC I/O coordination (pull/push)
 *   - Rpc: Method registry and JSON-RPC execution
 *   - RpcScheduler: Time-based task execution
 *   - Remote: Coordinator that combines all three
 *
 * Usage:
 *   Remote remote(requestSource, responseSink);
 *   remote.bind("setLed", [](int i, int r, int g, int b) { leds[i] = CRGB(r,g,b); });
 *   remote.update(millis());  // pull + tick + push
 */
class Remote : public Server {
public:
    /// Configuration for method registration (forwards to Rpc::Config)
    template<typename Callable>
    using Config = typename fl::Rpc::Config<Callable>;

    // Import types from various sources
    using RpcResult = fl::RpcResult;      // from fl/remote/types.h
    using ClearFlags = fl::ClearFlags;    // from fl/remote/types.h
    using MethodInfo = fl::MethodInfo;    // from fl/remote/rpc/rpc.h
    using ParamInfo = fl::ParamInfo;      // from fl/remote/rpc/rpc.h

    /**
     * @brief Construct with I/O callbacks
     * @param source Function that returns next JSON-RPC request (or nullopt if none)
     * @param sink Function that handles outgoing JSON-RPC responses
     *
     * Example:
     *   fl::Remote remote(
     *       [&]() { return parseJsonRpcFromSerial(); },
     *       [](const fl::Json& r) { writeJsonRpcToSerial(r); }
     *   );
     */
    Remote(RequestSource source, ResponseSink sink);

    // Non-copyable, non-movable (lambda captures 'this')
    Remote(const Remote&) = delete;
    Remote(Remote&&) = delete;
    Remote& operator=(const Remote&) = delete;
    Remote& operator=(Remote&&) = delete;

    // =========================================================================
    // Method Registration
    // =========================================================================

    /// Register method with config (name, function, optional metadata)
    template<typename Callable>
    void bind(const Config<Callable>& config) {
        mRpc.bind(config);
    }

    /// Register method by name and function
    template<typename Callable>
    void bind(const char* name, Callable fn) {
        bind(fl::Rpc::Config<Callable>{name, fl::move(fn)});
    }

    /// Get bound method by name for direct C++ invocation
    template<class Sig>
    fl::BindResult<Sig> get(const char* name) const {
        return mRpc.get<Sig>(name);
    }

    /// Check if method is registered
    bool has(const fl::string& name) const;

    /// Unregister method by name
    bool unbind(const fl::string& name);

    // =========================================================================
    // RPC Processing
    // =========================================================================

    /// Process JSON-RPC request (with optional "timestamp" field for scheduling)
    /// Returns JSON-RPC response: {"result": ...} or {"error": {...}}
    fl::Json processRpc(const fl::Json& request);

    // =========================================================================
    // Server Coordination
    // =========================================================================

    /// Main update: pull + tick + push (overrides Server::update)
    size_t update(u32 currentTimeMs);

    /// Process scheduled calls (call regularly)
    size_t tick(u32 currentTimeMs);

    // Note: pull() and push() inherited from Server<fl::Json, fl::Json>

    // =========================================================================
    // Results and State
    // =========================================================================

    /// Get number of pending scheduled calls
    size_t pendingCount() const;

    /// Clear state (bitwise OR of ClearFlags)
    void clear(ClearFlags flags);

    // =========================================================================
    // Schema
    // =========================================================================

    /// Get method information for all registered methods
    fl::vector<MethodInfo> methods() const;

    /// Returns flat schema document
    /// Format: {"schema": [["methodName", "returnType", [["param1", "type1"], ...]], ...]}
    fl::Json schema() const {
        return mRpc.schema();
    }

    /// Get number of registered methods
    fl::size count() const {
        return mRpc.count();
    }

protected:
    void scheduleFunction(u32 timestamp, u32 receivedAt, const fl::Json& jsonRpcRequest);
    void recordResult(const fl::string& funcName, const fl::Json& result, u32 scheduledAt, u32 receivedAt, u32 executedAt, bool wasScheduled);

    // Method registry and execution
    fl::Rpc mRpc;

    // Generic task scheduler
    fl::RpcScheduler<> mScheduler;

    // RPC-specific result tracking
    fl::vector<RpcResult> mResults;

    // Note: mRequestSource, mResponseSink, mOutgoingQueue inherited from Server
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON
