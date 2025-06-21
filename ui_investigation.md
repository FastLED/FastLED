# UI Investigation Report

## Root Cause Found ✅

The issue was that the JsonUiManager was being destroyed and recreated during UI updates, causing all registered components to be lost.

### The Problem Chain:
1. UI slider interaction triggers `jsUpdateUiComponents()` in WASM
2. For some reason, `g_updateEngineState` becomes null
3. WASM code detects null `g_updateEngineState` and forces reinitialization 
4. `ensureWasmUiSystemInitialized()` calls `setJsonUiHandlers()` again
5. `setJsonUiHandlers()` destroys old JsonUiManager and creates new one
6. New JsonUiManager has 0 components (all lost)
7. `is<FLArduinoJson::JsonObject>()` fails because there are no components to match against
