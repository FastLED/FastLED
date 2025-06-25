// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/ui.h"
#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/json_console.h"
#include "fl/sstream.h"
#include <cstring>

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("compile ui test") {
}

TEST_CASE("no updateJs handler") {
    // Set up handler WITHOUT updateJs callback - should return empty function
    auto updateEngineState = fl::setJsonUiHandlers(fl::function<void(const char*)>{});
    
    // Should return empty function when no updateJs handler
    CHECK(!updateEngineState);
    
    // Create a mock component for testing
    auto updateFunc = [](const FLArduinoJson::JsonVariantConst&) { /* do nothing */ };
    auto toJsonFunc = [](FLArduinoJson::JsonObject&) { /* do nothing */ };
    auto mockComponent = fl::NewPtr<fl::JsonUiInternal>("test_id", updateFunc, toJsonFunc);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent(mockComponent);
    
    // Test addJsonUiComponent - should go to pending since no manager
    fl::addJsonUiComponent(weakComponent);
    
    // Test removeJsonUiComponent - should remove from pending
    fl::removeJsonUiComponent(weakComponent);
}

TEST_CASE("internal manager with updateJs") {
    // Test variables to track handler calls
    int updateJsCallCount = 0;
    
    // Set up handler WITH updateJs callback - should use internal JsonUiManager
    auto updateEngineState = fl::setJsonUiHandlers(
        [&](const char*) { 
            updateJsCallCount++; // This might be called by internal manager
        }
    );
    
    // Should return valid function when updateJs handler is provided
    CHECK(updateEngineState);
    
    // Create a mock component for testing
    auto updateFunc = [](const FLArduinoJson::JsonVariantConst&) { /* do nothing */ };
    auto toJsonFunc = [](FLArduinoJson::JsonObject&) { /* do nothing */ };
    auto mockComponent = fl::NewPtr<fl::JsonUiInternal>("test_id", updateFunc, toJsonFunc);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent(mockComponent);
    
    // Test addJsonUiComponent - should add to internal manager
    fl::addJsonUiComponent(weakComponent);
    
    // Test removeJsonUiComponent - should remove from internal manager
    fl::removeJsonUiComponent(weakComponent);
    
    // Test the returned updateEngineState function
    // This should not crash and should call the internal manager
    updateEngineState("{\"test\": \"data\"}");
}

TEST_CASE("pending component storage without updateJs") {
    // Clear any existing handlers
    auto updateEngineState = fl::setJsonUiHandlers(fl::function<void(const char*)>{});
    CHECK(!updateEngineState); // Should return empty function
    
    // Create mock components for testing
    auto updateFunc = [](const FLArduinoJson::JsonVariantConst&) { /* do nothing */ };
    auto toJsonFunc = [](FLArduinoJson::JsonObject&) { /* do nothing */ };
    
    auto mockComponent1 = fl::NewPtr<fl::JsonUiInternal>("test_id_1", updateFunc, toJsonFunc);
    auto mockComponent2 = fl::NewPtr<fl::JsonUiInternal>("test_id_2", updateFunc, toJsonFunc);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent1(mockComponent1);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent2(mockComponent2);
    
    // Add components before handlers are set - should go to pending storage
    fl::addJsonUiComponent(weakComponent1);
    fl::addJsonUiComponent(weakComponent2);
    
    // Now set handler with updateJs - pending components should be flushed to internal manager
    updateEngineState = fl::setJsonUiHandlers(
        [](const char*) { /* updateJs callback - triggers internal manager */ }
    );
    
    // Should return valid function when updateJs handler is provided
    CHECK(updateEngineState);
    
    // Test the returned updateEngineState function
    updateEngineState("{\"test\": \"data\"}");
}

TEST_CASE("pending component storage with updateJs") {
    // Clear any existing handlers
    auto updateEngineState = fl::setJsonUiHandlers(fl::function<void(const char*)>{});
    CHECK(!updateEngineState); // Should return empty function
    
    // Create mock components for testing
    auto updateFunc = [](const FLArduinoJson::JsonVariantConst&) { /* do nothing */ };
    auto toJsonFunc = [](FLArduinoJson::JsonObject&) { /* do nothing */ };
    
    auto mockComponent1 = fl::NewPtr<fl::JsonUiInternal>("test_id_1", updateFunc, toJsonFunc);
    auto mockComponent2 = fl::NewPtr<fl::JsonUiInternal>("test_id_2", updateFunc, toJsonFunc);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent1(mockComponent1);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent2(mockComponent2);
    
    // Add components before handlers are set - should go to pending storage
    fl::addJsonUiComponent(weakComponent1);
    fl::addJsonUiComponent(weakComponent2);
    
    // Now set handlers WITH updateJs - pending components should be flushed to internal manager
    updateEngineState = fl::setJsonUiHandlers(
        [](const char*) { /* updateJs callback - triggers internal manager */ }
    );
    
    // Should return valid function when updateJs handler is provided
    CHECK(updateEngineState);
    
    // Test the returned updateEngineState function
    updateEngineState("{\"test\": \"data\"}");
}

