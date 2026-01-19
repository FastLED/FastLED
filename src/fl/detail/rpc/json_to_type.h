#pragma once

#include "fl/json.h"
#include "fl/detail/rpc/type_conversion_result.h"
#include "fl/detail/rpc/json_visitors.h"
#include "fl/stl/tuple.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/string.h"

#if FASTLED_ENABLE_JSON

namespace fl {
namespace detail {

// =============================================================================
// JsonToType - Primary template and specializations using visitor pattern
// =============================================================================

// Primary template declaration for JsonToType (with two template parameters for SFINAE)
template <typename T, typename Enable = void>
struct JsonToType {
    static fl::tuple<T, TypeConversionResult> convert(const Json& j) {
        (void)j;
        TypeConversionResult result;
        result.setError("unsupported type for JSON conversion");
        return fl::make_tuple(T(), result);
    }
};

// Integer conversion (excluding bool) - uses visitor pattern
template <typename T>
struct JsonToType<T, typename fl::enable_if<fl::is_integral<T>::value && !fl::is_same<T, bool>::value>::type> {
    static fl::tuple<T, TypeConversionResult> convert(const Json& j) {
        // Access internal JsonValue directly instead of re-parsing
        const JsonValue* val = j.internal_value();
        if (!val) {
            TypeConversionResult result;
            result.setError("failed to access JSON value");
            return fl::make_tuple(T(), result);
        }

        JsonToIntegerVisitor<T> visitor;
        val->data.visit(visitor);
        return fl::make_tuple(visitor.mValue, visitor.mResult);
    }
};

// Boolean conversion - uses visitor pattern
template <>
struct JsonToType<bool, void> {
    static fl::tuple<bool, TypeConversionResult> convert(const Json& j) {
        // Access internal JsonValue directly instead of re-parsing
        const JsonValue* val = j.internal_value();
        if (!val) {
            TypeConversionResult result;
            result.setError("failed to access JSON value");
            return fl::make_tuple(false, result);
        }

        JsonToBoolVisitor visitor;
        val->data.visit(visitor);
        return fl::make_tuple(visitor.mValue, visitor.mResult);
    }
};

// Float/double conversion - uses visitor pattern
template <typename T>
struct JsonToType<T, typename fl::enable_if<fl::is_floating_point<T>::value>::type> {
    static fl::tuple<T, TypeConversionResult> convert(const Json& j) {
        // Access internal JsonValue directly instead of re-parsing
        const JsonValue* val = j.internal_value();
        if (!val) {
            TypeConversionResult result;
            result.setError("failed to access JSON value");
            return fl::make_tuple(T(), result);
        }

        JsonToFloatVisitor<T> visitor;
        val->data.visit(visitor);
        return fl::make_tuple(visitor.mValue, visitor.mResult);
    }
};

// String conversion - uses visitor pattern
template <>
struct JsonToType<fl::string, void> {
    static fl::tuple<fl::string, TypeConversionResult> convert(const Json& j) {
        // Access internal JsonValue directly instead of re-parsing
        const JsonValue* val = j.internal_value();
        if (!val) {
            TypeConversionResult result;
            result.setError("failed to access JSON value");
            return fl::make_tuple(fl::string(), result);
        }

        JsonToStringVisitor visitor;
        val->data.visit(visitor);
        return fl::make_tuple(visitor.mValue, visitor.mResult);
    }
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ENABLE_JSON
