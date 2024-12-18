#pragma once

// This file must not be in the fl namespace, it must be in the global namespace.

#ifdef __AVR__
inline void* operator new(size_t, void* ptr) noexcept {
    return ptr;
}
#elif __has_include(<new>)
#include <new>
#elif __has_include(<new.h>)
#include <new.h>
#elif __has_include("new.h")
#include "new.h"
#endif

