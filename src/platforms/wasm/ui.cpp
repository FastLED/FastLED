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
static bool g_uiSystemInitialized = false;

void jsUpdateUiComponents(const std::string &jsonStr) {
    // Ensure UI system is initialized when first used
    ensureWasmUiSystemInitialized();
    
    if (g_updateEngineState) {
        g_updateEngineState(jsonStr.c_str());
    } else {
        FL_WARN("jsUpdateUiComponents called but no engine state updater available");
    }
}

// Ensure the UI system is initialized - called when needed
void ensureWasmUiSystemInitialized() {
    if (!g_uiSystemInitialized) {
        FL_WARN("ensureWasmUiSystemInitialized: setting up generic UI handlers");
        
        // Set up the generic UI system with updateJs as the output handler
        // Wrap fl::updateJs in a fl::function to match the expected signature
        JsonUiUpdateOutput updateJsWrapper = [](const char* jsonStr) {
            fl::updateJs(jsonStr);
        };
        g_updateEngineState = setJsonUiHandlers(updateJsWrapper);
        g_uiSystemInitialized = true;
        
        FL_WARN("ensureWasmUiSystemInitialized: wasm UI system initialized");
    }
}

EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents",
                         &jsUpdateUiComponents);
}

} // namespace fl

#endif // __EMSCRIPTEN__
