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
 * #include "fl/warn.h" // For FL_WARN
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
 * #include "fl/warn.h"
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

namespace fl {

class json {
private:
    fl::shared_ptr<json_value> m_value;

public:
    // Constructors
    json() : m_value() {}  // Default initialize to nullptr
    json(fl::nullptr_t) : m_value(fl::make_shared<json_value>(nullptr)) {}
    json(bool b) : m_value(fl::make_shared<json_value>(b)) {}
    json(int i) : m_value(fl::make_shared<json_value>(static_cast<i64>(i))) {}
    json(i64 i) : m_value(fl::make_shared<json_value>(i)) {}
    json(float f) : m_value(fl::make_shared<json_value>(f)) {}  // Use float directly
    json(double d) : m_value(fl::make_shared<json_value>(static_cast<float>(d))) {}  // Convert double to float
    json(const fl::string& s) : m_value(fl::make_shared<json_value>(s)) {}
    json(const char* s): json(fl::string(s)) {}
    json(json_array a) : m_value(fl::make_shared<json_value>(fl::move(a))) {}
    json(json_object o) : m_value(fl::make_shared<json_value>(fl::move(o))) {}
    // Constructor from shared_ptr<json_value>
    json(const fl::shared_ptr<json_value>& value) : m_value(value) {}
    
    // Factory method to create a json from a json_value
    static json from_value(const json_value& value) {
        json result;
        result.m_value = fl::make_shared<json_value>(value);
        return result;
    }
    
    // Constructor for fl::vector<float> - converts to JSON array
    json(const fl::vector<float>& vec) : m_value(fl::make_shared<json_value>(json_array{})) {
        auto ptr = m_value->data.ptr<json_array>();
        if (ptr) {
            for (const auto& item : vec) {
                ptr->push_back(fl::make_shared<json_value>(item));  // Use float directly
            }
        }
    }
    
    // Special constructor for char values
    static json from_char(char c) {
        json result;
        auto value = fl::make_shared<json_value>(fl::string(1, c));
        //FASTLED_WARN("Created json_value with string: " << value->is_string() << ", int: " << value->is_int());
        result.m_value = value;
        //FASTLED_WARN("json has string: " << result.is_string() << ", int: " << result.is_int());
        return result;
    }

    // Copy constructor
    json(const json& other) : m_value(other.m_value) {}

    // Assignment operator
    json& operator=(const json& other) {
        //FL_WARN("json& operator=(const json& other): " << (other.m_value ? other.m_value.get() : 0));
        if (this != &other) {
            m_value = other.m_value;
        }
        return *this;
    }

    json& operator=(json&& other) {
        if (this != &other) {
            m_value = fl::move(other.m_value);
        }
        return *this;
    }
    
    // Assignment operators for primitive types to avoid ambiguity
    json& operator=(bool value) {
        m_value = fl::make_shared<json_value>(value);
        return *this;
    }
    
    json& operator=(int value) {
        m_value = fl::make_shared<json_value>(static_cast<i64>(value));
        return *this;
    }
    
    json& operator=(float value) {
        m_value = fl::make_shared<json_value>(value);
        return *this;
    }
    
    json& operator=(double value) {
        m_value = fl::make_shared<json_value>(static_cast<float>(value));
        return *this;
    }
    
    json& operator=(const fl::string& value) {
        m_value = fl::make_shared<json_value>(value);
        return *this;
    }
    
    json& operator=(const char* value) {
        m_value = fl::make_shared<json_value>(fl::string(value));
        return *this;
    }
    
    // Assignment operator for fl::vector<float>
    json& operator=(fl::vector<float> vec) {
        m_value = fl::make_shared<json_value>(json_array{});
        auto ptr = m_value->data.ptr<json_array>();
        if (ptr) {
            for (const auto& item : vec) {
                ptr->push_back(fl::make_shared<json_value>(item));  // Use float directly
            }
        }
        return *this;
    }

