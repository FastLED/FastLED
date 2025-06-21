
#include "platforms/wasm/ui/ui_deps.h"
#include "platforms/wasm/ui/ui_internal.h"

#include "fl/unused.h"

#ifdef __EMSCRIPTEN__

namespace fl {
#include "platforms/wasm/ui.h"
void addUiComponent(fl::WeakPtr<jsUiInternal> component) {
    jsUiManager::instance().addComponent(component);
}

void removeUiComponent(fl::WeakPtr<jsUiInternal> component) {
    jsUiManager::instance().removeComponent(component);
}
} // namespace fl

#else

namespace fl {
void addUiComponent(fl::WeakPtr<jsUiInternal> component) {
    FL_UNUSED(component);
    // do nothing
}

void removeUiComponent(fl::WeakPtr<jsUiInternal> component) {
    FL_UNUSED(component);
    // do nothing
}
} // namespace fl

#endif // __EMSCRIPTEN__
