#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/tuple.h"
#include "fl/stl/type_traits.h"
#include "fl/detail/rpc/type_conversion_result.h"
#include "fl/detail/rpc/json_to_type.h"

namespace fl {

// =============================================================================
// JsonArgConverter - Convert JSON array to typed tuple
// =============================================================================

template <typename Signature>
class JsonArgConverter;

template <typename R, typename... Args>
class JsonArgConverter<R(Args...)> {
public:
    using args_tuple = fl::tuple<Args...>;
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
                           fl::to_string(static_cast<int64_t>(sizeof...(Args))) +
                           ", got " + fl::to_string(static_cast<int64_t>(count)));
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

        using ArgType = typename fl::tuple_element<I, args_tuple>::type;
        // C++11 compatible: avoid structured bindings
        fl::tuple<ArgType, TypeConversionResult> convTuple = detail::JsonToType<ArgType>::convert(jsonArgs[I]);
        ArgType value = fl::get<0>(convTuple);
        TypeConversionResult convResult = fl::get<1>(convTuple);

        fl::get<I>(tuple) = value;

        // Add argument index to warnings/errors
        for (fl::size i = 0; i < convResult.warnings().size(); i++) {
            result.addWarning("arg " + fl::to_string(static_cast<int64_t>(I)) + ": " + convResult.warnings()[i]);
        }
        if (convResult.hasError()) {
            result.setError("arg " + fl::to_string(static_cast<int64_t>(I)) + ": " + convResult.errorMessage());
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
                           fl::to_string(static_cast<int64_t>(jsonArgs.size())));
        }

        return fl::make_tuple(tuple, result);
    }
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON
