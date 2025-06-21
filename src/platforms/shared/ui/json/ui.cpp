#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/warn.h"
#include "fl/ptr.h"

namespace fl {
__attribute__((weak)) void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    FL_WARN("addJsonUiComponent is not implemented, received component: " << component);
}

__attribute__((weak)) void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    FL_WARN("removeJsonUiComponent is not implemented, received component: " << component);
}
} // namespace fl
