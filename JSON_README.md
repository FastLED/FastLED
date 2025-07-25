# JSON API Improvements

This directory contains documentation and examples for the improved JSON API in FastLED.

## Files

- `JSON_ENHANCEMENTS_SUMMARY.md` - Detailed analysis of the JSON API improvements
- `JSON_IMPROVEMENTS.md` - Alternative documentation of proposed improvements
- `FLUID_JSON.md` - Documentation of the fluid JSON API concept
- `PULL_REQUEST_DESCRIPTION.md` - Description for a pull request implementing these improvements
- `examples/Json/JsonExample.ino` - Example sketch demonstrating the improved API
- `tests/test_json_improvements.cpp` - Tests for the JSON improvements
- `tests/test_json.cpp` - Updated tests showcasing the improved API

## Key Improvements

1. **Enhanced Default Value Operator** - Automatic type conversion for cleaner code
2. **Contains Methods** - Check for key/index existence safely
3. **Size Method** - Get the number of elements in arrays or objects
4. **Factory Methods** - Easy creation of arrays and objects
5. **Direct Construction** - Create JSON values from native types directly

## Usage

To use the improved JSON API in your FastLED project:

```cpp
#include "FastLED.h"
#include "fl/json.h"

// Parse JSON
fl::Json json = fl::Json::parse("{\"leds\": 100, \"brightness\": 128}");

// Extract values with automatic type conversion
int leds = json["leds"] | 60;  // Gets 100 or defaults to 60
int brightness = json["brightness"] | 64;  // Gets 128 or defaults to 64

// Check for key existence
if (json.contains("color")) {
    // Process color setting
}

// Create new JSON objects
fl::Json config = fl::Json::object();
config["speed"] = fl::Json(50);
```

See `examples/Json/JsonExample.ino` for a complete example.