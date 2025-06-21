#include "platforms/wasm/ui/ui_deps.h"
#include "platforms/shared/ui/json/ui_internal.h"

#include "fl/unused.h"

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/ui.h"
namespace fl {

void addUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    JsonUiManager::instance().addComponent(component);
}

void removeUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    JsonUiManager::instance().removeComponent(component);
}
} // namespace fl

#else

namespace fl {
void addUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    FL_UNUSED(component);
    // do nothing
}

void removeUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    FL_UNUSED(component);
    // do nothing
}
} // namespace fl

#endif // __EMSCRIPTEN__
