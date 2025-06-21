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

TEST_CASE("external handlers without updateJs") {
    // Test variables to track handler calls
    fl::WeakPtr<fl::JsonUiInternal> lastAddedComponent;
    fl::WeakPtr<fl::JsonUiInternal> lastRemovedComponent;
    int addCallCount = 0;
    int removeCallCount = 0;
    
    // Set up handlers WITHOUT updateJs callback - should use external handlers
    auto updateEngineState = fl::setJsonUiHandlers(
        [&](fl::WeakPtr<fl::JsonUiInternal> component) {
            lastAddedComponent = component;
            addCallCount++;
        },
        [&](fl::WeakPtr<fl::JsonUiInternal> component) {
            lastRemovedComponent = component;
            removeCallCount++;
        },
        fl::JsonUiUpdateJsHandler{} // Empty updateJs handler
    );
    
    // Should return empty function when no updateJs handler
    CHECK(!updateEngineState);
    
    // Create a mock component for testing
    auto updateFunc = [](const FLArduinoJson::JsonVariantConst&) { /* do nothing */ };
    auto toJsonFunc = [](FLArduinoJson::JsonObject&) { /* do nothing */ };
    auto mockComponent = fl::NewPtr<fl::JsonUiInternal>("test_id", updateFunc, toJsonFunc);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent(mockComponent);
    
    // Test addJsonUiComponent - should call external handler
    fl::addJsonUiComponent(weakComponent);
    CHECK(addCallCount == 1);
    CHECK(lastAddedComponent == weakComponent);
    
    // Test removeJsonUiComponent - should call external handler
    fl::removeJsonUiComponent(weakComponent);
    CHECK(removeCallCount == 1);
    CHECK(lastRemovedComponent == weakComponent);
}

TEST_CASE("internal manager with updateJs") {
    // Test variables to track handler calls
    int addCallCount = 0;
    int removeCallCount = 0;
    int updateJsCallCount = 0;
    
    // Set up handlers WITH updateJs callback - should use internal JsonUiManager, not external handlers
    auto updateEngineState = fl::setJsonUiHandlers(
        [&](fl::WeakPtr<fl::JsonUiInternal>) {
            addCallCount++; // This should NOT be called
        },
        [&](fl::WeakPtr<fl::JsonUiInternal>) {
            removeCallCount++; // This should NOT be called
        },
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
    
    // Test addJsonUiComponent - should NOT call external add handler since internal manager is used
    fl::addJsonUiComponent(weakComponent);
    CHECK(addCallCount == 0); // External handler should not be called
    
    // Test removeJsonUiComponent - should NOT call external remove handler since internal manager is used
    fl::removeJsonUiComponent(weakComponent);
    CHECK(removeCallCount == 0); // External handler should not be called
    
    // Test the returned updateEngineState function
    // This should not crash and should call the internal manager
    updateEngineState("{\"test\": \"data\"}");
}

TEST_CASE("pending component storage without updateJs") {
    // Clear any existing handlers
    auto updateEngineState = fl::setJsonUiHandlers(fl::JsonUiAddHandler{}, fl::JsonUiRemoveHandler{}, fl::JsonUiUpdateJsHandler{});
    CHECK(!updateEngineState); // Should return empty function
    
    // Test variables to track handler calls
    fl::WeakPtr<fl::JsonUiInternal> lastAddedComponent;
    int addCallCount = 0;
    
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
    
    // Now set external handlers without updateJs - pending components should be flushed to external handler
    updateEngineState = fl::setJsonUiHandlers(
        [&](fl::WeakPtr<fl::JsonUiInternal> component) {
            lastAddedComponent = component;
            addCallCount++;
        },
        [](fl::WeakPtr<fl::JsonUiInternal>) { /* do nothing */ },
        fl::JsonUiUpdateJsHandler{} // Empty updateJs handler
    );
    
    // Should return empty function when no updateJs handler
    CHECK(!updateEngineState);
    
    // Should have received 2 add calls from flushing pending components to external handler
    CHECK(addCallCount == 2);
}

TEST_CASE("pending component storage with updateJs") {
    // Clear any existing handlers
    auto updateEngineState = fl::setJsonUiHandlers(fl::JsonUiAddHandler{}, fl::JsonUiRemoveHandler{}, fl::JsonUiUpdateJsHandler{});
    CHECK(!updateEngineState); // Should return empty function
    
    // Test variables to track handler calls
    int addCallCount = 0;
    
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
    
    // Now set handlers WITH updateJs - pending components should be flushed to internal manager, NOT external handler
    updateEngineState = fl::setJsonUiHandlers(
        [&](fl::WeakPtr<fl::JsonUiInternal>) {
            addCallCount++; // This should NOT be called
        },
        [](fl::WeakPtr<fl::JsonUiInternal>) { /* do nothing */ },
        [](const char*) { /* updateJs callback - triggers internal manager */ }
    );
    
    // Should return valid function when updateJs handler is provided
    CHECK(updateEngineState);
    
    // Should NOT have received any add calls since internal manager handles it
    CHECK(addCallCount == 0);
    
    // Test the returned updateEngineState function
    updateEngineState("{\"test\": \"data\"}");
}

TEST_CASE("pending component cleanup with destroyed components") {
    // Clear any existing handlers
    auto updateEngineState = fl::setJsonUiHandlers(fl::JsonUiAddHandler{}, fl::JsonUiRemoveHandler{}, fl::JsonUiUpdateJsHandler{});
    CHECK(!updateEngineState); // Should return empty function
    
    int addCallCount = 0;
    
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
    
    // Set handlers without updateJs - should only flush valid components to external handler
    updateEngineState = fl::setJsonUiHandlers(
        [&](fl::WeakPtr<fl::JsonUiInternal> component) {
            // Only valid components should reach here
            CHECK(component.operator bool()); // Should be valid
            addCallCount++;
        },
        [](fl::WeakPtr<fl::JsonUiInternal>) { /* do nothing */ },
        fl::JsonUiUpdateJsHandler{} // Empty updateJs handler
    );
    
    // Should return empty function when no updateJs handler
    CHECK(!updateEngineState);
    
    // Should only receive 1 add call for the valid component
    CHECK(addCallCount == 1);
}

TEST_CASE("null handlers behavior") {
    // Test with null handlers (should not crash)
    auto updateEngineState = fl::setJsonUiHandlers(fl::JsonUiAddHandler{}, fl::JsonUiRemoveHandler{}, fl::JsonUiUpdateJsHandler{});
    
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
        [](fl::WeakPtr<fl::JsonUiInternal>) { /* do nothing */ },
        [](fl::WeakPtr<fl::JsonUiInternal>) { /* do nothing */ },
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
