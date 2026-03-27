#pragma once

/// @file fl/task/Promise.h
/// @brief Promise-based fluent API for FastLED - standalone async primitives
///
/// The fl::task::Promise<T> API provides fluent .then() semantics that are intuitive
/// and chainable for async operations in FastLED. This is a lightweight, standalone
/// implementation that doesn't depend on fl::future.
///
/// @section Key Features
/// - **Fluent API**: Chainable .then() and .catch_() methods
/// - **Non-Blocking**: Perfect for setup() + loop() programming model
/// - **Lightweight**: Standalone implementation without heavy dependencies
/// - **JavaScript-Like**: Familiar Promise API patterns
///
/// @section Basic Usage
/// @code
/// // HTTP request with Promise-based API
/// fl::http_get("http://fastled.io")
///     .then([](const Response& resp) {
///         FL_WARN("Success: " << resp.text());
///     })
///     .catch_([](const Error& err) {
///         FL_WARN("Error: " << err.message());
///     });
/// @endcode

#include "fl/stl/function.h"
#include "fl/stl/string.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/move.h"
#include "fl/stl/atomic.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace task {

/// Error type for promises
struct Error {
    fl::string message;

    Error() FL_NOEXCEPT = default;
    Error(const fl::string& msg) : message(msg) {}
    Error(const char* msg) : message(msg) {}
    Error(fl::string&& msg) : message(fl::move(msg)) {}
    bool is_empty() const { return message.empty(); }
};

// Forward declaration for implementation
namespace detail {
    template<typename T>
    class PromiseImpl;
}

/// Promise class that provides fluent .then() and .catch_() semantics
/// This is a lightweight wrapper around a shared PromiseImpl for easy copying/sharing
template<typename T>
class Promise {
public:
    /// Create a pending Promise
    static Promise<T> create() {
        auto impl = fl::make_shared<detail::PromiseImpl<T>>();
        return Promise<T>(impl);
    }

    /// Create a resolved Promise with value
    static Promise<T> resolve(const T& value) {  // okay static in header
        auto p = create();
        p.complete_with_value(value);
        return p;
    }

    /// Create a resolved Promise with value (move version)
    static Promise<T> resolve(T&& value) {  // okay static in header
        auto p = create();
        p.complete_with_value(fl::move(value));
        return p;
    }

    /// Create a rejected Promise with error
    static Promise<T> reject(const Error& error) {  // okay static in header
        auto p = create();
        p.complete_with_error(error);
        return p;
    }

    /// Create a rejected Promise with error message
    static Promise<T> reject(const fl::string& error_message) {  // okay static in header
        return reject(Error(error_message));
    }

    /// Default constructor - creates invalid Promise
    Promise() FL_NOEXCEPT : mImpl(nullptr) {}

    /// Copy constructor (promises are now copyable via shared implementation)
    Promise(const Promise& other) = default;

    /// Move constructor
    Promise(Promise&& other) FL_NOEXCEPT = default;

    /// Copy assignment operator
    Promise& operator=(const Promise& other) = default;

    /// Move assignment operator
    Promise& operator=(Promise&& other) FL_NOEXCEPT = default;

    /// Check if Promise is valid
    bool valid() const {
        return mImpl != nullptr;
    }

    /// Register success callback - returns reference for chaining
    /// @param callback Function to call when Promise resolves successfully
    /// @returns Reference to this Promise for chaining
    Promise& then(fl::function<void(const T&)> callback) {
        if (!valid()) return *this;

        mImpl->set_then_callback(fl::move(callback));
        return *this;
    }

    /// Register error callback - returns reference for chaining
    /// @param callback Function to call when Promise rejects with error
    /// @returns Reference to this Promise for chaining
    Promise& catch_(fl::function<void(const Error&)> callback) {
        if (!valid()) return *this;

        mImpl->set_catch_callback(fl::move(callback));
        return *this;
    }

    /// Update Promise state in main loop - should be called periodically
    /// This processes pending callbacks when the Promise completes
    void update() {
        if (!valid()) return;
        mImpl->update();
    }

    /// Check if Promise is completed (resolved or rejected)
    bool is_completed() const {
        if (!valid()) return false;
        return mImpl->is_completed();
    }

    /// Check if Promise is resolved (completed successfully)
    bool is_resolved() const {
        if (!valid()) return false;
        return mImpl->is_resolved();
    }

    /// Check if Promise is rejected (completed with error)
    bool is_rejected() const {
        if (!valid()) return false;
        return mImpl->is_rejected();
    }

    /// Get the result value (only valid if is_resolved() returns true)
    const T& value() const {
        if (!valid()) {
            static const T default_value{};
            return default_value;
        }
        return mImpl->value();
    }

    /// Get the error (only valid if is_rejected() returns true)
    const Error& error() const {
        if (!valid()) {
            static const Error default_error;
            return default_error;
        }
        return mImpl->error();
    }

    /// Clear Promise to invalid state
    void clear() {
        mImpl.reset();
    }

    // ========== PRODUCER INTERFACE (INTERNAL USE) ==========

    /// Complete the Promise with a result (used by networking library)
    bool complete_with_value(const T& value) {
        if (!valid()) return false;
        return mImpl->resolve(value);
    }

