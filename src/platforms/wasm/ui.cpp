#if defined(__EMSCRIPTEN__)


#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>


#include "fl/json.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "platforms/wasm/ui.h"
#include "platforms/wasm/js_bindings.h"

#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/ui_internal.h"
#include "fl/warn.h"

namespace fl {

jsUiManager &jsUiManager::instance() {
    return fl::Singleton<jsUiManager>::instance();
}

void jsUiManager::jsUpdateUiComponents(const std::string &jsonStr) {
    instance().updateUiComponents(jsonStr.c_str());
}

jsUiManager::jsUiManager(): JsonUiManager(fl::updateJs) {}


EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents",
                         &jsUiManager::jsUpdateUiComponents);
}


EMSCRIPTEN_KEEPALIVE void addJsonUiComponentPlatform(fl::WeakPtr<JsonUiInternal> component) {
    jsUiManager::instance().addComponent(component);
}

EMSCRIPTEN_KEEPALIVE void removeJsonUiComponentPlatform(fl::WeakPtr<JsonUiInternal> component) {
    jsUiManager::instance().removeComponent(component);
}



} // namespace fl

#endif // __EMSCRIPTEN__
