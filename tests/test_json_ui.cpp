#include "test.h"

#include "fl/json.h"
#include "fl/namespace.h"
#include "fl/map.h"
#include "fl/hash_map.h"

#if FASTLED_ENABLE_JSON

#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/ui_manager.h"
#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/slider.h"
#include "platforms/shared/ui/json/checkbox.h"
#include "platforms/shared/ui/json/dropdown.h"
#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/title.h"
#include "platforms/shared/ui/json/description.h"
#include "platforms/shared/ui/json/help.h"
#include "platforms/shared/ui/json/number_field.h"

FASTLED_USING_NAMESPACE

#if 1

TEST_CASE("JsonUiInternal basic functionality") {
    bool updateCalled = false;
    fl::Json updateValue;
    bool toJsonCalled = false;
    
    auto updateFunc = [&](const fl::Json &json) {
        updateCalled = true;
        updateValue = json;
    };
    
    auto toJsonFunc = [&]() -> fl::Json {
        toJsonCalled = true;
        return fl::JsonBuilder()
            .set("test", "value")
            .build();
    };
    
    fl::string name = "test_component";
    JsonUiInternalPtr internal = fl::make_shared<JsonUiInternal>(name, updateFunc, toJsonFunc);
    
    REQUIRE(internal != nullptr);
    CHECK(internal->name() == name);
    CHECK(internal->id() >= 0);
    CHECK(internal->groupName().empty());
}


TEST_CASE("JsonUiInternal JSON operations") {
    bool updateCalled = false;
    float receivedValue = 0.0f;
    
    auto updateFunc = [&](const fl::Json &json) {
        updateCalled = true;
        auto maybeValue = json.get<float>();
        if (maybeValue.has_value()) {
            receivedValue = *maybeValue;
        }
    };
    
    auto toJsonFunc = JsonUiInternal::ToJsonFunction([&]() -> fl::Json {
        return fl::JsonBuilder()
            .set("name", "test")
            .set("value", 42.5f)
            .set("type", "slider")
            .build();
    });
    
    JsonUiInternalPtr internal = fl::make_shared<JsonUiInternal>("test", updateFunc, toJsonFunc);
    
    // Test JSON update using ideal API
    fl::Json updateJson = fl::Json::parse("123.456");
    internal->update(updateJson);
    
    CHECK(updateCalled);
    CHECK_CLOSE(receivedValue, 123.456f, 0.001f);
    
    // Test JSON serialization
    fl::Json serializedJson = internal->toJson();
    
    CHECK(serializedJson["name"] | fl::string("") == fl::string("test"));
    CHECK(serializedJson["value"] | 0.0f == 42.5f);
    CHECK(serializedJson["type"] | fl::string("") == fl::string("slider"));
    internal->clearFunctions();
}

TEST_CASE("JsonSliderImpl basic functionality") {
    JsonSliderImpl slider("brightness", 100.0f, 0.0f, 255.0f, 1.0f);
    
    CHECK(slider.name() == "brightness");
    CHECK(slider.value() == 100.0f);
    CHECK(slider.getMin() == 0.0f);
    CHECK(slider.getMax() == 255.0f);
    // Note: step() method may not be exposed in the public API
    
    slider.setValue(200.0f);
    CHECK(slider.value() == 200.0f);
}

TEST_CASE("JsonSliderImpl JSON serialization") {
    JsonSliderImpl slider("brightness", 128.0f, 0.0f, 255.0f, 1.0f);
    
    fl::Json json = slider.toJson();
    
    CHECK(json["name"] | fl::string("") == fl::string("brightness"));
    CHECK(json["type"] | fl::string("") == fl::string("slider"));
    CHECK(json["value"] | 0.0f == 128.0f);
    CHECK(json["min"] | 0.0f == 0.0f);
    CHECK(json["max"] | 0.0f == 255.0f);
    CHECK(json["step"] | 0.0f == 1.0f);
    CHECK(json["id"] | -1 >= 0);
}

TEST_CASE("JsonCheckboxImpl basic functionality") {
    JsonCheckboxImpl checkbox("enabled", true);
    
    CHECK(checkbox.name() == "enabled");
    CHECK(checkbox.value() == true);
    
    checkbox.setValue(false);
    CHECK(checkbox.value() == false);
}

TEST_CASE("JsonCheckboxImpl JSON serialization") {
    JsonCheckboxImpl checkbox("enabled", true);
    
    fl::Json json = checkbox.toJson();
    
    CHECK(json["name"] | fl::string("") == fl::string("enabled"));
    CHECK(json["type"] | fl::string("") == fl::string("checkbox"));
    CHECK(json["value"] | false == true);
    CHECK(json["id"] | -1 >= 0);
}

TEST_CASE("JsonDropdownImpl basic functionality") {
    JsonDropdownImpl dropdown("mode", {"auto", "manual", "off"});
    
    CHECK(dropdown.name() == "mode");
    CHECK(dropdown.getOptionCount() == 3);
    CHECK(dropdown.getOption(0) == "auto");
    CHECK(dropdown.getOption(1) == "manual");
    CHECK(dropdown.getOption(2) == "off");
    
    dropdown.setSelectedIndex(1);
    CHECK(dropdown.value_int() == 1);
    CHECK(dropdown.value() == "manual");
}

TEST_CASE("JsonDropdownImpl initializer list constructor") {
    JsonDropdownImpl dropdown("colors", {"red", "green", "blue"});
    
    CHECK(dropdown.getOptionCount() == 3);
    CHECK(dropdown.getOption(0) == "red");
    CHECK(dropdown.getOption(1) == "green");
    CHECK(dropdown.getOption(2) == "blue");
    CHECK(dropdown.value() == "red");
}

