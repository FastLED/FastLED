#pragma once

#include "fl/stl/int.h"
#include "fl/stl/type_traits.h"

namespace fl {
namespace detail {

template <typename T>
inline typename fl::enable_if<fl::is_floating_point<T>::value, T>::type
div_by_count(T sum, fl::size count) { return sum / static_cast<T>(count); }

template <typename T>
inline typename fl::enable_if<fl::is_integral<T>::value, T>::type
div_by_count(T sum, fl::size count) { return sum / static_cast<T>(count); }

template <typename T>
inline typename fl::enable_if<!fl::is_floating_point<T>::value &&
                              !fl::is_integral<T>::value, T>::type
div_by_count(T sum, fl::size count) { return sum / T(static_cast<float>(count)); }

} // namespace detail
} // namespace fl
