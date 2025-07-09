#pragma once

#include "fl/type_traits.h"
#include "fl/int.h"

namespace fl {

//-------------------------------------------------------------------------------
// bit_cast - Safe type-punning utility (C++20 std::bit_cast equivalent)
//-------------------------------------------------------------------------------

// Helper trait for bitcast - check if a type can be bitcast (relax POD requirement)
template <typename T>
struct is_bitcast_compatible {
    static constexpr bool value = fl::is_integral<T>::value || 
                                  fl::is_floating_point<T>::value ||
                                  fl::is_pod<T>::value;
};

// Specializations for const types
template <typename T>
struct is_bitcast_compatible<const T> {
    static constexpr bool value = is_bitcast_compatible<T>::value;
};

// Specializations for pointer types
template <typename T>
struct is_bitcast_compatible<T*> {
    static constexpr bool value = true;
};

// C++20-style bit_cast for safe type reinterpretation
// Uses union for zero-cost type punning - compiler optimizes to direct assignment
template <typename To, typename From>
typename fl::enable_if<
    sizeof(To) == sizeof(From) && 
    is_bitcast_compatible<To>::value && 
    is_bitcast_compatible<From>::value,
    To
>::type
bit_cast(const From& from) noexcept {
    union {
        From from_val;
        To to_val;
    } u;
    u.from_val = from;
    return u.to_val;
}

// Overload for pointer types - converts storage pointer to typed pointer safely
template <typename To>
To* bit_cast_ptr(void* storage) noexcept {
    static_assert(sizeof(uptr) == sizeof(void*), "uptr must be the same size as void*");
    return static_cast<To*>(storage);
}

template <typename To>
const To* bit_cast_ptr(const void* storage) noexcept {
    return static_cast<const To*>(storage);
}

// Additional utility for uptr conversions (common pattern in the codebase)
template <typename T>
uptr ptr_to_int(T* ptr) noexcept {
    static_assert(sizeof(uptr) == sizeof(void*), "uptr must be the same size as void*");
    union { void* p; uptr i; } u;
    u.p = const_cast<void*>(static_cast<const void*>(ptr));
    return u.i;
}

template <typename T>
T* int_to_ptr(uptr value) noexcept {
    static_assert(sizeof(uptr) == sizeof(void*), "uptr must be the same size as void*");
    union { void* p; uptr i; } u;
    u.i = value;
    return static_cast<T*>(u.p);
}

} // namespace fl
