#pragma once

// JSON implementation types and helpers
// Internal header - do not include directly, use fl/json.h instead

#include "fl/int.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/variant.h"
#include "fl/stl/optional.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/cctype.h"
#include "fl/stl/charconv.h"
#include "fl/stl/limits.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/move.h"
#include "fl/stl/strstream.h"
#include "fl/stl/type_traits.h"
#include "fl/log.h"
#include "fl/promise.h" // For Error type
#include "fl/string_view.h"
#include "fl/stl/span.h"
#include "fl/stl/string_interner.h"
#include "fl/warn.h"

#include "fl/sketch_macros.h"

#ifndef FASTLED_ENABLE_JSON
// FASTLED_ENABLE_JSON enables JSON UI components (always enabled on platforms with memory)
// This flag controls UI component compilation (sliders, checkboxes, etc.)
#define FASTLED_ENABLE_JSON SKETCH_HAS_LOTS_OF_MEMORY
#endif

#ifndef FASTLED_ARDUINO_JSON_PARSING_ENABLED
// FASTLED_ARDUINO_JSON_PARSING_ENABLED enables ArduinoJson parser (DISABLED by default)
// Set to 1 to enable ArduinoJson for benchmarking purposes only
// fl::Json::parse() ALWAYS uses native parser regardless of this flag
// ArduinoJson is ONLY used when explicitly calling fl::Json::parse_arduino_json()
#define FASTLED_ARDUINO_JSON_PARSING_ENABLED 0
#endif

namespace fl {

struct JsonValue;

// Define Array and Object as pointers to avoid incomplete type issues
// We'll use heap-allocated containers for these to avoid alignment issues
using JsonArray = fl::vector<fl::shared_ptr<JsonValue>>;
using JsonObject = fl::unordered_map<fl::string, fl::shared_ptr<JsonValue>>;

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

    // This is the method that fl::variant expects
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
template<typename IntType = i64>
struct IntConversionVisitor {
    fl::optional<IntType> result;
    
    template<typename U>
    void accept(const U& value) {
        // Dispatch to the correct operator() overload
        (*this)(value);
    }
    
    // Special handling to avoid conflict when IntType is i64
    template<typename T = IntType>
    typename fl::enable_if<!fl::is_same<T, i64>::value && !fl::is_same<T, double>::value, void>::type
    operator()(const IntType& value) {
        result = value;
    }
    
    // Special handling for i64 case
    template<typename T = IntType>
    typename fl::enable_if<fl::is_same<T, i64>::value, void>::type
    operator()(const i64& value) {
        result = value;
    }
    
    // Special handling for double case (when IntType is double)
    template<typename T = IntType>
    typename fl::enable_if<fl::is_same<T, double>::value, void>::type
    operator()(const double& value) {
        result = value;
    }
    
    // Special handling to avoid conflict when IntType is i64
    // With overflow detection and logging
    template<typename T = IntType>
    typename fl::enable_if<!fl::is_same<T, i64>::value, void>::type
    operator()(const i64& value) {
        // Check for overflow before casting
        // For signed types: check if value is within [min, max] range
        // For unsigned types: check if value is non-negative and within [0, max] range
        // Use parentheses around min/max to protect against Arduino min/max macros
        const i64 min_val = static_cast<i64>((fl::numeric_limits<IntType>::min)());
        const i64 max_val = static_cast<i64>((fl::numeric_limits<IntType>::max)());

        if (value < min_val || value > max_val) {
            // Log overflow error but still perform conversion (value will be truncated)
            FL_ERROR("JSON integer overflow: value " << value << " does not fit in target type (range: "
                     << min_val << " to " << max_val << "), truncating");
        }

        // Always perform conversion, even if overflow detected
        result = static_cast<IntType>(value);
    }
    
    void operator()(const bool& value) {
        result = static_cast<IntType>(value ? 1 : 0);
    }

    // Special handling for float to int conversion
    void operator()(const float& value) {
        // NEW INSTRUCTIONS: AUTO CONVERT FLOAT TO INT
        result = static_cast<IntType>(value);
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
            if (!fl::isdigit(str[i])) {
                isValidInt = false;
                break;
            }
        }
        
        // If it looks like a valid integer, try to parse it
        if (isValidInt && str.length() > 0) {
            int parsed = fl::parseInt(str.c_str(), str.length());
            result = static_cast<IntType>(parsed);
        }
    }
    
    template<typename T>
    void operator()(const T&) {
        // Do nothing for other types
    }
};

// Specialization for i64 to avoid template conflicts
template<>
struct IntConversionVisitor<i64> {
    fl::optional<i64> result;
    
