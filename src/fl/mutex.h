#pragma once

#include "fl/thread.h"

#if FASTLED_MULTITHREADED
#include <mutex>
#endif

namespace fl {

template <typename T> class MutexFake;
template <typename MutexType> class LockGuardFake;
class MutexReal;

#if FASTLED_MULTITHREADED
using mutex = MutexReal;
#else
using mutex = MutexFake<void>;
#endif

template <typename MutexType> using lock_guard = LockGuardFake<MutexType>;

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

template <typename MutexType> class LockGuardFake {
  public:
    explicit LockGuardFake(MutexType& mutex) : mMutex(mutex) {
        mMutex.lock();
    }
    
    ~LockGuardFake() {
        mMutex.unlock();
    }
    
    // Non-copyable and non-movable
    LockGuardFake(const LockGuardFake&) = delete;
    LockGuardFake& operator=(const LockGuardFake&) = delete;
    LockGuardFake(LockGuardFake&&) = delete;
    LockGuardFake& operator=(LockGuardFake&&) = delete;
    
  private:
    MutexType& mMutex;
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

} // namespace fl
