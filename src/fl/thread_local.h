#pragma once

#include "fl/thread.h"

#if FASTLED_MULTITHREADED
#warning                                                                       \
    "ThreadLocal is not implemented, using the fake version with globally shared data"
#endif

namespace fl {

template <typename T> class ThreadLocalFake;

template <typename T> using ThreadLocal = ThreadLocalFake<T>;

///////////////////// IMPLEMENTATION //////////////////////////////////////

template <typename T> class ThreadLocalFake {
  public:
    // Default: each thread’s object is default-constructed
    ThreadLocalFake() : mValue() {}

    // With default: each thread’s object is copy-constructed from defaultVal
    template <typename U>
    explicit ThreadLocalFake(const U &defaultVal) : mValue(defaultVal) {}

    // Access the thread-local instance
    T &access() { return mValue; }
    const T &access() const { return mValue; }

    // Convenience operators for ThreadLocal<T> = x;”
    operator T &() { return access(); }
    ThreadLocalFake &operator=(const T &v) {
        set(v);
        return *this;
    }

  private:
    T mValue;
};

} // namespace fl