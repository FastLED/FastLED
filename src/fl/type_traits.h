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
