#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/stdint.h"
#include "fl/stl/tuple.h"
#include "fl/stl/function.h"
#include "fl/stl/type_traits.h"
#include "fl/remote/rpc/type_conversion_result.h"
#include "fl/remote/rpc/type_to_json.h"
#include "fl/remote/rpc/json_arg_converter.h"

namespace fl {

// =============================================================================
// TypedRpcBinding - Wraps a typed function for RPC invocation
// =============================================================================

template <typename Signature>
class TypedRpcBinding;

// Specialization for void return type
template <typename... Args>
class TypedRpcBinding<void(Args...)> {
public:
    using FunctionType = fl::function<void(Args...)>;
    using Converter = JsonArgConverter<void(Args...)>;
    using StorageTuple = typename Converter::args_tuple;  // Stripped types for storage

    TypedRpcBinding(FunctionType fn) : mFunction(fn) {}

    TypeConversionResult invoke(const Json& jsonArgs) {
        // C++11 compatible: avoid structured bindings
        fl::tuple<StorageTuple, TypeConversionResult> convTuple = Converter::convert(jsonArgs);
        StorageTuple tuple = fl::get<0>(convTuple);
        TypeConversionResult result = fl::get<1>(convTuple);
        if (!result.ok()) {
            return result;
        }
        invokeImpl(tuple, make_index_sequence<sizeof...(Args)>{});
        return result;
    }

private:
    template <fl::size... Is>
    void invokeImpl(StorageTuple& args, index_sequence<Is...>) {
        // fl::get returns T& from tuple<T>, which converts to const T& for const ref parameters
        mFunction(fl::get<Is>(args)...);
    }

    FunctionType mFunction;
};

// Specialization for non-void return type
template <typename R, typename... Args>
class TypedRpcBinding<R(Args...)> {
public:
    using FunctionType = fl::function<R(Args...)>;
    using Converter = JsonArgConverter<R(Args...)>;
    using StorageTuple = typename Converter::args_tuple;  // Stripped types for storage

    TypedRpcBinding(FunctionType fn) : mFunction(fn) {}

    TypeConversionResult invoke(const Json& jsonArgs) {
        // C++11 compatible: avoid structured bindings
        fl::tuple<StorageTuple, TypeConversionResult> convTuple = Converter::convert(jsonArgs);
        StorageTuple tuple = fl::get<0>(convTuple);
        TypeConversionResult result = fl::get<1>(convTuple);
        if (!result.ok()) {
            return result;
        }
        invokeImpl(tuple, make_index_sequence<sizeof...(Args)>{});
        return result;
    }

    fl::tuple<TypeConversionResult, Json> invokeWithReturn(const Json& jsonArgs) {
        // C++11 compatible: avoid structured bindings
        fl::tuple<StorageTuple, TypeConversionResult> convTuple = Converter::convert(jsonArgs);
        StorageTuple tuple = fl::get<0>(convTuple);
        TypeConversionResult result = fl::get<1>(convTuple);
        if (!result.ok()) {
            return fl::make_tuple(result, Json(nullptr));
        }
        R returnValue = invokeImplWithReturn(tuple, make_index_sequence<sizeof...(Args)>{});
        Json jsonResult = detail::TypeToJson<R>::convert(returnValue);
        return fl::make_tuple(result, jsonResult);
    }

private:
    template <fl::size... Is>
    void invokeImpl(StorageTuple& args, index_sequence<Is...>) {
        // fl::get returns T& from tuple<T>, which converts to const T& for const ref parameters
        mFunction(fl::get<Is>(args)...);
    }

    template <fl::size... Is>
    R invokeImplWithReturn(StorageTuple& args, index_sequence<Is...>) {
        // fl::get returns T& from tuple<T>, which converts to const T& for const ref parameters
        return mFunction(fl::get<Is>(args)...);
    }

    FunctionType mFunction;
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON
