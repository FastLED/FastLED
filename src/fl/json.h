#pragma once

/**
 * @file fl/json.h
 * @brief FastLED's Elegant JSON Library: `fl::Json`
 *
 * @details
 *
 * The `fl::Json` library provides a lightweight, type-safe, and highly ergonomic
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
 *   `fl::Json::object()` and `fl::Json::array()`.
 *
 * Parsing JSON Data - The Clean Way:
 * ------------------------------------
 * Parse a JSON string and extract values with graceful defaults.
 *
 * @code
 * #include "fl/json.h"
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
 * fl::Json jsonDoc = fl::Json::parse(jsonStr);
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
 * #include "fl/json.h"
 * #include "fl/stl/string.h"
 * #include "fl/stl/vector.h"
 * #include "fl/warn.h"
 *
 * // Create a root JSON object
 * fl::Json newJson = fl::Json::object();
 *
 * // Set primitive values
 * newJson.set("version", 1.0);
 * newJson.set("isActive", true);
 * newJson.set("message", "Hello, FastLED!");
 *
 * // Create and set a nested object
 * fl::Json settings = fl::Json::object();
 * settings.set("mode", "dynamic");
 * settings.set("speed", 50);
 * newJson.set("settings", settings);
 *
 * // Create and set a nested array
 * fl::Json colors = fl::Json::array();
 * colors.push_back(fl::Json("red"));
 * colors.push_back(fl::Json("green"));
 * colors.push_back(fl::Json("blue"));
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
 * - **Null Values**: A `Json` object can represent JSON `null` or be completely
 *   uninitialized. Use `is_null()` to explicitly check for JSON null values.
 * - **Memory Management**: `fl::Json` uses `fl::shared_ptr` internally, enabling
 *   efficient copying and avoiding manual memory management pitfalls.
 * - **Error Handling**: Missing keys or type mismatches result in `fl::optional`
 *   values that can be safely queried or defaulted using `operator|`.
 *
 * Thread Safety:
 * ---------------
 * `fl::Json` is **not inherently thread-safe**. If you need to share JSON data
 * across threads, ensure proper synchronization using mutexes or similar mechanisms.
 *
 * Performance Notes:
 * -------------------
 * - Parsing: Native parser is **1.62x faster** than ArduinoJson with **62.5% higher throughput**
 * - Memory: **45% fewer allocations** and **24% lower peak memory** than ArduinoJson
 * - Validation: **Zero heap allocations** during validation phase
 * - See fl/json/README.md for detailed benchmarks
 *
 * ArduinoJson Support (Benchmarking Only):
 * ------------------------------------------
 * - **ArduinoJson is DISABLED by default** - native parser is always used
 * - To enable ArduinoJson for benchmarking: compile with `-DFASTLED_ARDUINO_JSON_PARSING_ENABLED=1`
 * - Use `fl::Json::parse_arduino_json(txt)` to explicitly use ArduinoJson parser
 * - If ArduinoJson is disabled, `parse_arduino_json()` emits a warning and falls back to native parser
 * - **Recommendation**: Use `fl::Json::parse(txt)` for all production code (native parser)
 *
 * @see fl::Json
 * @see fl::optional
 */

// Implementation details - users should not rely on these directly
#include "fl/json/detail/types.h"

namespace fl {

class Json {
private:
    fl::shared_ptr<JsonValue> m_value;

public:
    // Constructors
    Json() : m_value() {}  // Default initialize to nullptr
    Json(fl::nullptr_t) : m_value(fl::make_shared<JsonValue>(nullptr)) {}
    Json(bool b) : m_value(fl::make_shared<JsonValue>(b)) {}
    Json(int i) : m_value(fl::make_shared<JsonValue>(static_cast<i64>(i))) {}
    Json(i64 i) : m_value(fl::make_shared<JsonValue>(i)) {}
    Json(float f) : m_value(fl::make_shared<JsonValue>(f)) {}  // Use float directly
    Json(double d) : m_value(fl::make_shared<JsonValue>(static_cast<float>(d))) {}  // Convert double to float
    Json(const fl::string& s) : m_value(fl::make_shared<JsonValue>(s)) {}
    Json(const char* s): Json(fl::string(s)) {}
    Json(JsonArray a) : m_value(fl::make_shared<JsonValue>(fl::move(a))) {}
    Json(JsonObject o) : m_value(fl::make_shared<JsonValue>(fl::move(o))) {}
    // Constructor from shared_ptr<JsonValue>
    Json(const fl::shared_ptr<JsonValue>& value) : m_value(value) {}
    
