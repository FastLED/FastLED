#pragma once

// ok no namespace fl

/// @file is_posix.h
/// POSIX platform detection header
///
/// This header detects POSIX-compliant platforms by checking compiler-defined
/// macros and defines standardized FL_IS_POSIX* macros.
///
/// Defines:
/// - FL_IS_POSIX: General POSIX-compliant platform (Linux, macOS, BSD, etc.)
/// - FL_IS_POSIX_LINUX: Linux systems
/// - FL_IS_POSIX_MACOS: macOS/Darwin systems
/// - FL_IS_POSIX_BSD: BSD variants (FreeBSD, OpenBSD, NetBSD)

// ============================================================================
// FL_IS_POSIX_LINUX - Linux systems
// ============================================================================
#if defined(__linux__)
#define FL_IS_POSIX_LINUX
#endif

// ============================================================================
// FL_IS_POSIX_MACOS - macOS/Darwin systems
// ============================================================================
#if defined(__APPLE__)
#define FL_IS_POSIX_MACOS
#endif

// ============================================================================
// FL_IS_POSIX_BSD - BSD variants (FreeBSD, OpenBSD, NetBSD)
// ============================================================================
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#define FL_IS_POSIX_BSD
#endif

// ============================================================================
// FL_IS_POSIX - General POSIX-compliant platform detection
// ============================================================================
#if defined(FL_IS_POSIX_LINUX) || defined(FL_IS_POSIX_MACOS) || defined(FL_IS_POSIX_BSD) || defined(__unix__) || defined(_POSIX_VERSION)
#define FL_IS_POSIX
#endif
