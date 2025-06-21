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
    fl::setJsonUiAddHandler([&](fl::WeakPtr<fl::JsonUiInternal> component) {
        lastAddedComponent = component;
        addCallCount++;
    });
    
    fl::setJsonUiRemoveHandler([&](fl::WeakPtr<fl::JsonUiInternal> component) {
        lastRemovedComponent = component;
        removeCallCount++;
    });
    
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
    fl::setJsonUiAddHandler(fl::JsonUiAddHandler{});
    fl::setJsonUiRemoveHandler(fl::JsonUiRemoveHandler{});
    
    // These should not crash and should produce warnings
    fl::addJsonUiComponent(weakComponent);
    fl::removeJsonUiComponent(weakComponent);
    
    // Counts should remain the same
    CHECK(addCallCount == 1);
    CHECK(removeCallCount == 1);
}
