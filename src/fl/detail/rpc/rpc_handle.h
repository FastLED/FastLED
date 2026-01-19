#pragma once

#include "fl/stl/function.h"
#include "fl/stl/type_traits.h"

namespace fl {

// =============================================================================
// RpcFn - Type alias for typed RPC callables
// =============================================================================

template<class Sig>
using RpcFn = fl::function<Sig>;

// =============================================================================
// RpcHandle - Callable handle returned from method() for immediate use
// =============================================================================

template<typename Sig>
class RpcHandle;

// Specialization for function signatures
template<typename R, typename... Args>
class RpcHandle<R(Args...)> {
public:
    using signature = R(Args...);
    using function_type = fl::function<R(Args...)>;

    // Default constructor - creates invalid handle
    RpcHandle() : mValid(false) {}

    // Constructor from function (internal use by RpcFactory)
    explicit RpcHandle(function_type fn) : mFn(fn), mValid(true) {}

    // Call operator - invoke the underlying function
    template<typename... CallArgs>
    R operator()(CallArgs&&... args) const {
        return mFn(fl::forward<CallArgs>(args)...);
    }

    // Validity check
    explicit operator bool() const { return mValid && static_cast<bool>(mFn); }

    // Get underlying function
    function_type get() const { return mFn; }

    // Implicit conversion to fl::function
    operator function_type() const { return mFn; }

private:
    function_type mFn;
    bool mValid;
};

} // namespace fl
