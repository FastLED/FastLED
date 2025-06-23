#pragma once

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript BINDING HEADER - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS HEADER DECLARES C++ TO JAVASCRIPT BINDING FUNCTIONS 🚨
//
// DO NOT MODIFY FUNCTION DECLARATIONS WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
//
// This header declares functions that are exposed to JavaScript via WebAssembly.
// Any changes to:
// - Function declarations and signatures
// - Parameter types or return types
// - Function names
// - Namespace changes
//
// Will BREAK JavaScript code that calls these functions and cause SILENT RUNTIME FAILURES!
//
// Key functions declared here that are called from JavaScript:
// - jsSetCanvasSize() - Canvas/screen mapping setup
// - jsOnFrame() - Frame rendering callbacks
// - jsOnStripAdded() - LED strip initialization
// - updateJs() - UI update notifications
//
// Before making ANY changes:
// 1. Update corresponding JavaScript Module.cwrap() calls
// 2. Verify implementation files match these declarations exactly
// 3. Test with real WASM builds that use these functions
// 4. Check that JavaScript integration still works
//
// ⚠️⚠️⚠️ REMEMBER: Header changes affect ALL JavaScript callers! ⚠️⚠️⚠️

#include "fl/stdint.h"

namespace fl {

class ScreenMap;
class ActiveStripData;

void jsSetCanvasSize(int cledcontoller_id, const fl::ScreenMap &screenmap);
void jsOnFrame(ActiveStripData &active_strips);
void jsOnStripAdded(uintptr_t strip, uint32_t num_leds);
void updateJs(const char *jsonStr);

} // namespace fl
