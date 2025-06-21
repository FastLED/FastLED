#pragma once

#include <string>

namespace fl {

/**
 * Update UI components from JavaScript with JSON data.
 * This is a thin wrapper around the generic UI system.
 * 
 * @param jsonStr JSON string containing UI component updates
 */
void jsUpdateUiComponents(const std::string &jsonStr);

/**
 * Ensure the WASM UI system is initialized. This function is called
 * automatically when needed but can also be called explicitly.
 */
void ensureWasmUiSystemInitialized();

} // namespace fl
