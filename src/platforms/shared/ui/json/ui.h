#pragma once

// IWYU pragma: private

#include "fl/stl/function.h"
#include "fl/stl/weak_ptr.h"  // Full definition needed by callers of addJsonUiComponent
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declare the JsonUiInternal class.
class JsonUiInternal;
class JsonUiManager;

// From driver -> UI
using JsonUiUpdateOutput = fl::function<void(const char*)>;

// from UI -> driver
using JsonUiUpdateInput = fl::function<void(const char*)>;

/**
 * Set the global handler for sending UI updates to JavaScript.
 * 
 * @param updateJsHandler Function to call when UI needs to send updates to JavaScript
 * @return Function to call when you want to update the driver state with JSON data
 */
JsonUiUpdateInput setJsonUiHandlers(const JsonUiUpdateOutput& updateJsHandler) FL_NOEXCEPT;

/**
 * Add a UI component to the global component registry.
 * 
 * @param component weak_ptr to the JsonUiInternal component to add
 */
void addJsonUiComponent(fl::weak_ptr<JsonUiInternal> component) FL_NOEXCEPT;


/**
 * Remove a UI component from the global component registry.
 * 
 * @param component weak_ptr to the JsonUiInternal component to remove
 */
void removeJsonUiComponent(fl::weak_ptr<JsonUiInternal> component) FL_NOEXCEPT;

/**
 * Force immediate processing of any pending UI updates (for testing).
 * In normal operation, updates are processed during the driver loop.
 */
void processJsonUiPendingUpdates() FL_NOEXCEPT;


} // namespace fl
