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

jsUiManager::jsUiManager(): JsonUiManager(fl::updateJs) {
    // Register our handlers for UI component management
    // setJsonUiHandlers returns a function that can be used to update engine state
    auto updateEngineState = setJsonUiHandlers(fl::updateJs);
    FL_WARN("jsUiManager: initialized with updateJs handler");
    
    // Store the engine state update function (could be used later if needed)
    // For now, we rely on the direct jsUpdateUiComponents call from JS
}

// Startup function to ensure the jsUiManager is initialized at program startup
// Use GCC constructor attribute to prevent linker elimination
__attribute__((constructor))
__attribute__((used))
static void initializeWasmUiManager() {
    FL_WARN("initializeWasmUiManager: ensuring jsUiManager singleton is created");
    // Force creation of the singleton instance
    jsUiManager::instance();
    FL_WARN("initializeWasmUiManager: jsUiManager singleton initialized");
}

EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents",
                         &jsUiManager::jsUpdateUiComponents);
}

} // namespace fl

#endif // __EMSCRIPTEN__
