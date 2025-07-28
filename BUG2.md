# BUG2: Duplicate Canvas Map Data Notifications - **UNDER INVESTIGATION** ⚠️

## Issue Description
The FastLED WASM system was generating **DUPLICATE notifications** for canvas map data with the same `strip_id: 0`, causing duplicate console output and potentially redundant processing.

## **LATEST OBSERVATION - Issue May Still Persist** ⚠️

**Console output from latest test (19:15:42):**
```
index.js:187 0.3s UI elements added, showing UI controls containers
19:15:42.599 index.js:187 0.3s UI elements added directly by C++ routing: Array(5)
19:15:42.600 index.js:187 0.3s Missing screenmap for strip 0
19:15:42.600 index.js:187 0.3s Creating square screenmap for 4096
19:15:42.605 index.js:187 0.3s Canvas map data: {"strip_id":0,"event":"set_canvas_map","map":{"x":
```

**Key observations:**
- "Missing screenmap for strip 0" still appears
- Auto-generation still triggers: "Creating square screenmap for 4096"  
- "Canvas map data" notification still occurs

This suggests the **ID consistency fix may not be complete** or there might be **additional issues**.

## Root Cause Analysis - **PARTIALLY RESOLVED** ⚠️

### **The Problem: ID System Mismatch - FIXED** ✅

There were **two different strip ID generation systems** being used simultaneously:

#### **Path 1: EngineListener (WASM-specific)**
```cpp
// src/platforms/wasm/engine_listener.cpp:40
void EngineListener::onCanvasUiSet(CLEDController *strip, const ScreenMap &screenmap) {
    int controller_id = StripIdMap::addOrGetId(strip);  // Uses StripIdMap (sequential: 0,1,2...)
    jsSetCanvasSize(controller_id, screenmap);
}
```

#### **Path 2: ActiveStripData (shared/generic)**  
```cpp
// src/platforms/shared/active_strip_data/active_strip_data.cpp:28 - BUGGY CODE
void ActiveStripData::onCanvasUiSet(CLEDController *strip, const ScreenMap &screenmap) {
    int id = reinterpret_cast<intptr_t>(strip);  // Uses pointer address (large numbers)
    updateScreenMap(id, screenmap);
}
```

### **The Sequence Causing Duplicates:**

1. **User sets screenmap** → `EngineListener::onCanvasUiSet()` called
   - Uses `StripIdMap::addOrGetId(strip)` → generates **ID = 0**
   - Calls `jsSetCanvasSize(0, screenmap)` → **First "Canvas map data" printf**

2. **ActiveStripData also receives the same event**
   - Uses `reinterpret_cast<intptr_t>(strip)` → generates **ID = 12345678** (large pointer address)
   - Stores screenmap with wrong ID in `mScreenMap`

3. **Next frame**: `jsFillInMissingScreenMaps()` checks for missing screenmaps
   - Calls `active_strips.hasScreenMap(0)` → **returns false!** (screenmap stored with ID 12345678)
   - Auto-generation kicks in for strip 0 → **Second "Canvas map data" printf**

## **The Fix - IMPLEMENTED** ✅

Changed both systems to use the **consistent `fl::IdTracker`** system from `ActiveStripData`:

### **Fixed ActiveStripData** ✅
```cpp
// src/platforms/shared/active_strip_data/active_strip_data.cpp
void ActiveStripData::onCanvasUiSet(CLEDController *strip, const ScreenMap &screenmap) {
    // Use the IdTracker for consistent strip ID management across all platforms
    int id = mIdTracker.getOrCreateId(strip);
    updateScreenMap(id, screenmap);
}
```

### **Fixed EngineListener** ✅  
```cpp
// src/platforms/wasm/engine_listener.cpp
void EngineListener::onCanvasUiSet(CLEDController *strip, const ScreenMap &screenmap) {
    // Use ActiveStripData's IdTracker for consistent ID management
    ActiveStripData &active_strips = ActiveStripData::Instance();
    int controller_id = active_strips.getIdTracker().getOrCreateId(strip);
    jsSetCanvasSize(controller_id, screenmap);
}

void EngineListener::onStripAdded(CLEDController *strip, uint32_t num_leds) {
    // Use ActiveStripData's IdTracker for consistent ID management
    ActiveStripData &active_strips = ActiveStripData::Instance();
    int id = active_strips.getIdTracker().getOrCreateId(strip);
    jsOnStripAdded(id, num_leds);
}
```

## **Status: UNDER INVESTIGATION** ⚠️

- **Root cause identified**: ID system mismatch between `StripIdMap` and `reinterpret_cast` ✅
- **Fix implemented**: Both systems now use `ActiveStripData::mIdTracker.getOrCreateId()` ✅
- **Tests passing**: All 95 unit tests pass ✅
- **Compilation verified**: UNO compilation successful ✅
- **⚠️ HOWEVER**: Latest console output shows "Missing screenmap for strip 0" still occurs

## **Possible Additional Issues to Investigate** 🔍

1. **Timing Issue**: The screenmap might be set **after** the first `jsFillInMissingScreenMaps()` call
2. **Event Order**: UI setup might happen **before** the strip is properly registered
3. **JavaScript-side Issue**: The screenmap might not be reaching the C++ side correctly
4. **Multiple Strip Creation**: There might be multiple strips being created with the same ID
5. **Cache/State Issue**: Previous state might not be properly cleared between runs

## **Next Investigation Steps** 📋

1. **Add debug logging** to track:
   - When `onCanvasUiSet()` is called vs when `jsFillInMissingScreenMaps()` is called
   - What IDs are being generated and stored
   - The timing sequence of events

2. **Check if this is a single occurrence** vs duplicate notifications
   - The current log shows only ONE "Canvas map data" message
   - This might be **normal auto-generation** rather than **duplication**

3. **Verify the fix in WASM environment**
   - Compile and test the actual WASM build
   - Check if the issue occurs in real browser environment

## **Benefits of the Current Fix**

1. **Consistent ID management** - All systems use the same ID for the same CLEDController ✅
2. **Platform independence** - Uses shared `fl::IdTracker` instead of WASM-specific `StripIdMap` ✅
3. **Thread safety** - `fl::IdTracker` includes proper synchronization ✅

## **Technical Details - Historical Reference**

**Auto-Generation Pattern**: The notifications showed a perfect 64x64 square grid:
- X coordinates: `[0,0,0...0,1,1,1...1,2,2,2...2,...]` (64 zeros, 64 ones, 64 twos, etc.)
- Y coordinates: `[0,1,2,3...63,0,1,2,3...63,0,1,2,3...]` (repeating 0-63 pattern)

This confirmed the notifications were from the auto-generation logic in `jsFillInMissingScreenMaps()`, specifically the square matrix branch:
```cpp
if (pixel_count > 255 && Function::isSquare(pixel_count)) {
    printf("Creating square screenmap for %d\n", pixel_count);
    // Square grid generation code...
}
```

**Current status: The ID consistency fix has been implemented, but further investigation is needed to determine if the latest console output represents normal behavior or indicates remaining issues.** 