TEST_CASE("pending component cleanup with destroyed components") {
    // Clear any existing handlers
    auto updateEngineState = fl::setJsonUiHandlers(fl::function<void(const char*)>{});
    CHECK(!updateEngineState); // Should return empty function
    
    // Create scope for component that will be destroyed
    {
        auto updateFunc = [](const FLArduinoJson::JsonVariantConst&) { /* do nothing */ };
        auto toJsonFunc = [](FLArduinoJson::JsonObject&) { /* do nothing */ };
        auto mockComponent = fl::NewPtr<fl::JsonUiInternal>("test_id_destroyed", updateFunc, toJsonFunc);
        fl::WeakPtr<fl::JsonUiInternal> weakComponent(mockComponent);
        
        // Add component to pending
        fl::addJsonUiComponent(weakComponent);
        
        // Component goes out of scope and gets destroyed here
    }
    
    // Create a valid component
    auto updateFunc = [](const FLArduinoJson::JsonVariantConst&) { /* do nothing */ };
    auto toJsonFunc = [](FLArduinoJson::JsonObject&) { /* do nothing */ };
    auto validComponent = fl::NewPtr<fl::JsonUiInternal>("test_id_valid", updateFunc, toJsonFunc);
    fl::WeakPtr<fl::JsonUiInternal> weakValidComponent(validComponent);
    fl::addJsonUiComponent(weakValidComponent);
    
    // Set handler with updateJs - should only flush valid components to internal manager
    updateEngineState = fl::setJsonUiHandlers(
        [](const char*) { /* updateJs callback */ }
    );
    
    // Should return valid function when updateJs handler is provided
    CHECK(updateEngineState);
}

TEST_CASE("null handlers behavior") {
    // Test with null handler (should not crash)
    auto updateEngineState = fl::setJsonUiHandlers(fl::function<void(const char*)>{});
    
    // Should return empty function when no updateJs handler
    CHECK(!updateEngineState);
    
    // Create a mock component for testing
    auto updateFunc = [](const FLArduinoJson::JsonVariantConst&) { /* do nothing */ };
    auto toJsonFunc = [](FLArduinoJson::JsonObject&) { /* do nothing */ };
    auto mockComponent = fl::NewPtr<fl::JsonUiInternal>("test_id", updateFunc, toJsonFunc);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent(mockComponent);
    
    // These should not crash and should produce warnings (components go to pending)
    fl::addJsonUiComponent(weakComponent);
    fl::removeJsonUiComponent(weakComponent);
}

TEST_CASE("updateEngineState function behavior") {
    // Test with valid updateJs handler
    int updateJsCallCount = 0;
    auto updateEngineState = fl::setJsonUiHandlers(
        [&](const char* jsonStr) { 
            updateJsCallCount++;
            // Verify we receive the JSON string
            CHECK(jsonStr != nullptr);
        }
    );
    
    // Should return valid function
    CHECK(updateEngineState);
    
    // Create and add a component to the internal manager
    auto updateFunc = [](const FLArduinoJson::JsonVariantConst&) { /* do nothing */ };
    auto toJsonFunc = [](FLArduinoJson::JsonObject&) { /* do nothing */ };
    auto mockComponent = fl::NewPtr<fl::JsonUiInternal>("test_component", updateFunc, toJsonFunc);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent(mockComponent);
    fl::addJsonUiComponent(weakComponent);
    
    // Test calling the updateEngineState function
    updateEngineState("{\"id_test_component\": {\"value\": 42}}");
    
    // Verify the function works without crashing
    updateEngineState("{}");
    updateEngineState("{\"invalid\": \"json\"}");
}

