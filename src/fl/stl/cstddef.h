#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED Standard Definition Types (stddef.h equivalents)
//
// Provides size_t, ptrdiff_t, max_align_t, and offsetof macro.
///////////////////////////////////////////////////////////////////////////////

// Let the toolchain's real size/ptrdiff guards own the global declarations.
// This also handles system headers included before or after FastLED headers.
// IWYU pragma: begin_keep
#include <stddef.h>
// IWYU pragma: end_keep

namespace fl {

// FastLED equivalent of std::nullptr_t
typedef decltype(nullptr) nullptr_t;

// FastLED equivalent of std::size_t and std::ptrdiff_t
typedef ::size_t size_t;
typedef ::ptrdiff_t ptrdiff_t;

// FastLED equivalent of std::max_align_t
// Use union approach (like standard library implementations) for fast compilation
// Template-based selection to handle platforms where long double == double
namespace detail {
    // Union variant with long double (for platforms with distinct long double like x86/x64)
    typedef union {
        long long ll;
        long double ld;
        void* p;
    } max_align_with_ld;

    // Union variant without long double (for ARM/AVR where long double == double)
    typedef union {
        long long ll;
        double d;
        void* p;
    } max_align_without_ld;
}

// Template selector - automatically chooses correct variant at compile time
template <bool UseLD>
struct max_align_selector {
    typedef detail::max_align_with_ld type;
};

template <>
struct max_align_selector<false> {
    typedef detail::max_align_without_ld type;
};

// Select appropriate union based on whether long double is distinct from double
typedef typename max_align_selector<(sizeof(long double) > sizeof(double))>::type max_align_t;

} // namespace fl

// FastLED equivalent of offsetof macro (stddef.h)
// Computes byte offset of a member within a struct/class at compile time
// Uses the compiler builtin independently of the system offsetof macro.
#define FL_OFFSETOF(type, member) __builtin_offsetof(type, member)
