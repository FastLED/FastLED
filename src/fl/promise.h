#pragma once

/// @file promise.h
/// @brief Promise-based fluent API for FastLED - standalone async primitives
///
/// The fl::promise<T> API provides fluent .then() semantics that are intuitive and chainable
/// for async operations in FastLED. This is a lightweight, standalone implementation
/// that doesn't depend on fl::future.
///
/// @section Key Features
/// - **Fluent API**: Chainable .then() and .catch_() methods
/// - **Non-Blocking**: Perfect for setup() + loop() programming model
/// - **Lightweight**: Standalone implementation without heavy dependencies
/// - **JavaScript-Like**: Familiar Promise API patterns
///
/// @section Basic Usage
/// @code
/// // HTTP request with promise-based API
/// fl::http_get("http://fastled.io")
///     .then([](const Response& resp) {
///         FL_WARN("Success: " << resp.text());
///     })
///     .catch_([](const Error& err) {
///         FL_WARN("Error: " << err.message());
///     });
/// @endcode
///
/// @section Fluent Interface
/// @code
/// // Chainable operations
/// fl::fetch.get("http://api.example.com/data")
///     .header("Authorization", "Bearer token123")
///     .timeout(5000)
///     .then([](const Response& resp) {
///         if (resp.ok()) {
///             process_data(resp.text());
///         }
///     })
///     .catch_([](const Error& err) {
///         handle_error(err.message());
///     });
/// @endcode

#include "fl/namespace.h"
#include "fl/function.h"
#include "fl/string.h"
#include "fl/shared_ptr.h"
#include "fl/move.h"

namespace fl {

/// Error type for promises
struct Error {
    fl::string message;
    
    Error() = default;
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
class promise {
public:
    /// Create a pending promise
    static promise<T> create() {
        auto impl = fl::make_shared<detail::PromiseImpl<T>>();
        return promise<T>(impl);
    }
    
    /// Create a resolved promise with value
    static promise<T> resolve(const T& value) {
        auto p = create();
        p.complete_with_value(value);
        return p;
    }
    
    /// Create a resolved promise with value (move version)
    static promise<T> resolve(T&& value) {
        auto p = create();
        p.complete_with_value(fl::move(value));
        return p;
    }
    
    /// Create a rejected promise with error
    static promise<T> reject(const Error& error) {
        auto p = create();
        p.complete_with_error(error);
        return p;
    }
    
    /// Create a rejected promise with error message
    static promise<T> reject(const fl::string& error_message) {
        return reject(Error(error_message));
    }
    
    /// Default constructor - creates invalid promise
    promise() : mImpl(nullptr) {}
    
    /// Copy constructor (promises are now copyable via shared implementation)
    promise(const promise& other) = default;
    
    /// Move constructor
    promise(promise&& other) noexcept = default;
    
    /// Copy assignment operator
    promise& operator=(const promise& other) = default;
    
    /// Move assignment operator
    promise& operator=(promise&& other) noexcept = default;
    
    /// Check if promise is valid
    bool valid() const {
        return mImpl != nullptr;
    }
    
    /// Register success callback - returns reference for chaining
    /// @param callback Function to call when promise resolves successfully
    /// @returns Reference to this promise for chaining
    promise& then(fl::function<void(const T&)> callback) {
        if (!valid()) return *this;
        
        mImpl->set_then_callback(fl::move(callback));
        return *this;
    }
    
    /// Register error callback - returns reference for chaining
    /// @param callback Function to call when promise rejects with error
    /// @returns Reference to this promise for chaining
    promise& catch_(fl::function<void(const Error&)> callback) {
        if (!valid()) return *this;
        
        mImpl->set_catch_callback(fl::move(callback));
        return *this;
    }
    
    /// Update promise state in main loop - should be called periodically
    /// This processes pending callbacks when the promise completes
    void update() {
        if (!valid()) return;
        mImpl->update();
    }
    
    /// Check if promise is completed (resolved or rejected)
    bool is_completed() const {
        if (!valid()) return false;
        return mImpl->is_completed();
    }
    
    /// Check if promise is resolved (completed successfully)
    bool is_resolved() const {
        if (!valid()) return false;
        return mImpl->is_resolved();
    }
    
    /// Check if promise is rejected (completed with error)
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
    
    /// Clear promise to invalid state
    void clear() {
        mImpl.reset();
    }
    
