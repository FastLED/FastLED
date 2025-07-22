#if defined(__EMSCRIPTEN__)

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript ASYNC UI BRIDGE - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS FILE CONTAINS C++ TO JAVASCRIPT ASYNC UI BINDINGS 🚨
//
// DO NOT MODIFY FUNCTION SIGNATURES WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
//
// This file bridges C++ UI updates with JavaScript UI components using async patterns.
// Any changes to:
// - Async jsUpdateUiComponents() function signature
// - extern "C" async wrapper functions
// - Parameter types (const char* vs std::string)
// - EMSCRIPTEN_KEEPALIVE async function signatures
//
// Will BREAK the JavaScript UI system and cause SILENT RUNTIME FAILURES!
//
// Key async integration points that MUST remain synchronized:
// - extern "C" jsUpdateUiComponents(const char* jsonStr) - now async-aware
// - fl::jsUpdateUiComponents(const std::string &jsonStr) - now async-aware
// - JavaScript Module.cwrap('jsUpdateUiComponents', null, ['string']) - now handles async
// - globalThis.onFastLedUiUpdateFunction callbacks - now async
//
// Before making ANY changes:
// 1. Understand this affects async UI rendering in the browser
// 2. Test with real WASM builds that include UI components
// 3. Verify JSON parsing works correctly on both sides with async
// 4. Check that async UI update callbacks still fire properly
//
// ⚠️⚠️⚠️ REMEMBER: Async UI failures are often silent and hard to debug! ⚠️⚠️⚠️

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include "platforms/wasm/ui.h"
#include "platforms/wasm/js_bindings.h"
#include "platforms/shared/ui/json/ui.h"
#include "fl/warn.h"
#include "fl/compiler_control.h"

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