    template<typename U>
    void accept(const U& value) {
        // Dispatch to the correct operator() overload
        (*this)(value);
    }
    
    void operator()(const i64& value) {
        result = value;
    }
    
    void operator()(const bool& value) {
        result = value ? 1 : 0;
    }

    void operator()(const float& value) {
        // NEW INSTRUCTIONS: AUTO CONVERT FLOAT TO INT
        result = static_cast<i64>(value);
    }

    void operator()(const double& value) {
        // NEW INSTRUCTIONS: AUTO CONVERT FLOAT TO INT
        result = static_cast<i64>(value);
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
            if (!fl::isdigit(str[i])) {
                isValidInt = false;
                break;
            }
        }
        
        // If it looks like a valid integer, try to parse it
        if (isValidInt && str.length() > 0) {
            int parsed = fl::parseInt(str.c_str(), str.length());
            result = static_cast<i64>(parsed);
        }
    }
    
    template<typename T>
    void operator()(const T&) {
        // Do nothing for other types
    }
};

// Visitor for converting values to bool
struct BoolConversionVisitor {
    fl::optional<bool> result;

    template<typename U>
    void accept(const U& value) {
        // Dispatch to the correct operator() overload
        (*this)(value);
    }

    void operator()(const bool& value) {
        result = value;
    }

    void operator()(const i64& value) {
        // NEW INSTRUCTIONS: AUTO CONVERT INT TO BOOL
        // 0 → false, non-zero → true
        result = (value != 0);
    }

    void operator()(const float& value) {
        // NEW INSTRUCTIONS: AUTO CONVERT FLOAT TO BOOL
        // 0.0 → false, non-zero → true
        result = (value != 0.0f);
    }

    void operator()(const double& value) {
        // NEW INSTRUCTIONS: AUTO CONVERT DOUBLE TO BOOL
        // 0.0 → false, non-zero → true
        result = (value != 0.0);
    }

    void operator()(const fl::string& str) {
        // NEW INSTRUCTIONS: AUTO CONVERT STRING TO BOOL
        // "true", "1", "yes", "on" (case insensitive) → true
        // "false", "0", "no", "off" (case insensitive) → false
        // Empty string → false
        // Invalid string → nullopt

        if (str.empty()) {
            result = false;
            return;
        }

        // Convert to lowercase for case-insensitive comparison
        fl::string lower = str;
        for (fl::size i = 0; i < lower.length(); i++) {
            lower[i] = fl::tolower(lower[i]);
        }

        // Check for true values
        if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
            result = true;
            return;
        }

        // Check for false values
        if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
            result = false;
            return;
        }

        // Invalid string - don't set result (leave as nullopt)
    }

    template<typename T>
    void operator()(const T&) {
        // Do nothing for other types (arrays, objects, null)
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
    
    void operator()(const i64& value) {
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
            } else if (!fl::isdigit(c) && c != 'e' && c != 'E') {
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
                if (c != '.' && !fl::isdigit(c)) {
                    isSimpleDecimal = false;
                    break;
                }
            }
            
            if (isSimpleDecimal) {
                // For simple decimals, we can do a more direct conversion
                float parsed = fl::parseFloat(str.c_str(), str.length());
                result = static_cast<FloatType>(parsed);
            } else {
                // For complex floats (with exponents), use the standard approach
                float parsed = fl::parseFloat(str.c_str(), str.length());
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
    
    void operator()(const i64& value) {
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
            } else if (!fl::isdigit(c) && c != 'e' && c != 'E') {
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
                if (c != '.' && !fl::isdigit(c)) {
                    isSimpleDecimal = false;
                    break;
                }
            }
            
            if (isSimpleDecimal) {
                // For simple decimals, we can do a more direct conversion
                float parsed = fl::parseFloat(str.c_str(), str.length());
                result = static_cast<double>(parsed);
            } else {
                // For complex floats (with exponents), use the standard approach
                float parsed = fl::parseFloat(str.c_str(), str.length());
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

    void operator()(const i64& value) {
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

// Visitor for getting size of arrays and objects
struct SizeVisitor {
    size_t result = 0;

    template<typename U>
    void accept(const U& value) {
        // Dispatch to the correct operator() overload
        (*this)(value);
    }

    void operator()(const JsonArray& arr) { result = arr.size(); }
    void operator()(const JsonObject& obj) { result = obj.size(); }
    void operator()(const fl::vector<i16>& vec) { result = vec.size(); }
    void operator()(const fl::vector<u8>& vec) { result = vec.size(); }
    void operator()(const fl::vector<float>& vec) { result = vec.size(); }

    // Generic fallback for other types (primitives, null)
    void operator()(const fl::nullptr_t&) { result = 0; }
    void operator()(const bool&) { result = 0; }
    void operator()(const i64&) { result = 0; }
    void operator()(const float&) { result = 0; }
    void operator()(const fl::string&) { result = 0; }
};

// The JSON node
struct JsonValue {
    // Forward declarations for nested iterator classes
    class iterator;
    class const_iterator;
    
    // Friend declarations
    friend class Json;
    
    
    // The variant holds exactly one of these alternatives
    using variant_t = fl::variant<
        fl::nullptr_t,   // null
        bool,            // true/false
        i64,         // integer
        float,           // floating-point (changed from double to float)
        fl::string,      // string
        JsonArray,           // array
        JsonObject,          // object
        fl::vector<i16>, // audio data (specialized array of int16_t)
        fl::vector<u8>, // byte data (specialized array of uint8_t)
        fl::vector<float>    // float data (specialized array of float)
    >;

    typedef JsonValue::iterator iterator;
    typedef JsonValue::const_iterator const_iterator;

    variant_t data;

    // Constructors
    JsonValue() noexcept : data(nullptr) {}
    JsonValue(fl::nullptr_t) noexcept : data(nullptr) {}
    JsonValue(bool b) noexcept : data(b) {}
    JsonValue(i64 i) noexcept : data(i) {}
    JsonValue(float f) noexcept : data(f) {}  // Changed from double to float
    JsonValue(const fl::string& s) : data(s) {
    }
    JsonValue(const JsonArray& a) : data(a) {
        //FASTLED_WARN("Created JsonValue with array");
    }
    JsonValue(const JsonObject& o) : data(o) {
        //FASTLED_WARN("Created JsonValue with object");
    }
    JsonValue(const fl::vector<i16>& audio) : data(audio) {
        //FASTLED_WARN("Created JsonValue with audio data");
    }
    
    JsonValue(fl::vector<i16>&& audio) : data(fl::move(audio)) {
        //FASTLED_WARN("Created JsonValue with moved audio data");
    }
    
    JsonValue(const fl::vector<u8>& bytes) : data(bytes) {
        //FASTLED_WARN("Created JsonValue with byte data");
    }
    
    JsonValue(fl::vector<u8>&& bytes) : data(fl::move(bytes)) {
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

    JsonValue& operator=(i64 i) {
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
    
    JsonValue& operator=(fl::vector<i16> audio) {
        data = fl::move(audio);
        return *this;
    }
    
    JsonValue& operator=(fl::vector<u8> bytes) {
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

    // Type queries - using is<T>() instead of index() for fl::variant
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
        return data.is<i64>();
    }
    bool is_double() const noexcept { 
        //FASTLED_WARN("is_double called, tag=" << data.tag());
        return data.is<float>(); 
    }
    bool is_float() const noexcept {
        return data.is<float>();
    }
    // is_number() returns true if the value is any numeric type (int or float)
    bool is_number() const noexcept {
        return is_int() || is_float();
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
        void operator()(const fl::vector<i16>&) {
            result = true;  // Audio data is still an array
        }
        
        void operator()(const fl::vector<u8>&) {
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
        return data.is<fl::vector<i16>>();
    }
    bool is_bytes() const noexcept {
        //FASTLED_WARN("is_bytes called, tag=" << data.tag());
        return data.is<fl::vector<u8>>();
    }
    bool is_floats() const noexcept {
        //FASTLED_WARN("is_floats called, tag=" << data.tag());
        return data.is<fl::vector<float>>();
    }

    // Safe extractors (return optional values, not references)
    fl::optional<bool> as_bool() {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }

        BoolConversionVisitor visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    fl::optional<i64> as_int() {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }
        
        IntConversionVisitor<i64> visitor;
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
        if (data.is<fl::vector<i16>>()) {
            auto audioPtr = data.ptr<fl::vector<i16>>();
            JsonArray result;
            for (const auto& item : *audioPtr) {
                result.push_back(fl::make_shared<JsonValue>(static_cast<i64>(item)));
            }
            return fl::optional<JsonArray>(result);
        }
        
        if (data.is<fl::vector<u8>>()) {
            auto bytePtr = data.ptr<fl::vector<u8>>();
            JsonArray result;
            for (const auto& item : *bytePtr) {
                result.push_back(fl::make_shared<JsonValue>(static_cast<i64>(item)));
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
    
    fl::optional<fl::vector<i16>> as_audio() {
        auto ptr = data.ptr<fl::vector<i16>>();
        return ptr ? fl::optional<fl::vector<i16>>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::vector<u8>> as_bytes() {
        auto ptr = data.ptr<fl::vector<u8>>();
        return ptr ? fl::optional<fl::vector<u8>>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::vector<float>> as_floats() {
        auto ptr = data.ptr<fl::vector<float>>();
        return ptr ? fl::optional<fl::vector<float>>(*ptr) : fl::nullopt;
    }

    // Const overloads
    fl::optional<bool> as_bool() const {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }

        BoolConversionVisitor visitor;
        data.visit(visitor);
        return visitor.result;
    }
    
    fl::optional<i64> as_int() const {
        // Check if we have a valid value first
        if (data.empty()) {
            return fl::nullopt;
        }
        
        IntConversionVisitor<i64> visitor;
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
        if (data.is<fl::vector<i16>>()) {
            auto audioPtr = data.ptr<fl::vector<i16>>();
            JsonArray result;
            for (const auto& item : *audioPtr) {
                result.push_back(fl::make_shared<JsonValue>(static_cast<i64>(item)));
            }
            return fl::optional<JsonArray>(result);
        }
        
        if (data.is<fl::vector<u8>>()) {
            auto bytePtr = data.ptr<fl::vector<u8>>();
            JsonArray result;
            for (const auto& item : *bytePtr) {
                result.push_back(fl::make_shared<JsonValue>(static_cast<i64>(item)));
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
    
    fl::optional<fl::vector<i16>> as_audio() const {
        auto ptr = data.ptr<fl::vector<i16>>();
        return ptr ? fl::optional<fl::vector<i16>>(*ptr) : fl::nullopt;
    }
    
    fl::optional<fl::vector<u8>> as_bytes() const {
        auto ptr = data.ptr<fl::vector<u8>>();
        return ptr ? fl::optional<fl::vector<u8>>(*ptr) : fl::nullopt;
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
            
            if (m_variant->is<fl::vector<i16>>()) {
                auto ptr = m_variant->ptr<fl::vector<i16>>();
                return ptr ? ptr->size() : 0;
            }
            
            if (m_variant->is<fl::vector<u8>>()) {
                auto ptr = m_variant->ptr<fl::vector<u8>>();
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
            
            if (m_variant->is<fl::vector<i16>>()) {
                auto ptr = m_variant->ptr<fl::vector<i16>>();
                if (ptr && m_index < ptr->size()) {
                    return ParseResult<T>(static_cast<T>((*ptr)[m_index]));
                } else {
                    return ParseResult<T>(Error("Index out of bounds in i16 array"));
                }
            }
            
            if (m_variant->is<fl::vector<u8>>()) {
                auto ptr = m_variant->ptr<fl::vector<u8>>();
                if (ptr && m_index < ptr->size()) {
                    return ParseResult<T>(static_cast<T>((*ptr)[m_index]));
                } else {
                    return ParseResult<T>(Error("Index out of bounds in u8 array"));
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
        if (data.is<fl::vector<i16>>() || 
            data.is<fl::vector<u8>>() || 
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
        if (data.is<fl::vector<i16>>()) {
            auto ptr = data.ptr<fl::vector<i16>>();
            return ptr && idx < ptr->size();
        }
        if (data.is<fl::vector<u8>>()) {
            auto ptr = data.ptr<fl::vector<u8>>();
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
        SizeVisitor visitor;
        data.visit(visitor);
        return visitor.result;
    }

    // Serialization
    fl::string to_string() const;
    
    // Visitor-based serialization helper
    friend struct SerializerVisitor;

    // Parsing factory (FLArduinoJson implementation)
    static fl::shared_ptr<JsonValue> parse(const fl::string &txt);

    // Custom two-phase JSON parser (PRODUCTION READY)
    // Features:
    //   - Two-phase: validation (zero alloc) + building
    //   - Array lookahead: Direct parsing to typed vectors (uint8, int16, float)
    //   - Zero-copy tokenization with fl::span<const char>
    //   - ~608 bytes stack overhead, recursion depth limit 32
    //   - Identical behavior to parse() (validated with A/B tests)
    static fl::shared_ptr<JsonValue> parse2(const fl::string &txt);
    static fl::shared_ptr<JsonValue> parse2(fl::string_view txt);  // Zero-copy version
    static bool parse2_validate_only(const fl::string &txt);  // Phase 1 validation only (for testing)
    static bool parse2_validate_only(fl::string_view txt);  // Zero-copy version (no allocation)

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

} // namespace fl
