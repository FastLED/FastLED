#pragma once

#include "fl/json.h"

#if FASTLED_ENABLE_JSON

#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/vector.h"

namespace fl {
namespace detail {

// =============================================================================
// TypeSchema - Generate JSON Schema for C++ types
// =============================================================================

// Primary template - unknown types
template <typename T, typename Enable = void>
struct TypeSchema {
    static const char* typeName() {
        return "unknown";
    }
};

// Integer types (excluding bool)
template <typename T>
struct TypeSchema<T, typename fl::enable_if<
    fl::is_integral<T>::value && !fl::is_same<T, bool>::value
>::type> {
    static const char* typeName() {
        return "integer";
    }
};

// Boolean type
template <>
struct TypeSchema<bool, void> {
    static const char* typeName() {
        return "boolean";
    }
};

// Floating point types
template <typename T>
struct TypeSchema<T, typename fl::enable_if<fl::is_floating_point<T>::value>::type> {
    static const char* typeName() {
        return "number";
    }
};

// String type
template <>
struct TypeSchema<fl::string, void> {
    static const char* typeName() {
        return "string";
    }
};

// Void type (for return types)
template <>
struct TypeSchema<void, void> {
    static const char* typeName() {
        return "void";
    }
};

// =============================================================================
// MethodSchema - Generate schema for a function signature
// =============================================================================

template <typename Sig>
struct MethodSchema;

// Specialization for non-void return
template <typename R, typename... Args>
struct MethodSchema<R(Args...)> {
    // Flat params: [["name", "type"], ...] optimized for low-memory devices
    static Json params(const fl::vector<fl::string>& names) {
        Json arr = Json::array();
        addParams<0, Args...>(arr, names);
        return arr;
    }

    static const char* resultTypeName() {
        return TypeSchema<R>::typeName();
    }

private:
    template <fl::size Index>
    static void addParams(Json& arr, const fl::vector<fl::string>& names) {
        // Base case: no more parameters
        (void)arr;
        (void)names;
    }

    template <fl::size Index, typename First, typename... Rest>
    static void addParams(Json& arr, const fl::vector<fl::string>& names) {
        Json param = Json::array();
        // Name
        if (Index < names.size() && !names[Index].empty()) {
            param.push_back(names[Index].c_str());
        } else {
            param.push_back("arg" + fl::to_string(static_cast<i64>(Index)));
        }
        // Type name
        param.push_back(TypeSchema<First>::typeName());
        arr.push_back(param);
        addParams<Index + 1, Rest...>(arr, names);
    }
};

// Specialization for void return
template <typename... Args>
struct MethodSchema<void(Args...)> {
    // Flat params: [["name", "type"], ...] optimized for low-memory devices
    static Json params(const fl::vector<fl::string>& names) {
        Json arr = Json::array();
        addParams<0, Args...>(arr, names);
        return arr;
    }

    static const char* resultTypeName() {
        return TypeSchema<void>::typeName();
    }

private:
    template <fl::size Index>
    static void addParams(Json& arr, const fl::vector<fl::string>& names) {
        // Base case: no more parameters
        (void)arr;
        (void)names;
    }

    template <fl::size Index, typename First, typename... Rest>
    static void addParams(Json& arr, const fl::vector<fl::string>& names) {
        Json param = Json::array();
        // Name
        if (Index < names.size() && !names[Index].empty()) {
            param.push_back(names[Index].c_str());
        } else {
            param.push_back("arg" + fl::to_string(static_cast<i64>(Index)));
        }
        // Type name
        param.push_back(TypeSchema<First>::typeName());
        arr.push_back(param);
        addParams<Index + 1, Rest...>(arr, names);
    }
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ENABLE_JSON