// Add a periodic check function that can be called from JavaScript
extern "C" void checkUpdateEngineState() {
    FL_WARN("*** ASYNC PERIODIC CHECK: g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
    FL_WARN("*** ASYNC PERIODIC CHECK: g_uiSystemInitialized=" << (getUiSystemInitialized() ? "true" : "false"));
}

/**
 * Async-aware UI component updater
 * Now handles async operations and provides better error handling
 */
void jsUpdateUiComponents(const std::string &jsonStr) {
    // FL_WARN("*** jsUpdateUiComponents ASYNC ENTRY ***");
    // FL_WARN("*** jsUpdateUiComponents ASYNC RECEIVED JSON: " << jsonStr.c_str());
    // FL_WARN("*** jsUpdateUiComponents ASYNC JSON LENGTH: " << jsonStr.length());
    // FL_WARN("*** jsUpdateUiComponents ASYNC ENTRY: g_uiSystemInitialized=" << (getUiSystemInitialized() ? "true" : "false") << ", g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
    
    // Only initialize if not already initialized - don't force reinitialization
    if (!getUiSystemInitialized()) {
        FL_WARN("*** ASYNC WASM: UI system not initialized, initializing for first time");
        ensureWasmUiSystemInitialized();
    }
    
    //FL_WARN("*** jsUpdateUiComponents ASYNC AFTER INIT: g_uiSystemInitialized=" << (getUiSystemInitialized() ? "true" : "false") << ", g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
    
    if (getUpdateEngineState()) {
        //FL_WARN("*** ASYNC WASM CALLING BACKEND WITH JSON: " << jsonStr.c_str());
        
        // Call the backend function - handle errors via early return
        if (getUpdateEngineState()) {
            getUpdateEngineState()(jsonStr.c_str());
            //FL_WARN("*** ASYNC WASM BACKEND CALL COMPLETED SUCCESSFULLY");
        } else {
            FL_WARN("*** ASYNC WASM BACKEND CALL FAILED: updateEngineState is null");
            return; // Early return on error
        }
        
    } else {
        FL_WARN("*** ASYNC WASM ERROR: No engine state updater available, attempting emergency reinitialization...");
        
        // Try to reinitialize as a recovery mechanism
        getUiSystemInitialized() = false;  // Force reinitialization
        ensureWasmUiSystemInitialized();
        
        FL_WARN("*** ASYNC AFTER EMERGENCY REINIT: g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
        
        if (getUpdateEngineState()) {
            FL_WARN("*** ASYNC EMERGENCY REINIT SUCCESSFUL - retrying JSON processing");
            getUpdateEngineState()(jsonStr.c_str());
            FL_WARN("*** ASYNC EMERGENCY RETRY COMPLETED SUCCESSFULLY");
        } else {
            FL_WARN("*** ASYNC EMERGENCY REINIT FAILED - g_updateEngineState still NULL");
            return; // Early return on failure
        }
    }
}

/**
 * Async-aware UI system initializer
 * Ensures the UI system is initialized with proper async support
 */
void ensureWasmUiSystemInitialized() {
    // FL_WARN("*** ASYNC CODE UPDATE VERIFICATION: This message confirms the C++ code has been rebuilt! ***");
    // FL_WARN("*** ensureWasmUiSystemInitialized ASYNC ENTRY: g_uiSystemInitialized=" << (getUiSystemInitialized() ? "true" : "false") << ", g_updateEngineState=" << (getUpdateEngineState() ? "VALID" : "NULL"));
    
    // Return early if already initialized - CRITICAL FIX
    if (getUiSystemInitialized()) {
        FL_WARN("*** ensureWasmUiSystemInitialized ASYNC: Already initialized, returning early");
        return;
    }
    
    if (!getUiSystemInitialized() || !getUpdateEngineState()) {
        FL_WARN("*** ASYNC WASM INITIALIZING UI SYSTEM ***");
        
        // Create async-aware UI update handler with error handling via early return
        JsonUiUpdateOutput updateJsHandler = [](const char* jsonStr) {
            if (!jsonStr) {
                FL_WARN("*** ASYNC UI UPDATE HANDLER ERROR: Received null jsonStr");
                return; // Early return on error
            }
            
            // Detect if this is UI element definitions (JSON array) or UI state updates (JSON object)
            if (jsonStr[0] == '[') {
                // This is a JSON array of UI element definitions - route directly to UI manager
                FL_WARN("*** ROUTING UI ELEMENT DEFINITIONS DIRECTLY TO UI MANAGER");
                
                // Call UI manager directly to avoid circular event loops
                EM_ASM({
                    try {
                        const jsonStr = UTF8ToString($0);
                        const uiElements = JSON.parse(jsonStr);
                        
                        // Log the inbound event to the inspector if available
                        if (window.jsonInspector) {
                            window.jsonInspector.logInboundEvent(uiElements, 'C++ → JS (Direct)');
                        }
                        
                        // Add UI elements directly using UI manager (bypass callback to avoid loops)
                        if (window.uiManager && typeof window.uiManager.addUiElements === 'function') {
                            window.uiManager.addUiElements(uiElements);
                            console.log('UI elements added directly by C++ routing:', uiElements);
                        } else {
                            console.warn('UI Manager not available for direct routing');
                        }
                        
                    } catch (error) {
                        console.error('Error in direct UI element routing:', error);
                    }
                }, jsonStr);
            } else {
                // This is a JSON object of UI state updates - route to update system
                FL_WARN("*** ROUTING UI STATE UPDATES TO updateJs");
                fl::updateJs(jsonStr);
            }
        };
        
        // Initialize with error checking via early return
        auto tempResult = setJsonUiHandlers(updateJsHandler);
        if (!tempResult) {
            FL_WARN("*** ASYNC WASM UI SYSTEM INITIALIZATION FAILED: setJsonUiHandlers returned null");
            return; // Early return on failure
        }
        
        getUpdateEngineState() = tempResult;
        getUiSystemInitialized() = true;
        
        FL_WARN("*** ASYNC WASM UI SYSTEM INITIALIZATION COMPLETED ***");
    }
}

/**
 * Async-aware startup initializer
 * Called automatically when the WASM module loads
 */
__attribute__((constructor))
void on_startup_initialize_wasm_ui() {
    FL_WARN("*** ASYNC WASM UI STARTUP INITIALIZER CALLED ***");
    ensureWasmUiSystemInitialized();
}

} // namespace fl

// C binding wrapper for async jsUpdateUiComponents
extern "C" {
    /**
     * Async-aware C binding for UI component updates
     * Now includes comprehensive error handling for async operations
     */
    EMSCRIPTEN_KEEPALIVE void jsUpdateUiComponents(const char* jsonStr) {
        // Input validation with early return
        if (!jsonStr) {
            FL_WARN("*** ASYNC C BINDING: Received NULL jsonStr");
            return;
        }
        
        // Call the async-aware C++ implementation
        fl::jsUpdateUiComponents(std::string(jsonStr));
    }
}

#endif // __EMSCRIPTEN__
