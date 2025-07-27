#include "test.h"
#include "fl/json.h"

using fl::string;


TEST_CASE("Simple JSON test") {
    // Test creating a simple JSON object
    fl::Json obj = fl::Json::object();
    obj.set("key1", "value1");
    obj.set("key2", 42);
    obj.set("key3", 3.14);
    
    // Test creating a JSON array
    fl::Json arr = fl::Json::array();
    arr.push_back("item1");
    arr.push_back(123);
    arr.push_back(2.71);
    
    // Test nested objects
    fl::Json nested = fl::Json::object();
    nested.set("array", arr);
    nested.set("value", "nested_value");
    
    obj.set("nested", nested);
    
    // Test serialization
    string jsonStr = obj.to_string();
    CHECK_FALSE(jsonStr.empty());
    
    // Print the serialized JSON for debugging
    printf("Serialized JSON: %s\n", jsonStr.c_str());
    
    // Test parsing
    fl::Json parsed = fl::Json::parse(jsonStr);
    CHECK(parsed.has_value());
    CHECK(parsed.is_object());
    
    // Print parsed object keys for debugging
    auto keys = parsed.keys();
    printf("Parsed object has %zu keys:\n", keys.size());
    for (const auto& key : keys) {
        printf("  %s\n", key.c_str());
    }
    
    // Test accessing values
    CHECK(parsed.contains("key1"));
    CHECK(parsed["key1"].is_string());
    CHECK(parsed["key1"].as_or(string("")) == "value1");
    
    CHECK(parsed.contains("key2"));
    CHECK(parsed["key2"].is_int());
    CHECK(parsed["key2"].as_or(0) == 42);
    
    CHECK(parsed.contains("key3"));
    CHECK(parsed["key3"].is_double());
    CHECK(parsed["key3"].as_or(0.0) == 3.14);
}



TEST_CASE("Json as_or test") {
    // Test with primitive values - using correct types
    fl::Json intJson(42); // This creates an int64_t
    CHECK(intJson.is_int());
    CHECK(intJson.as_or(int64_t(0)) == 42);
    CHECK(intJson.as_or(int64_t(99)) == 42); // Should still be 42, not fallback
    
    fl::Json doubleJson(3.14);
    CHECK(doubleJson.is_double());
    CHECK(doubleJson.as_or(0.0) == 3.14);
    CHECK(doubleJson.as_or(9.9) == 3.14); // Should still be 3.14, not fallback
    
    fl::Json stringJson("hello");
    CHECK(stringJson.is_string());
    CHECK(stringJson.as_or(string("")) == "hello");
    CHECK(stringJson.as_or(string("world")) == "hello"); // Should still be "hello", not fallback
    
    fl::Json boolJson(true);
    CHECK(boolJson.is_bool());
    CHECK(boolJson.as_or(false) == true);
    CHECK(boolJson.as_or(true) == true); // Should still be true, not fallback
    
    // Test with null Json (no value)
    fl::Json nullJson;
    CHECK(nullJson.is_null());
    CHECK(nullJson.as_or(int64_t(100)) == 100); // Should use fallback
    CHECK(nullJson.as_or(string("default")) == "default"); // Should use fallback
    CHECK(nullJson.as_or(5.5) == 5.5); // Should use fallback
    CHECK(nullJson.as_or(false) == false); // Should use fallback
    
    // Test operator| still works the same way
    CHECK((intJson | int64_t(0)) == 42);
    CHECK((nullJson | int64_t(100)) == 100);
}
