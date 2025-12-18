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
#include "fl/stl/variant.h"
#include "fl/str.h"

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

/// @brief Error information for expected type
/// @tparam E The error code type
template<typename E>
struct ErrorInfo {
    E code;
    fl::string message;

    ErrorInfo(E err, const char* msg = nullptr) : code(err), message(msg ? msg : "") {}
};

/// @brief expected type for operations that can fail (C++23-style)
/// @details Explicit error handling without exceptions, using fl::variant for safe storage
/// @tparam T The type of the successful value
/// @tparam E The type of the error code (must be an enum or integral type)
template<typename T, typename E>
class expected {
public:
    /// @brief Check if operation succeeded
    bool ok() const { return mData.template is<T>(); }

    /// @brief Get error code (only meaningful if !ok())
    E error() const {
        auto* err = mData.template ptr<ErrorInfo<E>>();
        return err ? err->code : E{};
    }

    /// @brief Get error message (only meaningful if !ok())
    const char* message() const {
        auto* err = mData.template ptr<ErrorInfo<E>>();
        return err ? err->message.c_str() : "";
    }

    /// @brief Get value (only valid if ok() == true)
    /// @warning Undefined behavior if called when !ok()
    T& value() { return mData.template get<T>(); }
    const T& value() const { return mData.template get<T>(); }

    /// @brief Explicit conversion to bool for contextual evaluation
    explicit operator bool() const { return ok(); }

    /// @brief Create successful result
    static expected success(T value) {
        expected r;
        r.mData = fl::move(value);
        return r;
    }

    /// @brief Create error result
    static expected failure(E err, const char* msg = nullptr) {
        expected r;
        r.mData = ErrorInfo<E>(err, msg);
        return r;
    }

    /// @brief Default constructor (creates error state)
    expected() : mData(ErrorInfo<E>(E{}, nullptr)) {}

    /// @brief Move constructor (defaulted - variant handles it)
    expected(expected&& other) noexcept = default;

    /// @brief Move assignment (defaulted - variant handles it)
    expected& operator=(expected&& other) noexcept = default;

    /// @brief Destructor (defaulted - variant handles cleanup)
    ~expected() = default;

private:
    fl::variant<T, ErrorInfo<E>> mData;

    // Non-copyable
    expected(const expected&) = delete;
    expected& operator=(const expected&) = delete;
};

/// @brief Dummy type for void expected success state
struct VoidSuccess {};

/// @brief Specialization for void (no value to return)
/// @details Uses fl::variant<VoidSuccess, ErrorInfo<E>> for consistent implementation
/// @tparam E The type of the error code (must be an enum or integral type)
template<typename E>
class expected<void, E> {
public:
    bool ok() const { return mData.template is<VoidSuccess>(); }

    E error() const {
        auto* err = mData.template ptr<ErrorInfo<E>>();
        return err ? err->code : E{};
    }

    const char* message() const {
        auto* err = mData.template ptr<ErrorInfo<E>>();
        return err ? err->message.c_str() : "";
    }

    explicit operator bool() const { return ok(); }

    static expected success() {
        expected r;
        r.mData = VoidSuccess{};
        return r;
    }

    static expected failure(E err, const char* msg = nullptr) {
        expected r;
        r.mData = ErrorInfo<E>(err, msg);
        return r;
    }

    /// @brief Default constructor (creates error state)
    expected() : mData(ErrorInfo<E>(E{}, nullptr)) {}

    /// @brief Move constructor (defaulted - variant handles it)
    expected(expected&& other) noexcept = default;

    /// @brief Move assignment (defaulted - variant handles it)
    expected& operator=(expected&& other) noexcept = default;

    /// @brief Copy constructor (needed for some use cases like Impl initialization)
    expected(const expected& other) : mData() {
        if (other.ok()) {
            mData = VoidSuccess{};
        } else {
            auto* err = other.mData.template ptr<ErrorInfo<E>>();
            if (err) {
                mData = ErrorInfo<E>(err->code, err->message.c_str());
            }
        }
    }

    /// @brief Copy assignment
    expected& operator=(const expected& other) {
        if (this != &other) {
            if (other.ok()) {
                mData = VoidSuccess{};
            } else {
                auto* err = other.mData.template ptr<ErrorInfo<E>>();
                if (err) {
                    mData = ErrorInfo<E>(err->code, err->message.c_str());
                }
            }
        }
        return *this;
    }

    /// @brief Destructor (defaulted - variant handles cleanup)
    ~expected() = default;

private:
    fl::variant<VoidSuccess, ErrorInfo<E>> mData;
};

} // namespace fl
