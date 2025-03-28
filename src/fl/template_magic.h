#pragma once

/*
Provides eanble_if and is_derived for compilers before C++14.
*/

#include <stdint.h>

#include "fl/namespace.h"

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

// Define is_same trait
template <typename T, typename U>
struct is_same {
    static constexpr bool value = false;
};

// Specialization for when T and U are the same type
template <typename T>
struct is_same<T, T> {
    static constexpr bool value = true;
};

// Define is_same_v for compatibility with variable templates
template <typename T, typename U>
struct is_same_v_helper {
    static constexpr bool value = is_same<T, U>::value;
};

// Define is_pod trait (basic implementation)
template <typename T>
struct is_pod {
    static constexpr bool value = false;  // Default to false for safety
};

// Specializations for fundamental types
template<> struct is_pod<bool> { static constexpr bool value = true; };
template<> struct is_pod<char> { static constexpr bool value = true; };
template<> struct is_pod<signed char> { static constexpr bool value = true; };
template<> struct is_pod<unsigned char> { static constexpr bool value = true; };
template<> struct is_pod<short> { static constexpr bool value = true; };
template<> struct is_pod<unsigned short> { static constexpr bool value = true; };
template<> struct is_pod<int> { static constexpr bool value = true; };
template<> struct is_pod<unsigned int> { static constexpr bool value = true; };
template<> struct is_pod<long> { static constexpr bool value = true; };
template<> struct is_pod<unsigned long> { static constexpr bool value = true; };
template<> struct is_pod<long long> { static constexpr bool value = true; };
template<> struct is_pod<unsigned long long> { static constexpr bool value = true; };
template<> struct is_pod<float> { static constexpr bool value = true; };
template<> struct is_pod<double> { static constexpr bool value = true; };
template<> struct is_pod<long double> { static constexpr bool value = true; };

// Helper struct for is_pod_v (similar to other _v helpers)
template <typename T>
struct is_pod_v_helper {
    static constexpr bool value = is_pod<T>::value;
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





// For comparison operators that return bool against pod data. The class obj will need
// to supply the comparison operator for the pod type.
// This example will show how to define a comparison operator for a class that can be
// compared against a pod type.
// Example:
//   FASTLED_DEFINE_POD_COMPARISON_OPERATOR(Myclass, >=) will allow MyClass to be compared
//   MyClass obj;
//   return obj >= 0;
#define FASTLED_DEFINE_POD_COMPARISON_OPERATOR(CLASS, OP) \
template <typename T, typename U>  \
typename fl::enable_if<fl::is_same<U, CLASS>::value && fl::is_pod<T>::value, bool>::type \
operator OP (const T& pod, const CLASS& obj) { return pod OP obj; } \
template <typename T> \
typename fl::enable_if<fl::is_pod<T>::value, bool>::type \
operator OP (const CLASS& obj, const T& pod) { return obj OP pod; }
