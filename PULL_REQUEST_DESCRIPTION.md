# Make JSON API More Fluid and Easy to Use

## Description

This PR enhances the JSON API in FastLED to make it more fluid and user-friendly based on the analysis in `JSON_ENHANCEMENTS_SUMMARY.md`. The improvements focus on making common JSON operations more intuitive while maintaining backward compatibility.

## Key Improvements

### 1. Enhanced Default Value Operator
- Added automatic type conversion in the `|` operator
- Users can now use native types like `int` and `float` directly with appropriate conversions from stored types

### 2. Added `contains()` Methods
- Added `contains(size_t idx)` for arrays to check if an index exists
- Added `contains(const fl::string &key)` for objects to check if a key exists

### 3. Added `size()` Method
- Added a `size()` method that returns the number of elements in arrays or objects

### 4. Enhanced `Json` Wrapper Class
- Added static factory methods `Json::array()` and `Json::object()` for easy container creation
- Improved constructor overloads for direct value creation

## Usage Examples

```cpp
// Parse JSON
fl::Json json = fl::Json::parse("{\"version\": 5, \"brightness\": 200}");

// Extract values with automatic type conversion
int version = json["version"] | 0;  // Automatic conversion from int64_t
float brightness = json["brightness"] | 0.5f;  // Gets default value

// Check for key existence
if (json.contains("features")) {
    // Process features
}

// Get container size
size_t featureCount = json["features"].size();

// Create new containers
fl::Json settings = fl::Json::object();
settings["brightness"] = fl::Json(200);

fl::Json colors = fl::Json::array();
colors[0] = fl::Json("red");
```

## Benefits

1. **More Intuitive API**: Users can work with native types directly without explicit casting
2. **Safety**: No crashes on missing fields with clear existence checking
3. **Cleaner Code**: Less verbose with automatic type conversions
4. **Flexibility**: Easy container creation and manipulation
5. **Better Debugging**: Size and existence methods help with troubleshooting

## Backward Compatibility

All improvements maintain full backward compatibility with existing code. Users can continue to use the existing API as before while benefiting from the new enhancements when they choose to use them.

## Related Documentation

- `JSON_ENHANCEMENTS_SUMMARY.md` - Detailed analysis of improvements
- `JSON_BUG.md` - Previous JSON-related fixes
- `JSON_IDEAL.md` - Original vision for an ideal JSON API

Closes #[issue-number]