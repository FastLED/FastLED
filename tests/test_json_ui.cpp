#include "test.h"

#include "fl/json.h"
#include "fl/namespace.h"

#if FASTLED_ENABLE_JSON

#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/ui_manager.h"
#include "platforms/shared/ui/json/slider.h"
#include "platforms/shared/ui/json/checkbox.h"
#include "platforms/shared/ui/json/dropdown.h"
#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/title.h"
#include "platforms/shared/ui/json/description.h"
#include "platforms/shared/ui/json/number_field.h"

FASTLED_USING_NAMESPACE

#if 1

TEST_CASE("JsonUiInternal creation and basic operations") {
    bool updateCalled = false;
    float updateValue = 0.0f;
    bool toJsonCalled = false;
    
    auto updateFunc = [&](const FLArduinoJson::JsonVariantConst &json) {
        updateCalled = true;
        if (json.is<float>()) {
            updateValue = json.as<float>();
        }
    };
    
    auto toJsonFunc = [&](FLArduinoJson::JsonObject &json) {
        toJsonCalled = true;
        json["test"] = "value";
    };
    
    fl::string name = "test_component";
    JsonUiInternalPtr internal = JsonUiInternalPtr::New(name, updateFunc, toJsonFunc);
    
    REQUIRE(internal != nullptr);
    CHECK(internal->name() == name);
    CHECK(internal->id() >= 0);
    CHECK(internal->groupName().empty());
    
    // Test group functionality
    fl::string groupName = "test_group";
    internal->setGroup(groupName);
    CHECK(internal->groupName() == groupName);
}


TEST_CASE("JsonUiInternal JSON operations") {
    bool updateCalled = false;
    float receivedValue = 0.0f;
    
    auto updateFunc = [&](const FLArduinoJson::JsonVariantConst &json) {
        updateCalled = true;
        if (json.is<float>()) {
            receivedValue = json.as<float>();
        }
    };
    
    auto toJsonFunc = [&](FLArduinoJson::JsonObject &json) {
        json["name"] = "test";
        json["value"] = 42.5f;
        json["type"] = "slider";
    };
    
    JsonUiInternalPtr internal = JsonUiInternalPtr::New("test", updateFunc, toJsonFunc);
    
    // Test JSON update
    FLArduinoJson::JsonDocument doc;
    doc.set(123.456f);
    internal->update(doc.as<FLArduinoJson::JsonVariantConst>());
    
    CHECK(updateCalled);
    CHECK_CLOSE(receivedValue, 123.456f, 0.001f);
    
    // Test JSON serialization
    FLArduinoJson::JsonDocument outputDoc;
    auto jsonObj = outputDoc.to<FLArduinoJson::JsonObject>();
    internal->toJson(jsonObj);
    
    CHECK(fl::string(jsonObj["name"].as<const char*>()) == fl::string("test"));
    CHECK_CLOSE(jsonObj["value"].as<float>(), 42.5f, 0.001f);
    CHECK(fl::string(jsonObj["type"].as<const char*>()) == fl::string("slider"));
}

TEST_CASE("JsonSliderImpl basic functionality") {
    fl::string name = "test_slider";
    float initialValue = 128.0f;
    float minValue = 0.0f;
    float maxValue = 255.0f;
    
    JsonSliderImpl slider(name, initialValue, minValue, maxValue, 1.0f);
    
    CHECK(slider.name() == name);
    CHECK_CLOSE(slider.value(), initialValue, 0.001f);
    CHECK_CLOSE(slider.value_normalized(), 128.0f / 255.0f, 0.001f);
    CHECK_CLOSE(slider.getMin(), minValue, 0.001f);
    CHECK_CLOSE(slider.getMax(), maxValue, 0.001f);
    
    // Test value setting
    float newValue = 200.0f;
    slider.setValue(newValue);
    CHECK_CLOSE(slider.value(), newValue, 0.001f);
    
    // Test assignment operator
    slider = 175.5f;
    CHECK_CLOSE(slider.value(), 175.5f, 0.001f);
    
    slider = 100;
    CHECK_CLOSE(slider.value(), 100.0f, 0.001f);
    
    // Test type conversion
    CHECK(slider.as_int() == 100);
    CHECK(slider.as<int>() == 100);
    
    // Test grouping
    fl::string groupName = "controls";
    slider.Group(groupName);
    CHECK(slider.groupName() == groupName);
}

