#if defined(__EMSCRIPTEN__)

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript UI BRIDGE - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS FILE CONTAINS C++ TO JAVASCRIPT UI BINDINGS 🚨
//
// DO NOT MODIFY FUNCTION SIGNATURES WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
//
// This file bridges C++ UI updates with JavaScript UI components. Any changes to:
// - jsUpdateUiComponents() function signature
// - extern "C" wrapper functions
// - Parameter types (const char* vs std::string)
// - EMSCRIPTEN_KEEPALIVE function signatures
//
// Will BREAK the JavaScript UI system and cause SILENT RUNTIME FAILURES!
//
// Key integration points that MUST remain synchronized:
// - extern "C" jsUpdateUiComponents(const char* jsonStr)  
// - fl::jsUpdateUiComponents(const std::string &jsonStr)
// - JavaScript Module.cwrap('jsUpdateUiComponents', null, ['string'])
// - globalThis.onFastLedUiUpdateFunction callbacks
//
// Before making ANY changes:
// 1. Understand this affects UI rendering in the browser
// 2. Test with real WASM builds that include UI components
// 3. Verify JSON parsing works correctly on both sides
// 4. Check that UI update callbacks still fire properly
//
// ⚠️⚠️⚠️ REMEMBER: UI failures are often silent and hard to debug! ⚠️⚠️⚠️

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include "platforms/wasm/ui.h"
#include "platforms/wasm/js_bindings.h"
#include "platforms/shared/ui/json/ui.h"
#include "fl/warn.h"

using fl::JsonUiUpdateOutput;

namespace fl {

// Use function-local statics for better initialization order safety
static JsonUiUpdateInput& getUpdateEngineState() {
    static JsonUiUpdateInput updateEngineState;
    return updateEngineState;
}

static bool& getUiSystemInitialized() {
    static bool uiSystemInitialized = false;
    return uiSystemInitialized;
}

// Add a debug function to track when g_updateEngineState changes
static void logUpdateEngineStateChange(const char* location) {
    FL_WARN("*** " << location << ": g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
}

// Add a periodic check function that can be called from JavaScript
extern "C" void checkUpdateEngineState() {
    FL_WARN("*** PERIODIC CHECK: g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
    FL_WARN("*** PERIODIC CHECK: g_uiSystemInitialized=" << (getUiSystemInitialized() ? "true" : "false"));
}

void jsUpdateUiComponents(const std::string &jsonStr) {
    // FL_WARN("*** jsUpdateUiComponents ENTRY ***");
    // FL_WARN("*** jsUpdateUiComponents RECEIVED JSON: " << jsonStr.c_str());
    // FL_WARN("*** jsUpdateUiComponents JSON LENGTH: " << jsonStr.length());
    // FL_WARN("*** jsUpdateUiComponents ENTRY: g_uiSystemInitialized=" << (getUiSystemInitialized() ? "true" : "false") << ", g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
    

    
    // Only initialize if not already initialized - don't force reinitialization
    if (!getUiSystemInitialized()) {
        FL_WARN("*** WASM: UI system not initialized, initializing for first time");
        ensureWasmUiSystemInitialized();
    }
    
    //FL_WARN("*** jsUpdateUiComponents AFTER INIT: g_uiSystemInitialized=" << (getUiSystemInitialized() ? "true" : "false") << ", g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
    
    if (getUpdateEngineState()) {
        //FL_WARN("*** WASM CALLING BACKEND WITH JSON: " << jsonStr.c_str());
        try {
            getUpdateEngineState()(jsonStr.c_str());
            //FL_WARN("*** WASM BACKEND CALL COMPLETED SUCCESSFULLY");
        } catch (...) {
            FL_WARN("*** WASM BACKEND CALL THREW EXCEPTION!");
        }
    } else {
        FL_WARN("*** WASM ERROR: No engine state updater available, attempting emergency reinitialization...");
        //FL_WARN("*** WASM ERROR: No engine state updater available");
        //FL_WARN("*** WASM ERROR: Cannot process JSON: " << jsonStr.c_str());
        //FL_WARN("*** ATTEMPTING EMERGENCY REINITIALIZATION...");
        
        // Try to reinitialize as a recovery mechanism
        getUiSystemInitialized() = false;  // Force reinitialization
        ensureWasmUiSystemInitialized();
        
        FL_WARN("*** AFTER EMERGENCY REINIT: g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
        
        if (getUpdateEngineState()) {
            FL_WARN("*** EMERGENCY REINIT SUCCESSFUL - retrying JSON processing");
            getUpdateEngineState()(jsonStr.c_str());
            FL_WARN("*** EMERGENCY RETRY COMPLETED SUCCESSFULLY");
        } else {
            FL_WARN("*** EMERGENCY REINIT FAILED - g_updateEngineState still NULL");
        }
    }
}

// Ensure the UI system is initialized - called when needed
void ensureWasmUiSystemInitialized() {
    // FL_WARN("*** CODE UPDATE VERIFICATION: This message confirms the C++ code has been rebuilt! ***");
    // FL_WARN("*** ensureWasmUiSystemInitialized ENTRY: g_uiSystemInitialized=" << (getUiSystemInitialized() ? "true" : "false") << ", g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
    
    // Return early if already initialized - CRITICAL FIX
    if (getUiSystemInitialized()) {
        FL_WARN("*** ensureWasmUiSystemInitialized: Already initialized, returning early");
        return;
    }
    
    if (!getUiSystemInitialized() || !getUpdateEngineState()) {
        FL_WARN("*** WASM INITIALIZING UI SYSTEM ***");
        
        // Test if fl::updateJs is accessible
        //fl::updateJs("[]");  // Valid empty JSON array
        
        JsonUiUpdateOutput updateJsHandler = [](const char* jsonStr) {
            fl::updateJs(jsonStr);
        };
        
        // Test the lambda
        //updateJsHandler("[]");  // Valid empty JSON array
        
        // FL_WARN("*** ABOUT TO CALL setJsonUiHandlers");
        // FL_WARN("*** BEFORE setJsonUiHandlers: g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
        
        auto tempResult = setJsonUiHandlers(updateJsHandler);
        // FL_WARN("*** setJsonUiHandlers RETURNED: " << (tempResult ? "VALID" : "NULL"));
        
        // FL_WARN("*** ABOUT TO ASSIGN TO g_updateEngineState");
        getUpdateEngineState() = tempResult;
        // FL_WARN("*** ASSIGNMENT COMPLETED");
        
        // logUpdateEngineStateChange("AFTER setJsonUiHandlers");
        getUiSystemInitialized() = true;
        
        // FL_WARN("*** WASM UI SYSTEM INITIALIZED ***");
        // FL_WARN("*** FINAL CHECK: g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
    }
}

} // namespace fl

// C binding wrapper for jsUpdateUiComponents
extern "C" {
    EMSCRIPTEN_KEEPALIVE void jsUpdateUiComponents(const char* jsonStr) {
        fl::jsUpdateUiComponents(std::string(jsonStr));
    }
}

#endif // __EMSCRIPTEN__
