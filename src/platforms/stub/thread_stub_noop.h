// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/stub/thread_stub_noop.h
/// @brief Fake thread implementation for single-threaded platforms
///
/// This header provides a fake thread API for platforms without threading support.
/// Functions execute synchronously in the constructor, and thread operations are no-ops.

#include "fl/stl/functional.h"

// FASTLED_MULTITHREADED is defined by fl/stl/thread_config.h
// This file provides the no-op thread implementation for single-threaded platforms

namespace fl {
namespace platforms {

// ============================================================================
// Single-threaded Platforms: Fake Thread (Synchronous Execution)
// ============================================================================

/// @brief Fake thread for single-threaded platforms
///
/// On platforms without threading support, this provides a compatible API
/// but executes functions synchronously in the constructor.
class ThreadFake {
  private:
    bool mJoinable = false;
    int mThreadId = 0;

  public:
    // Thread ID type for single-threaded mode
    class id {
      private:
        int mId;
        friend class ThreadFake;
        explicit id(int val) : mId(val) {}
      public:
        id() : mId(0) {}
        bool operator==(const id& other) const { return mId == other.mId; }
        bool operator!=(const id& other) const { return mId != other.mId; }
        bool operator<(const id& other) const { return mId < other.mId; }
        bool operator<=(const id& other) const { return mId <= other.mId; }
        bool operator>(const id& other) const { return mId > other.mId; }
        bool operator>=(const id& other) const { return mId >= other.mId; }
    };

    ThreadFake() = default;

    /// @brief Constructor with function (executes synchronously)
    /// @tparam Function Callable type
    /// @tparam Args Argument types
    /// @param f Function to execute
    /// @param args Arguments to pass to function
    ///
    /// In single-threaded mode, the function executes immediately and
    /// synchronously. The thread is marked as not joinable after execution.
    template<typename Function, typename... Args>
    explicit ThreadFake(Function&& f, Args&&... args) : mJoinable(true), mThreadId(1) {
        // In single-threaded mode, execute immediately
        fl::invoke(fl::forward<Function>(f), fl::forward<Args>(args)...);
        // Immediately mark as not joinable since we executed synchronously
        mJoinable = false;
    }

    // Non-copyable
    ThreadFake(const ThreadFake&) = delete;
    ThreadFake& operator=(const ThreadFake&) = delete;

    // Movable
    ThreadFake(ThreadFake&& other) noexcept
        : mJoinable(other.mJoinable), mThreadId(other.mThreadId) {
        other.mJoinable = false;
        other.mThreadId = 0;
    }

    ThreadFake& operator=(ThreadFake&& other) noexcept {
        if (this != &other) {
            mJoinable = other.mJoinable;
            mThreadId = other.mThreadId;
            other.mJoinable = false;
            other.mThreadId = 0;
        }
        return *this;
    }

    // Destructor
    ~ThreadFake() = default;

    /// @brief Join (no-op in single-threaded mode)
    ///
    /// Since the function executed synchronously in the constructor,
    /// join() is a no-op.
    void join() {
        mJoinable = false;
    }

    /// @brief Detach (no-op in single-threaded mode)
    void detach() {
        mJoinable = false;
    }

    /// @brief Check if thread is joinable
    /// @return true if joinable, false otherwise
    bool joinable() const noexcept {
        return mJoinable;
    }

    /// @brief Get thread ID
    /// @return Thread ID
    id get_id() const noexcept {
        return id(mThreadId);
    }

    /// @brief Swap with another thread
    /// @param other Thread to swap with
    void swap(ThreadFake& other) noexcept {
        bool temp_joinable = mJoinable;
        int temp_id = mThreadId;
        mJoinable = other.mJoinable;
        mThreadId = other.mThreadId;
        other.mJoinable = temp_joinable;
        other.mThreadId = temp_id;
    }

    /// @brief Get hardware concurrency
    /// @return Always returns 1 in single-threaded mode
    static unsigned int hardware_concurrency() noexcept {
        return 1;
    }

    // Native handle type (void* for compatibility)
    using native_handle_type = void*;

    /// @brief Get native handle (returns nullptr in fake mode)
    /// @return nullptr
    native_handle_type native_handle() {
        return nullptr;
    }
};

/// @brief Thread type for single-threaded platforms
using thread = ThreadFake;

/// @brief Thread ID type for single-threaded platforms
using thread_id = ThreadFake::id;

/// @brief this_thread namespace for single-threaded platforms
namespace this_thread {
    /// @brief Get current thread ID
    /// @return Thread ID (always returns the same ID in single-threaded mode)
    ///
    /// Note: No static local variable here - Teensy 3.0/3.1/3.2 has __cxa_guard_*
    /// symbol conflicts when static locals are used in header files. Since this
    /// is single-threaded mode and thread_id() always returns the same value
    /// (mId=0), we just return a fresh default-constructed thread_id each time.
    inline thread_id get_id() noexcept {
        return thread_id();
    }

    /// @brief Yield (no-op in single-threaded mode)
    inline void yield() noexcept {
        // No-op
    }

    /// @brief Sleep for duration (no-op in single-threaded mode)
    /// @tparam Rep Duration representation type
    /// @tparam Period Duration period type (unused)
    /// @param sleep_duration Duration to sleep (unused)
    template<typename Rep, typename Period>
    void sleep_for(const Rep& /* sleep_duration */) {
        // No-op - can't actually sleep in single-threaded mode
    }

    /// @brief Sleep until time point (no-op in single-threaded mode)
    /// @tparam Clock Clock type (unused)
    /// @tparam Duration Duration type (unused)
    /// @param sleep_time Time point to sleep until (unused)
    template<typename Clock, typename Duration>
    void sleep_until(const Clock& /* sleep_time */) {
        // No-op - can't actually sleep in single-threaded mode
    }
} // namespace this_thread

} // namespace platforms
} // namespace fl
