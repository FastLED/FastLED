#pragma once

/**
 * @file fl/stl/json.h
 * @brief FastLED's Elegant JSON Library: `fl::json`
 *
 * @details
 *
 * The `fl::json` library provides a lightweight, type-safe, and highly ergonomic
 * interface for both parsing and generating JSON data within the FastLED ecosystem.
 *
 * Key Features & Design Principles:
 * ------------------------------------
 * - **Fluid Chaining**: Effortlessly navigate nested JSON structures using
 *   `json["key"]["nested_key"]` or `json["array_key"][index]`.
 * - **Default Values (`operator|`)**: The cornerstone of robust parsing. Safely
 *   extract values with a fallback, preventing crashes from missing keys or
 *   type mismatches: `int value = json["path"]["to"]["key"] | 123;`
 * - **Type Safety**: Methods return `fl::optional<T>` for explicit handling of
 *   potential absence or type errors, ensuring predictable behavior.
 * - **Unified API**: A consistent and intuitive interface for both reading
 *   and writing JSON data.
 * - **Explicit Creation**: Clearly define JSON objects and arrays using
 *   `fl::json::object()` and `fl::json::array()`.
 *
 * Parsing JSON Data - The Clean Way:
 * ------------------------------------
 * Parse a JSON string and extract values with graceful defaults.
 *
 * @code
 * #include "fl/stl/json.h"
 * #include "fl/log/log.h" // For FL_WARN
 *
 * const char* jsonStr = R"({
 *   "config": {
 *     "brightness": 128,
 *     "enabled": true,
 *     "name": "my_device"
 *   },
 *   "status": "active"
 * })";
 *
 * fl::json jsonDoc = fl::json::parse(jsonStr);
 *
 * // Accessing an integer with a default value
 * int brightness = jsonDoc["config"]["brightness"] | 255; // Result: 128
 * FL_WARN("Brightness: " << brightness);
 *
 * // Accessing a boolean with a default value
 * bool enabled = jsonDoc["config"]["enabled"] | false;    // Result: true
 * FL_WARN("Enabled: " << enabled);
 *
 * // Accessing a string with a default value
 * fl::string deviceName = jsonDoc["config"]["name"] | fl::string("unknown"); // Result: "my_device"
 * FL_WARN("Device Name: " << deviceName);
 *
 * // Accessing a non-existent key with a default value
 * int nonExistent = jsonDoc["config"]["non_existent_key"] | 0; // Result: 0
 * FL_WARN("Non-existent: " << nonExistent);
 * @endcode
 *
 * Generating JSON Data - Build with Ease:
 * -----------------------------------------
 * Construct complex JSON objects and arrays programmatically.
 *
 * @code
 * #include "fl/stl/json.h"
 * #include "fl/stl/string.h"
 * #include "fl/stl/vector.h"
 * #include "fl/log/log.h"
 *
 * // Create a root JSON object
 * fl::json newJson = fl::json::object();
 *
 * // Set primitive values
 * newJson.set("version", 1.0);
 * newJson.set("isActive", true);
 * newJson.set("message", "Hello, FastLED!");
 *
 * // Create and set a nested object
 * fl::json settings = fl::json::object();
 * settings.set("mode", "dynamic");
 * settings.set("speed", 50);
 * newJson.set("settings", settings);
 *
 * // Create and set a nested array
 * fl::json colors = fl::json::array();
 * colors.push_back(fl::json("red"));
 * colors.push_back(fl::json("green"));
 * colors.push_back(fl::json("blue"));
 * newJson.set("colors", colors);
 *
 * // Convert the entire JSON object to a string
 * fl::string jsonString = newJson.to_string();
 * FL_WARN("Generated JSON:\n" << jsonString);
 * // Expected output (formatting may vary):
 * // {"version":1.0,"isActive":true,"message":"Hello, FastLED!","settings":{"mode":"dynamic","speed":50},"colors":["red","green","blue"]}
 * @endcode
 *
 * Important Considerations:
 * ---------------------------
 * - **Null Values**: A `json` object can represent JSON `null` or be completely
 *   uninitialized. Use `is_null()` to explicitly check for JSON null values.
 * - **Memory Management**: `fl::json` uses `fl::shared_ptr` internally, enabling
 *   efficient copying and avoiding manual memory management pitfalls.
 * - **Error Handling**: Missing keys or type mismatches result in `fl::optional`
 *   values that can be safely queried or defaulted using `operator|`.
 *
 * Thread Safety:
 * ---------------
 * `fl::json` is **not inherently thread-safe**. If you need to share JSON data
 * across threads, ensure proper synchronization using mutexes or similar mechanisms.
 *
 * Performance Notes:
 * -------------------
 * - Parsing: Native parser is **1.62x faster** than ArduinoJson with **62.5% higher throughput**
 * - Memory: **45% fewer allocations** and **24% lower peak memory** than ArduinoJson
 * - Validation: **Zero heap allocations** during validation phase
 *
 * @see fl::json
 * @see fl::optional
 */

