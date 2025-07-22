#pragma once

#include <string>

namespace fl {

/**
 * Async-aware UI component update function
 * Updates UI components from JavaScript with JSON data using async patterns.
 * 
 * This function now supports async operations and provides better error handling.
 * When called from JavaScript via Asyncify-enabled WASM, it can handle both
 * synchronous and asynchronous UI update operations.
 * 
 * @param jsonStr JSON string containing UI component updates
 * 
 * Note: This function may be called from async JavaScript contexts and
 * includes comprehensive error handling for async operations.
 */
void jsUpdateUiComponents(const std::string &jsonStr);

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
void ensureWasmUiSystemInitialized();

} // namespace fl
