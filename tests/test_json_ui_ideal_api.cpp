#include "test.h"
#include "fl/json.h"
#include "fl/str.h"
#include "fl/vector.h"

FASTLED_USING_NAMESPACE

#if FASTLED_ENABLE_JSON

TEST_CASE("JsonBuilder basic functionality") {
    // Test the existing JsonBuilder functionality
    auto json = JsonBuilder()
        .set("brightness", 128)
        .set("enabled", true)
        .set("name", "test_device")
        .build();
    
    // Verify the JSON structure
    CHECK((json["brightness"] | 0) == 128);
    CHECK((json["enabled"] | false) == true);
    CHECK((json["name"] | string("")) == "test_device");
}

TEST_CASE("Json basic parsing") {
    // Test basic JSON parsing with the ideal API
    const char* json_str = R"({
        "brightness": 128,
        "enabled": true,
        "name": "test_device",
        "temperature": 25.5
    })";
    
    fl::Json json = fl::Json::parse(json_str);
    CHECK(json.has_value());
    CHECK(json.is_object());
    
    // Test safe access with defaults
    CHECK((json["brightness"] | 0) == 128);
    CHECK((json["enabled"] | false) == true);
    CHECK((json["name"] | string("")) == "test_device");
    CHECK((json["temperature"] | 0.0f) == 25.5f);
    
    // Test missing values with defaults
    CHECK((json["missing"] | 99) == 99);
    CHECK((json["missing"] | string("default")) == "default");
}

TEST_CASE("Json type-safe default values") {
    // Test the | operator for safe defaults with various types
    auto json = JsonBuilder()
        .set("existing_int", 42)
        .set("existing_string", "hello")
        .set("existing_bool", true)
        .build();
    
    // Test existing values
    CHECK((json["existing_int"] | 0) == 42);
    CHECK((json["existing_string"] | string("default")) == "hello");
    CHECK((json["existing_bool"] | false) == true);
    
    // Test missing values with defaults
    CHECK((json["missing_int"] | 99) == 99);
    CHECK((json["missing_string"] | string("default")) == "default");
    CHECK((json["missing_bool"] | true) == true);
    
    // Test type mismatches with defaults
    CHECK((json["existing_string"] | 0) == 0); // String can't convert to int
    CHECK((json["existing_int"] | string("default")) == "default"); // Int can't convert to string
}

#endif // FASTLED_ENABLE_JSON 
