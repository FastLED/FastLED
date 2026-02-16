#pragma once

#include "fl/int.h"
#include "platforms/is_platform.h"
#include "fl/stl/cerrno.h"    // For fl::get_errno()
#include "fl/stl/cstring.h"   // For fl::strerror()

#ifdef FASTLED_TESTING
#include <cstdio>   // For ::FILE*, ::fopen, ::fclose, etc.
#endif

// Platform-agnostic file I/O abstraction
// Provides fl::FILE* and fl::fopen/fclose/etc. functions that platforms can implement
//
// Supported platforms:
// - POSIX/Testing (FASTLED_TESTING): Maps to standard FILE* and libc functions
// - Embedded (future): Can map to LittleFS, SPIFFS, SD card APIs, etc.

namespace fl {

// ============================================================================
// Opaque File Handle Type
// ============================================================================

// Forward declare the platform-specific file handle structure
// Each platform defines this in their implementation
struct FILE_impl;

// Opaque file handle pointer (like standard FILE*)
using FILE = FILE_impl;

// ============================================================================
// File I/O Constants
// ============================================================================

namespace io {
    // Seek origin constants (match SEEK_SET, SEEK_CUR, SEEK_END)
    constexpr int seek_set = 0;
    constexpr int seek_cur = 1;
    constexpr int seek_end = 2;

    // Error code for "bad file descriptor" (matches EBADF = 9 on POSIX)
    constexpr int err_bad_file = 9;
}

// ============================================================================
// Platform-Agnostic File I/O Functions
// ============================================================================
// These functions provide a consistent interface across all platforms.
// Each platform implements these in their own way.

/// Open a file
/// @param path File path
/// @param mode File mode string (e.g., "r", "w", "rb", "wb", "r+", "w+", "a", "ab")
/// @return File handle pointer, or nullptr on failure
FILE* fopen(const char* path, const char* mode);

/// Close a file
/// @param file File handle
/// @return 0 on success, non-zero on error
int fclose(FILE* file);

/// Read from file
/// @param buffer Destination buffer
/// @param size Size of each element
/// @param count Number of elements to read
/// @param file File handle
/// @return Number of elements actually read
fl::size_t fread(void* buffer, fl::size_t size, fl::size_t count, FILE* file);

/// Write to file
/// @param data Source data
/// @param size Size of each element
/// @param count Number of elements to write
/// @param file File handle
/// @return Number of elements actually written
fl::size_t fwrite(const void* data, fl::size_t size, fl::size_t count, FILE* file);

/// Get current file position
/// @param file File handle
/// @return Current position, or -1 on error
long ftell(FILE* file);

/// Set file position
/// @param file File handle
/// @param offset Offset from origin
/// @param origin Seek origin (io::seek_set, io::seek_cur, io::seek_end)
/// @return 0 on success, non-zero on error
int fseek(FILE* file, long offset, int origin);

/// Flush file buffers
/// @param file File handle
/// @return 0 on success, non-zero on error
int fflush(FILE* file);

// On some platforms (e.g., AVR), feof, ferror, and clearerr are macros
// Save them only if they exist, undefine temporarily to declare our functions
// Only needed on platforms where they are actually macros (AVR)
#ifdef FL_IS_AVR
#ifdef feof
#define FL_MACRO_NEEDS_RESTORE_feof
#pragma push_macro("feof")
#undef feof
#endif
#ifdef ferror
#define FL_MACRO_NEEDS_RESTORE_ferror
#pragma push_macro("ferror")
#undef ferror
#endif
#ifdef clearerr
#define FL_MACRO_NEEDS_RESTORE_clearerr
#pragma push_macro("clearerr")
#undef clearerr
#endif
#endif // FL_IS_AVR

/// Check for end-of-file
/// @param file File handle
/// @return Non-zero if EOF, 0 otherwise
#ifdef FL_IS_AVR
int (feof)(FILE* file);
#else
int feof(FILE* file);
#endif

/// Check for file error
/// @param file File handle
/// @return Non-zero if error, 0 otherwise
#ifdef FL_IS_AVR
int (ferror)(FILE* file);
#else
int ferror(FILE* file);
#endif

/// Clear file error indicators
/// @param file File handle
#ifdef FL_IS_AVR
void (clearerr)(FILE* file);
#else
void clearerr(FILE* file);
#endif

// Note: On AVR, we use parentheses around function names to prevent macro expansion
// and we do NOT restore the macros (see comment at end of file for rationale)

// Note: fl::get_errno() and fl::strerror() are already defined in fl/stl/cerrno.h and fl/stl/cstring.h
// We don't redeclare them here to avoid conflicts

} // namespace fl

// ============================================================================
// Platform-Specific Implementations
// ============================================================================

#ifdef FASTLED_TESTING
// POSIX/Testing platform: Use standard C FILE* and libc functions

