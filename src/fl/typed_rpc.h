#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/tuple.h"
#include "fl/stl/optional.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/function.h"
#include "fl/stl/cstdlib.h"

namespace fl {

// =============================================================================
// Index sequence implementation for tuple unpacking
// =============================================================================

template <fl::size... Is>
struct index_sequence {
    using type = index_sequence;
    static constexpr fl::size size() { return sizeof...(Is); }
};

template <fl::size N, fl::size... Is>
struct make_index_sequence_impl : make_index_sequence_impl<N - 1, N - 1, Is...> {};

template <fl::size... Is>
struct make_index_sequence_impl<0, Is...> {
    using type = index_sequence<Is...>;
};

template <fl::size N>
using make_index_sequence = typename make_index_sequence_impl<N>::type;

// =============================================================================
// TypeConversionResult - Warning/Error tracking for type conversions
// =============================================================================

class TypeConversionResult {
public:
    TypeConversionResult() : mHasError(false) {}

    static TypeConversionResult success() {
        return TypeConversionResult();
    }

    static TypeConversionResult warning(const fl::string& msg) {
        TypeConversionResult result;
        result.mWarnings.push_back(msg);
        return result;
    }

    static TypeConversionResult error(const fl::string& msg) {
        TypeConversionResult result;
        result.mHasError = true;
        result.mErrorMessage = msg;
        return result;
    }

    bool ok() const { return !mHasError; }
    bool hasWarning() const { return !mWarnings.empty(); }
    bool hasError() const { return mHasError; }

    const fl::vector<fl::string>& warnings() const { return mWarnings; }
    const fl::string& errorMessage() const { return mErrorMessage; }

    void addWarning(const fl::string& msg) {
        mWarnings.push_back(msg);
    }

    void setError(const fl::string& msg) {
        mHasError = true;
        mErrorMessage = msg;
    }

