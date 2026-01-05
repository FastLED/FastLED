#pragma once

#include "fl/stl/thread.h"
#include "fl/stl/assert.h"
#include "fl/stl/utility.h"  // for fl::move, fl::swap

#if FASTLED_MULTITHREADED
#include <algorithm>  // ok include - Required for <string_view> dependency in <mutex>
#include <mutex>  // ok include
#endif

namespace fl {

// Tag types for lock constructors
struct defer_lock_t { explicit defer_lock_t() = default; };
struct try_to_lock_t { explicit try_to_lock_t() = default; };
struct adopt_lock_t { explicit adopt_lock_t() = default; };

constexpr defer_lock_t defer_lock{};
constexpr try_to_lock_t try_to_lock{};
constexpr adopt_lock_t adopt_lock{};

// Forward declarations for single-threaded mode
template <typename T> class MutexFake;
template <typename T> class RecursiveMutexFake;

#if FASTLED_MULTITHREADED
// Alias to std::mutex for full compatibility with standard library
using MutexReal = std::mutex;  // okay std namespace
using RecursiveMutexReal = std::recursive_mutex;  // okay std namespace

using mutex = MutexReal;
using recursive_mutex = RecursiveMutexReal;
// In multithreaded mode, alias fl::unique_lock to std::unique_lock for compatibility
// with std::condition_variable and other standard library components
template<typename Mutex>
using unique_lock = std::unique_lock<Mutex>;  // okay std namespace
#else
using mutex = MutexFake<void>;
using recursive_mutex = RecursiveMutexFake<void>;
// In single-threaded mode, use custom implementation below
// Fake mutex (non-recursive) for single-threaded mode
template <typename T> class MutexFake {
  private:
    bool mLocked = false;

  public:
    MutexFake() = default;

    // Non-copyable and non-movable
    MutexFake(const MutexFake&) = delete;
    MutexFake& operator=(const MutexFake&) = delete;
    MutexFake(MutexFake&&) = delete;
    MutexFake& operator=(MutexFake&&) = delete;

    // Non-recursive mutex operations
    void lock() {
        FL_ASSERT(!mLocked, "MutexFake: attempting to lock already locked mutex (non-recursive)");
        mLocked = true;
    }

    void unlock() {
        FL_ASSERT(mLocked, "MutexFake: unlock called on unlocked mutex");
        mLocked = false;
    }

    bool try_lock() {
        if (mLocked) {
            return false;
        }
        mLocked = true;
        return true;
    }
};

// Fake recursive mutex for single-threaded mode
template <typename T> class RecursiveMutexFake {
  private:
    int mLockCount = 0;

  public:
    RecursiveMutexFake() = default;

    // Non-copyable and non-movable
    RecursiveMutexFake(const RecursiveMutexFake&) = delete;
    RecursiveMutexFake& operator=(const RecursiveMutexFake&) = delete;
    RecursiveMutexFake(RecursiveMutexFake&&) = delete;
    RecursiveMutexFake& operator=(RecursiveMutexFake&&) = delete;

    // Recursive mutex operations
    void lock() {
        // In single-threaded mode, we just track the lock count for debugging
        mLockCount++;
    }

    void unlock() {
        // In single-threaded mode, we just track the lock count for debugging
        FL_ASSERT(mLockCount > 0, "RecursiveMutexFake: unlock called without matching lock");
        mLockCount--;
    }

    bool try_lock() {
        // In single-threaded mode, always succeed and increment count
        mLockCount++;
        return true;
    }
};

// Single-threaded mode: custom unique_lock implementation
template<typename Mutex>
class unique_lock {
public:
    using mutex_type = Mutex;

private:
    Mutex* mMutex;
    bool mOwns;

public:
    // Constructors
    unique_lock() noexcept : mMutex(nullptr), mOwns(false) {}

    explicit unique_lock(Mutex& m) : mMutex(&m), mOwns(false) {
        lock();
        mOwns = true;
    }

    unique_lock(Mutex& m, defer_lock_t) noexcept : mMutex(&m), mOwns(false) {}

    unique_lock(Mutex& m, try_to_lock_t) : mMutex(&m), mOwns(m.try_lock()) {}

    unique_lock(Mutex& m, adopt_lock_t) noexcept : mMutex(&m), mOwns(true) {}

    // Destructor
    ~unique_lock() {
        if (mOwns) {
            unlock();
        }
    }

    // Copy semantics deleted
    unique_lock(const unique_lock&) = delete;
    unique_lock& operator=(const unique_lock&) = delete;

    // Move constructor
    unique_lock(unique_lock&& u) noexcept : mMutex(u.mMutex), mOwns(u.mOwns) {
        u.mMutex = nullptr;
        u.mOwns = false;
    }

    // Move assignment
    unique_lock& operator=(unique_lock&& u) noexcept {
        if (mOwns) {
            unlock();
        }

        mMutex = u.mMutex;
        mOwns = u.mOwns;

        u.mMutex = nullptr;
        u.mOwns = false;

        return *this;
    }

    // Locking operations
    void lock() {
        if (!mMutex) {
            // throw error: operation not permitted
            return;
        }
        if (mOwns) {
            // throw error: resource deadlock would occur
            return;
        }
        mMutex->lock();
        mOwns = true;
    }

    bool try_lock() {
        if (!mMutex) {
            return false;
        }
        if (mOwns) {
            return false;
        }
        mOwns = mMutex->try_lock();
        return mOwns;
    }

    void unlock() {
        if (!mOwns) {
            // throw error: operation not permitted
            return;
        }
        if (mMutex) {
            mMutex->unlock();
            mOwns = false;
        }
    }

    // Modifiers
    void swap(unique_lock& u) noexcept {
        using fl::swap;
        swap(mMutex, u.mMutex);
        swap(mOwns, u.mOwns);
    }

    Mutex* release() noexcept {
        Mutex* ret = mMutex;
        mMutex = nullptr;
        mOwns = false;
        return ret;
    }

    // Observers
    bool owns_lock() const noexcept { return mOwns; }
    explicit operator bool() const noexcept { return mOwns; }
    Mutex* mutex() const noexcept { return mMutex; }
};

template<typename Mutex>
void swap(unique_lock<Mutex>& lhs, unique_lock<Mutex>& rhs) noexcept {
    lhs.swap(rhs);
}

#endif // FASTLED_MULTITHREADED

} // namespace fl
