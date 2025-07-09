#pragma once

// What is bit cast?
// Bit cast is a safe version of reinterpret_cast that is robust against strict aliasing rules
// that are used in aggressive compiler optimizations.

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
To bit_cast(const From& from) noexcept {
    static_assert(sizeof(To) == sizeof(From), "bit_cast: types must have the same size");
    static_assert(is_bitcast_compatible<To>::value, "bit_cast: destination type must be bitcast compatible");
    static_assert(is_bitcast_compatible<From>::value, "bit_cast: source type must be bitcast compatible");
    
    union {  // robust against strict aliasing rules
        From from_val;
        To to_val;
    } u;
    u.from_val = from;
    return u.to_val;
}

// Overload for pointer types - converts storage pointer to typed pointer safely
template <typename To>
To* bit_cast_ptr(void* storage) noexcept {
    return bit_cast<To*>(storage);
}

template <typename To>
const To* bit_cast_ptr(const void* storage) noexcept {
    return bit_cast<const To*>(storage);
}

// Additional utility for uptr conversions (common pattern in the codebase)
template <typename T>
uptr ptr_to_int(T* ptr) noexcept {
    return bit_cast<uptr>(ptr);
}

template <typename T>
T* int_to_ptr(uptr value) noexcept {
    return bit_cast<T*>(value);
}

} // namespace fl
