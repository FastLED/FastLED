#pragma once

// ok no namespace fl

/// @file is_wasm.h
/// WebAssembly platform detection header
///
/// This header detects WebAssembly/Emscripten platforms by checking
/// compiler-defined macros and defines standardized FL_IS_WASM macro.
///
/// Defines:
/// - FL_IS_WASM: WebAssembly platform (Emscripten)

// ============================================================================
// FL_IS_WASM - WebAssembly platform detection
// ============================================================================
#if defined(__EMSCRIPTEN__) || defined(__wasm__) || defined(EMSCRIPTEN)
#define FL_IS_WASM
#endif
