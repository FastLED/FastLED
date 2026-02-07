#pragma once

// ok no namespace fl

/// @file is_posix.h
/// POSIX and OS platform detection header
///
/// This header detects operating systems and POSIX compliance by checking
/// compiler-defined macros. OS identity (Apple, Linux, BSD) is orthogonal
/// to POSIX capability detection.
///
/// Defines:
/// - FL_IS_LINUX: Linux systems
/// - FL_IS_APPLE: macOS/Darwin systems
/// - FL_IS_BSD: BSD variants (FreeBSD, OpenBSD, NetBSD)
/// - FL_IS_POSIX: General POSIX-compliant platform (derived from above + __unix__)

// ============================================================================
// FL_IS_LINUX - Linux systems
// ============================================================================
#if defined(__linux__)
#define FL_IS_LINUX
#endif

// ============================================================================
// FL_IS_APPLE - macOS/Darwin systems
// ============================================================================
#if defined(__APPLE__)
#define FL_IS_APPLE
#endif

// ============================================================================
// FL_IS_BSD - BSD variants (FreeBSD, OpenBSD, NetBSD)
// ============================================================================
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#define FL_IS_BSD
#endif

// ============================================================================
// FL_IS_POSIX - General POSIX-compliant platform detection
// ============================================================================
// Exclude WASM from POSIX detection (WASM has its own platform-specific implementation)
#if !defined(__EMSCRIPTEN__) && !defined(__wasm__) && !defined(EMSCRIPTEN)
#if defined(FL_IS_LINUX) || defined(FL_IS_APPLE) || defined(FL_IS_BSD) || defined(__unix__) || defined(_POSIX_VERSION)
#define FL_IS_POSIX
#endif
#endif
