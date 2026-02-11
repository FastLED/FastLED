#pragma once

#include "fl/json.h"
#include "fl/remote/rpc/type_conversion_result.h"
#include "fl/remote/rpc/json_visitors.h"
#include "fl/stl/tuple.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/string.h"
#include "fl/stl/move.h"
#include "fl/stl/vector.h"
#include "fl/stl/set.h"
#include "fl/stl/span.h"

#if FASTLED_ENABLE_JSON

namespace fl {

// Helper wrapper for const char* parameters in RPC
// Stores fl::string internally, provides implicit conversion to const char*
struct ConstCharPtrWrapper {
    fl::string value;

    ConstCharPtrWrapper() = default;
    ConstCharPtrWrapper(const fl::string& s) : value(s) {}
    ConstCharPtrWrapper(fl::string&& s) : value(fl::move(s)) {}

    operator const char*() const { return value.c_str(); }
    const char* c_str() const { return value.c_str(); }
};

// Helper wrapper for span<const T> parameters in RPC
// Stores fl::vector<T> internally, provides implicit conversion to span<const T>
template <typename T>
struct ConstSpanWrapper {
    fl::vector<T> value;

    ConstSpanWrapper() = default;
    ConstSpanWrapper(fl::vector<T>&& v) : value(fl::move(v)) {}
    ConstSpanWrapper(const fl::vector<T>& v) : value(v) {}

    operator fl::span<const T>() const {
        return fl::span<const T>(value.data(), value.size());
    }

    fl::span<const T> get() const {
        return fl::span<const T>(value.data(), value.size());
    }
};

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
            return fl::make_tuple(bool(false), result);
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

// Json identity conversion - pass Json through unchanged
// This enables RPC methods to accept fl::Json parameters for dynamic typing
template <>
struct JsonToType<fl::Json, void> {
    static fl::tuple<fl::Json, TypeConversionResult> convert(const Json& j) {
        TypeConversionResult result;  // Success by default
        return fl::make_tuple(j, result);
    }
};

// ConstCharPtrWrapper conversion - converts JSON string to wrapper
// The wrapper stores fl::string and converts to const char* on access
template <>
struct JsonToType<fl::ConstCharPtrWrapper, void> {
    static fl::tuple<fl::ConstCharPtrWrapper, TypeConversionResult> convert(const Json& j) {
        // Reuse the string converter
        auto stringResult = JsonToType<fl::string>::convert(j);
        fl::string str = fl::get<0>(stringResult);
        TypeConversionResult result = fl::get<1>(stringResult);

        return fl::make_tuple(fl::ConstCharPtrWrapper(fl::move(str)), result);
    }
};

// ConstSpanWrapper conversion - converts JSON array to vector wrapper
template <typename T>
struct JsonToType<fl::ConstSpanWrapper<T>, void> {
    static fl::tuple<fl::ConstSpanWrapper<T>, TypeConversionResult> convert(const Json& j) {
        TypeConversionResult result;

        if (!j.is_array()) {
            result.setError("expected array for span parameter");
            return fl::make_tuple(fl::ConstSpanWrapper<T>(), result);
        }

        fl::vector<T> vec;
        for (fl::size i = 0; i < j.size(); i++) {
            auto elemResult = JsonToType<T>::convert(j[i]);
            T elem = fl::get<0>(elemResult);
            TypeConversionResult elemConvResult = fl::get<1>(elemResult);

            if (elemConvResult.hasError()) {
                result.setError("element " + fl::to_string(static_cast<fl::i64>(i)) + ": " + elemConvResult.errorMessage());
                return fl::make_tuple(fl::ConstSpanWrapper<T>(), result);
            }

            vec.push_back(elem);
        }

        return fl::make_tuple(fl::ConstSpanWrapper<T>(fl::move(vec)), result);
    }
};

// fl::vector<T> conversion - converts JSON array to vector
// Works for any T that has a JsonToType specialization
template <typename T>
struct JsonToType<fl::vector<T>, void> {
    static fl::tuple<fl::vector<T>, TypeConversionResult> convert(const Json& j) {
        TypeConversionResult result;

        if (!j.is_array()) {
            result.setError("expected array for vector parameter");
            return fl::make_tuple(fl::vector<T>(), result);
        }

        fl::vector<T> vec;
        for (fl::size i = 0; i < j.size(); i++) {
            auto elemResult = JsonToType<T>::convert(j[i]);
            T elem = fl::get<0>(elemResult);
            TypeConversionResult elemConvResult = fl::get<1>(elemResult);

            if (elemConvResult.hasError()) {
                result.setError("element " + fl::to_string(static_cast<fl::i64>(i)) + ": " + elemConvResult.errorMessage());
                return fl::make_tuple(fl::vector<T>(), result);
            }

            vec.push_back(fl::move(elem));
        }

        return fl::make_tuple(fl::move(vec), result);
    }
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ENABLE_JSON
