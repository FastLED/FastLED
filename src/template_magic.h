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


// Convienence macro to define an output operator for a class to make it compatible
// with std::ostream. This is useful for debugging and logging. The operator will
// be defined as "os" and the right hand object will be named "obj".
//
// Example:
//  FASTLED_DEFINE_OUTPUT_OPERATOR(CRGB) {
//      os <<("CRGB(");
//      os <<(static_cast<int>(obj.r));
//      os <<(", ");
//      os <<(static_cast<int>(obj.g));
//      os <<(", ");
//      os <<(static_cast<int>(obj.b));
//      os <<(")");
//      return os;
//  }
//
// This is needed because in C++ there is two phase lookup, in which ONLY the first
// parameter will be considered if matched, even if the second argument is a non match.
//
// This macro get's around this issue.
//
// Consider the following templated operator definition:
// template<typename OutputStream>
// OutputStream &operator<<(OutputStream &os, const Str &str) {
//    os << str.c_str();
//    return os;
// }
//
// You would think this would only match if the left hand side is an ostream and the
// second parameter is "Str", but you would be wrong, because of two phase lookup
// this function will be considered for any type of ostream and ANY type of second
// parameter, even "float" or "int".
//
// This means that normally, this template will match std::stream << float
// then fail because of ambiguity, even though the second template is not
// a match. Therefore we use the enable_if which will generate a type if
// and only if the the second condition is a match.
//
// This essentially forces two phase lookup in one pass. Making the compiler skip
// the definition if the second parameter doesn't match.
#define FASTLED_DEFINE_OUTPUT_OPERATOR(CLASS)                  \
template <typename T, typename U>                              \
typename fl::enable_if<fl::is_same<U, CLASS>::value, T&>::type \
operator<<(T& os, const CLASS& obj)