// Implementation details - users should not rely on these directly
#include "fl/stl/json/types.h"
#include "fl/stl/noexcept.h"

namespace fl {

class json {
private:
    fl::shared_ptr<json_value> mValue;

public:
    // Constructors
    json() FL_NOEXCEPT : mValue() {}  // Default initialize to nullptr
    json(fl::nullptr_t) FL_NOEXCEPT : mValue(fl::make_shared<json_value>(nullptr)) {}
    json(bool b) FL_NOEXCEPT : mValue(fl::make_shared<json_value>(b)) {}
    json(int i) FL_NOEXCEPT : mValue(fl::make_shared<json_value>(static_cast<i64>(i))) {}
    json(i64 i) FL_NOEXCEPT : mValue(fl::make_shared<json_value>(i)) {}
    json(float f) FL_NOEXCEPT : mValue(fl::make_shared<json_value>(f)) {}  // Use float directly
    json(double d) FL_NOEXCEPT : mValue(fl::make_shared<json_value>(static_cast<float>(d))) {}  // Convert double to float
    json(const fl::string& s) FL_NOEXCEPT : mValue(fl::make_shared<json_value>(s)) {}
    json(const char* s) FL_NOEXCEPT : json(fl::string(s)) {}
    json(json_array a) FL_NOEXCEPT : mValue(fl::make_shared<json_value>(fl::move(a))) {}
    json(json_object o) FL_NOEXCEPT : mValue(fl::make_shared<json_value>(fl::move(o))) {}
    // Constructor from shared_ptr<json_value>
    json(const fl::shared_ptr<json_value>& value) FL_NOEXCEPT : mValue(value) {}
    
    // Factory method to create a json from a json_value
    static json fromValue(const json_value& value) FL_NOEXCEPT {
        json result;
        result.mValue = fl::make_shared<json_value>(value);
        return result;
    }
    
    // Constructor for fl::vector<float> - converts to JSON array
    json(const fl::vector<float>& vec) FL_NOEXCEPT : mValue(fl::make_shared<json_value>(json_array{})) {
        auto ptr = mValue->data.ptr<json_array>();
        if (ptr) {
            for (const auto& item : vec) {
                ptr->push_back(fl::make_shared<json_value>(item));  // Use float directly
            }
        }
    }
    
    // Special constructor for char values
    static json from_char(char c) FL_NOEXCEPT {
        json result;
        auto value = fl::make_shared<json_value>(fl::string(1, c));
        //FL_WARN("Created json_value with string: " << value->is_string() << ", int: " << value->is_int());
        result.mValue = value;
        //FL_WARN("json has string: " << result.is_string() << ", int: " << result.is_int());
        return result;
    }

    // Copy constructor
    json(const json& other) FL_NOEXCEPT : mValue(other.mValue) {}

