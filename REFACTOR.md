# JSON API Refactor: Ergonomic Type Conversion

## Overview

This refactor modernizes the FastLED JSON API to be more ergonomic and intuitive by providing three distinct conversion methods with clear semantics:

- `json.try_as<T>()` → `fl::optional<T>` (explicit optional handling)
- `json.as<T>()` → `T` (direct conversion with sensible defaults)
- `json.as_or<T>(default)` → `T` (conversion with custom defaults)

## Current State (Verbose and Inconsistent)

The current API requires verbose template syntax and always returns optionals:

```cpp
// Current verbose API
fl::optional<int32_t> value = json.as_int<int32_t>();
if (value.has_value()) {
    int32_t actualValue = *value;
    // Use actualValue...
}

// Different methods for different types
fl::optional<double> doubleVal = json.as_float<double>();
fl::optional<fl::string> stringVal = json.as_string();
fl::optional<bool> boolVal = json.as_bool();
```

## New Ergonomic API Design

### 1. `try_as<T>()` - Explicit Optional Handling

**Returns:** `fl::optional<T>`
**Use Case:** When you need to explicitly handle conversion failure

```cpp
// Explicit optional handling for when conversion might fail
fl::optional<int32_t> maybeValue = json.try_as<int32_t>();
if (maybeValue.has_value()) {
    int32_t value = *maybeValue;
    // Handle success case
} else {
    // Handle conversion failure
}

// All types supported
fl::optional<double> maybeDouble = json.try_as<double>();
fl::optional<fl::string> maybeString = json.try_as<fl::string>();
fl::optional<bool> maybeBool = json.try_as<bool>();
```

### 2. `as<T>()` - Direct Conversion with Sensible Defaults

**Returns:** `T`
**Use Case:** When you want a value immediately with reasonable defaults on failure

```cpp
// Direct conversion with sensible defaults
int32_t value = json.as<int32_t>();        // Returns 0 on failure
double pi = json.as<double>();              // Returns 0.0 on failure
fl::string name = json.as<fl::string>();    // Returns "" on failure
bool enabled = json.as<bool>();             // Returns false on failure

// Much cleaner for common cases
int brightness = config["brightness"].as<int>();
fl::string deviceName = config["device"]["name"].as<fl::string>();
```

### 3. `as_or<T>(default)` - Conversion with Custom Defaults

**Returns:** `T`
**Use Case:** When you want to specify your own default value

```cpp
// Custom defaults for better control
int brightness = json.as_or<int>(128);                    // Default to 128
double timeout = json.as_or<double>(5.0);                 // Default to 5.0 seconds  
fl::string name = json.as_or<fl::string>("Unknown");      // Default to "Unknown"
bool debug = json.as_or<bool>(true);                      // Default to true

// Excellent for configuration parsing
int ledCount = config["led_count"].as_or<int>(100);
fl::string mode = config["mode"].as_or<fl::string>("rainbow");
```

## Type-Specific Default Values

| Type | Default Value | Rationale |
|------|---------------|-----------|
| Integer types (`int8_t`, `int16_t`, `int32_t`, `int64_t`, etc.) | `0` | Mathematical zero, safe for calculations |
| Unsigned types (`uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`) | `0` | Mathematical zero, safe for calculations |
| Floating point (`float`, `double`) | `0.0` | Mathematical zero, safe for calculations |
| `bool` | `false` | Conservative default, explicit true required |
| `fl::string` | `""` (empty string) | Safe empty state, won't cause crashes |
| `JsonArray` | `{}` (empty array) | Safe empty container |
| `JsonObject` | `{}` (empty object) | Safe empty container |
| Specialized vectors | `{}` (empty vector) | Safe empty container |

## Migration Strategy

### Phase 1: Add New Methods (Backward Compatible)

Add the new methods alongside existing ones:

```cpp
class Json {
public:
    // NEW: Ergonomic API
    template<typename T> fl::optional<T> try_as() const;
    template<typename T> T as() const;
    template<typename T> T as_or(const T& defaultValue) const;
    
    // EXISTING: Backward compatibility (deprecated but functional)
    template<typename T> fl::optional<T> as_int() const;
    template<typename T> fl::optional<T> as_float() const;
    fl::optional<fl::string> as_string() const;
    fl::optional<bool> as_bool() const;
    // ... other existing methods
};
```

### Phase 2: Update Examples and Documentation

Update all examples to use the new ergonomic API:

```cpp
// OLD examples/Json/Json.ino
fl::optional<int> brightness = json.as_int<int>();
if (brightness.has_value()) {
    FastLED.setBrightness(*brightness);
}

// NEW examples/Json/Json.ino  
int brightness = json.as_or<int>(128);  // Default to 128
FastLED.setBrightness(brightness);
```

### Phase 3: Gradual Migration of Tests

