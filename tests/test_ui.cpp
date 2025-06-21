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

TEST_CASE("function handlers") {
    // Test variables to track handler calls
    fl::WeakPtr<fl::JsonUiInternal> lastAddedComponent;
    fl::WeakPtr<fl::JsonUiInternal> lastRemovedComponent;
    int addCallCount = 0;
    int removeCallCount = 0;
    
    // Set up handlers that track calls
    fl::setJsonUiHandlers(
        [&](fl::WeakPtr<fl::JsonUiInternal> component) {
            lastAddedComponent = component;
            addCallCount++;
        },
        [&](fl::WeakPtr<fl::JsonUiInternal> component) {
            lastRemovedComponent = component;
            removeCallCount++;
        }
    );
    
    // Create a mock component for testing
    auto updateFunc = [](const FLArduinoJson::JsonVariantConst&) { /* do nothing */ };
    auto toJsonFunc = [](FLArduinoJson::JsonObject&) { /* do nothing */ };
    auto mockComponent = fl::NewPtr<fl::JsonUiInternal>("test_id", updateFunc, toJsonFunc);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent(mockComponent);
    
    // Test addJsonUiComponent
    fl::addJsonUiComponent(weakComponent);
    CHECK(addCallCount == 1);
    CHECK(lastAddedComponent == weakComponent);
    
    // Test removeJsonUiComponent
    fl::removeJsonUiComponent(weakComponent);
    CHECK(removeCallCount == 1);
    CHECK(lastRemovedComponent == weakComponent);
    
    // Test with null handlers (should not crash)
    fl::setJsonUiHandlers(fl::JsonUiAddHandler{}, fl::JsonUiRemoveHandler{});
    
    // These should not crash and should produce warnings
    fl::addJsonUiComponent(weakComponent);
    fl::removeJsonUiComponent(weakComponent);
    
    // Counts should remain the same
    CHECK(addCallCount == 1);
    CHECK(removeCallCount == 1);
}

TEST_CASE("pending component storage") {
    // Clear any existing handlers
    fl::setJsonUiHandlers(fl::JsonUiAddHandler{}, fl::JsonUiRemoveHandler{});
    
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
    
    // Now set handlers - pending components should be flushed
    fl::setJsonUiHandlers(
        [&](fl::WeakPtr<fl::JsonUiInternal> component) {
            lastAddedComponent = component;
            addCallCount++;
        },
        [](fl::WeakPtr<fl::JsonUiInternal>) { /* do nothing */ }
    );
    
    // Should have received 2 add calls from flushing pending components
    CHECK(addCallCount == 2);
    
    // Test removing component from pending storage before handlers are set
    fl::setJsonUiHandlers(fl::JsonUiAddHandler{}, fl::JsonUiRemoveHandler{});
    
    auto mockComponent3 = fl::NewPtr<fl::JsonUiInternal>("test_id_3", updateFunc, toJsonFunc);
    fl::WeakPtr<fl::JsonUiInternal> weakComponent3(mockComponent3);
    
    // Add component to pending
    fl::addJsonUiComponent(weakComponent3);
    
    // Remove it from pending (should work without handlers set)
    fl::removeJsonUiComponent(weakComponent3);
    
    // Set handlers again - should not receive any calls since component was removed from pending
    int finalAddCallCount = 0;
    fl::setJsonUiHandlers(
        [&](fl::WeakPtr<fl::JsonUiInternal>) {
            finalAddCallCount++;
        },
        [](fl::WeakPtr<fl::JsonUiInternal>) { /* do nothing */ }
    );
    
    // Should receive no add calls since the pending component was removed
    CHECK(finalAddCallCount == 0);
}

TEST_CASE("pending component cleanup with destroyed components") {
    // Clear any existing handlers
    fl::setJsonUiHandlers(fl::JsonUiAddHandler{}, fl::JsonUiRemoveHandler{});
    
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
    
    // Set handlers - should only flush valid components
    fl::setJsonUiHandlers(
        [&](fl::WeakPtr<fl::JsonUiInternal> component) {
            // Only valid components should reach here
            CHECK(component.operator bool()); // Should be valid
            addCallCount++;
        },
        [](fl::WeakPtr<fl::JsonUiInternal>) { /* do nothing */ }
    );
    
    // Should only receive 1 add call for the valid component
    CHECK(addCallCount == 1);
}
