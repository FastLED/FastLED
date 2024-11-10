#pragma once

/*
Provides eanble_if and is_derived for compilers before C++14.
*/

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

// Helper alias to simplify usage
template <bool Condition, typename T = void>
using enable_if_t = typename enable_if<Condition, T>::type;

// Define is_base_of to check inheritance relationship
template <typename Base, typename Derived>
struct is_base_of {
private:
    static char test(Base*); // Matches if Derived is convertible to Base*
    static int test(...);    // Fallback if not convertible
public:
    static constexpr bool value = sizeof(test(static_cast<Derived*>(nullptr))) == sizeof(char);
};

// Define is_base_of_v for compatibility with pre-C++14
// Replaced variable template with a constant static member
template <typename Base, typename Derived>
struct is_base_of_v_helper {
    static constexpr bool value = is_base_of<Base, Derived>::value;
};


// Example of of how to use is_base_of_v to check if Derived is a subclass of Base
// and enable a constructor if it is.
// template <typename U, typename = fl::is_derived<T, U>>
// Ref(const Ref<U>& refptr) : referent_(refptr.get()) {
//     if (referent_ && isOwned()) {
//         referent_->ref();
//     }
// }
template <typename Base, typename Derived>
using is_derived = enable_if_t<is_base_of<Base, Derived>::value>;

} // namespace fl