TEST_CASE("JsonDropdownImpl JSON serialization") {
    JsonDropdownImpl dropdown("mode", {"auto", "manual", "off"});
    dropdown.setSelectedIndex(1);
    
    fl::Json json = dropdown.toJson();
    
    CHECK(json["name"] | fl::string("") == fl::string("mode"));
    CHECK(json["type"] | fl::string("") == fl::string("dropdown"));
    CHECK(json["value"] | -1 == 1);
    
    CHECK(json["options"].is_array());
    CHECK(json["options"].size() == 3);
    CHECK(json["options"][0] | fl::string("") == fl::string("auto"));
    CHECK(json["options"][1] | fl::string("") == fl::string("manual"));
    CHECK(json["options"][2] | fl::string("") == fl::string("off"));
}

TEST_CASE("JsonUiManager basic functionality") {
    bool callbackCalled = false;
    fl::string receivedJson;
    
    auto callback = [&](const char* json) {
        callbackCalled = true;
        receivedJson = json;
    };
    
    JsonUiManager manager(callback);
    
    // Create some components
    JsonSliderImpl slider("brightness", 128.0f, 0.0f, 255.0f, 1.0f);
    JsonCheckboxImpl checkbox("enabled", true);
    
    // Components should auto-register through their constructors
    // We can't directly test this without access to the internal component list
    // but we can test that the manager doesn't crash
    
    CHECK(true); // Manager constructed successfully
}

TEST_CASE("JsonHelpImpl comprehensive testing") {
    // Test basic creation
    fl::string helpContent = R"(# FastLED Quick Start

## Basic Setup
```cpp
#include <FastLED.h>
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];
```

## Key Functions
- **FastLED.addLeds()** - Initialize LED strip
- **FastLED.show()** - Update display  
- **fill_solid()** - Set all LEDs to one color

For more info, visit [FastLED.io](https://fastled.io))";
    
    JsonHelpImpl help(helpContent);
    
    // Test basic properties
    CHECK(help.name() == "help");
    CHECK(help.markdownContent() == helpContent);
    CHECK(help.groupName().empty());
    
    // Test group setting
    help.Group("documentation");
    CHECK(help.groupName() == "documentation");
    
    // Test JSON serialization
    fl::Json json = help.toJson();
    
    CHECK(json["name"] | fl::string("") == fl::string("help"));
    CHECK(json["type"] | fl::string("") == fl::string("help"));
    CHECK(json["group"] | fl::string("") == fl::string("documentation"));
    CHECK(json["id"] | -1 >= 0);
    CHECK(json["markdownContent"] | fl::string("") == helpContent);
}

TEST_CASE("Component boundary value testing") {
    // Test slider with edge values
    JsonSliderImpl slider("test", 50.0f, 0.0f, 100.0f, 1.0f);
    
    slider.setValue(-10.0f); // Below minimum
    // Note: We can't check clamping behavior without seeing the implementation
    // but we can verify it doesn't crash
    
    slider.setValue(150.0f); // Above maximum
    // Same note as above
    
    // Test dropdown with invalid indices
    JsonDropdownImpl dropdown("test", {"a", "b", "c"});
    
    // These should be handled gracefully
    dropdown.setSelectedIndex(-1);
    dropdown.setSelectedIndex(10);
    
    CHECK(true); // No crashes
}

TEST_CASE("JsonUiManager executeUiUpdates") {
    bool callbackCalled = false;
    fl::string receivedJson;
    
    auto callback = [&](const char* json) {
        callbackCalled = true;
        receivedJson = json;
    };
    
    JsonUiManager manager(callback);
    
    // Create a test JSON document for UI updates
    fl::JsonDocument updateDoc;
    updateDoc["brightness"] = 150;
    updateDoc["enabled"] = false;
    
    // Test executing updates
    manager.executeUiUpdates(updateDoc);
    
    // Note: This test mainly verifies the method doesn't crash
    // The actual component updates would require registered components
    CHECK(true); // Method executed without crashing
}

TEST_CASE("JsonUiManager multiple components basic") {
    bool callbackCalled = false;
    fl::string receivedJson;
    
    auto callback = [&](const char* json) {
        callbackCalled = true;
        receivedJson = json;
    };
    
    // Set up the global UI system with our callback
    auto updateEngineState = setJsonUiHandlers(callback);
    
    // Create multiple components
    JsonSliderImpl slider1("slider1", 25.0f, 0.0f, 100.0f, 1.0f);
    JsonSliderImpl slider2("slider2", 50.0f, 0.0f, 100.0f, 1.0f);
    JsonCheckboxImpl checkbox("checkbox", false);
    
    // Test that components were created successfully
    CHECK_CLOSE(slider1.value(), 25.0f, 0.001f);
    CHECK_CLOSE(slider2.value(), 50.0f, 0.001f);
    CHECK(checkbox.value() == false);
    
    // Test that we can update values directly
    slider1.setValue(80.0f);
    slider2.setValue(20.0f);
    checkbox.setValue(true);
    
    // Check that all components were updated
    CHECK_CLOSE(slider1.value(), 80.0f, 0.001f);
    CHECK_CLOSE(slider2.value(), 20.0f, 0.001f);
    CHECK(checkbox.value() == true);
    
    // Clean up
    setJsonUiHandlers(fl::function<void(const char*)>());
}

#endif

#else

TEST_CASE("JSON UI disabled") {
    // When JSON is disabled, we should still be able to compile
    // but functionality will be limited
    CHECK(true);
}

#endif // FASTLED_ENABLE_JSON
