#pragma once

#include "fl/thread.h"

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
  public:
    MutexFake() = default;
    
    // Non-copyable and non-movable
    MutexFake(const MutexFake&) = delete;
    MutexFake& operator=(const MutexFake&) = delete;
    MutexFake(MutexFake&&) = delete;
    MutexFake& operator=(MutexFake&&) = delete;
    
    // Fake mutex operations - these do nothing but provide the interface
    void lock() {
        // In a real implementation, this would block until the mutex is acquired
        // Fake implementation: do nothing
    }
    
    void unlock() {
        // In a real implementation, this would release the mutex
        // Fake implementation: do nothing
    }
    
    bool try_lock() {
        // In a real implementation, this would try to acquire the mutex without blocking
        // Fake implementation: always return true (success)
        return true;
    }
};


#if FASTLED_MULTITHREADED
class MutexReal : public std::mutex {
  public:
    MutexReal() = default;
    
    // Non-copyable and non-movable (inherited from std::mutex)
    MutexReal(const MutexReal&) = delete;
    MutexReal& operator=(const MutexReal&) = delete;
    MutexReal(MutexReal&&) = delete;
    MutexReal& operator=(MutexReal&&) = delete;
    
    // Mutex operations are inherited from std::mutex:
    // - void lock()
    // - void unlock()
    // - bool try_lock()
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
