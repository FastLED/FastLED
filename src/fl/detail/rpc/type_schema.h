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
    static Json schema() {
        Json s = Json::object();
        s.set("type", "unknown");
        return s;
    }

    static const char* typeName() {
        return "unknown";
    }
};

// Integer types (excluding bool)
template <typename T>
struct TypeSchema<T, typename fl::enable_if<
    fl::is_integral<T>::value && !fl::is_same<T, bool>::value
>::type> {
    static Json schema() {
        Json s = Json::object();
        s.set("type", "integer");
        return s;
    }

    static const char* typeName() {
        return "integer";
    }
};

// Boolean type
template <>
struct TypeSchema<bool, void> {
    static Json schema() {
        Json s = Json::object();
        s.set("type", "boolean");
        return s;
    }

    static const char* typeName() {
        return "boolean";
    }
};

// Floating point types
template <typename T>
struct TypeSchema<T, typename fl::enable_if<fl::is_floating_point<T>::value>::type> {
    static Json schema() {
        Json s = Json::object();
        s.set("type", "number");
        return s;
    }

    static const char* typeName() {
        return "number";
    }
};

// String type
template <>
struct TypeSchema<fl::string, void> {
    static Json schema() {
        Json s = Json::object();
        s.set("type", "string");
        return s;
    }

    static const char* typeName() {
        return "string";
    }
};

// Void type (for return types)
template <>
struct TypeSchema<void, void> {
    static Json schema() {
        return Json(nullptr);  // No schema for void
    }

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
    static Json params() {
        Json arr = Json::array();
        fl::vector<fl::string> empty;
        addParams<0, Args...>(arr, empty);
        return arr;
    }

    static Json paramsWithNames(const fl::vector<fl::string>& names) {
        Json arr = Json::array();
        addParams<0, Args...>(arr, names);
        return arr;
    }

    static Json result() {
        return TypeSchema<R>::schema();
    }

    static bool hasResult() {
        return true;
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
        Json param = Json::object();
        // Use provided name if available, otherwise default to "argN"
        if (Index < names.size() && !names[Index].empty()) {
            param.set("name", names[Index].c_str());
        } else {
            param.set("name", "arg" + fl::to_string(static_cast<int64_t>(Index)));
        }
        param.set("schema", TypeSchema<First>::schema());
        param.set("required", true);
        arr.push_back(param);
        addParams<Index + 1, Rest...>(arr, names);
    }
};

// Specialization for void return
template <typename... Args>
struct MethodSchema<void(Args...)> {
    static Json params() {
        Json arr = Json::array();
        fl::vector<fl::string> empty;
        addParams<0, Args...>(arr, empty);
        return arr;
    }

    static Json paramsWithNames(const fl::vector<fl::string>& names) {
        Json arr = Json::array();
        addParams<0, Args...>(arr, names);
        return arr;
    }

    static Json result() {
        return Json(nullptr);
    }

    static bool hasResult() {
        return false;
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
        Json param = Json::object();
        // Use provided name if available, otherwise default to "argN"
        if (Index < names.size() && !names[Index].empty()) {
            param.set("name", names[Index].c_str());
        } else {
            param.set("name", "arg" + fl::to_string(static_cast<int64_t>(Index)));
        }
        param.set("schema", TypeSchema<First>::schema());
        param.set("required", true);
        arr.push_back(param);
        addParams<Index + 1, Rest...>(arr, names);
    }
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ENABLE_JSON
