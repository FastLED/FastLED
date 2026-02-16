#pragma once

#include <errno.h>  // okay banned header (STL wrapper requires standard header)
// Use C header <errno.h> instead of C++ <cerrno> for AVR platform compatibility

// Bring errno functionality into fl:: namespace
// Note: errno is a macro that expands to a thread-local lvalue.
// Error constants (EEXIST, ENOENT, etc.) are also macros, so we access them
// directly rather than trying to redefine them (which would cause macro expansion conflicts).

namespace fl {

// Inline function to get current errno value
// Use this instead of accessing ::errno directly for cleaner code
inline int get_errno() {
    return errno;
}

// Inline function to set errno value
inline void set_errno(int value) {
    errno = value;
}

// Inline function to clear errno
inline void clear_errno() {
    errno = 0;
}

// Error constants are available as global macros (EEXIST, ENOENT, etc.)
// They can be used directly: if (errno == EEXIST) { ... }
// We provide them in the fl:: namespace for documentation purposes

// Note: We do NOT redefine these as they are macros and will cause conflicts.
// Instead, users can access them directly: EEXIST, ENOENT, etc.
// Or use qualified access if needed, though the macros are global.

} // namespace fl
