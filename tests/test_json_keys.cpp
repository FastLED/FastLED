#include "test.h"
#include "fl/json.h"

TEST_CASE("Json keys method test") {
    SUBCASE("Test keys() method") {
        // Create a JSON object with some keys
        auto json = fl::Json::createObject();
        json.set("name", "FastLED");
        json.set("version", 5);
        json.set("active", true);
        
        // Test the new keys() method
        auto keys = json.keys();
        REQUIRE(keys.size() == 3);
        
        // Check that all expected keys are present
        bool foundName = false;
        bool foundVersion = false;
        bool foundActive = false;
        
        for (const auto& key : keys) {
            if (key == "name") foundName = true;
            if (key == "version") foundVersion = true;
            if (key == "active") foundActive = true;
        }
        
        REQUIRE(foundName);
        REQUIRE(foundVersion);
        REQUIRE(foundActive);
        
        // Test that the old getObjectKeys() method still works (backward compatibility)
        auto oldKeys = json.getObjectKeys();
        REQUIRE(oldKeys.size() == 3);
        REQUIRE(oldKeys == keys);
    }
    
    SUBCASE("Test keys() on empty object") {
        auto json = fl::Json::createObject();
        auto keys = json.keys();
        REQUIRE(keys.size() == 0);
    }
    
    SUBCASE("Test keys() on non-object returns empty vector") {
        auto json = fl::Json::createArray();
        auto keys = json.keys();
        REQUIRE(keys.size() == 0);
    }
}