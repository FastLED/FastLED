#include "test.h"
#include "fl/json.h"
#include "fl/screenmap.h"

TEST_CASE("ScreenMap JSON keys compatibility") {
    SUBCASE("Test that screenmap still works with keys() method") {
        // Create a simple JSON that represents a screenmap
        const char* jsonStr = R"({
            "map": {
                "strip1": {
                    "x": [1.0, 3.0],
                    "y": [2.0, 4.0]
                },
                "strip2": {
                    "x": [10.0, 30.0, 50.0],
                    "y": [20.0, 40.0, 60.0]
                }
            }
        })";
        
        fl::JsonDocument doc;
        fl::string error;
        
        // Parse the JSON
        bool success = fl::parseJson(jsonStr, &doc, &error);
        REQUIRE(success);
        REQUIRE(error.empty());
        
        // Check that we can access the map object
        auto mapJson = doc["map"];
        REQUIRE(!mapJson.is_null());
        REQUIRE(mapJson.is_object());
        
        // Test the new keys() method
        auto segmentKeys = mapJson.keys();
        REQUIRE(segmentKeys.size() == 2);
        
        // Check that we have the expected keys
        bool foundStrip1 = false;
        bool foundStrip2 = false;
        
        for (const auto& key : segmentKeys) {
            if (key == "strip1") foundStrip1 = true;
            if (key == "strip2") foundStrip2 = true;
        }
        
        REQUIRE(foundStrip1);
        REQUIRE(foundStrip2);
        
        // Test that the old getObjectKeys() method still works (backward compatibility)
        auto oldKeys = mapJson.getObjectKeys();
        REQUIRE(oldKeys.size() == 2);
        REQUIRE(oldKeys == segmentKeys);
    }
}