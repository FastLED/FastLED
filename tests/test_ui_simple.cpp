#include "test.h"
#include "fl/ui.h"
#include "fl/json_console.h"
#include "fl/warn.h"
#include "doctest.h"
#include "platforms/shared/ui/json/slider.h"
#include "platforms/shared/ui/json/ui.h"

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

#if defined(SKETCH_HAS_LOTS_OF_MEMORY) && FASTLED_ENABLE_JSON

TEST_CASE("Simple JsonConsole test") {
    FL_WARN("=== Starting simple JsonConsole test ===");
    
    // Mock callback functions for JsonConsole
    auto mockAvailable = []() -> int { return 0; };
    auto mockRead = []() -> int { return -1; };
    auto mockWrite = [](const char* str) { 
        if (str) {
            FL_WARN("JsonConsole output: " << str);
        }
    };
    
    // Create JsonConsole using smart pointer
    FL_WARN("Creating JsonConsole...");
    fl::JsonConsolePtr console = fl::JsonConsolePtr::New(mockAvailable, mockRead, mockWrite);
    
    // Initialize JsonConsole
    FL_WARN("Initializing JsonConsole...");
    console->init();
    
    // Test basic functionality
    FL_WARN("Testing basic functionality...");
    console->update();
    
    // Test command execution
    FL_WARN("Testing command execution...");
    bool result = console->executeCommand("help");
    CHECK(result == true);
    
    // Test dump functionality
    FL_WARN("Testing dump functionality...");
    fl::sstream dump;
    console->dump(dump);
    fl::string dumpStr = dump.str();
    FL_WARN("Dump output: " << dumpStr.c_str());
    
    // Verify dump contains expected content
    CHECK(dumpStr.find("JsonConsole State Dump") != fl::string::npos);
    CHECK(dumpStr.find("Initialized: true") != fl::string::npos);
    CHECK(dumpStr.find("Component Count: 0") != fl::string::npos);
    
    // Test component mapping update
    FL_WARN("Testing component mapping update...");
    const char* testJson = R"([
        {"id": 1, "name": "test_slider", "type": "slider", "value": 50.0, "min": 0.0, "max": 100.0}
    ])";
    console->updateComponentMapping(testJson);
    
    // Verify component was added to mapping
    fl::sstream dump2;
    console->dump(dump2);
    fl::string dump2Str = dump2.str();
    FL_WARN("Dump after component mapping: " << dump2Str.c_str());
    CHECK(dump2Str.find("Component Count: 1") != fl::string::npos);
    CHECK(dump2Str.find("test_slider") != fl::string::npos);
    
    FL_WARN("=== Simple JsonConsole test completed ===");
}

TEST_CASE("JsonConsole polling system test") {
    FL_WARN("=== Starting JsonConsole polling system test ===");
    
    // Set up the UI system
    bool updateJsCalled = false;
    string lastJsonUpdate;
    
    auto updateJsHandler = [&updateJsCalled, &lastJsonUpdate](const char* jsonStr) {
        updateJsCalled = true;
        lastJsonUpdate = jsonStr ? jsonStr : "";
        FL_WARN("UpdateJS called with: " << lastJsonUpdate);
    };
    
    auto updateEngineState = setJsonUiHandlers(updateJsHandler);
    REQUIRE(updateEngineState);
    
    // Create a slider using the JSON UI implementation directly
    JsonSliderImpl slider("test_slider", 25.0f, 0.0f, 100.0f, 1.0f);
    
    // Initial update should be triggered by component addition
    processJsonUiPendingUpdates();
    
    // Mock callback functions for JsonConsole
    auto mockAvailable = []() -> int { return 0; };
    auto mockRead = []() -> int { return -1; };
    auto mockWrite = [](const char* str) { 
        if (str) {
            FL_WARN("JsonConsole output: " << str);
        }
    };
    
    // Test JsonConsole integration with smart pointer
    auto console = fl::JsonConsolePtr::New(mockAvailable, mockRead, mockWrite);
    console->init();
    
    // Trigger the UI system to send component information to JsonConsole
    // This simulates what normally happens in onEndShowLeds()
    processJsonUiPendingUpdates(); // Make sure components are registered
    
    // Generate the component JSON based on the actual JsonSliderImpl component
    // Use the slider's toJson method to get the correct component information
    FLArduinoJson::JsonDocument componentDoc;
    auto componentObj = componentDoc.to<FLArduinoJson::JsonObject>();
    slider.toJson(componentObj);
    
    // Create array containing the component
    FLArduinoJson::JsonDocument arrayDoc;
    auto componentArray = arrayDoc.to<FLArduinoJson::JsonArray>();
    componentArray.add(componentObj);
    
    fl::string componentJson;
    serializeJson(arrayDoc, componentJson);
    
    // Call the processJsonFromUI method directly to simulate what the UI system should do
    console->processJsonFromUI(componentJson.c_str());
    
    FL_WARN("Testing JsonConsole executeCommand...");
    bool result = console->executeCommand("test_slider: 75");
    CHECK(result == true);
    
    // Verify the slider value was updated
    CHECK(slider.value() == 75.0f);
    
    FL_WARN("=== JsonConsole polling system test completed ===");
}

#endif // SKETCH_HAS_LOTS_OF_MEMORY
