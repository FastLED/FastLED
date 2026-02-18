#pragma once

/// @file assume_aligned.h
/// Pointer alignment hint for the optimizer.
///
/// fl::assume_aligned<N>(ptr) tells the compiler that ptr is aligned to N
/// bytes, enabling better codegen (SIMD loads, wider memory ops). This is
/// a hint only — it does not enforce alignment at the call site.

#include "fl/stl/cstddef.h"
#include "platforms/is_platform.h"

namespace fl {

template <fl::size_t N, typename T>
inline T *assume_aligned(T *ptr) {
#if defined(FL_IS_AVR)
    return ptr;
#elif defined(FL_IS_CLANG) || defined(FL_IS_GCC)
    return static_cast<T *>(__builtin_assume_aligned(ptr, N));
#elif defined(FL_IS_WIN_MSVC)
    // MSVC: __assume tells the optimizer the pointer is N-byte aligned.
    // No __builtin_assume_aligned equivalent exists on MSVC.
    __assume(((char *)ptr - (char *)0) % N == 0);
    return ptr;
#else
    return ptr;
#endif
}

template <fl::size_t N, typename T>
inline const T *assume_aligned(const T *ptr) {
#if defined(FL_IS_AVR)
    return ptr;
#elif defined(FL_IS_CLANG) || defined(FL_IS_GCC)
    return static_cast<const T *>(__builtin_assume_aligned(ptr, N));
#elif defined(FL_IS_WIN_MSVC)
    __assume(((const char *)ptr - (const char *)0) % N == 0);
    return ptr;
#else
    return ptr;
#endif
}

} // namespace fl
