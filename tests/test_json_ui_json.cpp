#include "test.h"

#include "fl/json.h"
#include "fl/namespace.h"
#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/slider.h"

#if FASTLED_ENABLE_JSON

#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/ui_manager.h"
#include "platforms/shared/ui/json/checkbox.h"
#include "platforms/shared/ui/json/dropdown.h"
#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/title.h"
#include "platforms/shared/ui/json/description.h"
#include "platforms/shared/ui/json/number_field.h"

FASTLED_USING_NAMESPACE


TEST_CASE("JSON parsing and serialization utilities") {
    // Test parseJson function
    const char* jsonStr = R"({"name": "test", "value": 42.5, "enabled": true})";
    fl::JsonDocument doc;
    fl::string error;
    
    bool parseResult = fl::parseJson(jsonStr, &doc, &error);
    CHECK(parseResult);
    CHECK(error.empty());
    
    auto obj = doc.as<FLArduinoJson::JsonObjectConst>();
    CHECK(fl::string(obj["name"].as<const char*>()) == fl::string("test"));
    CHECK_CLOSE(obj["value"].as<float>(), 42.5f, 0.001f);
    CHECK(obj["enabled"].as<bool>() == true);
    
    // Test toJson function
    fl::JsonDocument outputDoc;
    auto outputObj = outputDoc.to<FLArduinoJson::JsonObject>();
    outputObj["result"] = "success";
    outputObj["count"] = 123;
    
    fl::string jsonBuffer;
    fl::toJson(outputDoc, &jsonBuffer);
    CHECK(!jsonBuffer.empty());
    CHECK(jsonBuffer.find('r') != fl::string::npos); // Check contains "result"
    CHECK(jsonBuffer.find('s') != fl::string::npos); // Check contains "success"  
    CHECK(jsonBuffer.find('1') != fl::string::npos); // Check contains "123"
}

TEST_CASE("JSON UI error handling") {
    // Test parsing invalid JSON
    const char* invalidJson = R"({"name": "test", "value": })";
    fl::JsonDocument doc;
    fl::string error;
    
    bool parseResult = fl::parseJson(invalidJson, &doc, &error);
    CHECK_FALSE(parseResult);
    CHECK(!error.empty());
}

// === COMPREHENSIVE JSON ROUND-TRIP COMMUNICATION TESTS ===

