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
 * #include "fl/string.h"
 * #include "fl/vector.h"
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
 * - **Error Handling**: While `operator|` is powerful, for critical parsing
 *   steps (e.g., validating the root object), always use `has_value()` and
 *   `is_object()`/`is_array()` checks.
 * - **Memory Management**: `fl::Json` leverages `fl::shared_ptr` internally,
 *   simplifying memory management. You typically won't need manual `new`/`delete`.
 * - **`fl::` Namespace**: Adhere to FastLED's convention; always use the `fl::`
 *   prefix for library components (e.g., `fl::Json`, `fl::string`, `fl::vector`).
 *   Avoid `std::` equivalents.
 *
 * HIGH LEVEL: For platforms with a lot of memory, this parsing library (ArduinoJson) will automatically be included.
 * Othweise you'll just get Json -> str encoding (and no parsing). You can check if you haee
 * the full library by detecting if FASTLED_ENABLE_JSON is defined.
 *
 * It's important to note that ArduinoJson only is used for parsing. We use a custom serializer for
 * output to string. But this is usually easy to do. For help serializing out, look at fl/sstream.h
 * for converting a collection of values to a string.
 *
 * It's entirely possible that our json string output serializer is NOT 100% correct with respect to
 * complex string encoding (for example an HTML document). If you see bugs, then file an issue at
 * https://github.com/fastled/FastLED/issues
 *
 * for string parsing, we should be full featured when FASTLED_ENABLE_JSON is defined (automiatic for SKETCH_HAS_LOTS_OF_MEMORY).
 * If there is some string that doesn't correctly parse, use b64 encoding. For example, you might get better luck b64 encoding
 * 1024 elements of small ints then manually deserializing to a fl::vector<u8>. Infact, it's pretty much assured.
 *
 * That being said...
 *
 * This api is designed specifically to be fast for input <--> output of arrays of numbers in {u8, i16, float}.
 * If you have a different encodings scheme, for example an array of tuples, then this library will be MUCH MUCH
 * slower than Arduino Json. If you stick to the scheme we have optimized for (flat arrays of numbers
 * and few dictionaries) then this api will be blazing fast. Otherwise? Expect pain: slowdowns and lots of memory consumption.
 *
 * Why?
 *
 * ArduinoJson only has a nice interface if you agree to bring in std::-everything. Ever bring in std::sstream?
 * The amount of code that will be pulled for <sstream> will blow your mind. To keep things strict and tiny
 * we have to take ArduinoJson and strip all the optional components out then seal the remaining api in cement and
 * bolt on a nice fluid interface ontop. That's what fl/json.h is.
 *
 * And the good news is - it works great!! And if it doesn't? File a bug and we'll have it fixed in the next release.
*/


#include "fl/string.h"
#include "fl/vector.h"
#include "fl/hash_map.h"
#include "fl/variant.h"
#include "fl/optional.h"
#include "fl/unique_ptr.h"
#include "fl/shared_ptr.h"
#include "fl/functional.h"
#include "fl/str.h" // For StringFormatter
#include "fl/promise.h" // For Error type
#include "fl/warn.h" // For FL_WARN

#include "fl/sketch_macros.h"

#ifndef FASTLED_ENABLE_JSON
#define FASTLED_ENABLE_JSON SKETCH_HAS_LOTS_OF_MEMORY
#endif


namespace fl {


// Forward declarations
struct JsonValue;

// Define Array and Object as pointers to avoid incomplete type issues
// We'll use heap-allocated containers for these to avoid alignment issues
using JsonArray = fl::vector<fl::shared_ptr<JsonValue>>;
using JsonObject = fl::HashMap<fl::string, fl::shared_ptr<JsonValue>>;

// ParseResult struct to replace variant<T, Error>
template<typename T>
struct ParseResult {
    T value;
    Error error;
    
    ParseResult(const T& val) : value(val), error() {}
    ParseResult(const Error& err) : value(), error(err) {}
    
    bool has_error() const { return !error.is_empty(); }
    const T& get_value() const { return value; }
    const Error& get_error() const { return error; }
    
    // Implicit conversion operator to allow using ParseResult as T directly
    operator const T&() const { 
        if (has_error()) {
            // This should ideally trigger some kind of error handling
            // For now, we'll just return the value (which might be default-initialized)
        }
        return value; 
    }
};

// Function to get a reference to a static null JsonValue
JsonValue& get_null_value();

// Function to get a reference to a static empty JsonObject
JsonObject& get_empty_json_object();

// AI - pay attention to this - implementing visitor pattern
template<typename T>
struct DefaultValueVisitor {
    const T& fallback;
    const T* result = nullptr;
    T storage; // Use instance storage instead of static

    DefaultValueVisitor(const T& fb) : fallback(fb) {}

    // This is the method that fl::Variant expects
    template<typename U>
    void accept(const U& value) {
        // Dispatch to the correct operator() overload
        (*this)(value);
    }

    // Specific overload for the type T
    void operator()(const T& value) {
        result = &value;
    }
    
    // Special handling for integer conversions
    template<typename U>
    typename fl::enable_if<fl::is_integral<T>::value && fl::is_integral<U>::value, void>::type
    operator()(const U& value) {
        // Convert between integer types
        storage = static_cast<T>(value);
        result = &storage;
    }
    
    // Special handling for floating point to integer conversion
    template<typename U>
    typename fl::enable_if<fl::is_integral<T>::value && fl::is_floating_point<U>::value, void>::type
    operator()(const U& value) {
        // Convert float to integer
        storage = static_cast<T>(value);
        result = &storage;
    }
    
    // Special handling for integer to floating point conversion
    template<typename U>
    typename fl::enable_if<fl::is_floating_point<T>::value && fl::is_integral<U>::value, void>::type
    operator()(const U& value) {
        // Convert integer to float
        storage = static_cast<T>(value);
        result = &storage;
    }
    
    // Special handling for floating point to floating point conversion
    template<typename U>
    typename fl::enable_if<fl::is_floating_point<T>::value && fl::is_floating_point<U>::value && !fl::is_same<T, U>::value, void>::type
    operator()(const U& value) {
        // Convert between floating point types (e.g., double to float)
        storage = static_cast<T>(value);
        result = &storage;
    }
    