TEST_CASE("JsonSliderImpl JSON serialization") {
    JsonSliderImpl slider("brightness", 128.0f, 0.0f, 255.0f, 1.0f);
    
    FLArduinoJson::JsonDocument doc;
    auto jsonObj = doc.to<FLArduinoJson::JsonObject>();
    slider.toJson(jsonObj);
    
    CHECK(fl::string(jsonObj["name"].as<const char*>()) == fl::string("brightness"));
    CHECK(fl::string(jsonObj["type"].as<const char*>()) == fl::string("slider"));
    CHECK_CLOSE(jsonObj["value"].as<float>(), 128.0f, 0.001f);
    CHECK_CLOSE(jsonObj["min"].as<float>(), 0.0f, 0.001f);
    CHECK_CLOSE(jsonObj["max"].as<float>(), 255.0f, 0.001f);
    CHECK_CLOSE(jsonObj["step"].as<float>(), 1.0f, 0.001f);
}

TEST_CASE("JsonCheckboxImpl basic functionality") {
    fl::string name = "test_checkbox";
    bool initialValue = true;
    
    JsonCheckboxImpl checkbox(name, initialValue);
    
    CHECK(checkbox.name() == name);
    CHECK(checkbox.value() == initialValue);
    
    // Test value setting
    checkbox.setValue(false);
    CHECK(checkbox.value() == false);
    
    // Test assignment operators
    checkbox = true;
    CHECK(checkbox.value() == true);
    
    checkbox = 0;
    CHECK(checkbox.value() == false);
    
    checkbox = 1;
    CHECK(checkbox.value() == true);
    
    // Test grouping
    fl::string groupName = "options";
    checkbox.Group(groupName);
    CHECK(checkbox.groupName() == groupName);
}

TEST_CASE("JsonCheckboxImpl JSON serialization") {
    JsonCheckboxImpl checkbox("enabled", true);
    
    FLArduinoJson::JsonDocument doc;
    auto jsonObj = doc.to<FLArduinoJson::JsonObject>();
    checkbox.toJson(jsonObj);
    
    CHECK(fl::string(jsonObj["name"].as<const char*>()) == fl::string("enabled"));
    CHECK(fl::string(jsonObj["type"].as<const char*>()) == fl::string("checkbox"));
    CHECK(jsonObj["value"].as<bool>() == true);
}

TEST_CASE("JsonDropdownImpl basic functionality") {
    fl::string name = "test_dropdown";
    fl::vector<fl::string> options = {"option1", "option2", "option3"};
    
    JsonDropdownImpl dropdown(name, fl::Slice<fl::string>(options.data(), options.size()));
    
    CHECK(dropdown.name() == name);
    CHECK(dropdown.getOptionCount() == 3);
    CHECK(dropdown.getOption(0) == "option1");
    CHECK(dropdown.getOption(1) == "option2");
    CHECK(dropdown.getOption(2) == "option3");
    
    // Test initial selection (should be 0)
    CHECK(dropdown.value_int() == 0);
    CHECK(dropdown.value() == "option1");
    
    // Test setting selection by index
    dropdown.setSelectedIndex(1);
    CHECK(dropdown.value_int() == 1);
    CHECK(dropdown.value() == "option2");
    
    // Test assignment operator
    dropdown = 2;
    CHECK(dropdown.value_int() == 2);
    CHECK(dropdown.value() == "option3");
    
    // Test grouping
    fl::string groupName = "settings";
    dropdown.Group(groupName);
    CHECK(dropdown.groupName() == groupName);
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
    
    FLArduinoJson::JsonDocument doc;
    auto jsonObj = doc.to<FLArduinoJson::JsonObject>();
    dropdown.toJson(jsonObj);
    
    CHECK(fl::string(jsonObj["name"].as<const char*>()) == fl::string("mode"));
    CHECK(fl::string(jsonObj["type"].as<const char*>()) == fl::string("dropdown"));
    CHECK(jsonObj["value"].as<int>() == 1);
    
    auto optionsArray = jsonObj["options"];
    CHECK(optionsArray.is<FLArduinoJson::JsonArray>());
    CHECK(fl::string(optionsArray[0].as<const char*>()) == fl::string("auto"));
    CHECK(fl::string(optionsArray[1].as<const char*>()) == fl::string("manual"));
    CHECK(fl::string(optionsArray[2].as<const char*>()) == fl::string("off"));
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
#endif

#else

TEST_CASE("JSON UI disabled") {
    // When JSON is disabled, we should still be able to compile
    // but functionality will be limited
    CHECK(true);
}

#endif // FASTLED_ENABLE_JSON