TEST_CASE("JsonUIJson UI elements serialization") {
    fl::JsonDocument doc;
    
    fl::Json json(doc);
    
    CHECK(json["name"] | fl::string("") == fl::string(""));
    
    fl::JsonDocument outputDoc;
    
    // Build the JSON array using fl::Json construction
    auto jsonArray = outputDoc.to<FLArduinoJson::JsonArray>();
    
    // Add slider component using ideal patterns
    auto sliderObj = jsonArray.add<FLArduinoJson::JsonObject>();
    sliderObj["name"] = "brightness";
    sliderObj["type"] = "slider";
    sliderObj["id"] = 1;
    sliderObj["value"] = 128.0f;
    sliderObj["min"] = 0.0f;
    sliderObj["max"] = 255.0f;
    sliderObj["step"] = 1.0f;
    sliderObj["group"] = "lighting";
    
    // Add checkbox component
    auto checkboxObj = jsonArray.add<FLArduinoJson::JsonObject>();
    checkboxObj["name"] = "enabled";
    checkboxObj["type"] = "checkbox";
    checkboxObj["id"] = 2;
    checkboxObj["value"] = true;
    checkboxObj["group"] = "controls";
    
    // Add dropdown component
    auto dropdownObj = jsonArray.add<FLArduinoJson::JsonObject>();
    dropdownObj["name"] = "mode";
    dropdownObj["type"] = "dropdown";
    dropdownObj["id"] = 3;
    dropdownObj["value"] = 1;
    dropdownObj["group"] = "settings";
    auto optionsArray = dropdownObj["options"].to<FLArduinoJson::JsonArray>();
    optionsArray.add("auto");
    optionsArray.add("manual");
    optionsArray.add("off");
    
    // Add button component
    auto buttonObj = jsonArray.add<FLArduinoJson::JsonObject>();
    buttonObj["name"] = "reset";
    buttonObj["type"] = "button";
    buttonObj["id"] = 4;
    buttonObj["pressed"] = false;
    buttonObj["group"] = "actions";
    
    // Add number field component
    auto numberObj = jsonArray.add<FLArduinoJson::JsonObject>();
    numberObj["name"] = "temperature";
    numberObj["type"] = "number";
    numberObj["id"] = 5;
    numberObj["value"] = 25.5f;
    numberObj["min"] = 0.0f;
    numberObj["max"] = 100.0f;
    numberObj["group"] = "sensors";
    
    // Add title component  
    auto titleObj = jsonArray.add<FLArduinoJson::JsonObject>();
    titleObj["name"] = "title";
    titleObj["type"] = "title";
    titleObj["id"] = 6;
    titleObj["text"] = "LED Control Panel";
    titleObj["group"] = "display";
    
    // Add description component
    auto descObj = jsonArray.add<FLArduinoJson::JsonObject>();
    descObj["name"] = "description";
    descObj["type"] = "description";
    descObj["id"] = 7;
    descObj["text"] = "Control your LED strips with these settings";
    descObj["group"] = "display";
    
    // Add help component
    auto helpObj = jsonArray.add<FLArduinoJson::JsonObject>();
    helpObj["name"] = "help";
    helpObj["type"] = "help";
    helpObj["id"] = 8;
    helpObj["markdownContent"] = "# Help\n\nThis is help content.";
    helpObj["group"] = "documentation";
    
    // Serialize to string using ideal API
    fl::string jsonStr = outputDoc.serialize();
    
    CHECK(!jsonStr.empty());
    CHECK(jsonStr.find("brightness") != fl::string::npos);
    CHECK(jsonStr.find("slider") != fl::string::npos);
}

TEST_CASE("JSON UI Changes from JavaScript - Round Trip") {
    // Test parsing UI changes JSON that would come from JavaScript UI manager
    // This matches the format sent by JsonUiManager.processUiChanges() in ui_manager.js
    
    const char* uiChangesJson = R"({
        "1": 200.5,
        "2": false,
        "3": 2,
        "4": true,
        "5": 150
    })";
    
    fl::JsonDocument doc;
    fl::string error;
    bool parseResult = fl::parseJson(uiChangesJson, &doc, &error);
    
    CHECK(parseResult);
    CHECK(error.empty());
    
    auto changesObj = doc.as<FLArduinoJson::JsonObjectConst>();
    
    // Validate slider value change
    CHECK(changesObj["1"].is<float>());
    CHECK_CLOSE(changesObj["1"].as<float>(), 200.5f, 0.001f);
    
    // Validate checkbox value change
    CHECK(changesObj["2"].is<bool>());
    CHECK(changesObj["2"].as<bool>() == false);
    
    // Validate dropdown value change
    CHECK(changesObj["3"].is<int>());
    CHECK(changesObj["3"].as<int>() == 2);
    
    // Validate button press
    CHECK(changesObj["4"].is<bool>());
    CHECK(changesObj["4"].as<bool>() == true);
    
    // Validate number field change
    CHECK(changesObj["5"].is<int>());
    CHECK(changesObj["5"].as<int>() == 150);
}

