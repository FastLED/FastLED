#if defined(__EMSCRIPTEN__)

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include "platforms/wasm/ui.h"
#include "platforms/wasm/js_bindings.h"
#include "platforms/shared/ui/json/ui.h"
#include "fl/warn.h"

using fl::JsonUiUpdateOutput;

namespace fl {

// Static storage for the engine state update function
static JsonUiUpdateInput g_updateEngineState;
static bool g_uiSystemInitialized = false;

void jsUpdateUiComponents(const std::string &jsonStr) {
    FL_WARN("jsUpdateUiComponents: ENTRY - jsonStr length=" << jsonStr.length());
    FL_WARN("jsUpdateUiComponents: g_uiSystemInitialized=" << (g_uiSystemInitialized ? "true" : "false"));
    FL_WARN("jsUpdateUiComponents: g_updateEngineState=" << (g_updateEngineState ? "VALID" : "NULL"));
    
    // Ensure UI system is initialized when first used
    FL_WARN("jsUpdateUiComponents: About to call ensureWasmUiSystemInitialized()");
    ensureWasmUiSystemInitialized();
    FL_WARN("jsUpdateUiComponents: After ensureWasmUiSystemInitialized() - g_updateEngineState=" << (g_updateEngineState ? "VALID" : "NULL"));
    
    if (g_updateEngineState) {
        FL_WARN("jsUpdateUiComponents: Calling g_updateEngineState with JSON: " << jsonStr.substr(0, 100).c_str() << "...");
        g_updateEngineState(jsonStr.c_str());
        FL_WARN("jsUpdateUiComponents: g_updateEngineState call completed");
    } else {
        FL_WARN("jsUpdateUiComponents called but no engine state updater available");
    }
    FL_WARN("jsUpdateUiComponents: EXIT");
}

// Ensure the UI system is initialized - called when needed
void ensureWasmUiSystemInitialized() {
    FL_WARN("ensureWasmUiSystemInitialized: ENTRY - g_uiSystemInitialized=" << (g_uiSystemInitialized ? "true" : "false"));
    
    if (!g_uiSystemInitialized || !g_updateEngineState) {
        FL_WARN("ensureWasmUiSystemInitialized: setting up generic UI handlers (g_uiSystemInitialized=" << (g_uiSystemInitialized ? "true" : "false") << ", g_updateEngineState=" << (g_updateEngineState ? "valid" : "null") << ")");
        
        // Set up the generic UI system with updateJs as the output handler
        // Wrap the C function in a function object
        FL_WARN("ensureWasmUiSystemInitialized: Creating updateJsHandler lambda");
        
        // Test if fl::updateJs is accessible
        FL_WARN("ensureWasmUiSystemInitialized: Testing fl::updateJs accessibility");
        fl::updateJs("test-accessibility");
        FL_WARN("ensureWasmUiSystemInitialized: fl::updateJs test call completed");
        
        JsonUiUpdateOutput updateJsHandler = [](const char* jsonStr) {
            FL_WARN("updateJsHandler lambda called with: " << (jsonStr ? jsonStr : "NULL"));
            fl::updateJs(jsonStr);
            FL_WARN("updateJsHandler lambda: fl::updateJs call completed");
        };
        
        FL_WARN("ensureWasmUiSystemInitialized: Lambda created, testing it directly");
        updateJsHandler("test-lambda");
        FL_WARN("ensureWasmUiSystemInitialized: Direct lambda test completed");
        
        FL_WARN("ensureWasmUiSystemInitialized: updateJsHandler created, calling setJsonUiHandlers");
        FL_WARN("ensureWasmUiSystemInitialized: updateJsHandler is " << (updateJsHandler ? "VALID" : "NULL"));
        
        g_updateEngineState = setJsonUiHandlers(updateJsHandler);
        
        FL_WARN("ensureWasmUiSystemInitialized: setJsonUiHandlers returned, g_updateEngineState is " << (g_updateEngineState ? "VALID" : "NULL"));
        
        g_uiSystemInitialized = true;
        
        FL_WARN("ensureWasmUiSystemInitialized: wasm UI system initialized");
    } else {
        FL_WARN("ensureWasmUiSystemInitialized: UI system already initialized, g_updateEngineState=" << (g_updateEngineState ? "VALID" : "NULL"));
    }
    FL_WARN("ensureWasmUiSystemInitialized: EXIT");
}

EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents",
                         &jsUpdateUiComponents);
}

} // namespace fl

#endif // __EMSCRIPTEN__