namespace fl {

// On POSIX platforms, fl::FILE is just the standard FILE
struct FILE_impl : public ::FILE {};

// Map fl:: functions to standard libc functions
// Note: static_cast is safe here because FILE_impl inherits from ::FILE
// This is a platform abstraction layer that wraps the POSIX FILE* API
inline FILE* fopen(const char* path, const char* mode) {
    return static_cast<FILE*>(::fopen(path, mode));
}

inline int fclose(FILE* file) {
    return ::fclose(static_cast<::FILE*>(file));
}

inline fl::size_t fread(void* buffer, fl::size_t size, fl::size_t count, FILE* file) {
    return ::fread(buffer, size, count, static_cast<::FILE*>(file));
}

inline fl::size_t fwrite(const void* data, fl::size_t size, fl::size_t count, FILE* file) {
    return ::fwrite(data, size, count, static_cast<::FILE*>(file));
}

inline long ftell(FILE* file) {
    return ::ftell(static_cast<::FILE*>(file));
}

inline int fseek(FILE* file, long offset, int origin) {
    return ::fseek(static_cast<::FILE*>(file), offset, origin);
}

inline int fflush(FILE* file) {
    return ::fflush(static_cast<::FILE*>(file));
}

#ifdef FL_IS_AVR
inline int (feof)(FILE* file) {
    return ::feof(static_cast<::FILE*>(file));
}
#else
inline int feof(FILE* file) {
    return ::feof(static_cast<::FILE*>(file));
}
#endif

#ifdef FL_IS_AVR
inline int (ferror)(FILE* file) {
    return ::ferror(static_cast<::FILE*>(file));
}
#else
inline int ferror(FILE* file) {
    return ::ferror(static_cast<::FILE*>(file));
}
#endif

#ifdef FL_IS_AVR
inline void (clearerr)(FILE* file) {
    ::clearerr(static_cast<::FILE*>(file));
}
#else
inline void clearerr(FILE* file) {
    ::clearerr(static_cast<::FILE*>(file));
}
#endif

// Note: fl::get_errno() and fl::strerror() are defined in fl/stl/cerrno.h and fl/stl/cstring.h
// No implementation needed here - those files provide them

} // namespace fl

#else // !FASTLED_TESTING

// ============================================================================
// Embedded Platform (No File System)
// ============================================================================
// Provides no-op implementations that fail gracefully

namespace fl {

// Minimal stub structure for embedded platforms
struct FILE_impl {
    int dummy;  // Placeholder to make it a valid type
};

// No-op implementations
inline FILE* fopen(const char* /*path*/, const char* /*mode*/) {
    return nullptr;
}

inline int fclose(FILE* /*file*/) {
    return -1;
}

inline fl::size_t fread(void* /*buffer*/, fl::size_t /*size*/, fl::size_t /*count*/, FILE* /*file*/) {
    return 0;
}

inline fl::size_t fwrite(const void* /*data*/, fl::size_t /*size*/, fl::size_t /*count*/, FILE* /*file*/) {
    return 0;
}

inline long ftell(FILE* /*file*/) {
    return -1;
}

inline int fseek(FILE* /*file*/, long /*offset*/, int /*origin*/) {
    return -1;
}

inline int fflush(FILE* /*file*/) {
    return -1;
}

#ifdef FL_IS_AVR
inline int (feof)(FILE* /*file*/) {
    return 0;
}
#else
inline int feof(FILE* /*file*/) {
    return 0;
}
#endif

#ifdef FL_IS_AVR
inline int (ferror)(FILE* /*file*/) {
    return 1;
}
#else
inline int ferror(FILE* /*file*/) {
    return 1;
}
#endif

#ifdef FL_IS_AVR
inline void (clearerr)(FILE* /*file*/) {
}
#else
inline void clearerr(FILE* /*file*/) {
}
#endif

// Note: fl::get_errno() and fl::strerror() are defined in fl/stl/cerrno.h and fl/stl/cstring.h
// We use those implementations instead of defining our own here

} // namespace fl

#endif // FASTLED_TESTING

// DO NOT restore the feof/ferror/clearerr macros on AVR
// Rationale: Even with parentheses in function declarations, restoring these macros causes
// them to expand at call sites like `fl::feof(file)`, which breaks compilation.
// Macro expansion happens before namespace resolution, so `fl::feof(x)` becomes
// `fl::((x)->flags & _FEOF)` on AVR, which is invalid syntax.
//
// Solution: We only undefine these macros on AVR (where they exist as macros), and we
// use parentheses around function names `(feof)`, `(ferror)`, `(clearerr)` in declarations
// and definitions to prevent macro expansion. We do NOT restore the macros.
//
// On non-AVR platforms where these are regular functions (not macros), we use normal
// function syntax without parentheses.
