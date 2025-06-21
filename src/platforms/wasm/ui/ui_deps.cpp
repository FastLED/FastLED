#if defined(__EMSCRIPTEN__) || defined(FASTLED_TESTING)

#include "ui_deps.h"
#include "ui_internal.h"
#include "ui_manager.h"

namespace fl {


void addUiComponent(fl::WeakPtr<jsUiInternal> component) {
    jsUiManager::instance().addComponent(component);
}

void removeUiComponent(fl::WeakPtr<jsUiInternal> component) {
    jsUiManager::instance().removeComponent(component);
}

} // namespace fl

#endif // __EMSCRIPTEN__