    // Factory method to create a Json from a JsonValue
    static Json from_value(const JsonValue& value) {
        Json result;
        result.m_value = fl::make_shared<JsonValue>(value);
        return result;
    }
    
    // Constructor for fl::vector<float> - converts to JSON array
    Json(const fl::vector<float>& vec) : m_value(fl::make_shared<JsonValue>(JsonArray{})) {
        auto ptr = m_value->data.ptr<JsonArray>();
        if (ptr) {
            for (const auto& item : vec) {
                ptr->push_back(fl::make_shared<JsonValue>(item));  // Use float directly
            }
        }
    }
    
    // Special constructor for char values
    static Json from_char(char c) {
        Json result;
        auto value = fl::make_shared<JsonValue>(fl::string(1, c));
        //FASTLED_WARN("Created JsonValue with string: " << value->is_string() << ", int: " << value->is_int());
        result.m_value = value;
        //FASTLED_WARN("Json has string: " << result.is_string() << ", int: " << result.is_int());
        return result;
    }

    // Copy constructor
    Json(const Json& other) : m_value(other.m_value) {}

    // Assignment operator
    Json& operator=(const Json& other) {
        //FL_WARN("Json& operator=(const Json& other): " << (other.m_value ? other.m_value.get() : 0));
        if (this != &other) {
            m_value = other.m_value;
        }
        return *this;
    }

    Json& operator=(Json&& other) {
        if (this != &other) {
            m_value = fl::move(other.m_value);
        }
        return *this;
    }
    
    // Assignment operators for primitive types to avoid ambiguity
    Json& operator=(bool value) {
        m_value = fl::make_shared<JsonValue>(value);
        return *this;
    }
    
    Json& operator=(int value) {
        m_value = fl::make_shared<JsonValue>(static_cast<i64>(value));
        return *this;
    }
    
    Json& operator=(float value) {
        m_value = fl::make_shared<JsonValue>(value);
        return *this;
    }
    
    Json& operator=(double value) {
        m_value = fl::make_shared<JsonValue>(static_cast<float>(value));
        return *this;
    }
    
    Json& operator=(const fl::string& value) {
        m_value = fl::make_shared<JsonValue>(value);
        return *this;
    }
    
    Json& operator=(const char* value) {
        m_value = fl::make_shared<JsonValue>(fl::string(value));
        return *this;
    }
    
