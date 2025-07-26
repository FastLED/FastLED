#include "test.h"

#include "fl/json.h"
#include "fl/json.h"
#include "fl/string.h"

TEST_CASE("Json to Json2 conversion through string") {
    // Test with a complete JSON object containing a boolean value
    const char* boolJson = "{\"value\": true}";
    fl::Json original = fl::Json::parse(boolJson);
    CHECK(original.has_value());
    
    fl::string serialized = original.serialize();
    fl::Json converted = fl::Json::parse(serialized);
    CHECK(converted.has_value());
    
    bool result = converted["value"] | false;
    CHECK(result == true);
    
    // Test with a complete JSON object containing an integer value
    const char* intJson = "{\"value\": 42}";
    original = fl::Json::parse(intJson);
    CHECK(original.has_value());
    
    serialized = original.serialize();
    converted = fl::Json::parse(serialized);
    CHECK(converted.has_value());
    
    int int_result = converted["value"] | 0;
    CHECK(int_result == 42);
    
    // Test with a complete JSON object containing a string value
    const char* stringJson = "{\"value\": \"hello\"}";
    original = fl::Json::parse(stringJson);
    CHECK(original.has_value());
    
    serialized = original.serialize();
    converted = fl::Json::parse(serialized);
    CHECK(converted.has_value());
    
    fl::string string_result = converted["value"] | fl::string("");
    CHECK(string_result == "hello");
    
    // Test with a more complex object
    const char* complexJson = "{\"key1\": \"value1\", \"key2\": 123, \"key3\": true}";
    original = fl::Json::parse(complexJson);
    CHECK(original.has_value());
    
    serialized = original.serialize();
    converted = fl::Json::parse(serialized);
    CHECK(converted.has_value());
    
    CHECK(converted.is_object());
    CHECK((converted["key1"] | fl::string("")) == "value1");
    CHECK((converted["key2"] | 0) == 123);
    CHECK((converted["key3"] | false) == true);
}
