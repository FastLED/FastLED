#include "platforms/shared/ui/json/ui_deps.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/warn.h"
#include "fl/ptr.h"


namespace fl {
__attribute__((weak)) void addUiComponent(fl::WeakPtr<JsonUiInternal> component) {

    FL_WARN("addUiComponent is not implemented, received component: " << component);
}

__attribute__((weak)) void removeUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    FL_WARN("removeUiComponent is not implemented, received component: " << component);
}
} // namespace fl
