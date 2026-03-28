#pragma once

// FL_NOEXCEPT: intentionally a noop on all platforms.
// noexcept was causing too many platform compatibility issues (AVR, WASM, etc.)
// so it is disabled everywhere until a robust cross-platform solution is found.

#ifndef FL_NOEXCEPT
#define FL_NOEXCEPT
#endif
