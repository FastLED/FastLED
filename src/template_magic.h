#pragma once

/*
Provides eanble_if and is_derived for compilers before C++14.
*/

#include <stdint.h>

#include "namespace.h"

namespace fl {  // mandatory namespace to prevent name collision with std::enable_if.

// Define enable_if for SFINAE
template <bool Condition, typename T = void>
struct enable_if {};

// Specialization for true condition
template <typename T>
struct enable_if<true, T> {
    using type = T;
};

// if enable_if<Condition, T> is true, then there will be a member type
// called type. Otherwise it will not exist. This is (ab)used to enable
// constructors and other functions based on template parameters. If there
// is no member type, then the compiler will not fail to bind to the target
// function or operation.
template <bool Condition, typename T = void>
using enable_if_t = typename enable_if<Condition, T>::type;

// Define is_base_of to check inheritance relationship
template <typename Base, typename Derived>
struct is_base_of {
private:
    typedef uint8_t yes;
    typedef uint16_t no;
    static yes test(Base*); // Matches if Derived is convertible to Base*
    static no test(...);  // Fallback if not convertible
    enum {
        kSizeDerived = sizeof(test(static_cast<Derived*>(nullptr))),
    };
public:
    static constexpr bool value = (kSizeDerived == sizeof(yes));
};

// Define is_base_of_v for compatibility with pre-C++14
// Replaced variable template with a constant static member
template <typename Base, typename Derived>
struct is_base_of_v_helper {
    static constexpr bool value = is_base_of<Base, Derived>::value;
};


// This uses template magic to maybe generate a type for the given condition. If that type
// doesn't exist then a type will fail to be generated, and the compiler will skip the
// consideration of a target function. This is useful for enabling template constructors
// that only become available if the class can be upcasted to the desired type.
//
// Example:
// This is an optional upcasting constructor for a Ref<T>. If the type U is not derived from T
// then the constructor will not be generated, and the compiler will skip it.
//
// template <typename U, typename = fl::is_derived<T, U>>
// Ref(const Ref<U>& refptr) : referent_(refptr.get());
template <typename Base, typename Derived>
using is_derived = enable_if_t<is_base_of<Base, Derived>::value>;

} // namespace fl
