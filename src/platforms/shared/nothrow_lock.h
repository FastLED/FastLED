// ok no namespace fl
#pragma once

// IWYU pragma: private

/// @file platforms/shared/nothrow_lock.h
/// @brief Non-throwing `lock_guard` / `unique_lock` for platform mutexes.
///
/// `std::unique_lock::lock()`/`unlock()` call `std::__throw_system_error` on
/// misuse. On ESP32 that one reference extracts libstdc++'s system_error.o,
/// which drags sstream/ios/locale and their static constructors (~0.5 KB
/// flash, 256 B RAM) into every sketch (FastLED #4798). These versions report
/// misuse by doing nothing, matching FL_NO_EXCEPT.
///
/// Include after the platform has declared `fl::platforms::defer_lock_t`,
/// `try_to_lock_t` and `adopt_lock_t` (its own types or `using std::...`).

#include "fl/stl/utility.h"  // for fl::swap
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

// Non-throwing lock_guard
template<typename Mutex>
class lock_guard {
public:
    using mutex_type = Mutex;

    explicit lock_guard(Mutex& m) FL_NO_EXCEPT : mMutex(m) { mMutex.lock(); }
    lock_guard(Mutex& m, adopt_lock_t) FL_NO_EXCEPT : mMutex(m) {}

    ~lock_guard() { mMutex.unlock(); }

    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;

private:
    Mutex& mMutex;
};

// Non-throwing unique_lock
template<typename Mutex>
class unique_lock {
public:
    using mutex_type = Mutex;

private:
    Mutex* mMutex;
    bool mOwns;

public:
    // Constructors
    unique_lock() FL_NO_EXCEPT : mMutex(nullptr), mOwns(false) {}

    explicit unique_lock(Mutex& m) FL_NO_EXCEPT : mMutex(&m), mOwns(false) {
        lock();
        mOwns = true;
    }

    unique_lock(Mutex& m, defer_lock_t) FL_NO_EXCEPT : mMutex(&m), mOwns(false) {}

    unique_lock(Mutex& m, try_to_lock_t) FL_NO_EXCEPT : mMutex(&m), mOwns(m.try_lock()) {}

    unique_lock(Mutex& m, adopt_lock_t) FL_NO_EXCEPT : mMutex(&m), mOwns(true) {}

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
    unique_lock(unique_lock&& u) FL_NO_EXCEPT : mMutex(u.mMutex), mOwns(u.mOwns) {
        u.mMutex = nullptr;
        u.mOwns = false;
    }

    // Move assignment
    unique_lock& operator=(unique_lock&& u) FL_NO_EXCEPT {
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
    void lock() FL_NO_EXCEPT {
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

    bool try_lock() FL_NO_EXCEPT {
        if (!mMutex) {
            return false;
        }
        if (mOwns) {
            return false;
        }
        mOwns = mMutex->try_lock();
        return mOwns;
    }

    void unlock() FL_NO_EXCEPT {
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
    void swap(unique_lock& u) FL_NO_EXCEPT {
        using fl::swap;
        swap(mMutex, u.mMutex);
        swap(mOwns, u.mOwns);
    }

    Mutex* release() FL_NO_EXCEPT {
        Mutex* ret = mMutex;
        mMutex = nullptr;
        mOwns = false;
        return ret;
    }

    // Observers
    bool owns_lock() const FL_NO_EXCEPT { return mOwns; }
    explicit operator bool() const FL_NO_EXCEPT { return mOwns; }
    Mutex* mutex() const FL_NO_EXCEPT { return mMutex; }
};

template<typename Mutex>
void swap(unique_lock<Mutex>& lhs, unique_lock<Mutex>& rhs) FL_NO_EXCEPT {
    lhs.swap(rhs);
}

}  // namespace platforms
}  // namespace fl
