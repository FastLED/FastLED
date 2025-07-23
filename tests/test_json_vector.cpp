#include "test.h"
#include "fl/json.h"
#include "fl/str.h"

FASTLED_USING_NAMESPACE

#if FASTLED_ENABLE_JSON

TEST_CASE("JsonBuilder vector<Json> support") {
    // Create some component Json objects
    auto component1 = fl::JsonBuilder()
        .set("type", "slider")
        .set("name", "brightness")
        .set("value", 128)
        .build();
    
    auto component2 = fl::JsonBuilder()
        .set("type", "button")
        .set("name", "reset")
        .set("pressed", false)
        .build();
    
    auto component3 = fl::JsonBuilder()
        .set("type", "checkbox")
        .set("name", "enabled")
        .set("checked", true)
        .build();
    
    // Create array of Json objects using the new functionality
    fl::vector<fl::Json> components = {component1, component2, component3};
    
    // Build JSON with array of objects
    auto json = fl::JsonBuilder()
        .set("ui_components", components)
        .set("metadata", "ui_update")
        .set("timestamp", 123456789)
        .build();
    
    // Verify the structure
    CHECK(json["ui_components"].is_array());
    CHECK_EQ(json["ui_components"].size(), 3);
    CHECK_EQ(json["metadata"] | fl::string(""), "ui_update");
    CHECK_EQ(json["timestamp"] | 0, 123456789);
    
    // Verify individual components
    auto slider = json["ui_components"][0];
    CHECK_EQ(slider["type"] | fl::string(""), "slider");
    CHECK_EQ(slider["name"] | fl::string(""), "brightness");
    CHECK_EQ(slider["value"] | 0, 128);
    
    auto button = json["ui_components"][1];
    CHECK_EQ(button["type"] | fl::string(""), "button");
    CHECK_EQ(button["name"] | fl::string(""), "reset");
    CHECK_EQ(button["pressed"] | true, false);
    
    auto checkbox = json["ui_components"][2];
    CHECK_EQ(checkbox["type"] | fl::string(""), "checkbox");
    CHECK_EQ(checkbox["name"] | fl::string(""), "enabled");
    CHECK_EQ(checkbox["checked"] | false, true);
    
    // Test serialization
    fl::string jsonStr = json.serialize();
    CHECK(!jsonStr.empty());
    
    // Verify we can parse it back
    fl::Json reparsed = fl::Json::parse(jsonStr.c_str());
    CHECK(reparsed["ui_components"].is_array());
    CHECK_EQ(reparsed["ui_components"].size(), 3);
}

TEST_CASE("JsonBuilder vector<Json> empty array") {
    // Test with empty array
    fl::vector<fl::Json> empty_components;
    
    auto json = fl::JsonBuilder()
        .set("components", empty_components)
        .set("count", 0)
        .build();
    
    CHECK(json["components"].is_array());
    CHECK_EQ(json["components"].size(), 0);
    CHECK_EQ(json["count"] | -1, 0);
    
    fl::string jsonStr = json.serialize();
    CHECK(!jsonStr.empty());
}

TEST_CASE("JsonBuilder vector<Json> real world usage") {
    // Simulate the real-world use case from the design document
    fl::vector<fl::Json> uiComponents;
    
    // Add multiple components like a real UI would
    for (int i = 0; i < 5; ++i) {
        auto component = fl::JsonBuilder()
            .set("id", i)
            .set("type", i % 2 == 0 ? "slider" : "button")
            .set("value", i * 10)
            .set("enabled", i % 3 != 0)
            .build();
        uiComponents.push_back(component);
    }
    
    // Build final JSON structure
    auto json = fl::JsonBuilder()
        .set("ui_components", uiComponents)
        .set("version", "1.0")
        .set("total_components", static_cast<int>(uiComponents.size()))
        .build();
    
    // Verify structure
    CHECK(json["ui_components"].is_array());
    CHECK_EQ(json["ui_components"].size(), 5);
    CHECK_EQ(json["version"] | fl::string(""), "1.0");
    CHECK_EQ(json["total_components"] | 0, 5);
    
    // Verify component details
    for (size_t i = 0; i < 5; ++i) {
        auto component = json["ui_components"][i];
        CHECK_EQ(component["id"] | -1, static_cast<int>(i));
        CHECK_EQ(component["value"] | -1, static_cast<int>(i * 10));
        
        fl::string expectedType = (i % 2 == 0) ? "slider" : "button";
        CHECK_EQ(component["type"] | fl::string(""), expectedType);
        
        bool expectedEnabled = (i % 3 != 0);
        CHECK_EQ(component["enabled"] | !expectedEnabled, expectedEnabled);
    }
}

#endif // FASTLED_ENABLE_JSON 
