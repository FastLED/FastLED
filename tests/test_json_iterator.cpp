#include "test.h"
#include "fl/json.h"

TEST_CASE("JSON iterator test") {
    // Create a simple JSON object
    fl::Json obj = fl::Json::object();
    obj["key1"] = "value1";
    obj["key2"] = "value2";
    
    // Test that we can iterate over it
    int count = 0;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        count++;
    }
    
    CHECK_EQ(count, 2);
    
    // Test const iteration
    const fl::Json const_obj = obj;
    count = 0;
    for (auto it = const_obj.begin(); it != const_obj.end(); ++it) {
        count++;
    }
    
    CHECK_EQ(count, 2);
    
    // Test range-based for loop
    count = 0;
    for (const auto& kv : obj) {
        count++;
    }
    
    CHECK_EQ(count, 2);
}