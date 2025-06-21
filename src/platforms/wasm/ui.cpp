#if defined(__EMSCRIPTEN__)

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include "platforms/wasm/ui.h"
#include "platforms/wasm/js_bindings.h"
#include "platforms/shared/ui/json/ui.h"
#include "fl/warn.h"

namespace fl {

// Static storage for the engine state update function
static JsonUiUpdateInput g_updateEngineState;

void jsUpdateUiComponents(const std::string &jsonStr) {
    if (g_updateEngineState) {
        g_updateEngineState(jsonStr.c_str());
    } else {
        FL_WARN("jsUpdateUiComponents called but no engine state updater available");
    }
}

// Startup function to ensure the wasm UI system is initialized at program startup
// Use GCC constructor attribute to prevent linker elimination
__attribute__((constructor))
__attribute__((used))
static void initializeWasmUiSystem() {
    FL_WARN("initializeWasmUiSystem: setting up generic UI handlers");
    
    // Set up the generic UI system with updateJs as the output handler
    g_updateEngineState = setJsonUiHandlers(fl::updateJs);
    
    FL_WARN("initializeWasmUiSystem: wasm UI system initialized");
}

EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents",
                         &jsUpdateUiComponents);
}

} // namespace fl

#endif // __EMSCRIPTEN__
