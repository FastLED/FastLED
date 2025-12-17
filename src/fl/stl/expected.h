#pragma once

/// @file expected.h
/// @brief Generic expected<T, E> type for error handling without exceptions
///
/// This is a high-level error handling pattern that provides explicit error
/// handling without exceptions. It's modeled after C++23's std::expected.
///
/// Example (with default error type):
/// @code
/// expected<int> divide(int a, int b) {
///     if (b == 0) {
///         return expected<int>::failure(ResultError::INVALID_ARGUMENT, "Division by zero");
///     }
///     return expected<int>::success(a / b);
/// }
///
/// auto result = divide(10, 2);
/// if (result.ok()) {
///     int value = result.value();
/// } else {
///     ResultError err = result.error();
/// }
/// @endcode
///
/// Example (with custom error type):
/// @code
/// enum class MyError { NOT_FOUND, INVALID };
///
/// expected<int, MyError> divide(int a, int b) {
///     if (b == 0) {
///         return expected<int, MyError>::failure(MyError::INVALID, "Division by zero");
///     }
///     return expected<int, MyError>::success(a / b);
/// }
/// @endcode

#include "fl/stl/stdint.h"
#include "fl/stl/utility.h"
#include "fl/align.h"

namespace fl {

/// @brief Generic error codes for expected type
/// @details Default error type when no specific error enum is needed
enum class ResultError : uint8_t {
    OK,                     ///< No error (not typically used)
    UNKNOWN,                ///< Unknown or unspecified error
    INVALID_ARGUMENT,       ///< Invalid argument provided
    OUT_OF_RANGE,           ///< Value out of valid range
    NOT_INITIALIZED,        ///< Object not initialized
    ALREADY_INITIALIZED,    ///< Object already initialized
    ALLOCATION_FAILED,      ///< Memory allocation failed
    TIMEOUT,                ///< Operation timed out
    BUSY,                   ///< Resource is busy
    NOT_SUPPORTED,          ///< Operation not supported
    IO_ERROR                ///< Input/output error
};

// Forward declaration for expected template
template<typename T, typename E = ResultError> class expected;

/// @brief expected type for operations that can fail (C++23-style)
/// @details Explicit error handling without exceptions
/// @tparam T The type of the successful value
/// @tparam E The type of the error code (must be an enum or integral type)
template<typename T, typename E>
class expected {
public:
    /// @brief Check if operation succeeded
    bool ok() const { return is_ok; }

    /// @brief Get error code (only meaningful if !ok())
    E error() const { return error_code; }

    /// @brief Get error message (only meaningful if !ok())
    const char* message() const { return error_msg; }

    /// @brief Get value (only valid if ok() == true)
    /// @warning Undefined behavior if called when !ok()
    T& value() { return *reinterpret_cast<T*>(storage); }
    const T& value() const { return *reinterpret_cast<const T*>(storage); }

    /// @brief Explicit conversion to bool for contextual evaluation
    explicit operator bool() const { return ok(); }

    /// @brief Create successful result
    static expected success(T value) {
        expected r;
        r.is_ok = true;
        new (r.storage) T(fl::move(value));
        return r;
    }

    /// @brief Create error result
    static expected failure(E err, const char* msg = nullptr) {
        expected r;
        r.is_ok = false;
        r.error_code = err;
        r.error_msg = msg;
        return r;
    }

    /// @brief Destructor
    ~expected() {
        if (is_ok) {
            reinterpret_cast<T*>(storage)->~T();
        }
    }

    /// @brief Move constructor
    expected(expected&& other) noexcept
        : is_ok(other.is_ok)
        , error_code(other.error_code)
        , error_msg(other.error_msg) {
        if (is_ok) {
            new (storage) T(fl::move(other.value()));
        }
    }

    /// @brief Move assignment
    expected& operator=(expected&& other) noexcept {
        if (this != &other) {
            if (is_ok) {
                reinterpret_cast<T*>(storage)->~T();
            }
            is_ok = other.is_ok;
            error_code = other.error_code;
            error_msg = other.error_msg;
            if (is_ok) {
                new (storage) T(fl::move(other.value()));
            }
        }
        return *this;
    }

private:
    expected() : is_ok(false), error_msg(nullptr) {}

    bool is_ok;
    E error_code;
    const char* error_msg;
    FL_ALIGN_AS(T) char storage[sizeof(T)];

    // Non-copyable
    expected(const expected&) = delete;
    expected& operator=(const expected&) = delete;
};

/// @brief Specialization for void (no value to return)
/// @tparam E The type of the error code (must be an enum or integral type)
template<typename E>
class expected<void, E> {
public:
    bool ok() const { return is_ok; }
    E error() const { return error_code; }
    const char* message() const { return error_msg; }
    explicit operator bool() const { return ok(); }

    static expected success() {
        expected r;
        r.is_ok = true;
        return r;
    }

    static expected failure(E err, const char* msg = nullptr) {
        expected r;
        r.is_ok = false;
        r.error_code = err;
        r.error_msg = msg;
        return r;
    }

    // Default constructor
    expected() : is_ok(false), error_msg(nullptr) {}

    // Move constructor
    expected(expected&& other) noexcept
        : is_ok(other.is_ok)
        , error_code(other.error_code)
        , error_msg(other.error_msg) {
    }

    // Move assignment
    expected& operator=(expected&& other) noexcept {
        if (this != &other) {
            is_ok = other.is_ok;
            error_code = other.error_code;
            error_msg = other.error_msg;
        }
        return *this;
    }

    // Copy constructor (needed for Impl initialization)
    expected(const expected& other)
        : is_ok(other.is_ok)
        , error_code(other.error_code)
        , error_msg(other.error_msg) {
    }

    // Copy assignment
    expected& operator=(const expected& other) {
        if (this != &other) {
            is_ok = other.is_ok;
            error_code = other.error_code;
            error_msg = other.error_msg;
        }
        return *this;
    }

private:
    bool is_ok = false;
    E error_code{};
    const char* error_msg = nullptr;
};

} // namespace fl
