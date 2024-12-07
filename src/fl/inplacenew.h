#pragma once

// This file must not be in the fl namespace, it must be in the global namespace.

#if __has_include(<new>)
#include <new>
#else
inline void* operator new(size_t, void* ptr) noexcept {
    return ptr;
}
#endif