    // Assignment operator
    json& operator=(const json& other) FL_NOEXCEPT {
        //FL_WARN("json& operator=(const json& other): " << (other.mValue ? other.mValue.get() : 0));
        if (this != &other) {
            mValue = other.mValue;
        }
        return *this;
    }

    json& operator=(json&& other) FL_NOEXCEPT {
        if (this != &other) {
            mValue = fl::move(other.mValue);
        }
        return *this;
    }
    
    // Assignment operators for primitive types to avoid ambiguity
    json& operator=(bool value) FL_NOEXCEPT {
        mValue = fl::make_shared<json_value>(value);
        return *this;
    }
    
    json& operator=(int value) FL_NOEXCEPT {
        mValue = fl::make_shared<json_value>(static_cast<i64>(value));
        return *this;
    }
    
    json& operator=(float value) FL_NOEXCEPT {
        mValue = fl::make_shared<json_value>(value);
        return *this;
    }
    
    json& operator=(double value) FL_NOEXCEPT {
        mValue = fl::make_shared<json_value>(static_cast<float>(value));
        return *this;
    }
    
    json& operator=(const fl::string& value) FL_NOEXCEPT {
        mValue = fl::make_shared<json_value>(value);
        return *this;
    }
    
    json& operator=(const char* value) FL_NOEXCEPT {
        mValue = fl::make_shared<json_value>(fl::string(value));
        return *this;
    }
    
    // Assignment operator for fl::vector<float>
    json& operator=(fl::vector<float> vec) FL_NOEXCEPT {
        mValue = fl::make_shared<json_value>(json_array{});
        auto ptr = mValue->data.ptr<json_array>();
        if (ptr) {
            for (const auto& item : vec) {
                ptr->push_back(fl::make_shared<json_value>(item));  // Use float directly
            }
        }
        return *this;
    }

    // Type queries
    bool is_null() const FL_NOEXCEPT { return mValue ? mValue->is_null() : true; }
    bool is_bool() const FL_NOEXCEPT { return mValue && mValue->is_bool(); }
    bool is_int() const FL_NOEXCEPT { return mValue && (mValue->is_int() || mValue->is_bool()); }
    bool is_float() const FL_NOEXCEPT { return mValue && mValue->is_float(); }
    bool is_double() const FL_NOEXCEPT { return mValue && mValue->is_double(); }
    // is_number() returns true if the value is any numeric type (int or float)
    bool is_number() const FL_NOEXCEPT { return mValue && mValue->is_number(); }
    bool is_string() const FL_NOEXCEPT { return mValue && mValue->is_string(); }
    bool is_array() const FL_NOEXCEPT { return mValue && mValue->is_array(); }
    bool is_generic_array() const FL_NOEXCEPT { return mValue && mValue->is_generic_array(); }
    bool is_object() const FL_NOEXCEPT { return mValue && mValue->is_object(); }
    bool is_audio() const FL_NOEXCEPT { return mValue && mValue->is_audio(); }
    bool is_bytes() const FL_NOEXCEPT { return mValue && mValue->is_bytes(); }
    bool is_floats() const FL_NOEXCEPT { return mValue && mValue->is_floats(); }

    // Safe extractors
    fl::optional<bool> as_bool() const FL_NOEXCEPT { return mValue ? mValue->as_bool() : fl::nullopt; }
    fl::optional<i64> as_int() const FL_NOEXCEPT {
        if (!mValue) return fl::nullopt;
        return mValue->as_int(); 
    }
    
    template<typename IntType>
    fl::optional<IntType> as_int() const FL_NOEXCEPT {
        if (!mValue) return fl::nullopt;
        return mValue->template as_int<IntType>(); 
    }
    
    fl::optional<float> as_float() const FL_NOEXCEPT {
        if (!mValue) return fl::nullopt;
        return mValue->as_float();
    }
    
    fl::optional<double> as_double() const FL_NOEXCEPT {
        if (!mValue) return fl::nullopt;
        return mValue->as_double();
    }
    
