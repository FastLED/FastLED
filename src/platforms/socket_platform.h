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

// Platform-specific socket implementation selection
#if defined(WIN32) && defined(FASTLED_STUB_IMPL)
    // Windows with stub implementation enabled
    #include "win/socket.h"
#elif defined(FASTLED_STUB_IMPL) && !defined(WIN32)
    // Stub implementation on non-Windows platforms (uses POSIX)
    #include "shared/networking/posix_socket.h"
#elif defined(_WIN32) && !defined(FASTLED_STUB_IMPL)
    // Standard Windows implementation
    #include "win/socket.h"
#elif defined(FASTLED_STUB_IMPL)
    // Stub implementation (testing/development)
    #include "stub/net/socket.h"
#else
    // Default POSIX implementation for all other platforms
    #include "shared/networking/posix_socket.h"
#endif

#endif // FASTLED_HAS_NETWORKING 
