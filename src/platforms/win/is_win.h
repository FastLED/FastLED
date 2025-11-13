#pragma once

// ok no namespace fl

/// @file is_win.h
/// Windows platform detection header
///
/// This header detects Windows platforms by checking compiler-defined macros
/// and defines standardized FL_IS_WIN* macros for toolchain detection.
///
/// Defines:
/// - FL_IS_WIN: General Windows platform (any toolchain)
/// - FL_IS_WIN_MSVC: Microsoft Visual C++ compiler
/// - FL_IS_WIN_MINGW: MinGW/MinGW-w64 GCC toolchain
///
/// Note: MinGW produces native Windows executables, not POSIX binaries.
/// For bitness detection, use the raw _WIN64 macro directly.

// ============================================================================
// FL_IS_WIN - General Windows platform detection
// ============================================================================
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#define FL_IS_WIN
#endif

// ============================================================================
// FL_IS_WIN_MSVC - Microsoft Visual C++ compiler
// ============================================================================
#if defined(_MSC_VER)
#define FL_IS_WIN_MSVC
#endif

// ============================================================================
// FL_IS_WIN_MINGW - MinGW/MinGW-w64 GCC toolchain
// ============================================================================
#if defined(__MINGW32__) || defined(__MINGW64__)
#define FL_IS_WIN_MINGW
#endif

// Note: For Cygwin detection, check __CYGWIN__ directly
// Note: For 64-bit detection, check _WIN64 directly (not wrapped yet)