    // Generic overload for all other types
    template<typename U>
    typename fl::enable_if<
        !(fl::is_integral<T>::value && fl::is_integral<U>::value) &&
        !(fl::is_integral<T>::value && fl::is_floating_point<U>::value) &&
        !(fl::is_floating_point<T>::value && fl::is_integral<U>::value) &&
        !(fl::is_floating_point<T>::value && fl::is_floating_point<U>::value && !fl::is_same<T, U>::value),
        void>::type
    operator()(const U&) {
        // Do nothing for other types
    }
    
    // Special handling for nullptr_t
    void operator()(const fl::nullptr_t&) {
        // Do nothing - will return fallback
    }
};

// Visitor for converting values to int
template<typename IntType = int64_t>
struct IntConversionVisitor {
    fl::optional<IntType> result;
    
    template<typename U>
    void accept(const U& value) {
        // Dispatch to the correct operator() overload
        (*this)(value);
    }
    
    // Special handling to avoid conflict when IntType is int64_t
    template<typename T = IntType>
    typename fl::enable_if<!fl::is_same<T, int64_t>::value && !fl::is_same<T, double>::value, void>::type
    operator()(const IntType& value) {
        result = value;
    }
    
    // Special handling for int64_t case
    template<typename T = IntType>
    typename fl::enable_if<fl::is_same<T, int64_t>::value, void>::type
    operator()(const int64_t& value) {
        result = value;
    }
    
    // Special handling for double case (when IntType is double)
    template<typename T = IntType>
    typename fl::enable_if<fl::is_same<T, double>::value, void>::type
    operator()(const double& value) {
        result = value;
    }
    
    // Special handling to avoid conflict when IntType is int64_t
    template<typename T = IntType>
    typename fl::enable_if<!fl::is_same<T, int64_t>::value, void>::type
    operator()(const int64_t& value) {
        result = static_cast<IntType>(value);
    }
    
    void operator()(const bool& value) {
        result = static_cast<IntType>(value ? 1 : 0);
    }
    
    // Special handling for double to int conversion
    template<typename T = IntType>
    typename fl::enable_if<!fl::is_same<T, double>::value, void>::type
    operator()(const double& value) {
        // NEW INSTRUCTIONS: AUTO CONVERT FLOAT TO INT
        result = static_cast<IntType>(value);
    }
    
    void operator()(const fl::string& str) {
        // NEW INSTRUCTIONS: AUTO CONVERT STRING TO INT
        // Try to parse the string as an integer using FastLED's StringFormatter
        // Validate by checking if string contains only digits (and optional +/- sign)
        bool isValidInt = true;
        fl::size startPos = 0;
        
        // Check for sign
        if (str.length() > 0 && (str[0] == '+' || str[0] == '-')) {
            startPos = 1;
        }
        
        // Check that all remaining characters are digits
        for (fl::size i = startPos; i < str.length(); i++) {
            if (!StringFormatter::isDigit(str[i])) {
                isValidInt = false;
                break;
            }
        }
        
        // If it looks like a valid integer, try to parse it
        if (isValidInt && str.length() > 0) {
            int parsed = StringFormatter::parseInt(str.c_str(), str.length());
            result = static_cast<IntType>(parsed);
        }
    }
    
    template<typename T>
    void operator()(const T&) {
        // Do nothing for other types
    }
};

// Specialization for int64_t to avoid template conflicts
template<>
struct IntConversionVisitor<int64_t> {
    fl::optional<int64_t> result;
    
    template<typename U>
    void accept(const U& value) {
        // Dispatch to the correct operator() overload
        (*this)(value);
    }
    
    void operator()(const int64_t& value) {
        result = value;
    }
    
    void operator()(const bool& value) {
        result = value ? 1 : 0;
    }
    
    void operator()(const double& value) {
        // NEW INSTRUCTIONS: AUTO CONVERT FLOAT TO INT
        result = static_cast<int64_t>(value);
    }
    
    void operator()(const fl::string& str) {
        // NEW INSTRUCTIONS: AUTO CONVERT STRING TO INT
        // Try to parse the string as an integer using FastLED's StringFormatter
        // Validate by checking if string contains only digits (and optional +/- sign)
        bool isValidInt = true;
        fl::size startPos = 0;
        
        // Check for sign
        if (str.length() > 0 && (str[0] == '+' || str[0] == '-')) {
            startPos = 1;
        }
        
        // Check that all remaining characters are digits
        for (fl::size i = startPos; i < str.length(); i++) {
            if (!StringFormatter::isDigit(str[i])) {
                isValidInt = false;
                break;
            }
        }
        
        // If it looks like a valid integer, try to parse it
        if (isValidInt && str.length() > 0) {
            int parsed = StringFormatter::parseInt(str.c_str(), str.length());
            result = static_cast<int64_t>(parsed);
        }
    }
    
    template<typename T>
    void operator()(const T&) {
        // Do nothing for other types
    }
};

// Visitor for converting values to float
template<typename FloatType = double>
struct FloatConversionVisitor {
    fl::optional<FloatType> result;
    
    template<typename U>
    void accept(const U& value) {
        // Dispatch to the correct operator() overload
        (*this)(value);
    }
    
    void operator()(const FloatType& value) {
        result = value;
    }
    
    // Special handling to avoid conflict when FloatType is double
    template<typename T = FloatType>
    typename fl::enable_if<!fl::is_same<T, double>::value, void>::type
    operator()(const double& value) {
        result = static_cast<FloatType>(value);
    }
    
    // Special handling to avoid conflict when FloatType is float
    template<typename T = FloatType>
    typename fl::enable_if<!fl::is_same<T, float>::value, void>::type
    operator()(const float& value) {
        result = static_cast<FloatType>(value);
    }
    
    void operator()(const int64_t& value) {
        // NEW INSTRUCTIONS: AUTO CONVERT INT TO FLOAT
        result = static_cast<FloatType>(value);
    }
    
    void operator()(const bool& value) {
        result = static_cast<FloatType>(value ? 1.0 : 0.0);
    }
    