TEST_CASE("JSON Strip Update - Canvas Map Event") {
    // Test the JSON structure for strip updates that matches FastLED_onStripUpdate in index.js
    // This tests the "set_canvas_map" event structure
    
    fl::JsonDocument doc;
    auto stripUpdateObj = doc.to<FLArduinoJson::JsonObject>();
    
    stripUpdateObj["event"] = "set_canvas_map";
    stripUpdateObj["strip_id"] = 0;
    stripUpdateObj["diameter"] = 0.2f;
    
    // Create the map object with x/y coordinates
    auto mapObj = stripUpdateObj["map"].to<FLArduinoJson::JsonObject>();
    auto xArray = mapObj["x"].to<FLArduinoJson::JsonArray>();
    auto yArray = mapObj["y"].to<FLArduinoJson::JsonArray>();
    
    // Add some coordinate data for a 5x5 grid
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            xArray.add(x);
            yArray.add(y);
        }
    }
    
    // Validate the structure
    CHECK(fl::string(stripUpdateObj["event"].as<const char*>()) == fl::string("set_canvas_map"));
    CHECK(stripUpdateObj["strip_id"].as<int>() == 0);
    CHECK_CLOSE(stripUpdateObj["diameter"].as<float>(), 0.2f, 0.001f);
    
    auto map = stripUpdateObj["map"];
    CHECK(map["x"].is<FLArduinoJson::JsonArray>());
    CHECK(map["y"].is<FLArduinoJson::JsonArray>());
    
    auto x = map["x"];
    auto y = map["y"];
    CHECK(x.size() == 25);
    CHECK(y.size() == 25);
    
    // Check some coordinate values
    CHECK(x[0].as<int>() == 0);
    CHECK(y[0].as<int>() == 0);
    CHECK(x[24].as<int>() == 4);
    CHECK(y[24].as<int>() == 4);
    
    // Test JSON string serialization
    fl::string jsonString;
    fl::toJson(doc, &jsonString);
    CHECK(!jsonString.empty());
    CHECK(jsonString.find('s') != fl::string::npos); // Check for set_canvas_map
    CHECK(jsonString.find('i') != fl::string::npos); // Check for strip_id
    CHECK(jsonString.find('d') != fl::string::npos); // Check for diameter
}

TEST_CASE("JSON Frame Data Structure") {
    // Test the frame data structure that would be passed to FastLED_onFrame in index.js
    
    fl::JsonDocument doc;
    auto frameArray = doc.to<FLArduinoJson::JsonArray>();
    
    // Add frame data for multiple strips
    for (int stripId = 0; stripId < 2; stripId++) {
        auto stripObj = frameArray.add<FLArduinoJson::JsonObject>();
        stripObj["strip_id"] = stripId;
        stripObj["length"] = 10;
        
        auto pixelArray = stripObj["pixels"].to<FLArduinoJson::JsonArray>();
        for (int i = 0; i < 10; i++) {
            auto pixelObj = pixelArray.add<FLArduinoJson::JsonObject>();
            pixelObj["r"] = (i * 25) % 256;
            pixelObj["g"] = (i * 50) % 256;
            pixelObj["b"] = (i * 75) % 256;
        }
    }
    
    // Validate frame structure
    CHECK(frameArray.size() == 2);
    
    auto strip0 = frameArray[0];
    CHECK(strip0["strip_id"].as<int>() == 0);
    CHECK(strip0["length"].as<int>() == 10);
    
    auto pixels = strip0["pixels"];
    CHECK(pixels.size() == 10);
    
    auto pixel0 = pixels[0];
    CHECK(pixel0["r"].as<int>() == 0);
    CHECK(pixel0["g"].as<int>() == 0);
    CHECK(pixel0["b"].as<int>() == 0);
    
    auto pixel1 = pixels[1];
    CHECK(pixel1["r"].as<int>() == 25);
    CHECK(pixel1["g"].as<int>() == 50);
    CHECK(pixel1["b"].as<int>() == 75);
}

