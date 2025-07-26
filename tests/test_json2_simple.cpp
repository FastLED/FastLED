#include "test.h"
#include "fl/json.h"

using fl::string;


TEST_CASE("Simple JSON2 test") {
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
    
    // Test parsing
    fl::Json parsed = fl::Json::parse(jsonStr);
    CHECK(parsed.has_value());
    CHECK(parsed.is_object());
    
    // Test accessing values
    CHECK(parsed.contains("key1"));
    CHECK(parsed["key1"].is_string());
    CHECK((parsed["key1"] | string("")) == "value1");
    
    CHECK(parsed.contains("key2"));
    CHECK(parsed["key2"].is_int());
    CHECK((parsed["key2"] | 0) == 42);
    
    CHECK(parsed.contains("key3"));
    CHECK(parsed["key3"].is_double());
    CHECK((parsed["key3"] | 0.0) == 3.14);
}
