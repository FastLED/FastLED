#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/tuple.h"
#include "fl/stl/function.h"

namespace fl {

// =============================================================================
// Function signature traits - extract return type and argument types
// =============================================================================

template <typename T>
struct function_traits;

// For free functions
template <typename R, typename... Args>
struct function_traits<R(Args...)> {
    using return_type = R;
    using args_tuple = fl::tuple<Args...>;
    static constexpr fl::size arity = sizeof...(Args);

    template <fl::size N>
    using arg = typename fl::tuple_element<N, args_tuple>::type;
};

// For function pointers
template <typename R, typename... Args>
struct function_traits<R(*)(Args...)> : function_traits<R(Args...)> {};

// For fl::function
template <typename R, typename... Args>
struct function_traits<fl::function<R(Args...)>> : function_traits<R(Args...)> {};

} // namespace fl
