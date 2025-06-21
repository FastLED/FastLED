#if defined(__EMSCRIPTEN__)


#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>


#include "fl/json.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "platforms/wasm/ui.h"

#include "platforms/shared/ui/json/ui_deps.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/warn.h"

namespace fl {

JsonUiManager &JsonUiManager::instance() {
    return fl::Singleton<JsonUiManager>::instance();
}


EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents",
                         &JsonUiManager::jsUpdateUiComponents);
}


void addUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    JsonUiManager::instance().addComponent(component);
}

void removeUiComponent(fl::WeakPtr<JsonUiInternal> component) {
    JsonUiManager::instance().removeComponent(component);
}



} // namespace fl

#endif // __EMSCRIPTEN__