    void operator()(const fl::string& str) {
        // NEW INSTRUCTIONS: AUTO CONVERT STRING TO FLOAT
        // Try to parse the string as a float using FastLED's StringFormatter
        // Validate by checking if string contains valid float characters
        bool isValidFloat = true;
        bool hasDecimal = false;
        fl::size startPos = 0;
        
        // Check for sign
        if (str.length() > 0 && (str[0] == '+' || str[0] == '-')) {
            startPos = 1;
        }
        
        // Check that all remaining characters are valid for a float
        for (fl::size i = startPos; i < str.length(); i++) {
            char c = str[i];
            if (c == '.') {
                if (hasDecimal) {
                    // Multiple decimal points
                    isValidFloat = false;
                    break;
                }
                hasDecimal = true;
            } else if (!StringFormatter::isDigit(c) && c != 'e' && c != 'E') {
                isValidFloat = false;
                break;
            }
        }
        
        // If it looks like a valid float, try to parse it
        if (isValidFloat && str.length() > 0) {
            // For simple cases, we can use a more precise approach
            // Check if it's a simple decimal number
            bool isSimpleDecimal = true;
            for (fl::size i = startPos; i < str.length(); i++) {
                char c = str[i];
                if (c != '.' && !StringFormatter::isDigit(c)) {
                    isSimpleDecimal = false;
                    break;
                }
            }
            
            if (isSimpleDecimal) {
                // For simple decimals, we can do a more direct conversion
                float parsed = StringFormatter::parseFloat(str.c_str(), str.length());
                result = static_cast<FloatType>(parsed);
            } else {
                // For complex floats (with exponents), use the standard approach
                float parsed = StringFormatter::parseFloat(str.c_str(), str.length());
                result = static_cast<FloatType>(parsed);
            }
        }
    }
    
    template<typename T>
    void operator()(const T&) {
        // Do nothing for other types
    }
};

// Specialization for double to avoid template conflicts
template<>
struct FloatConversionVisitor<double> {
    fl::optional<double> result;
    
    template<typename U>
    void accept(const U& value) {
        // Dispatch to the correct operator() overload
        (*this)(value);
    }
    
    void operator()(const double& value) {
        result = value;
    }
    
    void operator()(const float& value) {
        result = static_cast<double>(value);
    }
    
    void operator()(const int64_t& value) {
        // NEW INSTRUCTIONS: AUTO CONVERT INT TO FLOAT
        result = static_cast<double>(value);
    }
    
    void operator()(const bool& value) {
        result = value ? 1.0 : 0.0;
    }
    
    void operator()(const fl::string& str) {
        // NEW INSTRUCTIONS: AUTO CONVERT STRING TO FLOAT
        // Try to parse the string as a float using FastLED's StringFormatter
        // Validate by checking if string contains valid float characters
        bool isValidFloat = true;
        bool hasDecimal = false;
        fl::size startPos = 0;
        
        // Check for sign
        if (str.length() > 0 && (str[0] == '+' || str[0] == '-')) {
            startPos = 1;
        }
        
        // Check that all remaining characters are valid for a float
        for (fl::size i = startPos; i < str.length(); i++) {
            char c = str[i];
            if (c == '.') {
                if (hasDecimal) {
                    // Multiple decimal points
                    isValidFloat = false;
                    break;
                }
                hasDecimal = true;
            } else if (!StringFormatter::isDigit(c) && c != 'e' && c != 'E') {
                isValidFloat = false;
                break;
            }
        }
        
        // If it looks like a valid float, try to parse it
        if (isValidFloat && str.length() > 0) {
            // For simple cases, we can use a more precise approach
            // Check if it's a simple decimal number
            bool isSimpleDecimal = true;
            for (fl::size i = startPos; i < str.length(); i++) {
                char c = str[i];
                if (c != '.' && !StringFormatter::isDigit(c)) {
                    isSimpleDecimal = false;
                    break;
                }
            }
            
            if (isSimpleDecimal) {
                // For simple decimals, we can do a more direct conversion
                float parsed = StringFormatter::parseFloat(str.c_str(), str.length());
                result = static_cast<double>(parsed);
            } else {
                // For complex floats (with exponents), use the standard approach
                float parsed = StringFormatter::parseFloat(str.c_str(), str.length());
                result = static_cast<double>(parsed);
            }
        }
    }
    
    template<typename T>
    void operator()(const T&) {
        // Do nothing for other types
    }
};

// Visitor for converting values to string
struct StringConversionVisitor {
    fl::optional<fl::string> result;
    
    template<typename U>
    void accept(const U& value) {
        // Dispatch to the correct operator() overload
        (*this)(value);
    }
    
    void operator()(const fl::string& value) {
        result = value;
    }
    
    void operator()(const int64_t& value) {
        // Convert integer to string
        result = fl::to_string(value);
    }
    
    void operator()(const double& value) {
        // Convert double to string with higher precision for JSON representation
        result = fl::to_string(static_cast<float>(value), 6);
    }
    
    void operator()(const float& value) {
        // Convert float to string with higher precision for JSON representation
        result = fl::to_string(value, 6);
    }
    
    void operator()(const bool& value) {
        // Convert bool to string
        result = value ? "true" : "false";
    }
    
    void operator()(const fl::nullptr_t&) {
        // Convert null to string
        result = "null";
    }
    
    template<typename T>
    void operator()(const T&) {
        // Do nothing for other types (arrays, objects)
    }
};

// The JSON node
struct JsonValue {
    // Forward declarations for nested iterator classes
    class iterator;
    class const_iterator;
    
    // Friend declarations
    friend class Json;
    
    
    // The variant holds exactly one of these alternatives
    using variant_t = fl::Variant<
        fl::nullptr_t,   // null
        bool,            // true/false
        int64_t,         // integer
        float,           // floating-point (changed from double to float)
        fl::string,      // string
        JsonArray,           // array
        JsonObject,          // object
        fl::vector<int16_t>, // audio data (specialized array of int16_t)
        fl::vector<uint8_t>, // byte data (specialized array of uint8_t)
        fl::vector<float>    // float data (specialized array of float)
    >;

    typedef JsonValue::iterator iterator;
    typedef JsonValue::const_iterator const_iterator;

    variant_t data;