    template<typename FloatType>
    fl::optional<FloatType> as_float() const FL_NOEXCEPT {
        if (!mValue) return fl::nullopt;
        return mValue->template as_float<FloatType>(); 
    }
    
    fl::optional<fl::string> as_string() const FL_NOEXCEPT {
        if (!mValue) return fl::nullopt;
        return mValue->as_string(); 
    }
    // Zero-copy pointer accessors
    const json_array*        as_array()   const FL_NOEXCEPT { return mValue ? mValue->as_array() : nullptr; }
    const json_object*       as_object()  const FL_NOEXCEPT { return mValue ? mValue->as_object() : nullptr; }

    // Explicit copy methods - use when you need an owned copy or type conversion
    fl::optional<json_array>        clone_array()  const FL_NOEXCEPT { return mValue ? mValue->clone_array() : fl::nullopt; }
    fl::optional<json_object>       clone_object() const FL_NOEXCEPT { return mValue ? mValue->clone_object() : fl::nullopt; }

    // Copy packed-array elements into a caller-owned span with type conversion.
    // Returns number of elements copied (min of array size and span size).
    // Returns 0 if not a numeric array or json is null.
    template<typename T>
    size_t copy_to(fl::span<T> out) const FL_NOEXCEPT {
        return mValue ? mValue->copy_to(out) : 0;
    }

    // Stream packed-array elements into an output iterator with type conversion.
    // Use with fl::back_inserter(container) to append to any container.
    // Returns number of elements written. Returns 0 if not a numeric array.
    template<typename T, typename OutputIt>
    size_t copy_to_output_iterator(OutputIt out) const FL_NOEXCEPT {
        return mValue ? mValue->template copy_to_output_iterator<T, OutputIt>(out) : 0;
    }

    // Overload for back_insert_iterator: T deduced from container's value_type
    template<typename Container>
    size_t copy_to_output_iterator(fl::back_insert_iterator<Container> out) const FL_NOEXCEPT {
        return mValue ? mValue->copy_to_output_iterator(out) : 0;
    }

    // NEW ERGONOMIC API: try_as<T>() - Explicit optional handling
    // Use when you need to explicitly handle conversion failure
    template<typename T>
    fl::optional<T> try_as() const FL_NOEXCEPT {
        if (!mValue) {
            return fl::nullopt;
        }
        return as_impl<T>();
    }

    // BACKWARD COMPATIBILITY: Keep existing as<T>() that returns fl::optional<T>
    // This maintains compatibility with existing code
    template<typename T>
    fl::optional<T> as() const FL_NOEXCEPT {
        return try_as<T>();
    }

    // NEW ERGONOMIC API: value<T>() - Direct conversion with sensible defaults
    // Use when you want a value immediately with reasonable defaults on failure
    template<typename T>
    T value() const FL_NOEXCEPT {
        auto result = try_as<T>();
        return result.has_value() ? *result : get_default_value<T>();
    }

private:
    // Integer types (excluding bool)
    template<typename T>
    typename fl::enable_if<fl::is_integral<T>::value && !fl::is_same<T, bool>::value, fl::optional<T>>::type
    as_impl() const FL_NOEXCEPT {
        return mValue->template as_int<T>();
    }
    
    // Boolean type
    template<typename T>
    typename fl::enable_if<fl::is_same<T, bool>::value, fl::optional<T>>::type
    as_impl() const FL_NOEXCEPT {
        return mValue->as_bool();
    }
    
    // Floating point types
    template<typename T>
    typename fl::enable_if<fl::is_floating_point<T>::value, fl::optional<T>>::type
    as_impl() const FL_NOEXCEPT {
        // Force template call by explicitly using the templated method
        return mValue->template as_float<T>();
    }
    
    // String type
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::string>::value, fl::optional<T>>::type
    as_impl() const FL_NOEXCEPT {
        return mValue->as_string();
    }
    
