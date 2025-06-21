#pragma once

#include "fl/ptr.h"

namespace fl {

class JsonUiInternal;

// Define these functions if you want to use the JsonUiManager class.
/**
 * Add a UI component to the global component registry.
 * This is a free function replacement for JsonUiManager::addComponent.
 * 
 * @param component WeakPtr to the JsonUiInternal component to add
 */
void addUiComponent(fl::WeakPtr<JsonUiInternal> component) __attribute__((weak));


/**
 * Remove a UI component from the global component registry.
 * This is a free function replacement for JsonUiManager::removeComponent.
 * 
 * @param component WeakPtr to the JsonUiInternal component to remove
 */
void removeUiComponent(fl::WeakPtr<JsonUiInternal> component) __attribute__((weak));


} // namespace fl
