#pragma once

/// @file result.h
/// @brief Result<T, E> type alias for fl::expected (Rust-style naming)
///
/// This file provides a Rust-style naming alias for the C++23-style expected type.
/// The underlying implementation is in fl/stl/expected.h.
///
/// Example usage:
/// @code
/// Result<int> divide(int a, int b) {
///     if (b == 0) {
///         return Result<int>::failure(ResultError::INVALID_ARGUMENT, "Division by zero");
///     }
///     return Result<int>::success(a / b);
/// }
///
/// auto result = divide(10, 2);
/// if (result.ok()) {
///     int value = result.value();
/// }
/// @endcode

#include "fl/stl/expected.h"

namespace fl {

/// @brief Alias for expected (Rust-style naming)
/// @details Provides familiar Result naming while using fl::expected implementation
template<typename T, typename E = ResultError>
using Result = expected<T, E>;

} // namespace fl