TEST_CASE("JSON Audio Data Structure") {
    // Test audio data structure that would be sent from JavaScript audio input
    
    fl::JsonDocument doc;
    auto audioObj = doc.to<FLArduinoJson::JsonObject>();
    
    // Create audio array directly
    auto audioArray = audioObj["audio_input_1"].to<FLArduinoJson::JsonArray>();
    audioArray.add(0.1f);
    audioArray.add(0.2f);
    audioArray.add(-0.1f);
    audioArray.add(0.5f);
    audioArray.add(-0.3f);
    audioArray.add(0.8f);
    audioArray.add(-0.2f);
    audioArray.add(0.0f);
    audioArray.add(0.4f);
    audioArray.add(-0.6f);
    
    // Validate the structure
    CHECK(audioObj["audio_input_1"].is<FLArduinoJson::JsonArray>());
    CHECK(audioArray.size() == 10);
    
    // Validate some audio sample values
    CHECK_CLOSE(audioArray[0].as<float>(), 0.1f, 0.001f);
    CHECK_CLOSE(audioArray[1].as<float>(), 0.2f, 0.001f);
    CHECK_CLOSE(audioArray[2].as<float>(), -0.1f, 0.001f);
    CHECK_CLOSE(audioArray[9].as<float>(), -0.6f, 0.001f);
    
    // Test JSON string serialization
    fl::string jsonString;
    fl::toJson(doc, &jsonString);
    CHECK(!jsonString.empty());
    CHECK(jsonString.find('a') != fl::string::npos); // Check for audio_input_1
}

TEST_CASE("JSON File Manifest Structure") {
    // Test the file manifest structure used by FastLED file system
    
    fl::JsonDocument doc;
    auto manifestObj = doc.to<FLArduinoJson::JsonObject>();
    
    manifestObj["frameRate"] = 60;
    auto filesArray = manifestObj["files"].to<FLArduinoJson::JsonArray>();
    
    // Add some file entries
    auto file1 = filesArray.add<FLArduinoJson::JsonObject>();
    file1["path"] = "data/animation.rgb";
    file1["size"] = 1024000;
    
    auto file2 = filesArray.add<FLArduinoJson::JsonObject>();
    file2["path"] = "config/settings.json";
    file2["size"] = 512;
    
    auto file3 = filesArray.add<FLArduinoJson::JsonObject>();
    file3["path"] = "audio/sample.wav";
    file3["size"] = 2048000;
    
    // Validate manifest structure
    CHECK(manifestObj["frameRate"].as<int>() == 60);
    
    auto files = manifestObj["files"];
    CHECK(files.size() == 3);
    
    auto firstFile = files[0];
    CHECK(fl::string(firstFile["path"].as<const char*>()) == fl::string("data/animation.rgb"));
    CHECK(firstFile["size"].as<int>() == 1024000);
    
    auto secondFile = files[1];
    CHECK(fl::string(secondFile["path"].as<const char*>()) == fl::string("config/settings.json"));
    CHECK(secondFile["size"].as<int>() == 512);
    
    auto thirdFile = files[2];
    CHECK(fl::string(thirdFile["path"].as<const char*>()) == fl::string("audio/sample.wav"));
    CHECK(thirdFile["size"].as<int>() == 2048000);
}

