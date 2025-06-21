#pragma once

#include "fl/ptr.h"

namespace fl {

class jsUiInternal;

// Free function replacements for jsUiManager::addComponent and jsUiManager::removeComponent
// These provide the exact same functionality without requiring the jsUiManager class

/**
 * Add a UI component to the global component registry.
 * This is a free function replacement for jsUiManager::addComponent.
 * 
 * @param component WeakPtr to the jsUiInternal component to add
 */
void addUiComponent(fl::WeakPtr<jsUiInternal> component);

/**
 * Remove a UI component from the global component registry.
 * This is a free function replacement for jsUiManager::removeComponent.
 * 
 * @param component WeakPtr to the jsUiInternal component to remove
 */
void removeUiComponent(fl::WeakPtr<jsUiInternal> component);

} // namespace fl