    // Array type - clone_array() provides packed-array conversion for try_as
    template<typename T>
    typename fl::enable_if<fl::is_same<T, json_array>::value, fl::optional<T>>::type
    as_impl() const FL_NOEXCEPT {
        return mValue->clone_array();
    }

    // Object type
    template<typename T>
    typename fl::enable_if<fl::is_same<T, json_object>::value, fl::optional<T>>::type
    as_impl() const FL_NOEXCEPT {
        auto ptr = mValue->as_object();
        return ptr ? fl::optional<T>(*ptr) : fl::nullopt;
    }

    // Specialized vector types - use copy_to_output_iterator for cross-type conversion
    // e.g., try_as<vector<float>>() works on u8/i16/float packed arrays
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<i16>>::value, fl::optional<T>>::type
    as_impl() const FL_NOEXCEPT {
        if (!mValue->is_array()) return fl::nullopt;
        T result;
        size_t n = mValue->copy_to_output_iterator(fl::back_inserter(result));
        return n > 0 ? fl::optional<T>(fl::move(result)) : fl::nullopt;
    }

    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<u8>>::value, fl::optional<T>>::type
    as_impl() const FL_NOEXCEPT {
        if (!mValue->is_array()) return fl::nullopt;
        T result;
        size_t n = mValue->copy_to_output_iterator(fl::back_inserter(result));
        return n > 0 ? fl::optional<T>(fl::move(result)) : fl::nullopt;
    }

    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<float>>::value, fl::optional<T>>::type
    as_impl() const FL_NOEXCEPT {
        if (!mValue->is_array()) return fl::nullopt;
        T result;
        size_t n = mValue->copy_to_output_iterator(fl::back_inserter(result));
        return n > 0 ? fl::optional<T>(fl::move(result)) : fl::nullopt;
    }

