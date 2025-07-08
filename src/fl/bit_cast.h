#pragma once

#include "fl/type_traits.h"
#include <string.h>  // for memcpy

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
// This avoids strict aliasing violations that occur with reinterpret_cast
template <typename To, typename From>
typename fl::enable_if<
    sizeof(To) == sizeof(From) && 
    is_bitcast_compatible<To>::value && 
    is_bitcast_compatible<From>::value,
    To
>::type
bit_cast(const From& from) noexcept {
    To result;
    // Use memcpy for safe type-punning - compiler will optimize this away
    // when both types are bitcast compatible and same size
    memcpy(&result, &from, sizeof(To));
    return result;
}

// Overload for pointer types - converts storage pointer to typed pointer safely
template <typename To>
To* bit_cast_ptr(void* storage) noexcept {
    return static_cast<To*>(storage);
}

template <typename To>
const To* bit_cast_ptr(const void* storage) noexcept {
    return static_cast<const To*>(storage);
}

// Additional utility for uintptr_t conversions (common pattern in the codebase)
template <typename T>
uintptr_t ptr_to_int(T* ptr) noexcept {
    union { void* p; uintptr_t i; } u;
    u.p = const_cast<void*>(static_cast<const void*>(ptr));
    return u.i;
}

template <typename T>
T* int_to_ptr(uintptr_t value) noexcept {
    union { void* p; uintptr_t i; } u;
    u.i = value;
    return static_cast<T*>(u.p);
}

} // namespace fl
