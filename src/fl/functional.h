#pragma once
#include "fl/type_traits.h"
#include "fl/utility.h"

namespace fl {

template <typename T>
class Ptr; // Forward declare Ptr to avoid header inclusion

template <typename T, typename Deleter>
class unique_ptr; // Forward declare unique_ptr to avoid header inclusion

//----------------------------------------------------------------------------
// invoke implementation - equivalent to std::invoke from C++17
//----------------------------------------------------------------------------

namespace detail {

// Helper to detect member data pointers
template <typename T>
struct is_member_data_pointer : false_type {};

template <typename T, typename C>
struct is_member_data_pointer<T C::*> : integral_constant<bool, !is_function<T>::value> {};

// Helper to detect if T is a pointer type
template <typename T>
struct is_pointer_like : false_type {};

template <typename T>
struct is_pointer_like<T*> : true_type {};

template <typename T>
struct is_pointer_like<fl::Ptr<T>> : true_type {};

template <typename T, typename Deleter>
struct is_pointer_like<fl::unique_ptr<T, Deleter>> : true_type {};

// Helper to detect if we should use pointer-to-member syntax
template <typename T>
struct use_pointer_syntax : is_pointer_like<typename remove_reference<T>::type> {};

} // namespace detail

// Main invoke function overloads

// 1a. Member function pointer with object reference
template <typename F, typename T1, typename... Args>
auto invoke(F&& f, T1&& t1, Args&&... args)
    -> enable_if_t<is_member_function_pointer<typename remove_reference<F>::type>::value &&
                   !detail::use_pointer_syntax<T1>::value,
                   decltype((fl::forward<T1>(t1).*f)(fl::forward<Args>(args)...))>
{
    return (fl::forward<T1>(t1).*f)(fl::forward<Args>(args)...);
}

// 1b. Member function pointer with pointer-like object
template <typename F, typename T1, typename... Args>
auto invoke(F&& f, T1&& t1, Args&&... args)
    -> enable_if_t<is_member_function_pointer<typename remove_reference<F>::type>::value &&
                   detail::use_pointer_syntax<T1>::value,
                   decltype(((*fl::forward<T1>(t1)).*f)(fl::forward<Args>(args)...))>
{
    return ((*fl::forward<T1>(t1)).*f)(fl::forward<Args>(args)...);
}

// 2a. Member data pointer with object reference
template <typename F, typename T1>
auto invoke(F&& f, T1&& t1)
    -> enable_if_t<detail::is_member_data_pointer<typename remove_reference<F>::type>::value &&
                   !detail::use_pointer_syntax<T1>::value,
                   decltype(fl::forward<T1>(t1).*f)>
{
    return fl::forward<T1>(t1).*f;
}

// 2b. Member data pointer with pointer-like object
template <typename F, typename T1>
auto invoke(F&& f, T1&& t1)
    -> enable_if_t<detail::is_member_data_pointer<typename remove_reference<F>::type>::value &&
                   detail::use_pointer_syntax<T1>::value,
                   decltype((*fl::forward<T1>(t1)).*f)>
{
    return (*fl::forward<T1>(t1)).*f;
}

// 3. Regular callable (function pointer, lambda, functor)
template <typename F, typename... Args>
auto invoke(F&& f, Args&&... args)
    -> enable_if_t<!is_member_function_pointer<typename remove_reference<F>::type>::value &&
                   !detail::is_member_data_pointer<typename remove_reference<F>::type>::value,
                   decltype(fl::forward<F>(f)(fl::forward<Args>(args)...))>
{
    return fl::forward<F>(f)(fl::forward<Args>(args)...);
}

} // namespace fl
