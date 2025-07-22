#pragma once

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript ASYNC BINDING HEADER - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS HEADER DECLARES C++ TO JAVASCRIPT ASYNC BINDING FUNCTIONS 🚨
//
// DO NOT MODIFY FUNCTION DECLARATIONS WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
//
// This header declares async-aware functions that are exposed to JavaScript via WebAssembly.
// Any changes to:
// - Function declarations and async signatures
// - Parameter types or return types
// - Function names or async behavior
// - Namespace changes
//
// Will BREAK JavaScript code that calls these functions and cause SILENT RUNTIME FAILURES!
//
// Key async functions declared here that are called from JavaScript:
// - jsSetCanvasSize() - Canvas/screen mapping setup (now async-aware)
// - jsOnFrame() - Frame rendering callbacks (now async-aware)
// - jsOnStripAdded() - LED strip initialization (now async-aware)
// - updateJs() - UI update notifications (now async-aware)
//
// All functions now support async JavaScript callbacks that can return Promises.
// The C++ side will properly await JavaScript Promise returns when Asyncify is enabled.
//
// Before making ANY changes:
// 1. Update corresponding JavaScript Module.cwrap() calls
// 2. Verify implementation files match these declarations exactly
// 3. Test with real WASM builds that use these async functions
// 4. Check that JavaScript async integration still works
// 5. Ensure Emscripten Asyncify is properly configured
//
// ⚠️⚠️⚠️ REMEMBER: Header changes affect ALL JavaScript async callers! ⚠️⚠️⚠️

#include "fl/stdint.h"

namespace fl {

class ScreenMap;
class ActiveStripData;

/**
 * Async-aware canvas size setter
 * Sets up screen mapping for LED strips with async JavaScript callback support
 * 
 * @param cledcontoller_id Controller ID for the LED strip
 * @param screenmap Screen mapping configuration
 * 
 * Note: This function calls async JavaScript callbacks that may return Promises.
 * When Asyncify is enabled, Promise returns will be properly awaited.
 */
void jsSetCanvasSize(int cledcontoller_id, const fl::ScreenMap &screenmap);

/**
 * Async-aware frame processing function
 * Processes frame data and calls async JavaScript frame handlers
 * 
 * @param active_strips Current strip data with pixel information
 * 
 * Note: This function calls async JavaScript callbacks that may return Promises.
 * Frame processing will await JavaScript Promise returns when Asyncify is enabled.
 */
void jsOnFrame(ActiveStripData &active_strips);

/**
 * Async-aware strip addition notification
 * Notifies JavaScript of new LED strip registration
 * 
 * @param strip Strip identifier
 * @param num_leds Number of LEDs in the strip
 * 
 * Note: This function calls async JavaScript callbacks that may return Promises.
 * Strip addition will await JavaScript Promise returns when Asyncify is enabled.
 */
void jsOnStripAdded(uintptr_t strip, uint32_t num_leds);

/**
 * Async-aware UI update function
 * Sends UI updates to JavaScript with async callback support
 * 
 * @param jsonStr JSON string containing UI update data
 * 
 * Note: This function calls async JavaScript callbacks that may return Promises.
 * UI updates will await JavaScript Promise returns when Asyncify is enabled.
 */
void updateJs(const char *jsonStr);

} // namespace fl