    // Helper methods for getting default values for each type
    template<typename T>
    typename fl::enable_if<fl::is_integral<T>::value && !fl::is_same<T, bool>::value, T>::type
    get_default_value() const FL_NOEXCEPT {
        return T(0);  // All integer types default to 0
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, bool>::value, T>::type
    get_default_value() const FL_NOEXCEPT {
        return false;  // Boolean defaults to false
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_floating_point<T>::value, T>::type
    get_default_value() const FL_NOEXCEPT {
        return T(0.0);  // Floating point types default to 0.0
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::string>::value, T>::type
    get_default_value() const FL_NOEXCEPT {
        return fl::string();  // String defaults to empty string
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, json_array>::value, T>::type
    get_default_value() const FL_NOEXCEPT {
        return json_array();  // Array defaults to empty array
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, json_object>::value, T>::type
    get_default_value() const FL_NOEXCEPT {
        return json_object();  // Object defaults to empty object
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<i16>>::value, T>::type
    get_default_value() const FL_NOEXCEPT {
        return fl::vector<i16>();  // Audio vector defaults to empty
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<u8>>::value, T>::type
    get_default_value() const FL_NOEXCEPT {
        return fl::vector<u8>();  // Bytes vector defaults to empty
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<float>>::value, T>::type
    get_default_value() const FL_NOEXCEPT {
        return fl::vector<float>();  // Float vector defaults to empty
    }

public:

    // Iterator support for objects
    json_value::iterator begin() FL_NOEXCEPT {
        if (!mValue) return json_value::iterator(json_object().begin());
        return mValue->begin(); 
    }
    json_value::iterator end() FL_NOEXCEPT {
        if (!mValue) return json_value::iterator(json_object().end());
        return mValue->end(); 
    }
    json_value::const_iterator begin() const FL_NOEXCEPT {
        if (!mValue) return json_value::const_iterator::from_iterator(json_object().begin());
        return json_value::const_iterator::from_object_iterator(mValue->begin()); 
    }
    json_value::const_iterator end() const FL_NOEXCEPT {
        if (!mValue) return json_value::const_iterator::from_iterator(json_object().end());
        return json_value::const_iterator::from_object_iterator(mValue->end()); 
    }
    
    // Iterator support for arrays with type conversion
    template<typename T>
    typename json_value::template array_iterator<T> begin_array() FL_NOEXCEPT {
        if (!mValue) return typename json_value::template array_iterator<T>();
        return mValue->template begin_array<T>();
    }
    
    template<typename T>
    typename json_value::template array_iterator<T> end_array() FL_NOEXCEPT {
        if (!mValue) return typename json_value::template array_iterator<T>();
        return mValue->template end_array<T>();
    }
    
    template<typename T>
    typename json_value::template array_iterator<T> begin_array() const FL_NOEXCEPT {
        if (!mValue) return typename json_value::template array_iterator<T>();
        return mValue->template begin_array<T>();
    }
    
    template<typename T>
    typename json_value::template array_iterator<T> end_array() const FL_NOEXCEPT {
        if (!mValue) return typename json_value::template array_iterator<T>();
        return mValue->template end_array<T>();
    }
    
    // Free functions for range-based for loops
    friend json_value::iterator begin(json& j) FL_NOEXCEPT { return j.begin(); }
    friend json_value::iterator end(json& j) FL_NOEXCEPT { return j.end(); }
    friend json_value::const_iterator begin(const json& j) FL_NOEXCEPT { return j.begin(); }
    friend json_value::const_iterator end(const json& j) FL_NOEXCEPT { return j.end(); }

    // Object iteration support (needed for screenmap conversion)
    fl::vector<fl::string> keys() const FL_NOEXCEPT {
        fl::vector<fl::string> result;
        if (mValue && mValue->is_object()) {
            for (auto it = begin(); it != end(); ++it) {
                auto keyValue = *it;
                result.push_back(keyValue.first);
            }
        }
        return result;
    }
    
    // Backward compatibility methods
    fl::vector<fl::string> get_object_keys() const FL_NOEXCEPT { return keys(); }
    fl::vector<fl::string> getObjectKeys() const FL_NOEXCEPT { return keys(); }

    // Indexing for fluid chaining
    json operator[](size_t idx) FL_NOEXCEPT {
        if (!mValue) {
            mValue = fl::make_shared<json_value>(json_array{});
        }
        // If we're indexing into a specialized array, convert it to regular json_array first
        if (mValue->data.is<fl::vector<i16>>() ||
            mValue->data.is<fl::vector<u8>>() ||
            mValue->data.is<fl::vector<float>>()) {
            // Convert to regular json_array (needs a copy/conversion)
            auto arr = mValue->clone_array();
            if (arr) {
                mValue = fl::make_shared<json_value>(fl::move(*arr));
            }
        }
        // Get pointer directly to avoid copying - fixes silent mutation bug
        auto arrPtr = mValue->as_array();
        if (arrPtr) {
            // Ensure the array is large enough
            if (idx >= arrPtr->size()) {
                for (size_t i = arrPtr->size(); i <= idx; i++) {
                    arrPtr->push_back(fl::make_shared<json_value>(nullptr));
                }
            }
            return json((*arrPtr)[idx]);
        }
        return json(nullptr);
    }
    
    const json operator[](size_t idx) const FL_NOEXCEPT {
        if (!mValue) {
            return json(nullptr);
        }
        // Handle regular json_array - zero-copy pointer access
        auto arrPtr = mValue->as_array();
        if (arrPtr) {
            if (idx < arrPtr->size()) {
                return json((*arrPtr)[idx]);
            }
            return json(nullptr);
        }
        // For packed arrays, extract element directly without converting the whole array
        if (auto p = mValue->data.ptr<fl::vector<i16>>()) {
            if (idx < p->size()) return json(static_cast<i64>((*p)[idx]));
        } else if (auto p = mValue->data.ptr<fl::vector<u8>>()) {
            if (idx < p->size()) return json(static_cast<i64>((*p)[idx]));
        } else if (auto p = mValue->data.ptr<fl::vector<float>>()) {
            if (idx < p->size()) return json((*p)[idx]);
        }
        return json(nullptr);
    }
    
    json operator[](const fl::string &key) FL_NOEXCEPT {
        if (!mValue || !mValue->is_object()) {
            mValue = fl::make_shared<json_value>(json_object{});
        }
        // Get reference to the json_value
        auto objPtr = mValue->data.ptr<json_object>();
        if (objPtr) {
            // If key doesn't exist, create a new json_value and insert it
            if (objPtr->find(key) == objPtr->end()) {
                (*objPtr)[key] = fl::make_shared<json_value>(nullptr);
            }
            // Return a new json object that wraps the shared_ptr to the json_value
            return json((*objPtr)[key]);
        }
        // Should not happen if mValue is properly initialized as an object
        //return *reinterpret_cast<json*>(&get_null_value());
        return json(nullptr);
    }
    
    const json operator[](const fl::string &key) const FL_NOEXCEPT {
        if (!mValue || !mValue->is_object()) {
            return json(nullptr);
        }
        // Use ptr<> directly to avoid copying the entire map via as_object().
        // The variant stores json_object (not const), so use ptr<json_object>().
        // The const overload of ptr<T>() returns const T*, which is what we want.
        auto objPtr = mValue->data.ptr<json_object>();
        if (!objPtr) return json(nullptr);
        auto it = objPtr->find(key);
        if (it != objPtr->end()) {
            return json(it->second);
        }
        return json(nullptr);
    }

    // Contains methods for checking existence
    bool contains(size_t idx) const FL_NOEXCEPT {
        return mValue && mValue->contains(idx); 
    }
    bool contains(const fl::string &key) const FL_NOEXCEPT {
        return mValue && mValue->contains(key); 
    }

    // Size method
    size_t size() const FL_NOEXCEPT {
        return mValue ? mValue->size() : 0; 
    }

    // Default-value operator (pipe)
    template<typename T>
    T operator|(const T& fallback) const FL_NOEXCEPT {
        if (!mValue) return fallback;
        return (*mValue) | fallback;
    }
    
    // NEW ERGONOMIC API: as_or<T>(default) - Conversion with custom defaults
    // Use when you want to specify your own default value
    // This method uses try_as<T>() for proper string-to-number conversion
    template<typename T>
    T as_or(const T& fallback) const FL_NOEXCEPT {
        auto result = try_as<T>();
        return result.has_value() ? *result : fallback;
    }

    // has_value method for compatibility
    bool has_value() const FL_NOEXCEPT {
        return mValue && !mValue->is_null();
    }

    // Get access to the internal json_value for direct variant visiting
    // Returns nullptr if no value is set
    const json_value* internal_value() const FL_NOEXCEPT { return mValue.get(); }

    // Method to set the internal value (for json_value::to_string())
    void set_value(const fl::shared_ptr<json_value>& value) FL_NOEXCEPT {
        mValue = value;
    }
    
    // Public method to access to_string_native for json_value::to_string()
    fl::string to_string_native_public() const FL_NOEXCEPT { return to_string_native(); }

    // Serialization - now delegates to native implementation
    fl::string to_string() const FL_NOEXCEPT { return to_string_native(); }
    
    // Native serialization (without external libraries)
    fl::string to_string_native() const FL_NOEXCEPT;

    // Parsing factory method - uses native parser
    static json parse(const fl::string &txt) FL_NOEXCEPT {
        auto parsed = json_value::parse2(txt);
        if (parsed) {
            json result;
            result.mValue = parsed;
            return result;
        }
        return json(nullptr);
    }

    // Convenience methods for creating arrays and objects
    static json array() FL_NOEXCEPT {
        return json(json_array{});
    }

    static json object() FL_NOEXCEPT {
        return json(json_object{});
    }
    
    // Compatibility with existing API for array/object access
    size_t get_size() const FL_NOEXCEPT { return size(); }
    size_t getSize() const FL_NOEXCEPT { return size(); }
    
    // Set methods for building objects
    void set(const fl::string& key, const json& value) FL_NOEXCEPT {
        if (!mValue || !mValue->is_object()) {
            mValue = fl::make_shared<json_value>(json_object{});
        }
        // Directly assign the value to the object without going through json::operator[]
        auto objPtr = mValue->data.ptr<json_object>();
        if (objPtr) {
            // Create or update the entry directly
            if (value.mValue) {
                (*objPtr)[key] = value.mValue;
            } else {
                // If value has null mValue, create a null json_value
                (*objPtr)[key] = fl::make_shared<json_value>(nullptr);
            }
        }
    }
    
    void set(const fl::string& key, bool value) FL_NOEXCEPT { set(key, json(value)); }
    void set(const fl::string& key, int value) FL_NOEXCEPT { set(key, json(value)); }
    void set(const fl::string& key, i64 value) FL_NOEXCEPT { set(key, json(value)); }
    void set(const fl::string& key, float value) FL_NOEXCEPT { set(key, json(value)); }
    void set(const fl::string& key, double value) FL_NOEXCEPT { set(key, json(value)); }
    void set(const fl::string& key, const fl::string& value) FL_NOEXCEPT { set(key, json(value)); }
    void set(const fl::string& key, const char* value) FL_NOEXCEPT { set(key, json(value)); }
    template<typename T, typename = fl::enable_if_t<fl::is_same<T, char>::value>>
    void set(const fl::string& key, T value) FL_NOEXCEPT { set(key, json(value)); }

    // Generic setter for all integer types (excluding bool, int, and i64 which have explicit overloads)
    // Converts to i64 for internal storage
    template<typename IntType>
    typename fl::enable_if<
        fl::is_integral<IntType>::value &&
        !fl::is_same<IntType, bool>::value &&
        !fl::is_same<IntType, int>::value &&
        !fl::is_same<IntType, i64>::value &&
        !fl::is_same<IntType, char>::value,
        void
    >::type
    set(const fl::string& key, IntType value) FL_NOEXCEPT {
        // Convert to i64 for storage
        set(key, json(static_cast<i64>(value)));
    }
    
    // Array push_back methods
    void push_back(const json& value) FL_NOEXCEPT {
        if (!mValue || !mValue->is_array()) {
            mValue = fl::make_shared<json_value>(json_array{});
        }
        // If we're pushing to a packed array, convert it to regular json_array first
        if (mValue->is_array() &&
            (mValue->data.is<fl::vector<i16>>() ||
             mValue->data.is<fl::vector<u8>>() ||
             mValue->data.is<fl::vector<float>>())) {
            // Convert to regular json_array (needs a copy/conversion)
            auto arr = mValue->clone_array();
            if (arr) {
                mValue = fl::make_shared<json_value>(fl::move(*arr));
            }
        }
        // For arrays, we need to manually handle the insertion since our indexing
        // mechanism auto-creates elements
        auto ptr = mValue->data.ptr<json_array>();
        if (ptr) {
            ptr->push_back(value.mValue);
        }
    }
    
    // Create methods for compatibility
    static json create_array() FL_NOEXCEPT { return json::array(); }
    static json create_object() FL_NOEXCEPT { return json::object(); }
    static json createArray() FL_NOEXCEPT { return json::array(); }
    static json createObject() FL_NOEXCEPT { return json::object(); }

    // Serialize method for compatibility
    fl::string serialize() const FL_NOEXCEPT { return to_string(); }

    // Helper function to normalize JSON string (remove whitespace)
    static fl::string normalize_json_string(const char* jsonStr) FL_NOEXCEPT;
    static fl::string normalizeJsonString(const char* jsonStr) FL_NOEXCEPT { return normalize_json_string(jsonStr); }
};

// JSON memory profiling support - defined in json.cpp.hpp
// Set g_json_stack_base to a stack address to enable stack depth tracking
extern volatile size_t g_json_stack_base;  // cast address to size_t
extern volatile size_t g_json_max_stack_depth;


} // namespace fl
