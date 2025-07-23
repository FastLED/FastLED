#pragma once

/// @file promise_result.h
/// @brief Result type for promise operations with ok() semantics
///
/// PromiseResult<T> provides a Rust-like Result type that wraps either a success
/// value of type T or an Error. It provides convenient ok() checking and safe
/// value access with assertions on misuse.

#include "fl/namespace.h"
#include "fl/variant.h"
#include "fl/promise.h"  // For Error type

namespace fl {

/// @brief Result type for promise operations
/// @tparam T The success value type
/// 
/// result provides a clean API for handling success/error results from
/// async operations. It wraps a Variant<T, Error> but provides more ergonomic
/// access patterns with ok() checking and automatic assertions.
///
/// @section Usage
/// @code
/// auto result = fl::await_top_level<int>(some_promise);
/// 
/// if (result.ok()) {
///     int value = result.value();  // Safe access
///     FL_WARN("Success: " << value);
/// } else {
///     FL_WARN("Error: " << result.error().message);
/// }
/// 
/// // Or use operator bool
/// if (result) {
///     FL_WARN("Got: " << result.value());
/// }
/// @endcode
template<typename T>
class result {
public:
    /// @brief Construct a successful result
    /// @param value The success value
    result(const T& value) : mResult(value) {}
    
    /// @brief Construct a successful result (move)
    /// @param value The success value (moved)
    result(T&& value) : mResult(fl::move(value)) {}
    
    /// @brief Construct an error result
    /// @param error The error value
    result(const Error& error) : mResult(error) {}
    
    /// @brief Construct an error result (move)
    /// @param error The error value (moved)
    result(Error&& error) : mResult(fl::move(error)) {}
    
    /// @brief Check if the result is successful
    /// @return True if the result contains a value, false if it contains an error
    bool ok() const {
        return mResult.template is<T>();
    }
    
    /// @brief Boolean conversion operator (same as ok())
    /// @return True if the result is successful
    /// 
    /// Allows usage like: if (result) { ... }
    explicit operator bool() const {
        return ok();
    }
    
    /// @brief Get the success value (const)
    /// @return Reference to the success value
    /// @warning Returns static empty object if called on an error result
    /// @note Use ok() to check before calling for proper error handling
    const T& value() const {
        if (!ok()) {
            // Return static empty object instead of crashing
            static const T empty{};
            return empty;
        }
        return mResult.template get<T>();
    }
    
    /// @brief Get the success value (mutable)
    /// @return Reference to the success value
    /// @warning Returns static empty object if called on an error result
    /// @note Use ok() to check before calling for proper error handling
    T& value() {
        if (!ok()) {
            // Return static empty object instead of crashing
            static T empty{};
            return empty;
        }
        return mResult.template get<T>();
    }
    
    /// @brief Get the error value
    /// @return Reference to the error
    /// @warning Returns static descriptive error if called on a success result
    /// @note Use !ok() to check before calling for proper error handling
    const Error& error() const {
        if (ok()) {
            // Return descriptive error for misuse detection
            static const Error empty_error("No error - result contains success value");
            return empty_error;
        }
        return mResult.template get<Error>();
    }
    
    /// @brief Get the error message as a convenience
    /// @return Error message string, or empty string if successful
    /// @note Safe to call on success results (returns empty string)
    fl::string error_message() const {
        return ok() ? fl::string() : error().message;
    }
    
    /// @brief Access the underlying variant (for advanced usage)
    /// @return Reference to the internal variant
    const fl::Variant<T, Error>& variant() const {
        return mResult;
    }

private:
    fl::Variant<T, Error> mResult;
};

/// @brief Helper function to create a successful result
/// @tparam T The value type
/// @param value The success value
/// @return result containing the value
template<typename T>
result<T> make_success(const T& value) {
    return result<T>(value);
}

/// @brief Helper function to create a successful result (move)
/// @tparam T The value type  
/// @param value The success value (moved)
/// @return result containing the value
template<typename T>
result<T> make_success(T&& value) {
    return result<T>(fl::move(value));
}

/// @brief Helper function to create an error result
/// @tparam T The value type
/// @param error The error
/// @return result containing the error
template<typename T>
result<T> make_error(const Error& error) {
    return result<T>(error);
}

/// @brief Helper function to create an error result (move)
/// @tparam T The value type
/// @param error The error (moved)
/// @return result containing the error
template<typename T>
result<T> make_error(Error&& error) {
    return result<T>(fl::move(error));
}

/// @brief Helper function to create an error result from string
/// @tparam T The value type
/// @param message The error message
/// @return result containing the error
template<typename T>
result<T> make_error(const fl::string& message) {
    return result<T>(Error(message));
}

/// @brief Helper function to create an error result from C-string
/// @tparam T The value type
/// @param message The error message
/// @return result containing the error
template<typename T>
result<T> make_error(const char* message) {
    return result<T>(Error(message));
}

/// @brief Type alias for backwards compatibility
/// @deprecated Use result<T> instead of PromiseResult<T>
template<typename T>
using PromiseResult = result<T>;

} // namespace fl 
