#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/warn.h"
#include "fl/ptr.h"

namespace fl {

// Provide stub implementations for the weak symbols
__attribute__((weak)) void addJsonUiComponentPlatform(fl::WeakPtr<JsonUiInternal> component) {
    // Default stub implementation - do nothing
    (void)component;
}

__attribute__((weak)) void removeJsonUiComponentPlatform(fl::WeakPtr<JsonUiInternal> component) {
    // Default stub implementation - do nothing
    (void)component;
}

void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    if (addJsonUiComponentPlatform) {
        addJsonUiComponentPlatform(component);
    } else {
        FL_WARN("addJsonUiComponent is not implemented, received component: " << component);
    }
}

void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    if (removeJsonUiComponentPlatform) {
        removeJsonUiComponentPlatform(component);
    } else {
        FL_WARN("removeJsonUiComponent is not implemented, received component: " << component);
    }
}

} // namespace fl
