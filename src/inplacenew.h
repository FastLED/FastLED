#pragma once


#if __has_include(<new>)
#include <new>
#else
inline void* operator new(size_t, void* ptr) noexcept {
    return ptr;
}
#endif