    // Merge another result into this one
    void merge(const TypeConversionResult& other) {
        for (fl::size i = 0; i < other.mWarnings.size(); i++) {
            mWarnings.push_back(other.mWarnings[i]);
        }
        if (other.mHasError) {
            mHasError = true;
            mErrorMessage = other.mErrorMessage;
        }
    }

private:
    fl::vector<fl::string> mWarnings;
    fl::string mErrorMessage;
    bool mHasError;
};

// =============================================================================
// Type conversion helpers - convert JSON value to typed C++ value
// =============================================================================

namespace detail {

// Helper to check if JSON is the exact type we want
template <typename T>
struct is_json_exact_match {
    static bool check(const Json& j) { (void)j; return false; }
};

template <>
struct is_json_exact_match<int> {
    static bool check(const Json& j) { return j.is_int(); }
};

template <>
struct is_json_exact_match<int64_t> {
    static bool check(const Json& j) { return j.is_int(); }
};

template <>
struct is_json_exact_match<float> {
    static bool check(const Json& j) { return j.is_number(); }
};

template <>
struct is_json_exact_match<double> {
    static bool check(const Json& j) { return j.is_number(); }
};

template <>
struct is_json_exact_match<bool> {
    static bool check(const Json& j) { return j.is_bool(); }
};

template <>
struct is_json_exact_match<fl::string> {
    static bool check(const Json& j) { return j.is_string(); }
};

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

// Integer conversion (excluding bool)
template <typename T>
struct JsonToType<T, typename fl::enable_if<fl::is_integral<T>::value && !fl::is_same<T, bool>::value>::type> {
    static fl::tuple<T, TypeConversionResult> convert(const Json& j) {
        TypeConversionResult result;
        T value = T();

        // Check is_bool() first since is_int() may return true for bools
        if (j.is_bool()) {
            value = j.as_bool().value_or(false) ? 1 : 0;
            result.addWarning("bool converted to int " + fl::to_string(static_cast<int64_t>(value)));
        } else if (j.is_int()) {
            int64_t raw = j.as_int().value_or(0);
            value = static_cast<T>(raw);

            // Check for overflow/truncation
            if (static_cast<int64_t>(value) != raw) {
                result.addWarning("integer overflow/truncation: " + fl::to_string(raw) +
                                  " converted to " + fl::to_string(static_cast<int64_t>(value)));
            }
        } else if (j.is_float()) {
            double raw = j.as_float().value_or(0.0f);
            value = static_cast<T>(raw);

            // Float to int truncation warning
            if (raw != static_cast<double>(value)) {
                result.addWarning("float " + fl::to_string(raw) +
                                  " truncated to int " + fl::to_string(static_cast<int64_t>(value)));
            }
        } else if (j.is_string()) {
            fl::string str = j.as_string().value_or("");
            // Try to parse the string as an integer
            char* end = 0;
            long parsed = fl::strtol(str.c_str(), &end, 10);
            if (end != str.c_str() && *end == '\0') {
                value = static_cast<T>(parsed);
                result.addWarning("string '" + str + "' parsed to int " + fl::to_string(static_cast<int64_t>(value)));
            } else {
                result.setError("cannot parse string '" + str + "' as integer");
            }
        } else if (j.is_object()) {
            result.setError("cannot convert object to integer");
        } else if (j.is_array()) {
            result.setError("cannot convert array to integer");
        } else if (j.is_null()) {
            result.setError("cannot convert null to integer");
        } else {
            result.setError("unknown JSON type for integer conversion");
        }

        return fl::make_tuple(value, result);
    }
};

// Boolean conversion (full specialization, no Enable parameter)
template <>
struct JsonToType<bool, void> {
    static fl::tuple<bool, TypeConversionResult> convert(const Json& j) {
        TypeConversionResult result;
        bool value = false;

        if (j.is_bool()) {
            value = j.as_bool().value_or(false);
        } else if (j.is_int()) {
            int64_t raw = j.as_int().value_or(0);
            value = raw != 0;
            result.addWarning("int " + fl::to_string(raw) + " converted to bool " + (value ? "true" : "false"));
        } else if (j.is_float()) {
            double raw = j.as_float().value_or(0.0f);
            value = raw != 0.0;
            result.addWarning("float " + fl::to_string(raw) + " converted to bool " + (value ? "true" : "false"));
        } else if (j.is_string()) {
            fl::string str = j.as_string().value_or("");
            if (str == "true" || str == "1" || str == "yes") {
                value = true;
                result.addWarning("string '" + str + "' parsed as bool true");
            } else if (str == "false" || str == "0" || str == "no") {
                value = false;
                result.addWarning("string '" + str + "' parsed as bool false");
            } else {
                result.setError("cannot parse string '" + str + "' as bool");
            }
        } else if (j.is_object()) {
            result.setError("cannot convert object to bool");
        } else if (j.is_array()) {
            result.setError("cannot convert array to bool");
        } else if (j.is_null()) {
            result.setError("cannot convert null to bool");
        } else {
            result.setError("unknown JSON type for bool conversion");
        }

        return fl::make_tuple(value, result);
    }
};

// Float/double conversion
template <typename T>
struct JsonToType<T, typename fl::enable_if<fl::is_floating_point<T>::value>::type> {
    static fl::tuple<T, TypeConversionResult> convert(const Json& j) {
        TypeConversionResult result;
        T value = T();

        if (j.is_float()) {
            value = static_cast<T>(j.as_float().value_or(0.0f));
        } else if (j.is_int()) {
            int64_t raw = j.as_int().value_or(0);
            value = static_cast<T>(raw);
            // Large integers may lose precision
            if (sizeof(T) < 8 && (raw > 16777216 || raw < -16777216)) {
                result.addWarning("large int " + fl::to_string(raw) + " may lose precision as float");
            }
        } else if (j.is_bool()) {
            value = j.as_bool().value_or(false) ? T(1) : T(0);
            result.addWarning("bool converted to float " + fl::to_string(static_cast<double>(value)));
        } else if (j.is_string()) {
            fl::string str = j.as_string().value_or("");
            char* end = 0;
            double parsed = fl::strtod(str.c_str(), &end);
            if (end != str.c_str() && *end == '\0') {
                value = static_cast<T>(parsed);
                result.addWarning("string '" + str + "' parsed to float " + fl::to_string(static_cast<double>(value)));
            } else {
                result.setError("cannot parse string '" + str + "' as float");
            }
        } else if (j.is_object()) {
            result.setError("cannot convert object to float");
        } else if (j.is_array()) {
            result.setError("cannot convert array to float");
        } else if (j.is_null()) {
            result.setError("cannot convert null to float");
        } else {
            result.setError("unknown JSON type for float conversion");
        }

        return fl::make_tuple(value, result);
    }
};

// String conversion (full specialization, no Enable parameter)
template <>
struct JsonToType<fl::string, void> {
    static fl::tuple<fl::string, TypeConversionResult> convert(const Json& j) {
        TypeConversionResult result;
        fl::string value;

        if (j.is_string()) {
            value = j.as_string().value_or("");
        } else if (j.is_int()) {
            int64_t raw = j.as_int().value_or(0);
            value = fl::to_string(raw);
            result.addWarning("int " + value + " converted to string");
        } else if (j.is_float()) {
            double raw = j.as_float().value_or(0.0f);
            value = fl::to_string(raw);
            result.addWarning("float " + value + " converted to string");
        } else if (j.is_bool()) {
            value = j.as_bool().value_or(false) ? "true" : "false";
            result.addWarning("bool converted to string '" + value + "'");
        } else if (j.is_null()) {
            value = "null";
            result.addWarning("null converted to string 'null'");
        } else if (j.is_object()) {
            result.setError("cannot convert object to string");
        } else if (j.is_array()) {
            result.setError("cannot convert array to string");
        } else {
            result.setError("unknown JSON type for string conversion");
        }

        return fl::make_tuple(value, result);
    }
};

} // namespace detail

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

// =============================================================================
// Return type to JSON converter
// =============================================================================

namespace detail {

template <typename T>
struct TypeToJson {
    static Json convert(const T& value) {
        return Json(value);
    }
};

template <>
struct TypeToJson<fl::string> {
    static Json convert(const fl::string& value) {
        return Json(value.c_str());
    }
};

template <>
struct TypeToJson<void> {
    static Json convert() {
        return Json(nullptr);
    }
};

} // namespace detail

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

