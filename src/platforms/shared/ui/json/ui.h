#pragma once

#include "fl/function.h"

namespace fl {

template<typename T>
class WeakPtr;
// Forward declare the JsonUiInternal class.
class JsonUiInternal;
class JsonUiManager;

// Handler function types for UI component management
using JsonUiAddHandler = fl::function<void(fl::WeakPtr<JsonUiInternal>)>;
using JsonUiRemoveHandler = fl::function<void(fl::WeakPtr<JsonUiInternal>)>;
using JsonUiUpdateJsHandler = fl::function<void(const char*)>;

/**
 * Set the global handlers for adding and removing UI components, and the updateJs callback.
 * 
 * @param addHandler Function to call when a UI component needs to be added
 * @param removeHandler Function to call when a UI component needs to be removed
 * @param updateJsHandler Function to call when UI needs to send updates to JavaScript
 */
void setJsonUiHandlers(const JsonUiAddHandler& addHandler, const JsonUiRemoveHandler& removeHandler, const JsonUiUpdateJsHandler& updateJsHandler);

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
