#pragma once

#include "fl/function.h"

namespace fl {

template<typename T>
class WeakPtr;
// Forward declare the JsonUiInternal class.
class JsonUiInternal;

// Handler function types for UI component management
using JsonUiAddHandler = fl::function<void(fl::WeakPtr<JsonUiInternal>)>;
using JsonUiRemoveHandler = fl::function<void(fl::WeakPtr<JsonUiInternal>)>;

/**
 * Set the global handler for adding UI components.
 * 
 * @param handler Function to call when a UI component needs to be added
 */
void setJsonUiAddHandler(const JsonUiAddHandler& handler);

/**
 * Set the global handler for removing UI components.
 * 
 * @param handler Function to call when a UI component needs to be removed
 */
void setJsonUiRemoveHandler(const JsonUiRemoveHandler& handler);

/**
 * Add a UI component to the global component registry.
 * This is a free function replacement for jsUiManager::addComponent.
 * 
 * @param component WeakPtr to the JsonUiInternal component to add
 */
void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component);


/**
 * Remove a UI component from the global component registry.
 * This is a free function replacement for jsUiManager::removeComponent.
 * 
 * @param component WeakPtr to the JsonUiInternal component to remove
 */
void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component);


} // namespace fl
