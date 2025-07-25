#pragma once

#include "fl/string.h"
#include "fl/vector.h"
#include "fl/hash_map.h"
#include "fl/variant.h"
#include "fl/optional.h"

namespace fl {

// Forward declarations
struct json_value;

// Define json_array and json_object as pointers to avoid incomplete type issues
// We'll use heap-allocated containers for these to avoid alignment issues
using json_array = fl::vector<fl::shared_ptr<json_value>>;
using json_object = fl::HashMap<fl::string, fl::shared_ptr<json_value>>;

// The JSON node
struct json_value {
    // The variant holds exactly one of these alternatives
    using variant_t = fl::Variant<
        fl::nullptr_t,   // null
        bool,            // true/false
        int64_t,         // integer
        double,          // floating-point
        fl::string,      // string
        json_array,      // array
        json_object      // object
    >;

    variant_t data;

    // Constructors
    json_value() noexcept : data(nullptr) {}
    json_value(fl::nullptr_t) noexcept : data(nullptr) {}
    json_value(bool b) noexcept : data(b) {}
    json_value(int64_t i) noexcept : data(i) {}
    json_value(double d) noexcept : data(d) {}
    json_value(fl::string s) : data(fl::move(s)) {}
    json_value(json_array a) : data(fl::move(a)) {}
    json_value(json_object o) : data(fl::move(o)) {}

    // Type queries - using is<T>() instead of index() for fl::Variant
    bool is_null() const noexcept { return data.is<fl::nullptr_t>(); }
    bool is_bool() const noexcept { return data.is<bool>(); }
    bool is_int() const noexcept { return data.is<int64_t>(); }
    bool is_double() const noexcept { return data.is<double>(); }
    bool is_string() const noexcept { return data.is<fl::string>(); }
    bool is_array() const noexcept { return data.is<json_array>(); }
    bool is_object() const noexcept { return data.is<json_object>(); }

    // Safe extractors (return optional values, not references)
    fl::optional<bool> as_bool() {
        auto ptr = data.ptr<bool>();
        return ptr ? fl::optional<bool>(*ptr) : fl::nullopt;
    }
    
    fl::optional<int64_t> as_int() {
        auto ptr = data.ptr<int64_t>();
        return ptr ? fl::optional<int64_t>(*ptr) : fl::nullopt;
    }
    
    fl::optional<double> as_double() {
        auto ptr = data.ptr<double>();
        return ptr ? fl::optional<double>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::string> as_string() {
        auto ptr = data.ptr<fl::string>();
        return ptr ? fl::optional<fl::string>(*ptr) : fl::nullopt;
    }
    
    fl::optional<json_array> as_array() {
        auto ptr = data.ptr<json_array>();
        return ptr ? fl::optional<json_array>(*ptr) : fl::nullopt;
    }
    
    fl::optional<json_object> as_object() {
        auto ptr = data.ptr<json_object>();
        return ptr ? fl::optional<json_object>(*ptr) : fl::nullopt;
    }

    // Const overloads
    fl::optional<bool> as_bool() const {
        auto ptr = data.ptr<bool>();
        return ptr ? fl::optional<bool>(*ptr) : fl::nullopt;
    }
    
    fl::optional<int64_t> as_int() const {
        auto ptr = data.ptr<int64_t>();
        return ptr ? fl::optional<int64_t>(*ptr) : fl::nullopt;
    }
    
    fl::optional<double> as_double() const {
        auto ptr = data.ptr<double>();
        return ptr ? fl::optional<double>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::string> as_string() const {
        auto ptr = data.ptr<fl::string>();
        return ptr ? fl::optional<fl::string>(*ptr) : fl::nullopt;
    }
    
    fl::optional<json_array> as_array() const {
        auto ptr = data.ptr<json_array>();
        return ptr ? fl::optional<json_array>(*ptr) : fl::nullopt;
    }
    
    fl::optional<json_object> as_object() const {
        auto ptr = data.ptr<json_object>();
        return ptr ? fl::optional<json_object>(*ptr) : fl::nullopt;
    }

    // Indexing for fluid chaining
    json_value& operator[](size_t idx) {
        if (!is_array()) data = json_array{};
        auto ptr = data.ptr<json_array>();
        auto &arr = *ptr;
        if (idx >= arr.size()) {
            // Resize array and fill with null values
            for (size_t i = arr.size(); i <= idx; i++) {
                arr.push_back(fl::make_shared<json_value>());
            }
        }
        return *arr[idx];
    }
    
    json_value& operator[](const fl::string &key) {
        if (!is_object()) data = json_object{};
        auto ptr = data.ptr<json_object>();
        auto &obj = *ptr;
        if (obj.find(key) == obj.end()) {
            // Create a new entry if key doesn't exist
            obj[key] = fl::make_shared<json_value>();
        }
        return *obj[key];
    }

    // Default-value operator (pipe)
    template<typename T>
    T operator|(T const &fallback) const {
        if constexpr (fl::is_same<T, bool>::value) {
            if (auto p = data.ptr<bool>()) return *p;
        } else if constexpr (fl::is_same<T, int64_t>::value) {
            if (auto p = data.ptr<int64_t>()) return *p;
        } else if constexpr (fl::is_same<T, double>::value) {
            if (auto p = data.ptr<double>()) return *p;
        } else if constexpr (fl::is_same<T, fl::string>::value) {
            if (auto p = data.ptr<fl::string>()) return *p;
        }
        return fallback;
    }

    // Serialization
    fl::string to_string() const;

    // Parsing factory
    static fl::shared_ptr<json_value> parse(const fl::string &txt);
};

// Alias for user code - using a different name to avoid conflicts
using JsonValue = json_value;

} // namespace fl