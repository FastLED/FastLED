

#ifdef __EMSCRIPTEN__

// ⚠️⚠️⚠️ WASM-SPECIFIC ACTIVE STRIP DATA WRAPPER ⚠️⚠️⚠️
//
// This file provides WASM-specific initialization for ActiveStripData.
// The core logic has been moved to src/platforms/shared/active_strip_data/
// for better testability and platform independence.
//
// WASM-specific functionality:
// - Constructor attribute for early initialization
// - Integration with StripIdMap for WASM strip ID management
//
// Core functionality is now in the shared implementation:
// - JSON parsing and creation
// - Strip data management
// - Screen map handling
// 
// JavaScript bindings have been moved to js_bindings.cpp:
// - getStripPixelData() function
//
// ⚠️⚠️⚠️ DO NOT DUPLICATE SHARED FUNCTIONALITY HERE ⚠️⚠️⚠️

#include "platforms/shared/active_strip_data/active_strip_data.h"
#include "fl/id_tracker.h"

namespace fl {

// WASM-specific implementation of onCanvasUiSet that uses IdTracker
void ActiveStripData::onCanvasUiSet(CLEDController *strip,
                                   const ScreenMap &screenmap) {
    int id = getIdTracker().getOrCreateId(static_cast<void*>(strip));
    updateScreenMap(id, screenmap);
}

} // namespace fl

/// WASM-SPECIFIC: Early initialization using GCC constructor attribute
/// WARNING: For some reason the following code must be here, when
/// it was moved elsewhere, frame data stopped being updated.
__attribute__((constructor)) void __init_ActiveStripData() {
    fl::ActiveStripData::Instance();
}

#endif // __EMSCRIPTEN__