    // ========== PRODUCER INTERFACE (INTERNAL USE) ==========
    
    /// Complete the promise with a result (used by networking library)
    bool complete_with_value(const T& value) {
        if (!valid()) return false;
        return mImpl->resolve(value);
    }
    
    bool complete_with_value(T&& value) {
        if (!valid()) return false;
        return mImpl->resolve(fl::move(value));
    }
    
    /// Complete the promise with an error (used by networking library)
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
    explicit promise(fl::shared_ptr<detail::PromiseImpl<T>> impl) : mImpl(impl) {}
    
    /// Shared pointer to implementation - this allows copying and sharing promise state
    fl::shared_ptr<detail::PromiseImpl<T>> mImpl;
};

/// Convenience function to create a resolved promise
template<typename T>
promise<T> make_resolved_promise(T value) {
    return promise<T>::resolve(fl::move(value));
}

/// Convenience function to create a rejected promise
template<typename T>
promise<T> make_rejected_promise(const fl::string& error_message) {
    return promise<T>::reject(Error(error_message));
}

/// Convenience function to create a rejected promise (const char* overload)
template<typename T>
promise<T> make_rejected_promise(const char* error_message) {
    return promise<T>::reject(Error(error_message));
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

/// Implementation class for promise - holds the actual state and logic
template<typename T>
class PromiseImpl {
public:
    PromiseImpl() : mState(PromiseState_t::PENDING), mCallbacksProcessed(false) {}
    
    /// Set success callback
    void set_then_callback(fl::function<void(const T&)> callback) {
        mThenCallback = fl::move(callback);
        // If already resolved, process callback immediately
        if (mState == PromiseState_t::RESOLVED && !mCallbacksProcessed) {
            process_callbacks();
        }
    }
    
    /// Set error callback
    void set_catch_callback(fl::function<void(const Error&)> callback) {
        mCatchCallback = fl::move(callback);
        // If already rejected, process callback immediately
        if (mState == PromiseState_t::REJECTED && !mCallbacksProcessed) {
            process_callbacks();
        }
    }
    
    /// Update promise state - processes callbacks if needed
    void update() {
        // Process callbacks if we're completed and haven't processed them yet
        if (is_completed() && !mCallbacksProcessed) {
            process_callbacks();
        }
    }
    
    /// Resolve promise with value
    bool resolve(const T& value) {
        if (mState != PromiseState_t::PENDING) return false;
        
        mValue = value;
        mState = PromiseState_t::RESOLVED;
        
        // Process callback immediately if we have one
        if (mThenCallback && !mCallbacksProcessed) {
            process_callbacks();
        }
        
        return true;
    }
    
    bool resolve(T&& value) {
        if (mState != PromiseState_t::PENDING) return false;
        
        mValue = fl::move(value);
        mState = PromiseState_t::RESOLVED;
        
        // Process callback immediately if we have one
        if (mThenCallback && !mCallbacksProcessed) {
            process_callbacks();
        }
        
        return true;
    }
    
    /// Reject promise with error
    bool reject(const Error& error) {
        if (mState != PromiseState_t::PENDING) return false;
        
        mError = error;
        mState = PromiseState_t::REJECTED;
        
        // Process callback immediately if we have one
        if (mCatchCallback && !mCallbacksProcessed) {
            process_callbacks();
        }
        
        return true;
    }
    
    /// Check if promise is completed
    bool is_completed() const {
        return mState != PromiseState_t::PENDING;
    }
    
    /// Check if promise is resolved
    bool is_resolved() const {
        return mState == PromiseState_t::RESOLVED;
    }
    
    /// Check if promise is rejected
    bool is_rejected() const {
        return mState == PromiseState_t::REJECTED;
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
    PromiseState_t mState;
    T mValue;
    Error mError;
    
    fl::function<void(const T&)> mThenCallback;
    fl::function<void(const Error&)> mCatchCallback;
    
    bool mCallbacksProcessed;
    
    /// Process pending callbacks
    void process_callbacks() {
        if (mCallbacksProcessed) return;
        
        if (mState == PromiseState_t::RESOLVED && mThenCallback) {
            mThenCallback(mValue);
        } else if (mState == PromiseState_t::REJECTED && mCatchCallback) {
            mCatchCallback(mError);
        }
        
        mCallbacksProcessed = true;
    }
};

} // namespace detail

} // namespace fl 
