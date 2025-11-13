#pragma once

// ok no namespace fl

/// @file is_stub.h
/// Stub platform detection header
///
/// This header detects stub/testing platforms used for native builds and unit tests.
/// The stub platform provides no-op hardware implementations for testing FastLED
/// on desktop/host systems.
///
/// Defines:
/// - FL_IS_STUB: Stub/testing platform (native builds, unit tests, host environments)
///
/// Note: WebAssembly sets FASTLED_STUB_IMPL but is NOT detected as stub (has FL_IS_WASM instead)

// ============================================================================
// FL_IS_STUB - Stub/testing platform detection
// ============================================================================
// Detect stub platform, but exclude WebAssembly (which uses stub implementation
// but has its own platform detection via FL_IS_WASM)
#if defined(FASTLED_STUB_IMPL) && !defined(__EMSCRIPTEN__)
#define FL_IS_STUB
#elif defined(__x86_64__) && !defined(__EMSCRIPTEN__) && !defined(__linux__) && !defined(__APPLE__) && !defined(_WIN32)
// Catch x86_64 without other platform markers (desktop stub builds)
#define FL_IS_STUB
#endif
