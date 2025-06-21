#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/warn.h"
#include "fl/ptr.h"

namespace fl {

// Provide stub implementations for the weak symbols
__attribute__((weak)) void addJsonUiComponentPlatform(fl::WeakPtr<JsonUiInternal> component) {
    // Default stub implementation - do nothing
    FL_WARN("addJsonUiComponent is not implemented, received component: " << component);
    (void)component;
}

__attribute__((weak)) void removeJsonUiComponentPlatform(fl::WeakPtr<JsonUiInternal> component) {
    // Default stub implementation - do nothing
    FL_WARN("removeJsonUiComponent is not implemented, received component: " << component);
    (void)component;
}

void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    // Let the linker resolve weak vs strong symbol automatically
    addJsonUiComponentPlatform(component);
}

void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    // Let the linker resolve weak vs strong symbol automatically
    removeJsonUiComponentPlatform(component);
}

} // namespace fl
