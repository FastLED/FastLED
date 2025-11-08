#pragma once

/// @file result.h
/// @brief Generic Result<T, E> type for error handling without exceptions
///
/// This is a high-level error handling pattern that provides explicit error
/// handling without exceptions. It's modeled after Rust's Result type.
///
/// Example (with default error type):
/// @code
/// Result<int> divide(int a, int b) {
///     if (b == 0) {
///         return Result<int>::failure(Error::INVALID_ARGUMENT, "Division by zero");
///     }
///     return Result<int>::success(a / b);
/// }
///
/// auto result = divide(10, 2);
/// if (result.ok()) {
///     int value = result.value();
/// } else {
///     Error err = result.error();
/// }
/// @endcode
///
/// Example (with custom error type):
/// @code
/// enum class MyError { NOT_FOUND, INVALID };
///
/// Result<int, MyError> divide(int a, int b) {
///     if (b == 0) {
///         return Result<int, MyError>::failure(MyError::INVALID, "Division by zero");
///     }
///     return Result<int, MyError>::success(a / b);
/// }
/// @endcode

#include "fl/stdint.h"
#include "fl/utility.h"
#include "fl/align.h"

namespace fl {

/// @brief Generic error codes for Result type
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

// Forward declaration for Result template
template<typename T, typename E = ResultError> class Result;

/// @brief Result type for operations that can fail
/// @details Explicit error handling without exceptions
/// @tparam T The type of the successful value
/// @tparam E The type of the error code (must be an enum or integral type)
template<typename T, typename E>
class Result {
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
    static Result success(T value) {
        Result r;
        r.is_ok = true;
        new (r.storage) T(fl::move(value));
        return r;
    }

    /// @brief Create error result
    static Result failure(E err, const char* msg = nullptr) {
        Result r;
        r.is_ok = false;
        r.error_code = err;
        r.error_msg = msg;
        return r;
    }

    /// @brief Destructor
    ~Result() {
        if (is_ok) {
            reinterpret_cast<T*>(storage)->~T();
        }
    }

    /// @brief Move constructor
    Result(Result&& other) noexcept
        : is_ok(other.is_ok)
        , error_code(other.error_code)
        , error_msg(other.error_msg) {
        if (is_ok) {
            new (storage) T(fl::move(other.value()));
        }
    }

    /// @brief Move assignment
    Result& operator=(Result&& other) noexcept {
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
    Result() : is_ok(false), error_msg(nullptr) {}

    bool is_ok;
    E error_code;
    const char* error_msg;
    FL_ALIGN_AS(T) char storage[sizeof(T)];

    // Non-copyable
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
};

/// @brief Specialization for void (no value to return)
/// @tparam E The type of the error code (must be an enum or integral type)
template<typename E>
class Result<void, E> {
public:
    bool ok() const { return is_ok; }
    E error() const { return error_code; }
    const char* message() const { return error_msg; }
    explicit operator bool() const { return ok(); }

    static Result success() {
        Result r;
        r.is_ok = true;
        return r;
    }

    static Result failure(E err, const char* msg = nullptr) {
        Result r;
        r.is_ok = false;
        r.error_code = err;
        r.error_msg = msg;
        return r;
    }

    // Default constructor
    Result() : is_ok(false), error_msg(nullptr) {}

    // Move constructor
    Result(Result&& other) noexcept
        : is_ok(other.is_ok)
        , error_code(other.error_code)
        , error_msg(other.error_msg) {
    }

    // Move assignment
    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            is_ok = other.is_ok;
            error_code = other.error_code;
            error_msg = other.error_msg;
        }
        return *this;
    }

    // Copy constructor (needed for Impl initialization)
    Result(const Result& other)
        : is_ok(other.is_ok)
        , error_code(other.error_code)
        , error_msg(other.error_msg) {
    }

    // Copy assignment
    Result& operator=(const Result& other) {
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
