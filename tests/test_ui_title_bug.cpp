// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/ui.h"
#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "platforms/shared/ui/json/number_field.h"
#include "platforms/shared/ui/json/dropdown.h"
#include "platforms/shared/ui/json/title.h"
#include "platforms/shared/ui/json/description.h"
#include "platforms/shared/ui/json/audio.h"
#include "platforms/shared/ui/json/help.h"
#include "platforms/shared/ui/json/button.h"
#include "platforms/shared/ui/json/slider.h"
#include "platforms/shared/ui/json/checkbox.h"
#include "platforms/shared/ui/json/json_console.h"
#include "fl/sstream.h"
#include <cstring>
#include "fl/json.h"

#include "fl/ui.h"

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE



TEST_CASE("UI Bug - Memory Corruption") {
    // This test simulates the conditions that might lead to memory corruption
    // in the UI system, particularly when components are destroyed while 
    // the JsonUiManager still holds references to them.
    
    // Set up a handler to capture UI updates
    fl::string capturedJsonOutput;
    auto updateEngineState = fl::setJsonUiHandlers(
        [&](const char* jsonStr) {
            if (jsonStr) {
                capturedJsonOutput = jsonStr;
            }
        }
    );
    CHECK(updateEngineState);
    
    // Create UI components - these will be automatically registered
    // with the JsonUiManager through their constructors
    {
        // Create components in a scope to test proper destruction
        fl::UITitle title("Simple control of an xy path");
        fl::UIDescription description("This is more of a test for new features.");
        fl::UISlider offset("Offset", 0.0f, 0.0f, 1.0f, 0.01f);
        fl::UISlider steps("Steps", 100.0f, 1.0f, 200.0f, 1.0f);
        fl::UISlider length("Length", 1.0f, 0.0f, 1.0f, 0.01f);
        
        // Process pending updates to serialize the components
        fl::processJsonUiPendingUpdates();
        
        // Verify that the components were properly serialized
        CHECK(!capturedJsonOutput.empty());
        
        // Simulate an update from the UI side
        const char* updateJson = R"({
            "Offset": 0.5,
            "Steps": 150.0,
            "Length": 0.75
        })";
        
        // Process the update - this should update the component values
        updateEngineState(updateJson);
        
        // Process pending updates again to verify everything still works
        fl::processJsonUiPendingUpdates();
    } // Components go out of scope here and should be properly destroyed
    
    // // Create new components to verify the system is still functional
    // fl::JsonTitleImpl newTitle("NewTitle", "New UI component");
    // fl::processJsonUiPendingUpdates();
    
    // // Verify that the new component is properly serialized
    // CHECK(!capturedJsonOutput.empty());
    // CHECK(capturedJsonOutput.find("NewTitle") != fl::string::npos);
    
    // // Test component destruction with pending updates
    // {
    //     fl::JsonSliderImpl tempSlider("TempSlider", 0.5f, 0.0f, 1.0f, 0.01f);
    //     fl::processJsonUiPendingUpdates();
        
    //     // Send an update for the component that's about to be destroyed
    //     const char* updateJson = R"({"TempSlider": 0.8})";
    //     updateEngineState(updateJson);
        
    //     // Component goes out of scope here - should be properly cleaned up
    //     // even with pending updates
    // }
    
    // // Verify that the system is still functional after component destruction
    // fl::processJsonUiPendingUpdates();
    
    // // Test with null update handler to verify cleanup
    // auto nullUpdateEngineState = fl::setJsonUiHandlers(fl::function<void(const char*)>{});
    // CHECK(!nullUpdateEngineState);
    
    // // Reinstall handler
    // updateEngineState = fl::setJsonUiHandlers(
    //     [&](const char* jsonStr) {
    //         if (jsonStr) {
    //             capturedJsonOutput = jsonStr;
    //         }
    //     }
    // );
    // CHECK(updateEngineState);
    
    // // Final verification that the system works correctly
    // fl::JsonDescriptionImpl finalDescription("Final verification description");
    // fl::processJsonUiPendingUpdates();
    // CHECK(!capturedJsonOutput.empty());
    // CHECK(capturedJsonOutput.find("Final verification description") != fl::string::npos);
}
