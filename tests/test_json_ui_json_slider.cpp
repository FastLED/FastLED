#include "test.h"

#include "FastLED.h"



#include "fl/json.h"
#include "fl/namespace.h"
#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/slider.h"

#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/ui_manager.h"
#include "platforms/shared/ui/json/checkbox.h"
#include "platforms/shared/ui/json/dropdown.h"
#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/title.h"
#include "platforms/shared/ui/json/description.h"
#include "platforms/shared/ui/json/number_field.h"


TEST_CASE("Sanity check") {
    static_assert(FASTLED_ENABLE_JSON==1, "FASTLED_ENABLE_JSON must be defined during testing");
}


TEST_CASE("Test slider component") {
    // Create a callback to capture JSON updates that would be sent to JavaScript
    fl::vector<fl::string> capturedJsonStrings;
    auto jsCallback = [&capturedJsonStrings](const char* json) {
        capturedJsonStrings.push_back(fl::string(json));
        FL_WARN("Captured JSON: " << json);  // Print the captured JSON
    };
    
    // Set up the UI handler with our callback
    auto updateEngineState = fl::setJsonUiHandlers(jsCallback);
    
    // Create a slider with range 0-255, step 1, starting at 0
    fl::UISlider slider("test_slider", 0.0f, 0.0f, 255.0f, 1.0f);
    
    // Verify initial state
    CHECK_CLOSE(slider.value(), 0.0f, 0.001f);
    CHECK_CLOSE(slider.getMin(), 0.0f, 0.001f);
    CHECK_CLOSE(slider.getMax(), 255.0f, 0.001f);
    
    // Send a JSON update to the slider (simulating JavaScript UI interaction)
    // We'll use the ID from the captured JSON strings
    const char* updateJson = R"({"0": 128})";  // Using the slider's internal ID.
    updateEngineState(updateJson);
    
    // Process any pending UI updates (normally done by engine loop)
    fl::processJsonUiPendingUpdates();

    CHECK_EQ(capturedJsonStrings.size(), 1);
    fl::string expected_serialization = "[{\"name\":\"test_slider\",\"group\":\"\",\"type\":\"slider\",\"id\":0,\"value\":128,\"min\":0,\"max\":255,\"step\":1}]";
    CHECK_EQ(capturedJsonStrings[0], expected_serialization);
    
    // Verify the slider value was updated
    CHECK_CLOSE(slider.value(), 128.0f, 0.001f);

    // Now do it again but this time use the name instead of the id
    updateJson = R"({"test_slider": 255})";
    updateEngineState(updateJson);
    fl::processJsonUiPendingUpdates();
    //CHECK_EQ(capturedJsonStrings.size(), 2);
    //CHECK_EQ(capturedJsonStrings[1], expected_serialization);
    CHECK_CLOSE(slider.value(), 255.0f, 0.001f);
    
}