    TypedRpcBinding(FunctionType fn) : mFunction(fn) {}

    TypeConversionResult invoke(const Json& jsonArgs) {
        // C++11 compatible: avoid structured bindings
        fl::tuple<fl::tuple<Args...>, TypeConversionResult> convTuple = Converter::convert(jsonArgs);
        fl::tuple<Args...> tuple = fl::get<0>(convTuple);
        TypeConversionResult result = fl::get<1>(convTuple);
        if (!result.ok()) {
            return result;
        }
        invokeImpl(tuple, make_index_sequence<sizeof...(Args)>{});
        return result;
    }

private:
    template <fl::size... Is>
    void invokeImpl(fl::tuple<Args...>& args, index_sequence<Is...>) {
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

    TypedRpcBinding(FunctionType fn) : mFunction(fn) {}

    TypeConversionResult invoke(const Json& jsonArgs) {
        // C++11 compatible: avoid structured bindings
        fl::tuple<fl::tuple<Args...>, TypeConversionResult> convTuple = Converter::convert(jsonArgs);
        fl::tuple<Args...> tuple = fl::get<0>(convTuple);
        TypeConversionResult result = fl::get<1>(convTuple);
        if (!result.ok()) {
            return result;
        }
        invokeImpl(tuple, make_index_sequence<sizeof...(Args)>{});
        return result;
    }

    fl::tuple<TypeConversionResult, Json> invokeWithReturn(const Json& jsonArgs) {
        // C++11 compatible: avoid structured bindings
        fl::tuple<fl::tuple<Args...>, TypeConversionResult> convTuple = Converter::convert(jsonArgs);
        fl::tuple<Args...> tuple = fl::get<0>(convTuple);
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
    void invokeImpl(fl::tuple<Args...>& args, index_sequence<Is...>) {
        mFunction(fl::get<Is>(args)...);
    }

    template <fl::size... Is>
    R invokeImplWithReturn(fl::tuple<Args...>& args, index_sequence<Is...>) {
        return mFunction(fl::get<Is>(args)...);
    }

    FunctionType mFunction;
};

} // namespace fl

#endif // FASTLED_ENABLE_JSON
