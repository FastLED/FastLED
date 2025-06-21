#pragma once

#include "fl/function.h"

namespace fl {

template<typename T>
class WeakPtr;
// Forward declare the JsonUiInternal class.
class JsonUiInternal;
class JsonUiManager;

/**
 * Set the global handler for sending UI updates to JavaScript.
 * 
 * @param updateJsHandler Function to call when UI needs to send updates to JavaScript
 * @return Function to call when you want to update the engine state with JSON data
 */
fl::function<void(const char*)> setJsonUiHandlers(const fl::function<void(const char*)>& updateJsHandler);

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
