#pragma once

// FL_NO_EXCEPT: intentionally a noop on all platforms.
// noexcept was causing too many platform compatibility issues (AVR, WASM, etc.)
// so it is disabled everywhere until a robust cross-platform solution is found.

#ifndef FL_NO_EXCEPT
#define FL_NO_EXCEPT
#endif

// FL_HAS_NOEXCEPT: defined (as 1) when FL_NO_EXCEPT actually expands to the
// real noexcept keyword.  Currently FL_NO_EXCEPT is always a noop, so
// FL_HAS_NOEXCEPT is never defined.  Code that checks whether noexcept is
// in effect (e.g. static_assert(noexcept(...))) must be gated on this macro.
//
// When noexcept support is eventually re-enabled for a platform, add:
//   #define FL_NO_EXCEPT noexcept
//   #define FL_HAS_NOEXCEPT 1
