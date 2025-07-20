#pragma once

#include "fl/function.h"

namespace fl {

template<typename T>
class weak_ptr;
// Forward declare the JsonUiInternal class.
class JsonUiInternal;
class JsonUiManager;

// From engine -> UI
using JsonUiUpdateOutput = fl::function<void(const char*)>;

// from UI -> engine
using JsonUiUpdateInput = fl::function<void(const char*)>;

/**
 * Set the global handler for sending UI updates to JavaScript.
 * 
 * @param updateJsHandler Function to call when UI needs to send updates to JavaScript
 * @return Function to call when you want to update the engine state with JSON data
 */
JsonUiUpdateInput setJsonUiHandlers(const JsonUiUpdateOutput& updateJsHandler);

/**
 * Add a UI component to the global component registry.
 * 
 * @param component weak_ptr to the JsonUiInternal component to add
 */
void addJsonUiComponent(fl::weak_ptr<JsonUiInternal> component);


/**
 * Remove a UI component from the global component registry.
 * 
 * @param component weak_ptr to the JsonUiInternal component to remove
 */
void removeJsonUiComponent(fl::weak_ptr<JsonUiInternal> component);

/**
 * Force immediate processing of any pending UI updates (for testing).
 * In normal operation, updates are processed during the engine loop.
 */
void processJsonUiPendingUpdates();


} // namespace fl
