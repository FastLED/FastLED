#pragma once

#include "fl/thread.h"
#include "fl/assert.h"

#if FASTLED_MULTITHREADED
#include <mutex>  // ok include
#endif

namespace fl {

template <typename T> class MutexFake;
class MutexReal;

#if FASTLED_MULTITHREADED
using mutex = MutexReal;
#else
using mutex = MutexFake<void>;
#endif

///////////////////// IMPLEMENTATION //////////////////////////////////////

template <typename T> class MutexFake {
  private:
    int mLockCount = 0;
    
  public:
    MutexFake() = default;
    
    // Non-copyable and non-movable
    MutexFake(const MutexFake&) = delete;
    MutexFake& operator=(const MutexFake&) = delete;
    MutexFake(MutexFake&&) = delete;
    MutexFake& operator=(MutexFake&&) = delete;
    
    // Recursive fake mutex operations
    void lock() {
        // In single-threaded mode, we just track the lock count for debugging
        mLockCount++;
    }
    
    void unlock() {
        // In single-threaded mode, we just track the lock count for debugging
        FL_ASSERT(mLockCount > 0, "MutexFake: unlock called without matching lock");
        mLockCount--;
    }
    
    bool try_lock() {
        // In single-threaded mode, always succeed and increment count
        mLockCount++;
        return true;
    }
};


#if FASTLED_MULTITHREADED
class MutexReal : public std::recursive_mutex {
  public:
    MutexReal() = default;
    
    // Non-copyable and non-movable (inherited from std::recursive_mutex)
    MutexReal(const MutexReal&) = delete;
    MutexReal& operator=(const MutexReal&) = delete;
    MutexReal(MutexReal&&) = delete;
    MutexReal& operator=(MutexReal&&) = delete;
    
    // Mutex operations are inherited from std::recursive_mutex:
    // - void lock()     - can be called multiple times by the same thread
    // - void unlock()   - must be called the same number of times as lock()
    // - bool try_lock() - can succeed multiple times for the same thread
};
#endif

template<typename MutexType>
class lock_guard {
  public:
    lock_guard(MutexType& mutex) : mMutex(mutex) {
        mMutex.lock();
    }

    ~lock_guard() {
        mMutex.unlock();
    }

    // Non-copyable and non-movable
    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
    lock_guard(lock_guard&&) = delete;
    lock_guard& operator=(lock_guard&&) = delete;

  private:
    MutexType& mMutex;
};

} // namespace fl
