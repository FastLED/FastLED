#pragma once

#include "fl/stl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/stdint.h"  // IWYU pragma: keep
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/unordered_map.h"  // IWYU pragma: keep
#include "fl/remote/rpc/rpc_invokers.h"  // IWYU pragma: keep
#include "fl/remote/rpc/rpc_mode.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declaration
class ResponseSend;

namespace detail {

// =============================================================================
// CallableHolderBase - Type-erased holder for typed callables
// =============================================================================

struct CallableHolderBase {
    virtual ~CallableHolderBase() FL_NOEXCEPT = default;
};

// =============================================================================
// TypedCallableHolder - Holds a typed callable
// =============================================================================

template<typename Sig>
struct TypedCallableHolder : CallableHolderBase {
    fl::function<Sig> mFn;
    TypedCallableHolder(fl::function<Sig> fn) : mFn(fn) {}
};

// =============================================================================
// RpcEntry - Registry entry containing method metadata and invokers
// =============================================================================

struct RpcEntry {
    const void* mTypeTag = nullptr;
    fl::shared_ptr<ErasedInvoker> mInvoker;
    fl::shared_ptr<CallableHolderBase> mTypedCallable;
    fl::shared_ptr<ErasedSchemaGenerator> mSchemaGenerator;
    fl::string mDescription;
    fl::vector<fl::string> mTags;
    fl::RpcMode mMode = fl::RpcMode::SYNC;  // Default to synchronous

    // For response-aware async methods (with ResponseSend& parameter)
    fl::function<void(ResponseSend&, const json&)> mResponseAwareFn;
    bool mIsResponseAware = false;
};

// =============================================================================
// makeJsonRpcError - Helper to create JSON-RPC error responses
// =============================================================================

inline json makeJsonRpcError(int code, const fl::string& message, const json& id) {
    json response = json::object();
    response.set("jsonrpc", "2.0");

    json error = json::object();
    error.set("code", code);
    error.set("message", message);
    response.set("error", error);

    if (id.has_value()) {
        response.set("id", id);
    }

    return response;
}

} // namespace detail
} // namespace fl

#endif // FASTLED_ENABLE_JSON