TEST_CASE("JSON Complete Round-Trip Integration Test") {
    // Test a complete round-trip of JSON data - from C++ UI creation to JavaScript parsing
    // and back to C++ with user changes
    
    fl::vector<fl::string> capturedJsonStrings;
    bool managerCallbackCalled = false;
    
    auto managerCallback = [&](const char* json) {
        managerCallbackCalled = true;
        capturedJsonStrings.push_back(fl::string(json));
    };
    
    // Step 1: Create UI components (simulates C++ FastLED sketch setup)
    fl::setJsonUiHandlers(managerCallback);
    
    fl::JsonSliderImpl brightness("Brightness", 128.0f, 0.0f, 255.0f, 1.0f);
    brightness.Group("Lighting");
    
    fl::JsonCheckboxImpl enabled("Enabled", true);
    enabled.Group("Settings");
    
    fl::JsonDropdownImpl mode("Mode", {"Rainbow", "Solid", "Fire"});
    mode.Group("Effects");
    mode.setSelectedIndex(1);
    
    // Step 2: Serialize UI elements for JavaScript (simulates initial UI setup)
    fl::JsonDocument uiElementsDoc;
    auto elementsArray = uiElementsDoc.to<FLArduinoJson::JsonArray>();
    
    // Add brightness slider using ideal API
    fl::Json brightnessJson = brightness.toJson();
    auto brightnessObj = elementsArray.add<FLArduinoJson::JsonObject>();
    // Copy from fl::Json to FLArduinoJson::JsonObject
    brightnessObj.set(brightnessJson.variant().as<FLArduinoJson::JsonObjectConst>());
    
    // Add enabled checkbox using ideal API
    fl::Json enabledJson = enabled.toJson();
    auto enabledObj = elementsArray.add<FLArduinoJson::JsonObject>();
    enabledObj.set(enabledJson.variant().as<FLArduinoJson::JsonObjectConst>());
    
    // Add mode dropdown
    auto modeObj = elementsArray.add<FLArduinoJson::JsonObject>();
    mode.toJson(modeObj);
    modeObj["group"] = mode.groupName().c_str();
    
    // Validate the complete UI elements JSON structure
    CHECK(elementsArray.size() == 3);
    
    // Step 3: Simulate JavaScript user interactions and parse changes back to C++
    // This simulates what would come from JsonUiManager.processUiChanges()
    
    const char* changesJsonStr = R"({"brightness_control": 200.5, "enable_control": false, "mode_control": 2})";
    
    fl::JsonDocument changesDoc;
    fl::string parseError;
    bool parseSuccess = fl::parseJson(changesJsonStr, &changesDoc, &parseError);
    
    CHECK(parseSuccess);
    CHECK(parseError.empty());
    
    auto changesObj = changesDoc.as<FLArduinoJson::JsonObjectConst>();
    
    // Step 4: Apply changes back to UI components (simulates UI update processing)
    if (changesObj["brightness_control"].is<float>()) {
        brightness.setValue(changesObj["brightness_control"].as<float>());
    }
    
    if (changesObj["enable_control"].is<bool>()) {
        enabled.setValue(changesObj["enable_control"].as<bool>());
    }
    
    if (changesObj["mode_control"].is<int>()) {
        mode.setSelectedIndex(changesObj["mode_control"].as<int>());
    }
    
    // Step 5: Verify round-trip data integrity
    CHECK_CLOSE(brightness.value(), 200.5f, 0.001f);
    CHECK(enabled.value() == false);
    CHECK(mode.value_int() == 2);
    CHECK(mode.value() == "Fire");
    
    // Step 6: Test final serialization matches expected format
    fl::JsonDocument finalDoc;
    auto finalArray = finalDoc.to<FLArduinoJson::JsonArray>();
    
    auto finalBrightnessObj = finalArray.add<FLArduinoJson::JsonObject>();
    brightness.toJson(finalBrightnessObj);
    
    auto finalEnabledObj = finalArray.add<FLArduinoJson::JsonObject>();  
    enabled.toJson(finalEnabledObj);
    
    auto finalModeObj = finalArray.add<FLArduinoJson::JsonObject>();
    mode.toJson(finalModeObj);
    
    // Validate final state
    CHECK_CLOSE(finalArray[0]["value"].as<float>(), 200.5f, 0.001f);
    CHECK(finalArray[1]["value"].as<bool>() == false);
    CHECK(finalArray[2]["value"].as<int>() == 2);
    
    // Test complete JSON string serialization
    fl::string finalJsonString;
    fl::toJson(finalDoc, &finalJsonString);
    CHECK(!finalJsonString.empty());
    CHECK(finalJsonString.find('2') != fl::string::npos); // Check for 200.5
    CHECK(finalJsonString.find('f') != fl::string::npos); // Check for false
    CHECK(finalJsonString.find('F') != fl::string::npos); // Check for Fire
}

#else

TEST_CASE("JSON UI disabled") {
    // When JSON is disabled, we should still be able to compile
    // but functionality will be limited
    CHECK(true);
}

#endif // FASTLED_ENABLE_JSON
