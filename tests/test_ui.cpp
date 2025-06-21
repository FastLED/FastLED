// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/ui.h"
#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"

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
