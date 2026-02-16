#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/function.h"
#include "fl/stl/tuple.h"
#include "fl/remote/rpc/response_send.h"
#include "fl/remote/rpc/response_aware_traits.h"
#include "fl/remote/rpc/type_conversion_result.h"
#include "fl/remote/rpc/typed_rpc_binding.h"
#include "fl/remote/rpc/rpc_invokers.h"

namespace fl {
namespace detail {

// =============================================================================
// ResponseAwareInvoker - Invokes response-aware functions (with ResponseSend&)
// =============================================================================

/**
 * @brief Invoker for response-aware RPC methods
 *
 * Handles methods with signature: R(ResponseSend&, Args...)
 *
 * Steps:
 * 1. Create ResponseSend instance with request ID + response sink
 * 2. Convert JSON params to C++ arguments (Args...)
 * 3. Invoke user function with ResponseSend& + converted args
 * 4. Handle ACK sending for ASYNC/ASYNC_STREAM modes
 *
 * The ResponseSend& parameter is NOT part of the JSON params - it's injected automatically.
 */

// Forward declaration
template<typename Sig>
class ResponseAwareInvoker;

// Specialization for non-void return types
template<typename R, typename... Args>
class ResponseAwareInvoker<R(Args...)> : public ErasedInvoker {
public:
    ResponseAwareInvoker(fl::function<R(ResponseSend&, Args...)> fn,
                        const fl::Json& requestId,
                        fl::function<void(const fl::Json&)> responseSink)
        : mFn(fl::move(fn)), mRequestId(requestId), mResponseSink(fl::move(responseSink)) {}

    fl::tuple<TypeConversionResult, Json> invoke(const Json& args) override {
        // Create ResponseSend instance
        ResponseSend responseSend(mRequestId, mResponseSink);

        // Use TypedRpcBinding to convert JSON params to C++ args (without ResponseSend&)
        TypedRpcBinding<R(Args...)> binding([this, &responseSend](Args... cppArgs) -> R {
            // Invoke user function with ResponseSend& + converted args
            return mFn(responseSend, fl::forward<Args>(cppArgs)...);
        });

        return binding.invokeWithReturn(args);
    }

private:
    fl::function<R(ResponseSend&, Args...)> mFn;
    fl::Json mRequestId;
    fl::function<void(const fl::Json&)> mResponseSink;
};

// Specialization for void return type
template<typename... Args>
class ResponseAwareInvoker<void(Args...)> : public ErasedInvoker {
public:
    ResponseAwareInvoker(fl::function<void(ResponseSend&, Args...)> fn,
                        const fl::Json& requestId,
                        fl::function<void(const fl::Json&)> responseSink)
        : mFn(fl::move(fn)), mRequestId(requestId), mResponseSink(fl::move(responseSink)) {}

    fl::tuple<TypeConversionResult, Json> invoke(const Json& args) override {
        // Create ResponseSend instance
        ResponseSend responseSend(mRequestId, mResponseSink);

        // Use TypedRpcBinding to convert JSON params to C++ args (without ResponseSend&)
        TypedRpcBinding<void(Args...)> binding([this, &responseSend](Args... cppArgs) -> void {
            // Invoke user function with ResponseSend& + converted args
            mFn(responseSend, fl::forward<Args>(cppArgs)...);
        });

        TypeConversionResult result = binding.invoke(args);
        return fl::make_tuple(result, Json(nullptr));
    }

private:
    fl::function<void(ResponseSend&, Args...)> mFn;
    fl::Json mRequestId;
    fl::function<void(const fl::Json&)> mResponseSink;
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ENABLE_JSON
