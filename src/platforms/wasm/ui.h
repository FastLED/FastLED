#pragma once
#include "fl/stl/noexcept.h"

// IWYU pragma: private

namespace fl {

/**
 * Async-aware UI component update function
 * Updates UI components from JavaScript with JSON data using async patterns.
 *
 * @param jsonStr JSON string containing UI component updates (null-terminated)
 */
void jsUpdateUiComponents(const char* jsonStr) FL_NOEXCEPT;

/**
 * Async-aware WASM UI system initializer
 * Ensures the WASM UI system is initialized with proper async support.
 * 
 * This function is called automatically when needed but can also be called explicitly.
 * It now includes better error handling and async operation support.
 * 
 * Note: This function is safe to call multiple times and includes
 * comprehensive exception handling for async initialization scenarios.
 */
void ensureWasmUiSystemInitialized() FL_NOEXCEPT;

} // namespace fl
