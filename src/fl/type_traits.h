#pragma once

/*
Provides eanble_if and is_derived for compilers before C++14.
*/

#include <stddef.h>
#include <stdint.h>

#include "fl/namespace.h"

namespace fl { // mandatory namespace to prevent name collision with
               // std::enable_if.

// Define enable_if for SFINAE
template <bool Condition, typename T = void> struct enable_if {};

// Specialization for true condition
template <typename T> struct enable_if<true, T> {
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
template <typename Base, typename Derived> struct is_base_of {
  private:
    typedef uint8_t yes;
    typedef uint16_t no;
    static yes test(Base *); // Matches if Derived is convertible to Base*
    static no test(...);     // Fallback if not convertible
    enum {
        kSizeDerived = sizeof(test(static_cast<Derived *>(nullptr))),
    };

  public:
    static constexpr bool value = (kSizeDerived == sizeof(yes));
};

// Define is_base_of_v for compatibility with pre-C++14
// Replaced variable template with a constant static member
template <typename Base, typename Derived> struct is_base_of_v_helper {
    static constexpr bool value = is_base_of<Base, Derived>::value;
};

// Define is_same trait
template <typename T, typename U> struct is_same {
    static constexpr bool value = false;
};

// Specialization for when T and U are the same type
template <typename T> struct is_same<T, T> {
    static constexpr bool value = true;
};

// Define is_same_v for compatibility with variable templates
template <typename T, typename U> struct is_same_v_helper {
    static constexpr bool value = is_same<T, U>::value;
};

// Define remove_reference trait
template <typename T> struct remove_reference {
    using type = T;
};

// Specialization for lvalue reference
template <typename T> struct remove_reference<T &> {
    using type = T;
};

// Specialization for rvalue reference
template <typename T> struct remove_reference<T &&> {
    using type = T;
};

// Helper alias template for remove_reference
template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

// Define conditional trait
template <bool B, typename T, typename F> struct conditional {
    using type = T;
};

template <typename T, typename F> struct conditional<false, T, F> {
    using type = F;
};

template <bool B, typename T, typename F>
using conditional_t = typename conditional<B, T, F>::type;

// Define is_array trait
template <typename T> struct is_array {
    static constexpr bool value = false;
};

template <typename T> struct is_array<T[]> {
    static constexpr bool value = true;
};

template <typename T, size_t N> struct is_array<T[N]> {
    static constexpr bool value = true;
};

// Define remove_extent trait
template <typename T> struct remove_extent {
    using type = T;
};

template <typename T> struct remove_extent<T[]> {
    using type = T;
};

template <typename T, size_t N> struct remove_extent<T[N]> {
    using type = T;
};

// Define is_function trait
template <typename T> struct is_function {
    static constexpr bool value = false;
};

template <typename Ret, typename... Args> struct is_function<Ret(Args...)> {
    static constexpr bool value = true;
};

template <typename Ret, typename... Args>
struct is_function<Ret(Args...) const> {
    static constexpr bool value = true;
};

template <typename Ret, typename... Args>
struct is_function<Ret(Args...) volatile> {
    static constexpr bool value = true;
};

template <typename Ret, typename... Args>
struct is_function<Ret(Args...) const volatile> {
    static constexpr bool value = true;
};

// Define add_pointer trait
template <typename T> struct add_pointer {
    using type = T *;
};

template <typename T> struct add_pointer<T &> {
    using type = T *;
};

template <typename T> struct add_pointer<T &&> {
    using type = T *;
};

template <typename T> using add_pointer_t = typename add_pointer<T>::type;

// Define remove_const trait
template <typename T> struct remove_const {
    using type = T;
};

template <typename T> struct remove_const<const T> {
    using type = T;
};

// Implementation of move
template <typename T>
constexpr typename remove_reference<T>::type &&move(T &&t) noexcept {
    return static_cast<typename remove_reference<T>::type &&>(t);
}

// Define is_lvalue_reference trait
template <typename T> struct is_lvalue_reference {
    static constexpr bool value = false;
};

template <typename T> struct is_lvalue_reference<T &> {
    static constexpr bool value = true;
};

// Implementation of forward
template <typename T>
constexpr T &&forward(typename remove_reference<T>::type &t) noexcept {
    return static_cast<T &&>(t);
}

// Overload for rvalue references
template <typename T>
constexpr T &&forward(typename remove_reference<T>::type &&t) noexcept {
    static_assert(!is_lvalue_reference<T>::value,
                  "Cannot forward an rvalue as an lvalue");
    return static_cast<T &&>(t);
}

// Define remove_cv trait
template <typename T> struct remove_cv {
    using type = T;
};

template <typename T> struct remove_cv<const T> {
    using type = T;
};

template <typename T> struct remove_cv<volatile T> {
    using type = T;
};

template <typename T> struct remove_cv<const volatile T> {
    using type = T;
};

template <typename T> using remove_cv_t = typename remove_cv<T>::type;

// Define decay trait
template <typename T> struct decay {
  private:
    using U = typename remove_reference<T>::type;

  public:
    using type = typename conditional<
        is_array<U>::value, typename remove_extent<U>::type *,
        typename conditional<is_function<U>::value,
                             typename add_pointer<U>::type,
                             typename remove_cv<U>::type>::type>::type;
};

template <typename T> using decay_t = typename decay<T>::type;

// Define is_pod trait (basic implementation)
template <typename T> struct is_pod {
    static constexpr bool value = false; // Default to false for safety
};

// Specializations for fundamental types
template <> struct is_pod<bool> {
    static constexpr bool value = true;
};
template <> struct is_pod<char> {
    static constexpr bool value = true;
};
template <> struct is_pod<signed char> {
    static constexpr bool value = true;
};
template <> struct is_pod<unsigned char> {
    static constexpr bool value = true;
};
template <> struct is_pod<short> {
    static constexpr bool value = true;
};
template <> struct is_pod<unsigned short> {
    static constexpr bool value = true;
};
template <> struct is_pod<int> {
    static constexpr bool value = true;
};
template <> struct is_pod<unsigned int> {
    static constexpr bool value = true;
};
template <> struct is_pod<long> {
    static constexpr bool value = true;
};
template <> struct is_pod<unsigned long> {
    static constexpr bool value = true;
};
template <> struct is_pod<long long> {
    static constexpr bool value = true;
};
template <> struct is_pod<unsigned long long> {
    static constexpr bool value = true;
};
template <> struct is_pod<float> {
    static constexpr bool value = true;
};
template <> struct is_pod<double> {
    static constexpr bool value = true;
};
template <> struct is_pod<long double> {
    static constexpr bool value = true;
};

// Helper struct for is_pod_v (similar to other _v helpers)
template <typename T> struct is_pod_v_helper {
    static constexpr bool value = is_pod<T>::value;
};

//----------------------------------------------------------------------------
// trait to detect pointer‑to‑member‑function
// must come before Function so SFINAE sees it
//----------------------------------------------------------------------------
template <typename T> struct is_member_function_pointer;
template <typename C, typename Ret, typename... A>
struct is_member_function_pointer<Ret (C::*)(A...)>;
template <typename C, typename Ret, typename... A>
struct is_member_function_pointer<Ret (C::*)(A...) const>;

template <typename T> struct is_member_function_pointer {
    static constexpr bool value = false;
};

template <typename C, typename Ret, typename... A>
struct is_member_function_pointer<Ret (C::*)(A...)> {
    static constexpr bool value = true;
};

template <typename C, typename Ret, typename... A>
struct is_member_function_pointer<Ret (C::*)(A...) const> {
    static constexpr bool value = true;
};

//-------------------------------------------------------------------------------
// is_integral trait (built-in integer types only)
//-------------------------------------------------------------------------------
template <typename T> struct is_integral {
    static constexpr bool value = false;
};
template <> struct is_integral<bool> {
    static constexpr bool value = true;
};
template <> struct is_integral<char> {
    static constexpr bool value = true;
};
template <> struct is_integral<signed char> {
    static constexpr bool value = true;
};
template <> struct is_integral<unsigned char> {
    static constexpr bool value = true;
};
template <> struct is_integral<short> {
    static constexpr bool value = true;
};
template <> struct is_integral<unsigned short> {
    static constexpr bool value = true;
};
template <> struct is_integral<int> {
    static constexpr bool value = true;
};
template <> struct is_integral<unsigned int> {
    static constexpr bool value = true;
};
template <> struct is_integral<long> {
    static constexpr bool value = true;
};
template <> struct is_integral<unsigned long> {
    static constexpr bool value = true;
};
template <> struct is_integral<long long> {
    static constexpr bool value = true;
};
template <> struct is_integral<unsigned long long> {
    static constexpr bool value = true;
};

template <typename T> struct is_integral<const T> {
    static constexpr bool value = is_integral<T>::value;
};

template <typename T> struct is_integral<volatile T> {
    static constexpr bool value = is_integral<T>::value;
};

template <typename T> struct is_integral<T &> {
    static constexpr bool value = is_integral<T>::value;
};

//-------------------------------------------------------------------------------
// is_floating_point trait
//-------------------------------------------------------------------------------
template <typename T> struct is_floating_point {
    static constexpr bool value = false;
};
template <> struct is_floating_point<float> {
    static constexpr bool value = true;
};
template <> struct is_floating_point<double> {
    static constexpr bool value = true;
};
template <> struct is_floating_point<long double> {
    static constexpr bool value = true;
};

template <typename T> struct is_floating_point<const T> {
    static constexpr bool value = is_floating_point<T>::value;
};

template <typename T> struct is_floating_point<volatile T> {
    static constexpr bool value = is_floating_point<T>::value;
};

template <typename T> struct is_floating_point<T &> {
    static constexpr bool value = is_floating_point<T>::value;
};

//-------------------------------------------------------------------------------
// is_signed trait
//-------------------------------------------------------------------------------
template <typename T> struct is_signed {
    static constexpr bool value = false;
};
template <> struct is_signed<signed char> {
    static constexpr bool value = true;
};
template <> struct is_signed<short> {
    static constexpr bool value = true;
};
template <> struct is_signed<int> {
    static constexpr bool value = true;
};
template <> struct is_signed<long> {
    static constexpr bool value = true;
};
template <> struct is_signed<long long> {
    static constexpr bool value = true;
};
template <> struct is_signed<float> {
    static constexpr bool value = true;
};
template <> struct is_signed<double> {
    static constexpr bool value = true;
};
template <> struct is_signed<long double> {
    static constexpr bool value = true;
};
// Note: sized integer types (int8_t, int16_t, int32_t, int64_t) are typedefs
// for the basic types above, so they automatically inherit these specializations

//-------------------------------------------------------------------------------
// Type size ranking for promotion rules
//-------------------------------------------------------------------------------
template <typename T> struct type_rank {
    static constexpr int value = 0;
};
template <> struct type_rank<bool> {
    static constexpr int value = 1;
};
template <> struct type_rank<signed char> {
    static constexpr int value = 2;
};
template <> struct type_rank<unsigned char> {
    static constexpr int value = 2;
};
template <> struct type_rank<char> {
    static constexpr int value = 2;
};
template <> struct type_rank<short> {
    static constexpr int value = 3;
};
template <> struct type_rank<unsigned short> {
    static constexpr int value = 3;
};
template <> struct type_rank<int> {
    static constexpr int value = 4;
};
template <> struct type_rank<unsigned int> {
    static constexpr int value = 4;
};
template <> struct type_rank<long> {
    static constexpr int value = 5;
};
template <> struct type_rank<unsigned long> {
    static constexpr int value = 5;
};
template <> struct type_rank<long long> {
    static constexpr int value = 6;
};
template <> struct type_rank<unsigned long long> {
    static constexpr int value = 6;
};
template <> struct type_rank<float> {
    static constexpr int value = 10;
};
template <> struct type_rank<double> {
    static constexpr int value = 11;
};
template <> struct type_rank<long double> {
    static constexpr int value = 12;
};
// Note: sized integer types (int8_t, int16_t, int32_t, int64_t) are typedefs
// for the basic types above, so they automatically inherit these specializations

//-------------------------------------------------------------------------------
// Common type trait for type promotion - explicit specializations for integer types
//-------------------------------------------------------------------------------

// Primary template - fallback
template <typename T, typename U> struct common_type_impl {
    using type = T;
};

// Same type specialization - handles all cases where T == U
template <typename T> struct common_type_impl<T, T> { 
    using type = T; 
};

// Different size promotions - larger type wins
template <> struct common_type_impl<signed char, short> { using type = short; };
template <> struct common_type_impl<short, signed char> { using type = short; };
template <> struct common_type_impl<signed char, int> { using type = int; };
template <> struct common_type_impl<int, signed char> { using type = int; };
template <> struct common_type_impl<signed char, long> { using type = long; };
template <> struct common_type_impl<long, signed char> { using type = long; };
template <> struct common_type_impl<signed char, long long> { using type = long long; };
template <> struct common_type_impl<long long, signed char> { using type = long long; };

template <> struct common_type_impl<unsigned char, unsigned short> { using type = unsigned short; };
template <> struct common_type_impl<unsigned short, unsigned char> { using type = unsigned short; };
template <> struct common_type_impl<unsigned char, unsigned int> { using type = unsigned int; };
template <> struct common_type_impl<unsigned int, unsigned char> { using type = unsigned int; };
template <> struct common_type_impl<unsigned char, unsigned long> { using type = unsigned long; };
template <> struct common_type_impl<unsigned long, unsigned char> { using type = unsigned long; };
template <> struct common_type_impl<unsigned char, unsigned long long> { using type = unsigned long long; };
template <> struct common_type_impl<unsigned long long, unsigned char> { using type = unsigned long long; };

template <> struct common_type_impl<short, int> { using type = int; };
template <> struct common_type_impl<int, short> { using type = int; };
template <> struct common_type_impl<short, long> { using type = long; };
template <> struct common_type_impl<long, short> { using type = long; };
template <> struct common_type_impl<short, long long> { using type = long long; };
template <> struct common_type_impl<long long, short> { using type = long long; };

template <> struct common_type_impl<unsigned short, unsigned int> { using type = unsigned int; };
template <> struct common_type_impl<unsigned int, unsigned short> { using type = unsigned int; };
template <> struct common_type_impl<unsigned short, unsigned long> { using type = unsigned long; };
template <> struct common_type_impl<unsigned long, unsigned short> { using type = unsigned long; };
template <> struct common_type_impl<unsigned short, unsigned long long> { using type = unsigned long long; };
template <> struct common_type_impl<unsigned long long, unsigned short> { using type = unsigned long long; };

template <> struct common_type_impl<int, long> { using type = long; };
template <> struct common_type_impl<long, int> { using type = long; };
template <> struct common_type_impl<int, long long> { using type = long long; };
template <> struct common_type_impl<long long, int> { using type = long long; };

template <> struct common_type_impl<unsigned int, unsigned long> { using type = unsigned long; };
template <> struct common_type_impl<unsigned long, unsigned int> { using type = unsigned long; };
template <> struct common_type_impl<unsigned int, unsigned long long> { using type = unsigned long long; };
template <> struct common_type_impl<unsigned long long, unsigned int> { using type = unsigned long long; };

template <> struct common_type_impl<long, long long> { using type = long long; };
template <> struct common_type_impl<long long, long> { using type = long long; };

template <> struct common_type_impl<unsigned long, unsigned long long> { using type = unsigned long long; };
template <> struct common_type_impl<unsigned long long, unsigned long> { using type = unsigned long long; };

// Mixed signedness - promote to larger signed type that can hold both values  
template <> struct common_type_impl<signed char, unsigned char> { using type = short; };
template <> struct common_type_impl<unsigned char, signed char> { using type = short; };
template <> struct common_type_impl<short, unsigned short> { using type = int; };
template <> struct common_type_impl<unsigned short, short> { using type = int; };
template <> struct common_type_impl<int, unsigned int> { using type = long long; };
template <> struct common_type_impl<unsigned int, int> { using type = long long; };
template <> struct common_type_impl<long, unsigned long> { using type = long long; };
template <> struct common_type_impl<unsigned long, long> { using type = long long; };
template <> struct common_type_impl<long long, unsigned long long> { using type = long long; };
template <> struct common_type_impl<unsigned long long, long long> { using type = long long; };

// Cross signedness with different sizes - larger type wins unless mixed sign same size
template <> struct common_type_impl<signed char, unsigned short> { using type = unsigned short; };
template <> struct common_type_impl<unsigned short, signed char> { using type = unsigned short; };
template <> struct common_type_impl<signed char, unsigned int> { using type = unsigned int; };
template <> struct common_type_impl<unsigned int, signed char> { using type = unsigned int; };
template <> struct common_type_impl<signed char, unsigned long> { using type = unsigned long; };
template <> struct common_type_impl<unsigned long, signed char> { using type = unsigned long; };
template <> struct common_type_impl<signed char, unsigned long long> { using type = unsigned long long; };
template <> struct common_type_impl<unsigned long long, signed char> { using type = unsigned long long; };

template <> struct common_type_impl<unsigned char, short> { using type = short; };
template <> struct common_type_impl<short, unsigned char> { using type = short; };
template <> struct common_type_impl<unsigned char, int> { using type = int; };
template <> struct common_type_impl<int, unsigned char> { using type = int; };
template <> struct common_type_impl<unsigned char, long> { using type = long; };
template <> struct common_type_impl<long, unsigned char> { using type = long; };
template <> struct common_type_impl<unsigned char, long long> { using type = long long; };
template <> struct common_type_impl<long long, unsigned char> { using type = long long; };

template <> struct common_type_impl<short, unsigned int> { using type = unsigned int; };
template <> struct common_type_impl<unsigned int, short> { using type = unsigned int; };
template <> struct common_type_impl<short, unsigned long> { using type = unsigned long; };
template <> struct common_type_impl<unsigned long, short> { using type = unsigned long; };
template <> struct common_type_impl<short, unsigned long long> { using type = unsigned long long; };
template <> struct common_type_impl<unsigned long long, short> { using type = unsigned long long; };

template <> struct common_type_impl<unsigned short, int> { using type = int; };
template <> struct common_type_impl<int, unsigned short> { using type = int; };
template <> struct common_type_impl<unsigned short, long> { using type = long; };
template <> struct common_type_impl<long, unsigned short> { using type = long; };
template <> struct common_type_impl<unsigned short, long long> { using type = long long; };
template <> struct common_type_impl<long long, unsigned short> { using type = long long; };

template <> struct common_type_impl<int, unsigned long> { using type = unsigned long; };
template <> struct common_type_impl<unsigned long, int> { using type = unsigned long; };
template <> struct common_type_impl<int, unsigned long long> { using type = unsigned long long; };
template <> struct common_type_impl<unsigned long long, int> { using type = unsigned long long; };

template <> struct common_type_impl<unsigned int, long> { using type = long; };
template <> struct common_type_impl<long, unsigned int> { using type = long; };
template <> struct common_type_impl<unsigned int, long long> { using type = long long; };
template <> struct common_type_impl<long long, unsigned int> { using type = long long; };

template <> struct common_type_impl<long, unsigned long long> { using type = unsigned long long; };
template <> struct common_type_impl<unsigned long long, long> { using type = unsigned long long; };

template <> struct common_type_impl<unsigned long, long long> { using type = long long; };
template <> struct common_type_impl<long long, unsigned long> { using type = long long; };

// Floating point specializations (floats always win over integers)

// Mixed floating point sizes - larger wins
template <> struct common_type_impl<float, double> { using type = double; };
template <> struct common_type_impl<double, float> { using type = double; };
template <> struct common_type_impl<float, long double> { using type = long double; };
template <> struct common_type_impl<long double, float> { using type = long double; };
template <> struct common_type_impl<double, long double> { using type = long double; };
template <> struct common_type_impl<long double, double> { using type = long double; };

// Integer with float - float wins
template <> struct common_type_impl<int, float> { using type = float; };
template <> struct common_type_impl<float, int> { using type = float; };
template <> struct common_type_impl<long, double> { using type = double; };
template <> struct common_type_impl<double, long> { using type = double; };
template <> struct common_type_impl<float, unsigned char> { using type = float; };
template <> struct common_type_impl<unsigned char, float> { using type = float; };

// Add more common combinations
template <> struct common_type_impl<signed char, float> { using type = float; };
template <> struct common_type_impl<float, signed char> { using type = float; };
template <> struct common_type_impl<short, float> { using type = float; };
template <> struct common_type_impl<float, short> { using type = float; };
template <> struct common_type_impl<unsigned short, float> { using type = float; };
template <> struct common_type_impl<float, unsigned short> { using type = float; };
template <> struct common_type_impl<unsigned int, float> { using type = float; };
template <> struct common_type_impl<float, unsigned int> { using type = float; };
template <> struct common_type_impl<long long, float> { using type = float; };
template <> struct common_type_impl<float, long long> { using type = float; };
template <> struct common_type_impl<unsigned long, float> { using type = float; };
template <> struct common_type_impl<float, unsigned long> { using type = float; };
template <> struct common_type_impl<unsigned long long, float> { using type = float; };
template <> struct common_type_impl<float, unsigned long long> { using type = float; };

template <> struct common_type_impl<signed char, double> { using type = double; };
template <> struct common_type_impl<double, signed char> { using type = double; };
template <> struct common_type_impl<unsigned char, double> { using type = double; };
template <> struct common_type_impl<double, unsigned char> { using type = double; };
template <> struct common_type_impl<short, double> { using type = double; };
template <> struct common_type_impl<double, short> { using type = double; };
template <> struct common_type_impl<unsigned short, double> { using type = double; };
template <> struct common_type_impl<double, unsigned short> { using type = double; };
template <> struct common_type_impl<int, double> { using type = double; };
template <> struct common_type_impl<double, int> { using type = double; };
template <> struct common_type_impl<unsigned int, double> { using type = double; };
template <> struct common_type_impl<double, unsigned int> { using type = double; };
template <> struct common_type_impl<long long, double> { using type = double; };
template <> struct common_type_impl<double, long long> { using type = double; };
template <> struct common_type_impl<unsigned long, double> { using type = double; };
template <> struct common_type_impl<double, unsigned long> { using type = double; };
template <> struct common_type_impl<unsigned long long, double> { using type = double; };
template <> struct common_type_impl<double, unsigned long long> { using type = double; };

template <typename T, typename U> struct common_type {
    using type = typename common_type_impl<T, U>::type;
};

template <typename T, typename U>
using common_type_t = typename common_type<T, U>::type;

// This uses template magic to maybe generate a type for the given condition. If
// that type doesn't exist then a type will fail to be generated, and the
// compiler will skip the consideration of a target function. This is useful for
// enabling template constructors that only become available if the class can be
// upcasted to the desired type.
//
// Example:
// This is an optional upcasting constructor for a Ref<T>. If the type U is not
// derived from T then the constructor will not be generated, and the compiler
// will skip it.
//
// template <typename U, typename = fl::is_derived<T, U>>
// Ref(const Ref<U>& refptr) : referent_(refptr.get());
template <typename Base, typename Derived>
using is_derived = enable_if_t<is_base_of<Base, Derived>::value>;

//-----------------------------------------------------------------------------
// detect whether T has a member void swap(T&)
//-----------------------------------------------------------------------------
template <typename T> struct has_member_swap {
  private:
    // must be 1 byte vs. >1 byte for sizeof test
    typedef uint8_t yes;
    typedef uint16_t no;

    // helper<U, &U::swap> is only well-formed if U::swap(T&) exists with that
    // signature
    template <typename U, void (U::*M)(U &)> struct helper {};

    // picks this overload if helper<U, &U::swap> is valid
    template <typename U> static yes test(helper<U, &U::swap> *);

    // fallback otherwise
    template <typename> static no test(...);

  public:
    static constexpr bool value = sizeof(test<T>(nullptr)) == sizeof(yes);
};

// primary template: dispatch on has_member_swap<T>::value
template <typename T, bool = has_member_swap<T>::value> struct swap_impl;

// POD case
template <typename T> struct swap_impl<T, false> {
    static void apply(T &a, T &b) {
        T tmp = a;
        a = b;
        b = tmp;
    }
};

// non‑POD case (requires T implements swap)
template <typename T> struct swap_impl<T, true> {
    static void apply(T &a, T &b) { a.swap(b); }
};

// single entry‑point
template <typename T> void swap(T &a, T &b) {
    // if T is a POD, use use a simple data copy swap.
    // if T is not a POD, use the T::Swap method.
    swap_impl<T>::apply(a, b);
}

template <typename T> void swap_by_copy(T &a, T &b) {
    // if T is a POD, use use a simple data copy swap.
    // if T is not a POD, use the T::Swap method.
    T tmp = a;
    a = b;
    b = tmp;
}

// Container type checks.
template <typename T, typename... Types> struct contains_type;

template <typename T> struct contains_type<T> {
    static constexpr bool value = false;
};

template <typename T, typename U, typename... Rest>
struct contains_type<T, U, Rest...> {
    static constexpr bool value =
        fl::is_same<T, U>::value || contains_type<T, Rest...>::value;
};

// Helper to get maximum size of types
template <typename... Types> struct max_size;

template <> struct max_size<> {
    static constexpr size_t value = 0;
};

template <typename T, typename... Rest> struct max_size<T, Rest...> {
    static constexpr size_t value = (sizeof(T) > max_size<Rest...>::value)
                                        ? sizeof(T)
                                        : max_size<Rest...>::value;
};

// Helper to get maximum alignment of types
template <typename... Types> struct max_align;

template <> struct max_align<> {
    static constexpr size_t value = 1;
};

template <typename T, typename... Rest> struct max_align<T, Rest...> {
    static constexpr size_t value = (alignof(T) > max_align<Rest...>::value)
                                        ? alignof(T)
                                        : max_align<Rest...>::value;
};

} // namespace fl

// For comparison operators that return bool against pod data. The class obj
// will need to supply the comparison operator for the pod type. This example
// will show how to define a comparison operator for a class that can be
// compared against a pod type.
// Example:
//   FASTLED_DEFINE_POD_COMPARISON_OPERATOR(Myclass, >=) will allow MyClass to
//   be compared MyClass obj; return obj >= 0;
#define FASTLED_DEFINE_POD_COMPARISON_OPERATOR(CLASS, OP)                      \
    template <typename T, typename U>                                          \
    typename fl::enable_if<                                                    \
        fl::is_same<U, CLASS>::value && fl::is_pod<T>::value, bool>::type      \
    operator OP(const T &pod, const CLASS &obj) {                              \
        return pod OP obj;                                                     \
    }                                                                          \
    template <typename T>                                                      \
    typename fl::enable_if<fl::is_pod<T>::value, bool>::type operator OP(      \
        const CLASS &obj, const T &pod) {                                      \
        return obj OP pod;                                                     \
    }