    bool complete_with_value(T&& value) {
        if (!valid()) return false;
        return mImpl->resolve(fl::move(value));
    }

    /// Complete the Promise with an error (used by networking library)
    bool complete_with_error(const Error& error) {
        if (!valid()) return false;
        return mImpl->reject(error);
    }

    bool complete_with_error(const fl::string& error_message) {
        if (!valid()) return false;
        return mImpl->reject(Error(error_message));
    }

private:
    /// Constructor from shared implementation (used internally)
    explicit Promise(fl::shared_ptr<detail::PromiseImpl<T>> impl) : mImpl(impl) {}

    /// Shared pointer to implementation - this allows copying and sharing Promise state
    fl::shared_ptr<detail::PromiseImpl<T>> mImpl;
};

/// Convenience function to create a resolved Promise
template<typename T>
Promise<T> make_resolved_promise(T value) {
    return Promise<T>::resolve(fl::move(value));
}

/// Convenience function to create a rejected Promise
template<typename T>
Promise<T> make_rejected_promise(const fl::string& error_message) {
    return Promise<T>::reject(Error(error_message));
}

/// Convenience function to create a rejected Promise (const char* overload)
template<typename T>
Promise<T> make_rejected_promise(const char* error_message) {
    return Promise<T>::reject(Error(error_message));
}

// ============================================================================
// IMPLEMENTATION DETAILS
// ============================================================================

namespace detail {

/// State enumeration for promises
enum class PromiseState_t {
    PENDING,   // Promise is still pending
    RESOLVED,  // Promise completed successfully
    REJECTED   // Promise completed with error
};

/// Implementation class for Promise - holds the actual state and logic
template<typename T>
class PromiseImpl {
public:
    PromiseImpl() FL_NOEXCEPT : mState(static_cast<int>(PromiseState_t::PENDING)), mCallbacksProcessed(false) {}

    /// Set success callback
    void set_then_callback(fl::function<void(const T&)> callback) {
        mThenCallback = fl::move(callback);
        // If already resolved, process callback immediately
        if (state() == PromiseState_t::RESOLVED && !mCallbacksProcessed) {
            process_callbacks();
        }
    }

    /// Set error callback
    void set_catch_callback(fl::function<void(const Error&)> callback) {
        mCatchCallback = fl::move(callback);
        // If already rejected, process callback immediately
        if (state() == PromiseState_t::REJECTED && !mCallbacksProcessed) {
            process_callbacks();
        }
    }

    /// Update Promise state - processes callbacks if needed
    void update() {
        // Process callbacks if we're completed and haven't processed them yet
        if (is_completed() && !mCallbacksProcessed) {
            process_callbacks();
        }
    }

    /// Resolve Promise with value
    bool resolve(const T& value) {
        if (state() != PromiseState_t::PENDING) return false;

        // Write value BEFORE setting state — the atomic store provides
        // a release fence so the reader sees the value after observing
        // the state transition.
        mValue = value;
        set_state(PromiseState_t::RESOLVED);

        // Process callback immediately if we have one
        if (mThenCallback && !mCallbacksProcessed) {
            process_callbacks();
        }

        return true;
    }

    bool resolve(T&& value) {
        if (state() != PromiseState_t::PENDING) return false;

        mValue = fl::move(value);
        set_state(PromiseState_t::RESOLVED);

        if (mThenCallback && !mCallbacksProcessed) {
            process_callbacks();
        }

        return true;
    }

    /// Reject Promise with error
    bool reject(const Error& error) {
        if (state() != PromiseState_t::PENDING) return false;

        mError = error;
        set_state(PromiseState_t::REJECTED);

        if (mCatchCallback && !mCallbacksProcessed) {
            process_callbacks();
        }

        return true;
    }

    /// Check if Promise is completed
    bool is_completed() const {
        return state() != PromiseState_t::PENDING;
    }

    /// Check if Promise is resolved
    bool is_resolved() const {
        return state() == PromiseState_t::RESOLVED;
    }

    /// Check if Promise is rejected
    bool is_rejected() const {
        return state() == PromiseState_t::REJECTED;
    }

    /// Get value (only valid if resolved)
    const T& value() const {
        return mValue;
    }

    /// Get error (only valid if rejected)
    const Error& error() const {
        return mError;
    }

private:
    fl::atomic<int> mState;  // stores PromiseState_t as int (atomics require integer type)
    T mValue;
    Error mError;

    fl::function<void(const T&)> mThenCallback;
    fl::function<void(const Error&)> mCatchCallback;

    bool mCallbacksProcessed;

    /// Read the state atomically
    PromiseState_t state() const {
        return static_cast<PromiseState_t>(mState.load());
    }

    /// Write the state atomically
    void set_state(PromiseState_t s) {
        mState.store(static_cast<int>(s));
    }

    /// Process pending callbacks
    void process_callbacks() {
        if (mCallbacksProcessed) return;

        PromiseState_t s = state();
        if (s == PromiseState_t::RESOLVED && mThenCallback) {
            mThenCallback(mValue);
        } else if (s == PromiseState_t::REJECTED && mCatchCallback) {
            mCatchCallback(mError);
        }

        mCallbacksProcessed = true;
    }
};

} // namespace detail

} // namespace task
} // namespace fl