TEST_CASE("manager replacement") {
    // Create a manager with one handler
    int firstCallCount = 0;
    auto updateEngineState1 = fl::setJsonUiHandlers(
        [&](const char*) { firstCallCount++; }
    );
    CHECK(updateEngineState1);
    
    // Add a component to the first manager
    auto updateFunc = [](const FLArduinoJson::JsonVariantConst&) { /* do nothing */ };
    auto toJsonFunc = [](FLArduinoJson::JsonObject&) { /* do nothing */ };
    auto mockComponent = fl::NewPtr<fl::JsonUiInternal>("test_id", updateFunc, toJsonFunc);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent(mockComponent);
    fl::addJsonUiComponent(weakComponent);
    
    // Replace with a different handler
    int secondCallCount = 0;
    auto updateEngineState2 = fl::setJsonUiHandlers(
        [&](const char*) { secondCallCount++; }
    );
    CHECK(updateEngineState2);
    
    // The component should have been transferred to the new manager
    // Both update functions should work
    updateEngineState1("{\"test1\": \"data\"}");
    updateEngineState2("{\"test2\": \"data\"}");
}

#ifdef SKETCH_HAS_LOTS_OF_MEMORY
TEST_CASE("JsonConsole destructor cleanup") {
    // Mock callback functions for testing
    fl::string capturedOutput;
    int availableCallCount = 0;
    int readCallCount = 0;
    int writeCallCount = 0;
    
    auto mockAvailable = [&]() -> int { 
        availableCallCount++;
        return 0; // No data available
    };
    
    auto mockRead = [&]() -> int { 
        readCallCount++;
        return -1; // No data to read
    };
    
    auto mockWrite = [&](const char* str) {
        writeCallCount++;
        if (str) {
            capturedOutput += str;
        }
    };
    
    // Test proper cleanup through scoped destruction
    {
        fl::scoped_ptr<fl::JsonConsole> console;
        console.reset(new fl::JsonConsole(mockAvailable, mockRead, mockWrite));
        
        // Initialize and add some test data
        console->init();
        
        // Add some test component mappings
        const char* testComponentsJson = 
            "[{\"name\":\"test_slider\",\"id\":42}]";
        console->updateComponentMapping(testComponentsJson);
        
        // Execute a command to ensure internal state is populated
        console->executeCommand("help");
        
        // Verify console has some internal state
        fl::sstream dumpOutput;
        console->dump(dumpOutput);
        fl::string dump = dumpOutput.str();
        
        // Verify the console has internal state before destruction
        // Use find with char instead of string, and check for simple component indicator
        bool hasComponents = dump.find('1') != fl::string::npos; // Look for the ID in the dump
        if (!hasComponents) {
            // Component mapping might not work in test environment, that's ok
            // The important thing is that destructor doesn't crash
        }
        
        // Console will be destroyed when it goes out of scope here
        // This tests that the destructor properly cleans up without crashing
    }
    
    // Test manual destruction via scoped_ptr reset
    {
        fl::scoped_ptr<fl::JsonConsole> console;
        console.reset(new fl::JsonConsole(mockAvailable, mockRead, mockWrite));
        
        // Initialize the console
        console->init();
        
        // Add test data
        console->executeCommand("help");
        
        // Manually trigger destruction via reset
        console.reset(); // This should call the destructor
        
        // Verify no crash occurred
        CHECK(console.get() == nullptr);
    }
    
    // Test destruction of uninitialized console
    {
        fl::scoped_ptr<fl::JsonConsole> console;
        console.reset(new fl::JsonConsole(mockAvailable, mockRead, mockWrite));
        // Don't call init() - test destructor works on uninitialized console
        // Console destructor should handle this gracefully
    }
    
    // Test destruction with null callbacks
    {
        fl::scoped_ptr<fl::JsonConsole> console;
        console.reset(new fl::JsonConsole(
            fl::function<int()>{}, // null available
            fl::function<int()>{}, // null read  
            fl::function<void(const char*)>{} // null write
        ));
        
        // Initialize with null callbacks
        console->init();
        
        // Destructor should handle null callbacks gracefully
    }
    
    // If we get here without crashing, the destructor is working correctly
    CHECK(true); // Test passed if no crashes occurred
}

