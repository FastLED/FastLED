#include "test.h"
#include "fl/json.h"

using fl::string;

TEST_CASE("Debug JSON test") {
    // Test creating a simple JSON object
    fl::Json obj = fl::Json::object();
    printf("Created empty object\n");
    
    obj.set("key1", "value1");
    printf("Set key1 to value1\n");
    
    // Check if the key was actually set
    auto keys = obj.keys();
    printf("Object has %zu keys:\n", keys.size());
    for (const auto& key : keys) {
        printf("  %s\n", key.c_str());
    }
    
    CHECK(obj.contains("key1"));
    CHECK(obj["key1"].is_string());
    CHECK(obj["key1"].as_or(string("")) == "value1");
}