    // Assignment operator for fl::vector<float>
    Json& operator=(fl::vector<float> vec) {
        m_value = fl::make_shared<JsonValue>(JsonArray{});
        auto ptr = m_value->data.ptr<JsonArray>();
        if (ptr) {
            for (const auto& item : vec) {
                ptr->push_back(fl::make_shared<JsonValue>(item));  // Use float directly
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
    fl::optional<JsonArray> as_array() const { return m_value ? m_value->as_array() : fl::nullopt; }
    fl::optional<JsonObject> as_object() const { return m_value ? m_value->as_object() : fl::nullopt; }
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
    typename fl::enable_if<fl::is_same<T, JsonArray>::value, fl::optional<T>>::type
    as_impl() const {
        return m_value->as_array();
    }
    
    // Object type
    template<typename T>
    typename fl::enable_if<fl::is_same<T, JsonObject>::value, fl::optional<T>>::type
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
    typename fl::enable_if<fl::is_same<T, JsonArray>::value, T>::type
    get_default_value() const {
        return JsonArray();  // Array defaults to empty array
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, JsonObject>::value, T>::type
    get_default_value() const {
        return JsonObject();  // Object defaults to empty object
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
    JsonValue::iterator begin() { 
        if (!m_value) return JsonValue::iterator(JsonObject().begin());
        return m_value->begin(); 
    }
    JsonValue::iterator end() { 
        if (!m_value) return JsonValue::iterator(JsonObject().end());
        return m_value->end(); 
    }
    JsonValue::const_iterator begin() const { 
        if (!m_value) return JsonValue::const_iterator::from_iterator(JsonObject().begin());
        return JsonValue::const_iterator::from_object_iterator(m_value->begin()); 
    }
    JsonValue::const_iterator end() const { 
        if (!m_value) return JsonValue::const_iterator::from_iterator(JsonObject().end());
        return JsonValue::const_iterator::from_object_iterator(m_value->end()); 
    }
    
    // Iterator support for arrays with type conversion
    template<typename T>
    typename JsonValue::template array_iterator<T> begin_array() {
        if (!m_value) return typename JsonValue::template array_iterator<T>();
        return m_value->template begin_array<T>();
    }
    
    template<typename T>
    typename JsonValue::template array_iterator<T> end_array() {
        if (!m_value) return typename JsonValue::template array_iterator<T>();
        return m_value->template end_array<T>();
    }
    
    template<typename T>
    typename JsonValue::template array_iterator<T> begin_array() const {
        if (!m_value) return typename JsonValue::template array_iterator<T>();
        return m_value->template begin_array<T>();
    }
    
    template<typename T>
    typename JsonValue::template array_iterator<T> end_array() const {
        if (!m_value) return typename JsonValue::template array_iterator<T>();
        return m_value->template end_array<T>();
    }
    
    // Free functions for range-based for loops
    friend JsonValue::iterator begin(Json& j) { return j.begin(); }
    friend JsonValue::iterator end(Json& j) { return j.end(); }
    friend JsonValue::const_iterator begin(const Json& j) { return j.begin(); }
    friend JsonValue::const_iterator end(const Json& j) { return j.end(); }

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
    
    // Backward compatibility method
    fl::vector<fl::string> getObjectKeys() const { return keys(); }

    // Indexing for fluid chaining
    Json operator[](size_t idx) {
        if (!m_value) {
            m_value = fl::make_shared<JsonValue>(JsonArray{});
        }
        // If we're indexing into a specialized array, convert it to regular JsonArray first
        if (m_value->data.is<fl::vector<i16>>() || 
            m_value->data.is<fl::vector<u8>>() || 
            m_value->data.is<fl::vector<float>>()) {
            // Convert to regular JsonArray
            auto arr = m_value->as_array();
            if (arr) {
                m_value = fl::make_shared<JsonValue>(fl::move(*arr));
            }
        }
        // Get the shared_ptr directly from the JsonArray to maintain reference semantics
        if (m_value->data.is<JsonArray>()) {
            auto arr = m_value->as_array();
            if (arr) {
                // Ensure the array is large enough
                if (idx >= arr->size()) {
                    for (size_t i = arr->size(); i <= idx; i++) {
                        arr->push_back(fl::make_shared<JsonValue>(nullptr));
                    }
                }
                return Json((*arr)[idx]);
            }
        }
        return Json(nullptr);
    }
    
    const Json operator[](size_t idx) const {
        if (!m_value) {
            return Json(nullptr);
        }
        // Handle regular JsonArray
        if (m_value->data.is<JsonArray>()) {
            auto arr = m_value->as_array();
            if (arr && idx < arr->size()) {
                return Json((*arr)[idx]);
            }
        }
        // For specialized arrays, we need to convert them to regular arrays first
        // This is needed for compatibility with existing code that expects JsonArray
        if (m_value->data.is<fl::vector<i16>>() || 
            m_value->data.is<fl::vector<u8>>() || 
            m_value->data.is<fl::vector<float>>()) {
            // Convert to regular JsonArray
            auto arr = m_value->as_array();
            if (arr && idx < arr->size()) {
                return Json((*arr)[idx]);
            }
        }
        return Json(nullptr);
    }
    
    Json operator[](const fl::string &key) {
        if (!m_value || !m_value->is_object()) {
            m_value = fl::make_shared<JsonValue>(JsonObject{});
        }
        // Get reference to the JsonValue
        auto objPtr = m_value->data.ptr<JsonObject>();
        if (objPtr) {
            // If key doesn't exist, create a new JsonValue and insert it
            if (objPtr->find(key) == objPtr->end()) {
                (*objPtr)[key] = fl::make_shared<JsonValue>(nullptr);
            }
            // Return a new Json object that wraps the shared_ptr to the JsonValue
            return Json((*objPtr)[key]);
        }
        // Should not happen if m_value is properly initialized as an object
        //return *reinterpret_cast<Json*>(&get_null_value());
        return Json(nullptr);
    }
    
    const Json operator[](const fl::string &key) const {
        if (!m_value || !m_value->is_object()) {
            return Json(nullptr);
        }
        auto obj = m_value->as_object();
        if (obj && obj->find(key) != obj->end()) {
            return Json((*obj)[key]);
        }
        return Json(nullptr);
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

    // Get access to the internal JsonValue for direct variant visiting
    // Returns nullptr if no value is set
    const JsonValue* internal_value() const { return m_value.get(); }

    // Method to set the internal value (for JsonValue::to_string())
    void set_value(const fl::shared_ptr<JsonValue>& value) {
        m_value = value;
    }
    
    // Public method to access to_string_native for JsonValue::to_string()
    fl::string to_string_native_public() const { return to_string_native(); }

    // Serialization - now delegates to native implementation
    fl::string to_string() const { return to_string_native(); }
    
    // Native serialization (without external libraries)
    fl::string to_string_native() const;

    // Parsing factory method - uses native parser
    static Json parse(const fl::string &txt) {
        auto parsed = JsonValue::parse2(txt);
        if (parsed) {
            Json result;
            result.m_value = parsed;
            return result;
        }
        return Json(nullptr);
    }

    // ArduinoJson parsing (opt-in via FASTLED_ARDUINO_JSON_PARSING_ENABLED=1)
    // This is kept ONLY for benchmarking purposes
    static Json parse_arduino_json(const fl::string &txt) {
        #if FASTLED_ARDUINO_JSON_PARSING_ENABLED
        // ArduinoJson enabled - use ArduinoJson parser
        auto parsed = JsonValue::parse(txt);
        if (parsed) {
            Json result;
            result.m_value = parsed;
            return result;
        }
        return Json(nullptr);
        #else
        // ArduinoJson disabled - emit warning and delegate to native parser
        FL_WARN("ArduinoJson parsing is disabled (compiled out). Falling back to native fl::Json::parse(). "
                "Enable with -DFASTLED_ARDUINO_JSON_PARSING_ENABLED=1 if needed for benchmarking.");
        return parse(txt);  // Delegate to native parser
        #endif
    }

    // Convenience methods for creating arrays and objects
    static Json array() {
        return Json(JsonArray{});
    }

    static Json object() {
        return Json(JsonObject{});
    }
    
    // Compatibility with existing API for array/object access
    size_t getSize() const { return size(); }
    
    // Set methods for building objects
    void set(const fl::string& key, const Json& value) {
        if (!m_value || !m_value->is_object()) {
            m_value = fl::make_shared<JsonValue>(JsonObject{});
        }
        // Directly assign the value to the object without going through Json::operator[]
        auto objPtr = m_value->data.ptr<JsonObject>();
        if (objPtr) {
            // Create or update the entry directly
            if (value.m_value) {
                (*objPtr)[key] = value.m_value;
            } else {
                // If value has null m_value, create a null JsonValue
                (*objPtr)[key] = fl::make_shared<JsonValue>(nullptr);
            }
        }
    }
    
    void set(const fl::string& key, bool value) { set(key, Json(value)); }
    void set(const fl::string& key, int value) { set(key, Json(value)); }
    void set(const fl::string& key, i64 value) { set(key, Json(value)); }
    void set(const fl::string& key, float value) { set(key, Json(value)); }
    void set(const fl::string& key, double value) { set(key, Json(value)); }
    void set(const fl::string& key, const fl::string& value) { set(key, Json(value)); }
    void set(const fl::string& key, const char* value) { set(key, Json(value)); }
    template<typename T, typename = fl::enable_if_t<fl::is_same<T, char>::value>>
    void set(const fl::string& key, T value) { set(key, Json(value)); }

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
        set(key, Json(static_cast<i64>(value)));
    }
    
    // Array push_back methods
    void push_back(const Json& value) {
        if (!m_value || !m_value->is_array()) {
            m_value = fl::make_shared<JsonValue>(JsonArray{});
        }
        // If we're pushing to a packed array, convert it to regular JsonArray first
        if (m_value->is_array() && 
            (m_value->data.is<fl::vector<i16>>() || 
             m_value->data.is<fl::vector<u8>>() || 
             m_value->data.is<fl::vector<float>>())) {
            // Convert to regular JsonArray
            auto arr = m_value->as_array();
            if (arr) {
                m_value = fl::make_shared<JsonValue>(fl::move(*arr));
            }
        }
        // For arrays, we need to manually handle the insertion since our indexing
        // mechanism auto-creates elements
        auto ptr = m_value->data.ptr<JsonArray>();
        if (ptr) {
            ptr->push_back(value.m_value);
        }
    }
    
    // Create methods for compatibility
    static Json createArray() { return Json::array(); }
    static Json createObject() { return Json::object(); }
    
    // Serialize method for compatibility
    fl::string serialize() const { return to_string(); }

    // Helper function to normalize JSON string (remove whitespace)
    static fl::string normalizeJsonString(const char* jsonStr);
};

// JSON memory profiling support - defined in json.cpp.hpp
// Set g_json_stack_base to a stack address to enable stack depth tracking
extern volatile size_t g_json_stack_base;  // cast address to size_t
extern volatile size_t g_json_max_stack_depth;


} // namespace fl