    // Constructors
    JsonValue() noexcept : data(nullptr) {}
    JsonValue(fl::nullptr_t) noexcept : data(nullptr) {}
    JsonValue(bool b) noexcept : data(b) {}
    JsonValue(int64_t i) noexcept : data(i) {}
    JsonValue(float f) noexcept : data(f) {}  // Changed from double to float
    JsonValue(const fl::string& s) : data(s) {
    }
    JsonValue(const JsonArray& a) : data(a) {
        //FASTLED_WARN("Created JsonValue with array");
    }
    JsonValue(const JsonObject& o) : data(o) {
        //FASTLED_WARN("Created JsonValue with object");
    }
    JsonValue(const fl::vector<int16_t>& audio) : data(audio) {
        //FASTLED_WARN("Created JsonValue with audio data");
    }
    
    JsonValue(fl::vector<int16_t>&& audio) : data(fl::move(audio)) {
        //FASTLED_WARN("Created JsonValue with moved audio data");
    }
    
    JsonValue(const fl::vector<uint8_t>& bytes) : data(bytes) {
        //FASTLED_WARN("Created JsonValue with byte data");
    }
    
    JsonValue(fl::vector<uint8_t>&& bytes) : data(fl::move(bytes)) {
        //FASTLED_WARN("Created JsonValue with moved byte data");
    }
    
    JsonValue(const fl::vector<float>& floats) : data(floats) {
        //FASTLED_WARN("Created JsonValue with float data");
    }
    
    JsonValue(fl::vector<float>&& floats) : data(fl::move(floats)) {
        //FASTLED_WARN("Created JsonValue with moved float data");
    }

    // Copy constructor
    JsonValue(const JsonValue& other) : data(other.data) {}

    JsonValue& operator=(const JsonValue& other) {
        data = other.data;
        return *this;
    }

    JsonValue& operator=(JsonValue&& other) {
        data = fl::move(other.data);
        return *this;
    }

    template<typename T>
    typename fl::enable_if<!fl::is_same<typename fl::remove_cv<typename fl::remove_reference<T>::type>::type, JsonValue>::value, JsonValue&>::type
    operator=(T&& value) {
        data = fl::forward<T>(value);
        return *this;
    }

    JsonValue& operator=(fl::nullptr_t) {
        data = nullptr;
        return *this;
    }

    JsonValue& operator=(bool b) {
        data = b;
        return *this;
    }

    JsonValue& operator=(int64_t i) {
        data = i;
        return *this;
    }

    JsonValue& operator=(double d) {
        data = static_cast<float>(d);
        return *this;
    }
    
    JsonValue& operator=(float f) {
        data = f;
        return *this;
    }

    JsonValue& operator=(fl::string s) {
        data = fl::move(s);
        return *this;
    }

    JsonValue& operator=(JsonArray a) {
        data = fl::move(a);
        return *this;
    }
    
    JsonValue& operator=(fl::vector<int16_t> audio) {
        data = fl::move(audio);
        return *this;
    }
    
    JsonValue& operator=(fl::vector<uint8_t> bytes) {
        data = fl::move(bytes);
        return *this;
    }
    
    JsonValue& operator=(fl::vector<float> floats) {
        data = fl::move(floats);
        return *this;
    }


    
    // Special constructor for char values
    static fl::shared_ptr<JsonValue> from_char(char c) {
        return fl::make_shared<JsonValue>(fl::string(1, c));
    }
    
    // Visitor pattern implementation
    template<typename Visitor>
    auto visit(Visitor&& visitor) -> decltype(visitor(fl::nullptr_t{})) {
        return data.visit(fl::forward<Visitor>(visitor));
    }
    
    template<typename Visitor>
    auto visit(Visitor&& visitor) const -> decltype(visitor(fl::nullptr_t{})) {
        return data.visit(fl::forward<Visitor>(visitor));
    }

    // Type queries - using is<T>() instead of index() for fl::Variant
    bool is_null() const noexcept { 
        //FASTLED_WARN("is_null called, tag=" << data.tag());
        return data.is<fl::nullptr_t>(); 
    }
    bool is_bool() const noexcept { 
        //FASTLED_WARN("is_bool called, tag=" << data.tag());
        return data.is<bool>(); 
    }
    bool is_int() const noexcept { 
        //FASTLED_WARN("is_int called, tag=" << data.tag());
        return data.is<int64_t>() || data.is<bool>(); 
    }
    bool is_double() const noexcept { 
        //FASTLED_WARN("is_double called, tag=" << data.tag());
        return data.is<float>(); 
    }
    bool is_float() const noexcept { 
        return data.is<float>(); 
    }
    bool is_string() const noexcept { 
        //FASTLED_WARN("is_string called, tag=" << data.tag());
        return data.is<fl::string>(); 
    }
    // Visitor for array type checking
    struct IsArrayVisitor {
        bool result = false;
        
        template<typename T>
        void accept(const T& value) {
            // Dispatch to the correct operator() overload
            (*this)(value);
        }
        
        // JsonArray is an array
        void operator()(const JsonArray&) {
            result = true;
        }
        
        // Specialized array types ARE arrays
        void operator()(const fl::vector<int16_t>&) {
            result = true;  // Audio data is still an array
        }
        
        void operator()(const fl::vector<uint8_t>&) {
            result = true;  // Byte data is still an array
        }
        
        void operator()(const fl::vector<float>&) {
            result = true;  // Float data is still an array
        }
        
        // Generic handler for all other types
        template<typename T>
        void operator()(const T&) {
            result = false;
        }
    };
    
