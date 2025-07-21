#pragma once

/// Socket Platform Delegation Header
/// 
/// This header provides platform-specific socket implementation delegation
/// based on compile-time defines. It replaces the need for separate BSD, 
/// Windows, and stub-specific socket implementations by providing a unified
/// interface that delegates to the appropriate platform implementation.
///
/// Platform Selection Logic:
/// - If WIN32 and FASTLED_STUB_IMPL are both defined -> use socket_win.h
/// - If only FASTLED_STUB_IMPL is defined (and WIN32 is not) -> use socket_posix.h  
/// - Otherwise -> use appropriate platform implementation

#ifdef FASTLED_HAS_NETWORKING

// Platform-specific socket implementations
// These headers provide platform_* functions for socket operations

#if defined(_WIN32)
    #include "win/socket_win.h"
#elif defined(__EMSCRIPTEN__)
    // WASM platforms (Emscripten, standalone WASM)
    #include "wasm/socket_wasm.h"
#elif defined(FASTLED_STUB_IMPL)
    // Stub implementation for testing
    // Note: stub socket.hpp doesn't exist yet - uses inline stubs
#else
    // POSIX platforms (Linux, macOS, etc.)
    #include "posix/socket_posix.h"
#endif

#endif // FASTLED_HAS_NETWORKING 