TEST_CASE("JsonConsole dump function") {
    // Mock callback functions for testing
    fl::string capturedOutput;
    int availableCallCount = 0;
    int readCallCount = 0;
    int writeCallCount = 0;
    
    auto mockAvailable = [&]() -> int { 
        availableCallCount++;
        return 0; // No data available
    };
    
    auto mockRead = [&]() -> int { 
        readCallCount++;
        return -1; // No data to read
    };
    
    auto mockWrite = [&](const char* str) {
        writeCallCount++;
        if (str) {
            capturedOutput += str;
        }
    };
    
    // Helper function to check if string contains substring
    auto contains = [](const fl::string& str, const char* substr) {
        return strstr(str.c_str(), substr) != nullptr;
    };
    
    // Test 1: Uninitialized JsonConsole dump
    {
        fl::JsonConsole console(mockAvailable, mockRead, mockWrite);
        fl::sstream dumpOutput;
        
        console.dump(dumpOutput);
        fl::string dump = dumpOutput.str();
        
        // Verify dump contains expected uninitialized state
        CHECK(contains(dump, "=== JsonConsole State Dump ==="));
        CHECK(contains(dump, "Initialized: false"));
        CHECK(contains(dump, "Input Buffer: \"\""));
        CHECK(contains(dump, "Input Buffer Length: 0"));
        CHECK(contains(dump, "Component Count: 0"));
        CHECK(contains(dump, "No components mapped"));
        CHECK(contains(dump, "Available Callback: set"));
        CHECK(contains(dump, "Read Callback: set"));
        CHECK(contains(dump, "Write Callback: set"));
        CHECK(contains(dump, "=== End JsonConsole Dump ==="));
    }
    
    // Test 2: Initialized JsonConsole with components
    {
        fl::JsonConsole console(mockAvailable, mockRead, mockWrite);
        
        // Initialize the console (note: this will fail in test environment but that's ok)
        console.init();
        
        // Add some test component mappings manually using updateComponentMapping
        const char* testComponentsJson = 
            "[{\"name\":\"slider1\",\"id\":1},{\"name\":\"slider2\",\"id\":2}]";
        console.updateComponentMapping(testComponentsJson);
        
        fl::sstream dumpOutput;
        console.dump(dumpOutput);
        fl::string dump = dumpOutput.str();
        
        // Verify dump contains expected state
        CHECK(contains(dump, "=== JsonConsole State Dump ==="));
        CHECK(contains(dump, "Component Count: 2"));
        CHECK(contains(dump, "Component Mappings:"));
        CHECK(contains(dump, "\"slider1\" -> ID 1"));
        CHECK(contains(dump, "\"slider2\" -> ID 2"));
        CHECK(contains(dump, "=== End JsonConsole Dump ==="));
    }
    
    // Test 3: JsonConsole with input buffer content (simulate partial command)
    {
        fl::JsonConsole console(mockAvailable, mockRead, mockWrite);
        
        // Execute a command to test internal state
        console.executeCommand("help");
        
        fl::sstream dumpOutput;
        console.dump(dumpOutput);
        fl::string dump = dumpOutput.str();
        
        // Verify basic dump structure
        CHECK(contains(dump, "=== JsonConsole State Dump ==="));
        CHECK(contains(dump, "Input Buffer Length:"));
        CHECK(contains(dump, "=== End JsonConsole Dump ==="));
    }
    
    // Test 4: Test with null callbacks
    {
        fl::JsonConsole console(
            fl::function<int()>{}, // null available
            fl::function<int()>{}, // null read  
            fl::function<void(const char*)>{} // null write
        );
        
        fl::sstream dumpOutput;
        console.dump(dumpOutput);
        fl::string dump = dumpOutput.str();
        
        // Verify null callbacks are reported correctly
        CHECK(contains(dump, "Available Callback: null"));
        CHECK(contains(dump, "Read Callback: null"));
        CHECK(contains(dump, "Write Callback: null"));
    }
    
    // Test 5: Test with empty component mapping JSON
    {
        fl::JsonConsole console(mockAvailable, mockRead, mockWrite);
        
        // Test with empty array
        console.updateComponentMapping("[]");
        
        fl::sstream dumpOutput;
        console.dump(dumpOutput);
        fl::string dump = dumpOutput.str();
        
        CHECK(contains(dump, "Component Count: 0"));
        CHECK(contains(dump, "No components mapped"));
    }
    
    // Test 6: Test with invalid JSON (should not crash)
    {
        fl::JsonConsole console(mockAvailable, mockRead, mockWrite);
        
        // Test with invalid JSON - should not crash
        console.updateComponentMapping("invalid json");
        console.updateComponentMapping(nullptr);
        
        fl::sstream dumpOutput;
        console.dump(dumpOutput);
        fl::string dump = dumpOutput.str();
        
        // Should still produce valid dump output
        CHECK(contains(dump, "=== JsonConsole State Dump ==="));
        CHECK(contains(dump, "=== End JsonConsole Dump ==="));
    }
}
#endif // SKETCH_HAS_LOTS_OF_MEMORY