Update test files to demonstrate both APIs during transition:

```cpp
TEST_CASE("JSON API comparison") {
    fl::Json json(42);
    
    // Old verbose API (still works)
    fl::optional<int> oldWay = json.as_int<int>();
    REQUIRE(oldWay.has_value());
    CHECK_EQ(*oldWay, 42);
    
    // New ergonomic APIs
    CHECK_EQ(json.try_as<int>().value(), 42);  // Explicit optional
    CHECK_EQ(json.as<int>(), 42);              // Direct with default
    CHECK_EQ(json.as_or<int>(0), 42);         // Custom default
}
```

## Implementation Details

### SFINAE-Based Type Dispatch

Use template specialization with SFINAE to dispatch to appropriate internal methods:

```cpp
template<typename T>
fl::optional<T> try_as() const {
    if (!m_value) return fl::nullopt;
    return try_as_impl<T>();
}

template<typename T>
T as() const {
    auto result = try_as<T>();
    return result.has_value() ? *result : get_default_value<T>();
}

template<typename T>
T as_or(const T& defaultValue) const {
    auto result = try_as<T>();
    return result.has_value() ? *result : defaultValue;
}

private:
    // Type-specific implementations using SFINAE
    template<typename T>
    typename fl::enable_if<fl::is_integral<T>::value, fl::optional<T>>::type
    try_as_impl() const {
        return m_value->template as_int<T>();
    }
    
    // Default value providers
    template<typename T>
    typename fl::enable_if<fl::is_integral<T>::value, T>::type
    get_default_value() const {
        return T(0);
    }
    
    template<typename T>
    typename fl::enable_if<fl::is_same<T, fl::string>::value, T>::type
    get_default_value() const {
        return fl::string();
    }
    
    // ... more specializations
```

## Benefits of This Refactor

### 1. **Ergonomic API**
```cpp
// Before: Verbose and error-prone
fl::optional<int> brightness = config["brightness"].as_int<int>();
if (brightness.has_value()) {
    setBrightness(*brightness);
} else {
    setBrightness(128); // Default
}

// After: Clean and direct
setBrightness(config["brightness"].as_or<int>(128));
```

### 2. **Reduced Boilerplate**
- Eliminates repetitive optional checking for common cases
- Reduces lines of code by 50-70% for typical JSON processing
- Makes configuration parsing much cleaner

### 3. **Clear Intent**
- `try_as<T>()` clearly indicates "this might fail, handle it"
- `as<T>()` clearly indicates "give me a value with sensible defaults"
- `as_or<T>(default)` clearly indicates "give me a value or this specific default"

### 4. **Type Safety**
- All methods maintain compile-time type safety
- No runtime type confusion
- Clear conversion semantics

### 5. **Performance**
- No additional overhead compared to current implementation
- Potentially faster due to reduced optional handling in hot paths

## Example Use Cases

### Configuration Processing
```cpp
void loadConfig(const fl::Json& config) {
    // Clean, readable configuration loading
    int ledCount = config["led_count"].as_or<int>(100);
    double brightness = config["brightness"].as_or<double>(1.0);
    fl::string mode = config["mode"].as_or<fl::string>("rainbow");
    bool debug = config["debug"].as<bool>();  // false by default
    
    // Use values directly without optional gymnastics
    setupLEDs(ledCount, brightness, mode, debug);
}
```

### Network Response Processing
```cpp
void processResponse(const fl::Json& response) {
    // Handle potentially missing fields gracefully
    int statusCode = response["status"].as_or<int>(500);
    fl::string message = response["message"].as_or<fl::string>("Unknown error");
    
    if (statusCode == 200) {
        fl::string data = response["data"].as<fl::string>();
        processData(data);
    } else {
        handleError(statusCode, message);
    }
}
```

### Robust Parsing with Fallbacks
```cpp
void parseSettings(const fl::Json& json) {
    // When you need explicit failure handling
    auto maybeAdvancedConfig = json["advanced"].try_as<JsonObject>();
    if (maybeAdvancedConfig.has_value()) {
        // Parse advanced configuration
        parseAdvancedConfig(*maybeAdvancedConfig);
    } else {
        // Use simple configuration
        useSimpleDefaults();
    }
}
```

## Backward Compatibility

- All existing `as_int<T>()`, `as_float<T>()`, etc. methods remain functional
- Existing code continues to work without changes
- New code can gradually adopt the ergonomic API
- Documentation will guide users toward the new preferred methods

## Testing Strategy

1. **Comprehensive test coverage** for all three new methods
2. **Type safety verification** across all supported types  
3. **Default value validation** for each type category
4. **Performance benchmarks** to ensure no regression
5. **Migration examples** showing old vs new patterns

This refactor represents a significant usability improvement while maintaining full backward compatibility and type safety. 
