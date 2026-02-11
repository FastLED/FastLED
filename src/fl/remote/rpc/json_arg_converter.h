#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/tuple.h"
#include "fl/stl/type_traits.h"
#include "fl/remote/rpc/type_conversion_result.h"
#include "fl/remote/rpc/json_to_type.h"
#include "fl/stl/span.h"

namespace fl {

// Type mapper for RPC parameters:
// - const char* / char* → ConstCharPtrWrapper (stores string, converts to ptr)
// - span<const T> → ConstSpanWrapper<T> (stores vector, provides span view)
// - Other types → strip const/reference
template <typename T>
struct rpc_storage_type {
    using type = typename fl::remove_cv<typename fl::remove_reference<T>::type>::type;
};

template <>
struct rpc_storage_type<const char*> {
    using type = ConstCharPtrWrapper;
};

template <>
struct rpc_storage_type<char*> {
    using type = ConstCharPtrWrapper;
};

// Specializations for span<const T>
template <typename T>
struct rpc_storage_type<fl::span<const T>> {
    using type = fl::ConstSpanWrapper<T>;
};

// Common span specializations for explicit types
template <>
struct rpc_storage_type<fl::span<const int>> {
    using type = fl::ConstSpanWrapper<int>;
};

template <>
struct rpc_storage_type<fl::span<const float>> {
    using type = fl::ConstSpanWrapper<float>;
};

template <>
struct rpc_storage_type<fl::span<const double>> {
    using type = fl::ConstSpanWrapper<double>;
};

template <>
struct rpc_storage_type<fl::span<const fl::string>> {
    using type = fl::ConstSpanWrapper<fl::string>;
};

// =============================================================================
// JsonArgConverter - Convert JSON array to typed tuple
// =============================================================================

template <typename Signature>
class JsonArgConverter;

template <typename R, typename... Args>
class JsonArgConverter<R(Args...)> {
public:
    // Map Args to storage types (handles const char* → ConstCharPtrWrapper)
    using args_tuple = fl::tuple<typename rpc_storage_type<Args>::type...>;
    static constexpr fl::size argCount() { return sizeof...(Args); }

    static fl::tuple<args_tuple, TypeConversionResult> convert(const Json& jsonArgs) {
        TypeConversionResult result;
        args_tuple tuple;

        if (!jsonArgs.is_array()) {
            result.setError("arguments must be a JSON array");
            return fl::make_tuple(tuple, result);
        }

        fl::size count = jsonArgs.size();
        if (count != sizeof...(Args)) {
            result.setError("argument count mismatch: expected " +
                           fl::to_string(static_cast<i64>(sizeof...(Args))) +
                           ", got " + fl::to_string(static_cast<i64>(count)));
            return fl::make_tuple(tuple, result);
        }

        convertArgs(jsonArgs, tuple, result, make_index_sequence<sizeof...(Args)>{});
        return fl::make_tuple(tuple, result);
    }

private:
    template <fl::size... Is>
    static void convertArgs(const Json& jsonArgs, args_tuple& tuple,
                           TypeConversionResult& result, index_sequence<Is...>) {
        // Fold expression equivalent using initializer list
        int dummy[] = {0, (convertArg<Is>(jsonArgs, tuple, result), 0)...};
        (void)dummy;
    }

    template <fl::size I>
    static void convertArg(const Json& jsonArgs, args_tuple& tuple, TypeConversionResult& result) {
        if (result.hasError()) return;

        // Use the stripped type (no const/ref) for conversion and storage
        using StorageType = typename fl::tuple_element<I, args_tuple>::type;
        // C++11 compatible: avoid structured bindings
        fl::tuple<StorageType, TypeConversionResult> convTuple = detail::JsonToType<StorageType>::convert(jsonArgs[I]);
        StorageType value = fl::get<0>(convTuple);
        TypeConversionResult convResult = fl::get<1>(convTuple);

        fl::get<I>(tuple) = value;

        // Add argument index to warnings/errors
        for (fl::size i = 0; i < convResult.warnings().size(); i++) {
            result.addWarning("arg " + fl::to_string(static_cast<i64>(I)) + ": " + convResult.warnings()[i]);
        }
        if (convResult.hasError()) {
            result.setError("arg " + fl::to_string(static_cast<i64>(I)) + ": " + convResult.errorMessage());
        }
    }
};

// Specialization for void() - no arguments
template <typename R>
class JsonArgConverter<R()> {
public:
    using args_tuple = fl::tuple<>;
    static constexpr fl::size argCount() { return 0; }

    static fl::tuple<args_tuple, TypeConversionResult> convert(const Json& jsonArgs) {
        TypeConversionResult result;
        args_tuple tuple;

        if (!jsonArgs.is_array()) {
            result.setError("arguments must be a JSON array");
            return fl::make_tuple(tuple, result);
        }

        if (jsonArgs.size() != 0) {
            result.setError("argument count mismatch: expected 0, got " +
                           fl::to_string(static_cast<i64>(jsonArgs.size())));
        }

        return fl::make_tuple(tuple, result);
    }
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON
