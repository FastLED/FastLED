# JSON API Design Document

## Goals

1. **Clean Public Interface**: Hide all implementation details behind a clean, easy-to-use API
2. **Proper Encapsulation**: Use the PIMPL idiom to completely hide FLArduinoJson classes
3. **Compatibility**: Support both the "ideal API" and traditional JSON access patterns
4. **Performance**: Minimize overhead from abstraction layer
5. **Type Safety**: Provide compile-time type checking where possible

## Public API Design

### Core Classes

1. **fl::Json** - Main JSON class that can represent any JSON value
2. **fl::JsonObject** - Represents a JSON object (key-value pairs)
3. **fl::JsonArray** - Represents a JSON array (ordered list of values)
4. **fl::JsonValue** - Represents a single JSON value (string, number, boolean, null)

### fl::Json Class Interface

```cpp
class Json {
public:
    // Parsing
    static Json parse(const char* jsonString);
    
    // Constructors
    Json();  // Null value
    Json(bool value);
    Json(int value);
    Json(long value);
    Json(double value);
    Json(const char* value);
    Json(const fl::string& value);
    Json(const JsonObject& value);
    Json(const JsonArray& value);
    Json(JsonObject&& value);
    Json(JsonArray&& value);
    
    // Copy/Move
    Json(const Json& other);
    Json(Json&& other) noexcept;
    Json& operator=(const Json& other);
    Json& operator=(Json&& other) noexcept;
    
    // Type checking
    bool is_null() const;
    bool is_bool() const;
    bool is_int() const;
    bool is_long() const;
    bool is_double() const;
    bool is_string() const;
    bool is_object() const;
    bool is_array() const;
    
    // Value extraction with type conversion
    template<typename T>
    T get() const;
    
    template<typename T>
    T or(T defaultValue) const;
    
    // Operator for default values (e.g., json["key"] | defaultValue)
    template<typename T>
    T operator|(T defaultValue) const;
    
    // Object access
    Json& operator[](const fl::string& key);
    const Json& operator[](const fl::string& key) const;
    
    // Array access
    Json& operator[](size_t index);
    const Json& operator[](size_t index) const;
    
    // Array methods
    void push_back(const Json& value);
    void push_back(Json&& value);
    size_t size() const;
    
    // Serialization
    fl::string serialize() const;
    
    // Utility methods
    fl::vector<fl::string> keys() const;
    
    // Factory methods
    static Json createArray();
    static Json createObject();
    
private:
    // PIMPL - completely hides implementation details
    class Impl;
    fl::unique_ptr<Impl> impl_;
};
```

### Implementation Details (Hidden)

All implementation details, including FLArduinoJson classes, should be completely hidden in the implementation file:

1. **Json::Impl class** - Contains the actual implementation using FLArduinoJson
2. **FLArduinoJson includes** - Only in the implementation file
3. **Conversion functions** - Between public API types and FLArduinoJson types
4. **Helper functions** - All internal helper functions

## Benefits of This Design

1. **Clean Interface**: Users only see what they need to use
2. **Implementation Flexibility**: Can change underlying JSON library without affecting users
3. **Reduced Compilation Dependencies**: Changes to implementation don't require recompilation of user code
4. **Binary Compatibility**: Easier to maintain binary compatibility across versions
5. **Encapsulation**: Proper separation of interface and implementation

## Implementation Steps

1. Create clean separation between public API (header) and implementation (cpp)
2. Move all FLArduinoJson-related code to implementation file
3. Implement PIMPL pattern properly with complete encapsulation
4. Implement all methods referenced in example code
5. Ensure backward compatibility with existing usage patterns
6. Test thoroughly with all JSON examples

This design will provide a robust, clean, and well-encapsulated JSON API that meets all our goals.