    // Type queries
    bool is_null() const { return m_value ? m_value->is_null() : true; }
    bool is_bool() const { return m_value && m_value->is_bool(); }
    bool is_int() const { return m_value && (m_value->is_int() || m_value->is_bool()); }
    bool is_float() const { return m_value && m_value->is_float(); }
    bool is_double() const { return m_value && m_value->is_double(); }
    // is_number() returns true if the value is any numeric type (int or float)
    bool is_number() const { return m_value && m_value->is_number(); }
    bool is_string() const { return m_value && m_value->is_string(); }
    bool is_array() const { return m_value && m_value->is_array(); }
    bool is_generic_array() const { return m_value && m_value->is_generic_array(); }
    bool is_object() const { return m_value && m_value->is_object(); }
    bool is_audio() const { return m_value && m_value->is_audio(); }
    bool is_bytes() const { return m_value && m_value->is_bytes(); }
    bool is_floats() const { return m_value && m_value->is_floats(); }

    // Safe extractors
    fl::optional<bool> as_bool() const { return m_value ? m_value->as_bool() : fl::nullopt; }
    fl::optional<i64> as_int() const { 
        if (!m_value) return fl::nullopt;
        return m_value->as_int(); 
    }
    
    template<typename IntType>
    fl::optional<IntType> as_int() const { 
        if (!m_value) return fl::nullopt;
        return m_value->template as_int<IntType>(); 
    }
    
    fl::optional<float> as_float() const { 
        if (!m_value) return fl::nullopt;
        return m_value->as_float();
    }
    
    fl::optional<double> as_double() const { 
        if (!m_value) return fl::nullopt;
        return m_value->as_double();
    }
    
    template<typename FloatType>
    fl::optional<FloatType> as_float() const { 
        if (!m_value) return fl::nullopt;
        return m_value->template as_float<FloatType>(); 
    }
    
    fl::optional<fl::string> as_string() const { 
        if (!m_value) return fl::nullopt;
        return m_value->as_string(); 
    }
    fl::optional<json_array> as_array() const { return m_value ? m_value->as_array() : fl::nullopt; }
    fl::optional<json_object> as_object() const { return m_value ? m_value->as_object() : fl::nullopt; }
    fl::optional<fl::vector<i16>> as_audio() const { return m_value ? m_value->as_audio() : fl::nullopt; }
    fl::optional<fl::vector<u8>> as_bytes() const { return m_value ? m_value->as_bytes() : fl::nullopt; }
    fl::optional<fl::vector<float>> as_floats() const { return m_value ? m_value->as_floats() : fl::nullopt; }

    // NEW ERGONOMIC API: try_as<T>() - Explicit optional handling
    // Use when you need to explicitly handle conversion failure
    template<typename T>
    fl::optional<T> try_as() const {
        if (!m_value) {
            return fl::nullopt;
        }
        return as_impl<T>();
    }

    // BACKWARD COMPATIBILITY: Keep existing as<T>() that returns fl::optional<T>
    // This maintains compatibility with existing code
    template<typename T>
    fl::optional<T> as() const {
        return try_as<T>();
    }

    // NEW ERGONOMIC API: value<T>() - Direct conversion with sensible defaults
    // Use when you want a value immediately with reasonable defaults on failure
    template<typename T>
    T value() const {
        auto result = try_as<T>();
        return result.has_value() ? *result : get_default_value<T>();
    }

private:
    // Integer types (excluding bool)
    template<typename T>
    typename fl::enable_if<fl::is_integral<T>::value && !fl::is_same<T, bool>::value, fl::optional<T>>::type
    as_impl() const {
        return m_value->template as_int<T>();
    }
    
    // Boolean type
    template<typename T>
    typename fl::enable_if<fl::is_same<T, bool>::value, fl::optional<T>>::type
    as_impl() const {
        return m_value->as_bool();
    }
    
    // Floating point types
    template<typename T>
    typename fl::enable_if<fl::is_floating_point<T>::value, fl::optional<T>>::type
    as_impl() const {
        // Force template call by explicitly using the templated method
        return m_value->template as_float<T>();
    }
    
    // String type
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::string>::value, fl::optional<T>>::type
    as_impl() const {
        return m_value->as_string();
    }
    
    // Array type
    template<typename T>
    typename fl::enable_if<fl::is_same<T, json_array>::value, fl::optional<T>>::type
    as_impl() const {
        return m_value->as_array();
    }
    
    // Object type
    template<typename T>
    typename fl::enable_if<fl::is_same<T, json_object>::value, fl::optional<T>>::type
    as_impl() const {
        return m_value->as_object();
    }
    
    // Specialized vector types
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<i16>>::value, fl::optional<T>>::type
    as_impl() const {
        return m_value->as_audio();
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<u8>>::value, fl::optional<T>>::type
    as_impl() const {
        return m_value->as_bytes();
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<float>>::value, fl::optional<T>>::type
    as_impl() const {
        return m_value->as_floats();
    }

    // Helper methods for getting default values for each type
    template<typename T>
    typename fl::enable_if<fl::is_integral<T>::value && !fl::is_same<T, bool>::value, T>::type
    get_default_value() const {
        return T(0);  // All integer types default to 0
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, bool>::value, T>::type
    get_default_value() const {
        return false;  // Boolean defaults to false
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_floating_point<T>::value, T>::type
    get_default_value() const {
        return T(0.0);  // Floating point types default to 0.0
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::string>::value, T>::type
    get_default_value() const {
        return fl::string();  // String defaults to empty string
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, json_array>::value, T>::type
    get_default_value() const {
        return json_array();  // Array defaults to empty array
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, json_object>::value, T>::type
    get_default_value() const {
        return json_object();  // Object defaults to empty object
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<i16>>::value, T>::type
    get_default_value() const {
        return fl::vector<i16>();  // Audio vector defaults to empty
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<u8>>::value, T>::type
    get_default_value() const {
        return fl::vector<u8>();  // Bytes vector defaults to empty
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<float>>::value, T>::type
    get_default_value() const {
        return fl::vector<float>();  // Float vector defaults to empty
    }

public:

    // Iterator support for objects
    json_value::iterator begin() { 
        if (!m_value) return json_value::iterator(json_object().begin());
        return m_value->begin(); 
    }
    json_value::iterator end() { 
        if (!m_value) return json_value::iterator(json_object().end());
        return m_value->end(); 
    }
    json_value::const_iterator begin() const { 
        if (!m_value) return json_value::const_iterator::from_iterator(json_object().begin());
        return json_value::const_iterator::from_object_iterator(m_value->begin()); 
    }
    json_value::const_iterator end() const { 
        if (!m_value) return json_value::const_iterator::from_iterator(json_object().end());
        return json_value::const_iterator::from_object_iterator(m_value->end()); 
    }
    
    // Iterator support for arrays with type conversion
    template<typename T>
    typename json_value::template array_iterator<T> begin_array() {
        if (!m_value) return typename json_value::template array_iterator<T>();
        return m_value->template begin_array<T>();
    }
    
    template<typename T>
    typename json_value::template array_iterator<T> end_array() {
        if (!m_value) return typename json_value::template array_iterator<T>();
        return m_value->template end_array<T>();
    }
    
    template<typename T>
    typename json_value::template array_iterator<T> begin_array() const {
        if (!m_value) return typename json_value::template array_iterator<T>();
        return m_value->template begin_array<T>();
    }
    
    template<typename T>
    typename json_value::template array_iterator<T> end_array() const {
        if (!m_value) return typename json_value::template array_iterator<T>();
        return m_value->template end_array<T>();
    }
    
    // Free functions for range-based for loops
    friend json_value::iterator begin(json& j) { return j.begin(); }
    friend json_value::iterator end(json& j) { return j.end(); }
    friend json_value::const_iterator begin(const json& j) { return j.begin(); }
    friend json_value::const_iterator end(const json& j) { return j.end(); }

    // Object iteration support (needed for screenmap conversion)
    fl::vector<fl::string> keys() const {
        fl::vector<fl::string> result;
        if (m_value && m_value->is_object()) {
            for (auto it = begin(); it != end(); ++it) {
                auto keyValue = *it;
                result.push_back(keyValue.first);
            }
        }
        return result;
    }
    
    // Backward compatibility methods
    fl::vector<fl::string> get_object_keys() const { return keys(); }
    fl::vector<fl::string> getObjectKeys() const { return keys(); }

    // Indexing for fluid chaining
    json operator[](size_t idx) {
        if (!m_value) {
            m_value = fl::make_shared<json_value>(json_array{});
        }
        // If we're indexing into a specialized array, convert it to regular json_array first
        if (m_value->data.is<fl::vector<i16>>() || 
            m_value->data.is<fl::vector<u8>>() || 
            m_value->data.is<fl::vector<float>>()) {
            // Convert to regular json_array
            auto arr = m_value->as_array();
            if (arr) {
                m_value = fl::make_shared<json_value>(fl::move(*arr));
            }
        }
        // Get the shared_ptr directly from the json_array to maintain reference semantics
        if (m_value->data.is<json_array>()) {
            auto arr = m_value->as_array();
            if (arr) {
                // Ensure the array is large enough
                if (idx >= arr->size()) {
                    for (size_t i = arr->size(); i <= idx; i++) {
                        arr->push_back(fl::make_shared<json_value>(nullptr));
                    }
                }
                return json((*arr)[idx]);
            }
        }
        return json(nullptr);
    }
    
    const json operator[](size_t idx) const {
        if (!m_value) {
            return json(nullptr);
        }
        // Handle regular json_array
        if (m_value->data.is<json_array>()) {
            auto arr = m_value->as_array();
            if (arr && idx < arr->size()) {
                return json((*arr)[idx]);
            }
        }
        // For specialized arrays, we need to convert them to regular arrays first
        // This is needed for compatibility with existing code that expects json_array
        if (m_value->data.is<fl::vector<i16>>() || 
            m_value->data.is<fl::vector<u8>>() || 
            m_value->data.is<fl::vector<float>>()) {
            // Convert to regular json_array
            auto arr = m_value->as_array();
            if (arr && idx < arr->size()) {
                return json((*arr)[idx]);
            }
        }
        return json(nullptr);
    }
    
    json operator[](const fl::string &key) {
        if (!m_value || !m_value->is_object()) {
            m_value = fl::make_shared<json_value>(json_object{});
        }
        // Get reference to the json_value
        auto objPtr = m_value->data.ptr<json_object>();
        if (objPtr) {
            // If key doesn't exist, create a new json_value and insert it
            if (objPtr->find(key) == objPtr->end()) {
                (*objPtr)[key] = fl::make_shared<json_value>(nullptr);
            }
            // Return a new json object that wraps the shared_ptr to the json_value
            return json((*objPtr)[key]);
        }
        // Should not happen if m_value is properly initialized as an object
        //return *reinterpret_cast<json*>(&get_null_value());
        return json(nullptr);
    }
    
    const json operator[](const fl::string &key) const {
        if (!m_value || !m_value->is_object()) {
            return json(nullptr);
        }
        auto obj = m_value->as_object();
        if (obj && obj->find(key) != obj->end()) {
            return json((*obj)[key]);
        }
        return json(nullptr);
    }

    // Contains methods for checking existence
    bool contains(size_t idx) const { 
        return m_value && m_value->contains(idx); 
    }
    bool contains(const fl::string &key) const { 
        return m_value && m_value->contains(key); 
    }

    // Size method
    size_t size() const { 
        return m_value ? m_value->size() : 0; 
    }

    // Default-value operator (pipe)
    template<typename T>
    T operator|(const T& fallback) const {
        if (!m_value) return fallback;
        return (*m_value) | fallback;
    }
    
    // NEW ERGONOMIC API: as_or<T>(default) - Conversion with custom defaults
    // Use when you want to specify your own default value
    // This method uses try_as<T>() for proper string-to-number conversion
    template<typename T>
    T as_or(const T& fallback) const {
        auto result = try_as<T>();
        return result.has_value() ? *result : fallback;
    }

    // has_value method for compatibility
    bool has_value() const {
        return m_value && !m_value->is_null();
    }

    // Get access to the internal json_value for direct variant visiting
    // Returns nullptr if no value is set
    const json_value* internal_value() const { return m_value.get(); }

    // Method to set the internal value (for json_value::to_string())
    void set_value(const fl::shared_ptr<json_value>& value) {
        m_value = value;
    }
    
    // Public method to access to_string_native for json_value::to_string()
    fl::string to_string_native_public() const { return to_string_native(); }

    // Serialization - now delegates to native implementation
    fl::string to_string() const { return to_string_native(); }
    
    // Native serialization (without external libraries)
    fl::string to_string_native() const;

    // Parsing factory method - uses native parser
    static json parse(const fl::string &txt) {
        auto parsed = json_value::parse2(txt);
        if (parsed) {
            json result;
            result.m_value = parsed;
            return result;
        }
        return json(nullptr);
    }

    // Convenience methods for creating arrays and objects
    static json array() {
        return json(json_array{});
    }

    static json object() {
        return json(json_object{});
    }
    
    // Compatibility with existing API for array/object access
    size_t get_size() const { return size(); }
    size_t getSize() const { return size(); }
    
    // Set methods for building objects
    void set(const fl::string& key, const json& value) {
        if (!m_value || !m_value->is_object()) {
            m_value = fl::make_shared<json_value>(json_object{});
        }
        // Directly assign the value to the object without going through json::operator[]
        auto objPtr = m_value->data.ptr<json_object>();
        if (objPtr) {
            // Create or update the entry directly
            if (value.m_value) {
                (*objPtr)[key] = value.m_value;
            } else {
                // If value has null m_value, create a null json_value
                (*objPtr)[key] = fl::make_shared<json_value>(nullptr);
            }
        }
    }
    
    void set(const fl::string& key, bool value) { set(key, json(value)); }
    void set(const fl::string& key, int value) { set(key, json(value)); }
    void set(const fl::string& key, i64 value) { set(key, json(value)); }
    void set(const fl::string& key, float value) { set(key, json(value)); }
    void set(const fl::string& key, double value) { set(key, json(value)); }
    void set(const fl::string& key, const fl::string& value) { set(key, json(value)); }
    void set(const fl::string& key, const char* value) { set(key, json(value)); }
    template<typename T, typename = fl::enable_if_t<fl::is_same<T, char>::value>>
    void set(const fl::string& key, T value) { set(key, json(value)); }

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
    set(const fl::string& key, IntType value) {
        // Convert to i64 for storage
        set(key, json(static_cast<i64>(value)));
    }
    
    // Array push_back methods
    void push_back(const json& value) {
        if (!m_value || !m_value->is_array()) {
            m_value = fl::make_shared<json_value>(json_array{});
        }
        // If we're pushing to a packed array, convert it to regular json_array first
        if (m_value->is_array() && 
            (m_value->data.is<fl::vector<i16>>() || 
             m_value->data.is<fl::vector<u8>>() || 
             m_value->data.is<fl::vector<float>>())) {
            // Convert to regular json_array
            auto arr = m_value->as_array();
            if (arr) {
                m_value = fl::make_shared<json_value>(fl::move(*arr));
            }
        }
        // For arrays, we need to manually handle the insertion since our indexing
        // mechanism auto-creates elements
        auto ptr = m_value->data.ptr<json_array>();
        if (ptr) {
            ptr->push_back(value.m_value);
        }
    }
    
    // Create methods for compatibility
    static json create_array() { return json::array(); }
    static json create_object() { return json::object(); }
    static json createArray() { return json::array(); }
    static json createObject() { return json::object(); }

    // Serialize method for compatibility
    fl::string serialize() const { return to_string(); }

    // Helper function to normalize JSON string (remove whitespace)
    static fl::string normalize_json_string(const char* jsonStr);
    static fl::string normalizeJsonString(const char* jsonStr) { return normalize_json_string(jsonStr); }
};

// JSON memory profiling support - defined in json.cpp.hpp
// Set g_json_stack_base to a stack address to enable stack depth tracking
extern volatile size_t g_json_stack_base;  // cast address to size_t
extern volatile size_t g_json_max_stack_depth;


} // namespace fl
