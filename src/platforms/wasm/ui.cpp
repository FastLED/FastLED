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

// Add a debug function to track when g_updateEngineState changes
static void logUpdateEngineStateChange(const char* location) {
    FL_WARN("*** " << location << ": g_updateEngineState=" << (g_updateEngineState ? "VALID" : "NULL"));
}

void jsUpdateUiComponents(const std::string &jsonStr) {
    FL_WARN("*** jsUpdateUiComponents ENTRY ***");
    FL_WARN("*** jsUpdateUiComponents RECEIVED JSON: " << jsonStr.c_str());
    FL_WARN("*** jsUpdateUiComponents JSON LENGTH: " << jsonStr.length());
    FL_WARN("*** jsUpdateUiComponents ENTRY: g_uiSystemInitialized=" << (g_uiSystemInitialized ? "true" : "false") << ", g_updateEngineState=" << (g_updateEngineState ? "VALID" : "NULL"));
    
    // Only initialize if not already initialized - don't force reinitialization
    if (!g_uiSystemInitialized) {
        FL_WARN("*** WASM: UI system not initialized, initializing for first time");
        ensureWasmUiSystemInitialized();
    }
    
    FL_WARN("*** jsUpdateUiComponents AFTER INIT: g_uiSystemInitialized=" << (g_uiSystemInitialized ? "true" : "false") << ", g_updateEngineState=" << (g_updateEngineState ? "VALID" : "NULL"));
    
    if (g_updateEngineState) {
        FL_WARN("*** WASM CALLING BACKEND WITH JSON: " << jsonStr.c_str());
        g_updateEngineState(jsonStr.c_str());
        FL_WARN("*** WASM BACKEND CALL COMPLETED");
    } else {
        FL_WARN("*** WASM ERROR: No engine state updater available");
        FL_WARN("*** WASM ERROR: Cannot process JSON: " << jsonStr.c_str());
    }
}

// Ensure the UI system is initialized - called when needed
void ensureWasmUiSystemInitialized() {
    FL_WARN("*** CODE UPDATE VERIFICATION: This message confirms the C++ code has been rebuilt! ***");
    FL_WARN("*** ensureWasmUiSystemInitialized ENTRY: g_uiSystemInitialized=" << (g_uiSystemInitialized ? "true" : "false") << ", g_updateEngineState=" << (g_updateEngineState ? "VALID" : "NULL"));
    
    // Return early if already initialized - CRITICAL FIX
    if (g_uiSystemInitialized) {
        FL_WARN("*** ensureWasmUiSystemInitialized: Already initialized, returning early");
        return;
    }
    
    if (!g_uiSystemInitialized || !g_updateEngineState) {
        FL_WARN("*** WASM INITIALIZING UI SYSTEM ***");
        
        // Test if fl::updateJs is accessible
        //fl::updateJs("[]");  // Valid empty JSON array
        
        JsonUiUpdateOutput updateJsHandler = [](const char* jsonStr) {
            fl::updateJs(jsonStr);
        };
        
        // Test the lambda
        //updateJsHandler("[]");  // Valid empty JSON array
        
        g_updateEngineState = setJsonUiHandlers(updateJsHandler);
        logUpdateEngineStateChange("AFTER setJsonUiHandlers");
        g_uiSystemInitialized = true;
        
        FL_WARN("*** WASM UI SYSTEM INITIALIZED ***");
    }
}

EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents",
                         &jsUpdateUiComponents);
    emscripten::function("jsUpdateUiComponents",
                         &jsUpdateUiComponents);
}

} // namespace fl

#endif // __EMSCRIPTEN__
