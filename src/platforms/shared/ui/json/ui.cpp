#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/warn.h"
#include "fl/ptr.h"

namespace fl {

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
