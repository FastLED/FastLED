#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/unordered_map.h"
#include "fl/detail/rpc/rpc_invokers.h"

namespace fl {
namespace detail {

// =============================================================================
// CallableHolderBase - Type-erased holder for typed callables
// =============================================================================

struct CallableHolderBase {
    virtual ~CallableHolderBase() = default;
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
};

// =============================================================================
// makeJsonRpcError - Helper to create JSON-RPC error responses
// =============================================================================

inline Json makeJsonRpcError(int code, const fl::string& message, const Json& id) {
    Json response = Json::object();
    response.set("jsonrpc", "2.0");

    Json error = Json::object();
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