    bool is_array() const noexcept { 
        //FASTLED_WARN("is_array called, tag=" << data.tag());
        IsArrayVisitor visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    // Returns true only for JsonArray (not specialized array types)
    bool is_generic_array() const noexcept {
        return data.is<JsonArray>();
    }
    

    
    bool is_object() const noexcept { 
        //FASTLED_WARN("is_object called, tag=" << data.tag());
        return data.is<JsonObject>(); 
    }
    bool is_audio() const noexcept {
        //FASTLED_WARN("is_audio called, tag=" << data.tag());
        return data.is<fl::vector<int16_t>>();
    }
    bool is_bytes() const noexcept {
        //FASTLED_WARN("is_bytes called, tag=" << data.tag());
        return data.is<fl::vector<uint8_t>>();
    }
    bool is_floats() const noexcept {
        //FASTLED_WARN("is_floats called, tag=" << data.tag());
        return data.is<fl::vector<float>>();
    }

    // Safe extractors (return optional values, not references)
    fl::optional<bool> as_bool() {
        auto ptr = data.ptr<bool>();
        return ptr ? fl::optional<bool>(*ptr) : fl::nullopt;
    }
    
    fl::optional<int64_t> as_int() {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }
        
        IntConversionVisitor<int64_t> visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    template<typename IntType>
    fl::optional<IntType> as_int() {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }
        
        IntConversionVisitor<IntType> visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    fl::optional<double> as_double() const {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }
        
        FloatConversionVisitor<double> visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    fl::optional<float> as_float() {
        return as_float<float>();
    }
    
    template<typename FloatType>
    fl::optional<FloatType> as_float() {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }
        
        FloatConversionVisitor<FloatType> visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    fl::optional<fl::string> as_string() {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }
        
        StringConversionVisitor visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    fl::optional<JsonArray> as_array() {
        auto ptr = data.ptr<JsonArray>();
        if (ptr) return fl::optional<JsonArray>(*ptr);
        
        // Handle specialized array types by converting them to regular JsonArray
        if (data.is<fl::vector<int16_t>>()) {
            auto audioPtr = data.ptr<fl::vector<int16_t>>();
            JsonArray result;
            for (const auto& item : *audioPtr) {
                result.push_back(fl::make_shared<JsonValue>(static_cast<int64_t>(item)));
            }
            return fl::optional<JsonArray>(result);
        }
        
        if (data.is<fl::vector<uint8_t>>()) {
            auto bytePtr = data.ptr<fl::vector<uint8_t>>();
            JsonArray result;
            for (const auto& item : *bytePtr) {
                result.push_back(fl::make_shared<JsonValue>(static_cast<int64_t>(item)));
            }
            return fl::optional<JsonArray>(result);
        }
        
        if (data.is<fl::vector<float>>()) {
            auto floatPtr = data.ptr<fl::vector<float>>();
            JsonArray result;
            for (const auto& item : *floatPtr) {
                result.push_back(fl::make_shared<JsonValue>(item));  // Use float directly
            }
            return fl::optional<JsonArray>(result);
        }
        
        return fl::nullopt;
    }
    
    fl::optional<JsonObject> as_object() {
        auto ptr = data.ptr<JsonObject>();
        return ptr ? fl::optional<JsonObject>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::vector<int16_t>> as_audio() {
        auto ptr = data.ptr<fl::vector<int16_t>>();
        return ptr ? fl::optional<fl::vector<int16_t>>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::vector<uint8_t>> as_bytes() {
        auto ptr = data.ptr<fl::vector<uint8_t>>();
        return ptr ? fl::optional<fl::vector<uint8_t>>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::vector<float>> as_floats() {
        auto ptr = data.ptr<fl::vector<float>>();
        return ptr ? fl::optional<fl::vector<float>>(*ptr) : fl::nullopt;
    }

    // Const overloads
    fl::optional<bool> as_bool() const {
        auto ptr = data.ptr<bool>();
        return ptr ? fl::optional<bool>(*ptr) : fl::nullopt;
    }
    
    fl::optional<int64_t> as_int() const {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }
        
        IntConversionVisitor<int64_t> visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    template<typename IntType>
    fl::optional<IntType> as_int() const {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }
        
        IntConversionVisitor<IntType> visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    fl::optional<float> as_float() const {
        return as_float<float>();
    }
    
    template<typename FloatType>
    fl::optional<FloatType> as_float() const {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }
        
        FloatConversionVisitor<FloatType> visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    fl::optional<fl::string> as_string() const {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }
        
        StringConversionVisitor visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    fl::optional<JsonArray> as_array() const {
        auto ptr = data.ptr<JsonArray>();
        if (ptr) return fl::optional<JsonArray>(*ptr);
        
        // Handle specialized array types by converting them to regular JsonArray
        if (data.is<fl::vector<int16_t>>()) {
            auto audioPtr = data.ptr<fl::vector<int16_t>>();
            JsonArray result;
            for (const auto& item : *audioPtr) {
                result.push_back(fl::make_shared<JsonValue>(static_cast<int64_t>(item)));
            }
            return fl::optional<JsonArray>(result);
        }
        
        if (data.is<fl::vector<uint8_t>>()) {
            auto bytePtr = data.ptr<fl::vector<uint8_t>>();
            JsonArray result;
            for (const auto& item : *bytePtr) {
                result.push_back(fl::make_shared<JsonValue>(static_cast<int64_t>(item)));
            }
            return fl::optional<JsonArray>(result);
        }
        
        if (data.is<fl::vector<float>>()) {
            auto floatPtr = data.ptr<fl::vector<float>>();
            JsonArray result;
            for (const auto& item : *floatPtr) {
                result.push_back(fl::make_shared<JsonValue>(item));  // Use float directly
            }
            return fl::optional<JsonArray>(result);
        }
        
        return fl::nullopt;
    }
    
    fl::optional<JsonObject> as_object() const {
        auto ptr = data.ptr<JsonObject>();
        return ptr ? fl::optional<JsonObject>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::vector<int16_t>> as_audio() const {
        auto ptr = data.ptr<fl::vector<int16_t>>();
        return ptr ? fl::optional<fl::vector<int16_t>>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::vector<uint8_t>> as_bytes() const {
        auto ptr = data.ptr<fl::vector<uint8_t>>();
        return ptr ? fl::optional<fl::vector<uint8_t>>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::vector<float>> as_floats() const {
        auto ptr = data.ptr<fl::vector<float>>();
        return ptr ? fl::optional<fl::vector<float>>(*ptr) : fl::nullopt;
    }
    
    // Generic getter template method
    template<typename T>
    fl::optional<T> get() const {
        auto ptr = data.ptr<T>();
        return ptr ? fl::optional<T>(*ptr) : fl::nullopt;
    }
    
    template<typename T>
    fl::optional<T> get() {
        auto ptr = data.ptr<T>();
        return ptr ? fl::optional<T>(*ptr) : fl::nullopt;
    }

    // Iterator support for objects and arrays
    iterator begin() {
        if (is_object()) {
            auto ptr = data.ptr<JsonObject>();
            return iterator(ptr->begin());
        }
        // Use temporary empty object to avoid static initialization conflicts with Teensy
        return iterator(JsonObject().begin());
    }
    
    iterator end() {
        if (is_object()) {
            auto ptr = data.ptr<JsonObject>();
            return iterator(ptr->end());
        }
        // Use temporary empty object to avoid static initialization conflicts with Teensy
        return iterator(JsonObject().end());
    }
    
    const_iterator begin() const {
        if (is_object()) {
            auto ptr = data.ptr<const JsonObject>();
            if (!ptr) return const_iterator::from_iterator(JsonObject().begin());
            return const_iterator::from_iterator(ptr->begin());
        }
        // Use temporary empty object to avoid static initialization conflicts with Teensy
        return const_iterator::from_iterator(JsonObject().begin());
    }
    
    
    const_iterator end() const {
        if (is_object()) {
            auto ptr = data.ptr<const JsonObject>();
            if (!ptr) return const_iterator::from_iterator(JsonObject().end());
            return const_iterator::from_iterator(ptr->end());
        }
        // Use temporary empty object to avoid static initialization conflicts with Teensy
        return const_iterator::from_iterator(JsonObject().end());
    }
    
    // Iterator support for packed arrays
    template<typename T>
    class array_iterator {
    private:
        using variant_t = typename JsonValue::variant_t;
        variant_t* m_variant;
        size_t m_index;
        
        // Helper to get the size of the array regardless of its type
        size_t get_size() const {
            if (!m_variant) return 0;
            
            if (m_variant->is<JsonArray>()) {
                auto ptr = m_variant->ptr<JsonArray>();
                return ptr ? ptr->size() : 0;
            }
            
            if (m_variant->is<fl::vector<int16_t>>()) {
                auto ptr = m_variant->ptr<fl::vector<int16_t>>();
                return ptr ? ptr->size() : 0;
            }
            
            if (m_variant->is<fl::vector<uint8_t>>()) {
                auto ptr = m_variant->ptr<fl::vector<uint8_t>>();
                return ptr ? ptr->size() : 0;
            }
            
            if (m_variant->is<fl::vector<float>>()) {
                auto ptr = m_variant->ptr<fl::vector<float>>();
                return ptr ? ptr->size() : 0;
            }
            
            return 0;
        }
        
        // Helper to convert current element to target type T
        ParseResult<T> get_value() const {
            if (!m_variant || m_index >= get_size()) {
                return ParseResult<T>(Error("Index out of bounds"));
            }
            
            if (m_variant->is<JsonArray>()) {
                auto ptr = m_variant->ptr<JsonArray>();
                if (ptr && m_index < ptr->size() && (*ptr)[m_index]) {
                    auto& val = *((*ptr)[m_index]);
                    
                    // Try to convert to T using the JsonValue conversion methods
                    // Using FastLED type traits instead of std:: ones
                    if (fl::is_same<T, bool>::value) {
                        auto opt = val.as_bool();
                        if (opt) {
                            return ParseResult<T>(*opt);
                        } else {
                            return ParseResult<T>(Error("Cannot convert to bool"));
                        }
                    } else if (fl::is_integral<T>::value && fl::is_signed<T>::value) {
                        auto opt = val.template as_int<T>();
                        if (opt) {
                            return ParseResult<T>(*opt);
                        } else {
                            return ParseResult<T>(Error("Cannot convert to signed integer"));
                        }
                    } else if (fl::is_integral<T>::value && !fl::is_signed<T>::value) {
                        // For unsigned types, we check that it's integral but not signed
                        auto opt = val.template as_int<T>();
                        if (opt) {
                            return ParseResult<T>(*opt);
                        } else {
                            return ParseResult<T>(Error("Cannot convert to unsigned integer"));
                        }
                    } else if (fl::is_floating_point<T>::value) {
                        auto opt = val.template as_float<T>();
                        if (opt) {
                            return ParseResult<T>(*opt);
                        } else {
                            return ParseResult<T>(Error("Cannot convert to floating point"));
                        }
                    }
                } else {
                    return ParseResult<T>(Error("Invalid array access"));
                }
            }
            
            if (m_variant->is<fl::vector<int16_t>>()) {
                auto ptr = m_variant->ptr<fl::vector<int16_t>>();
                if (ptr && m_index < ptr->size()) {
                    return ParseResult<T>(static_cast<T>((*ptr)[m_index]));
                } else {
                    return ParseResult<T>(Error("Index out of bounds in int16_t array"));
                }
            }
            
            if (m_variant->is<fl::vector<uint8_t>>()) {
                auto ptr = m_variant->ptr<fl::vector<uint8_t>>();
                if (ptr && m_index < ptr->size()) {
                    return ParseResult<T>(static_cast<T>((*ptr)[m_index]));
                } else {
                    return ParseResult<T>(Error("Index out of bounds in uint8_t array"));
                }
            }
            
            if (m_variant->is<fl::vector<float>>()) {
                auto ptr = m_variant->ptr<fl::vector<float>>();
                if (ptr && m_index < ptr->size()) {
                    return ParseResult<T>(static_cast<T>((*ptr)[m_index]));
                } else {
                    return ParseResult<T>(Error("Index out of bounds in float array"));
                }
            }
            
            return ParseResult<T>(Error("Unknown array type"));
        }
        
    public:
        array_iterator() : m_variant(nullptr), m_index(0) {}
        array_iterator(variant_t* variant, size_t index) : m_variant(variant), m_index(index) {}
        
        ParseResult<T> operator*() const {
            return get_value();
        }
        
        array_iterator& operator++() {
            ++m_index;
            return *this;
        }
        
        array_iterator operator++(int) {
            array_iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        
        bool operator!=(const array_iterator& other) const {
            return m_index != other.m_index || m_variant != other.m_variant;
        }
        
        bool operator==(const array_iterator& other) const {
            return m_index == other.m_index && m_variant == other.m_variant;
        }
    };
    
    // Begin/end methods for array iteration
    template<typename T>
    array_iterator<T> begin_array() {
        if (is_array()) {
            return array_iterator<T>(&data, 0);
        }
        return array_iterator<T>();
    }
    
    template<typename T>
    array_iterator<T> end_array() {
        if (is_array()) {
            return array_iterator<T>(&data, size());
        }
        return array_iterator<T>();
    }
    
    template<typename T>
    array_iterator<T> begin_array() const {
        if (is_array()) {
            return array_iterator<T>(const_cast<variant_t*>(&data), 0);
        }
        return array_iterator<T>();
    }
    
    template<typename T>
    array_iterator<T> end_array() const {
        if (is_array()) {
            return array_iterator<T>(const_cast<variant_t*>(&data), size());
        }
        return array_iterator<T>();
    }
    
    // Free functions for range-based for loops
    friend iterator begin(JsonValue& v) { return v.begin(); }
    friend iterator end(JsonValue& v) { return v.end(); }
    friend const_iterator begin(const JsonValue& v) { return v.begin(); }
    friend const_iterator end(const JsonValue& v) { return v.end(); }

    // Indexing for fluid chaining
    JsonValue& operator[](size_t idx) {
        if (!is_array()) data = JsonArray{};
        // Handle regular JsonArray
        if (data.is<JsonArray>()) {
            auto ptr = data.ptr<JsonArray>();
            if (!ptr) return get_null_value(); // Handle error case
            auto &arr = *ptr;
            if (idx >= arr.size()) {
                // Resize array and fill with null values
                for (size_t i = arr.size(); i <= idx; i++) {
                    arr.push_back(fl::make_shared<JsonValue>());
                }
            }
            if (idx >= arr.size()) return get_null_value(); // Handle error case
            return *arr[idx];
        }
        // For packed arrays, we need to convert them to regular arrays first
        // This is needed for compatibility with existing code that expects JsonArray
        if (data.is<fl::vector<int16_t>>() || 
            data.is<fl::vector<uint8_t>>() || 
            data.is<fl::vector<float>>()) {
            // Convert to regular JsonArray
            auto arr = as_array();
            if (arr) {
                data = fl::move(*arr);
                auto ptr = data.ptr<JsonArray>();
                if (!ptr) return get_null_value();
                auto &jsonArr = *ptr;
                if (idx >= jsonArr.size()) {
                    // Resize array and fill with null values
                    for (size_t i = jsonArr.size(); i <= idx; i++) {
                        jsonArr.push_back(fl::make_shared<JsonValue>());
                    }
                }
                if (idx >= jsonArr.size()) return get_null_value();
                return *jsonArr[idx];
            }
        }
        return get_null_value();
    }
    
    JsonValue& operator[](const fl::string &key) {
        if (!is_object()) data = JsonObject{};
        auto ptr = data.ptr<JsonObject>();
        if (!ptr) return get_null_value(); // Handle error case
        auto &obj = *ptr;
        if (obj.find(key) == obj.end()) {
            // Create a new entry if key doesn't exist
            obj[key] = fl::make_shared<JsonValue>();
        }
        return *obj[key];
    }

    // Default-value operator (pipe)
    template<typename T>
    T operator|(const T& fallback) const {
        DefaultValueVisitor<T> visitor(fallback);
        data.visit(visitor);
        return visitor.result ? *visitor.result : fallback;
    }
    
    // Explicit method for default values (alternative to operator|)
    template<typename T>
    T as_or(const T& fallback) const {
        DefaultValueVisitor<T> visitor(fallback);
        data.visit(visitor);
        return visitor.result ? *visitor.result : fallback;
    }

    // Contains methods for checking existence
    bool contains(size_t idx) const {
        // Handle regular JsonArray first
        if (data.is<JsonArray>()) {
            auto ptr = data.ptr<JsonArray>();
            return ptr && idx < ptr->size();
        }
        
        // Handle specialized array types
        if (data.is<fl::vector<int16_t>>()) {
            auto ptr = data.ptr<fl::vector<int16_t>>();
            return ptr && idx < ptr->size();
        }
        if (data.is<fl::vector<uint8_t>>()) {
            auto ptr = data.ptr<fl::vector<uint8_t>>();
            return ptr && idx < ptr->size();
        }
        if (data.is<fl::vector<float>>()) {
            auto ptr = data.ptr<fl::vector<float>>();
            return ptr && idx < ptr->size();
        }
        return false;
    }
    
    bool contains(const fl::string &key) const {
        if (!is_object()) return false;
        auto ptr = data.ptr<JsonObject>();
        return ptr && ptr->find(key) != ptr->end();
    }

    // Object iteration support (needed for screenmap conversion)
    fl::vector<fl::string> keys() const {
        fl::vector<fl::string> result;
        if (is_object()) {
            for (auto it = begin(); it != end(); ++it) {
                auto keyValue = *it;
                result.push_back(keyValue.first);
            }
        }
        return result;
    }
    
    // Backward compatibility method
    fl::vector<fl::string> getObjectKeys() const { return keys(); }

    // Size methods
    size_t size() const {
        // Handle regular JsonArray first
        if (data.is<JsonArray>()) {
            auto ptr = data.ptr<JsonArray>();
            return ptr ? ptr->size() : 0;
        }
        
        // Handle specialized array types
        if (data.is<fl::vector<int16_t>>()) {
            auto ptr = data.ptr<fl::vector<int16_t>>();
            return ptr ? ptr->size() : 0;
        }
        if (data.is<fl::vector<uint8_t>>()) {
            auto ptr = data.ptr<fl::vector<uint8_t>>();
            return ptr ? ptr->size() : 0;
        }
        if (data.is<fl::vector<float>>()) {
            auto ptr = data.ptr<fl::vector<float>>();
            return ptr ? ptr->size() : 0;
        }
        
        if (is_object()) {
            auto ptr = data.ptr<JsonObject>();
            return ptr ? ptr->size() : 0;
        }
        return 0;
    }

    // Serialization
    fl::string to_string() const;
    
    // Visitor-based serialization helper
    friend class SerializerVisitor;

    // Parsing factory (FLArduinoJson implementation)
    static fl::shared_ptr<JsonValue> parse(const fl::string &txt);
    
    // Iterator support for objects
    class iterator {
    private:
        JsonObject::iterator m_iter;
        
    public:
        iterator() = default;
        iterator(JsonObject::iterator iter) : m_iter(iter) {}
        
        // Getter for const iterator conversion
        JsonObject::iterator get_iter() const { return m_iter; }
        
        iterator& operator++() {
            ++m_iter;
            return *this;
        }
        
        iterator operator++(int) {
            iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        
        bool operator!=(const iterator& other) const {
            return m_iter != other.m_iter;
        }
        
        bool operator==(const iterator& other) const {
            return m_iter == other.m_iter;
        }
        
        struct KeyValue {
            fl::string first;
            JsonValue& second;
            
            KeyValue(const fl::string& key, const fl::shared_ptr<JsonValue>& value_ptr) 
                : first(key), second(value_ptr ? *value_ptr : get_null_value()) {}
        };
        
        KeyValue operator*() const {
            return KeyValue(m_iter->first, m_iter->second);
        }
        
        // Remove operator-> to avoid static variable issues
    };
    
    // Iterator for JSON objects (const version)
    class const_iterator {
    private:
        JsonObject::const_iterator m_iter;
        
    public:
        const_iterator() = default;
        const_iterator(JsonObject::const_iterator iter) : m_iter(iter) {}
        
        // Factory method for conversion from iterator
        static const_iterator from_object_iterator(const iterator& other) {
            JsonObject::const_iterator const_iter(other.get_iter());
            return const_iterator(const_iter);
        }
        
        // Factory method for conversion from Object::iterator
        static const_iterator from_iterator(JsonObject::const_iterator iter) {
            return const_iterator(iter);
        }
        
        const_iterator& operator++() {
            ++m_iter;
            return *this;
        }
        
        const_iterator operator++(int) {
            const_iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        
        bool operator!=(const const_iterator& other) const {
            return m_iter != other.m_iter;
        }
        
        bool operator==(const const_iterator& other) const {
            return m_iter == other.m_iter;
        }
        
        struct KeyValue {
            fl::string first;
            const JsonValue& second;
            
            KeyValue(const fl::string& key, const fl::shared_ptr<JsonValue>& value_ptr) 
                : first(key), second(value_ptr ? *value_ptr : get_null_value()) {}
        };
        
        KeyValue operator*() const {
            return KeyValue(m_iter->first, m_iter->second);
        }
        
        // Remove operator-> to avoid static variable issues
    };
};

// Function to get a reference to a static null JsonValue
JsonValue& get_null_value();

// Main Json class that provides a more fluid and user-friendly interface
class Json {
private:
    fl::shared_ptr<JsonValue> m_value;

public:
    // Constructors
    Json() : m_value() {}  // Default initialize to nullptr
    Json(fl::nullptr_t) : m_value(fl::make_shared<JsonValue>(nullptr)) {}
    Json(bool b) : m_value(fl::make_shared<JsonValue>(b)) {}
    Json(int i) : m_value(fl::make_shared<JsonValue>(static_cast<int64_t>(i))) {}
    Json(int64_t i) : m_value(fl::make_shared<JsonValue>(i)) {}
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
        m_value = fl::make_shared<JsonValue>(static_cast<int64_t>(value));
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
    bool is_string() const { return m_value && m_value->is_string(); }
    bool is_array() const { return m_value && m_value->is_array(); }
    bool is_generic_array() const { return m_value && m_value->is_generic_array(); }
    bool is_object() const { return m_value && m_value->is_object(); }
    bool is_audio() const { return m_value && m_value->is_audio(); }
    bool is_bytes() const { return m_value && m_value->is_bytes(); }
    bool is_floats() const { return m_value && m_value->is_floats(); }

    // Safe extractors
    fl::optional<bool> as_bool() const { return m_value ? m_value->as_bool() : fl::nullopt; }
    fl::optional<int64_t> as_int() const { 
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
    fl::optional<fl::vector<int16_t>> as_audio() const { return m_value ? m_value->as_audio() : fl::nullopt; }
    fl::optional<fl::vector<uint8_t>> as_bytes() const { return m_value ? m_value->as_bytes() : fl::nullopt; }
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
    typename fl::enable_if<fl::is_same<T, fl::vector<int16_t>>::value, fl::optional<T>>::type
    as_impl() const {
        return m_value->as_audio();
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<uint8_t>>::value, fl::optional<T>>::type
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
    typename fl::enable_if<fl::is_same<T, fl::vector<int16_t>>::value, T>::type
    get_default_value() const {
        return fl::vector<int16_t>();  // Audio vector defaults to empty
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::vector<uint8_t>>::value, T>::type
    get_default_value() const {
        return fl::vector<uint8_t>();  // Bytes vector defaults to empty
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
        if (m_value->data.is<fl::vector<int16_t>>() || 
            m_value->data.is<fl::vector<uint8_t>>() || 
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
        if (m_value->data.is<fl::vector<int16_t>>() || 
            m_value->data.is<fl::vector<uint8_t>>() || 
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

    // Parsing factory method
    static Json parse(const fl::string &txt) {
        auto parsed = JsonValue::parse(txt);
        if (parsed) {
            Json result;
            result.m_value = parsed;
            return result;
        }
        return Json(nullptr);
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
            (*objPtr)[key] = value.m_value;
        }
    }
    
    void set(const fl::string& key, bool value) { set(key, Json(value)); }
    void set(const fl::string& key, int value) { set(key, Json(value)); }
    void set(const fl::string& key, int64_t value) { set(key, Json(value)); }
    void set(const fl::string& key, float value) { set(key, Json(value)); }
    void set(const fl::string& key, double value) { set(key, Json(value)); }
    void set(const fl::string& key, const fl::string& value) { set(key, Json(value)); }
    void set(const fl::string& key, const char* value) { set(key, Json(value)); }
    template<typename T, typename = fl::enable_if_t<fl::is_same<T, char>::value>>
    void set(const fl::string& key, T value) { set(key, Json(value)); }
    
    // Array push_back methods
    void push_back(const Json& value) {
        if (!m_value || !m_value->is_array()) {
            m_value = fl::make_shared<JsonValue>(JsonArray{});
        }
        // If we're pushing to a packed array, convert it to regular JsonArray first
        if (m_value->is_array() && 
            (m_value->data.is<fl::vector<int16_t>>() || 
             m_value->data.is<fl::vector<uint8_t>>() || 
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

} // namespace fl
