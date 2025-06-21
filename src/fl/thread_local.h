#pragma once

#include "fl/thread.h"
#if FASTLED_USE_THREAD_LOCAL
#include "fl/map.h"
#endif

#if FASTLED_USE_THREAD_LOCAL
// Real thread-local implementation
#else
// Fake thread-local implementation (globally shared data)
#if FASTLED_MULTITHREADED
#warning                                                                       \
    "ThreadLocal is not implemented, using the fake version with globally shared data"
#endif
#endif

namespace fl {

#if FASTLED_USE_THREAD_LOCAL
template <typename T> class ThreadLocalReal;
template <typename T> using ThreadLocal = ThreadLocalReal<T>;
#else
template <typename T> class ThreadLocalFake;
template <typename T> using ThreadLocal = ThreadLocalFake<T>;
#endif

///////////////////// REAL IMPLEMENTATION //////////////////////////////////////

#if FASTLED_USE_THREAD_LOCAL
template <typename T> class ThreadLocalReal {
  public:
    // Default: each thread's object is default-constructed
    ThreadLocalReal() : mDefaultValue(), mHasDefault(false) {}
    
    // With default: each thread's object is copy-constructed from defaultVal
    template <typename U>
    explicit ThreadLocalReal(const U &defaultVal) : mDefaultValue(defaultVal), mHasDefault(true) {}

    // Access the thread-local instance
    T &access() { 
        // Get the thread-local storage for this instance
        auto& storage = getStorage();
        if (mHasDefault && !storage.initialized) {
            storage.value = mDefaultValue;
            storage.initialized = true;
        }
        return storage.value; 
    }
    
    const T &access() const { 
        // Get the thread-local storage for this instance
        auto& storage = getStorage();
        if (mHasDefault && !storage.initialized) {
            storage.value = mDefaultValue;
            storage.initialized = true;
        }
        return storage.value; 
    }

    // Set the value for this thread
    void set(const T& value) {
        access() = value;
    }

    // Convenience operators
    operator T &() { return access(); }
    operator const T &() const { return access(); }
    
    ThreadLocalReal &operator=(const T &v) {
        set(v);
        return *this;
    }

  private:
    struct ThreadStorage {
        T value{};
        bool initialized = false;
    };
    
    // Each instance gets its own thread-local storage map
    // We use the instance address as the key to distinguish between different ThreadLocal instances
    ThreadStorage& getStorage() const {
        thread_local static fl::map<const void*, ThreadStorage> storageMap;
        return storageMap[this];
    }
    
    T mDefaultValue{};
    bool mHasDefault = false;
};

#endif // FASTLED_USE_THREAD_LOCAL

///////////////////// FAKE IMPLEMENTATION //////////////////////////////////////

template <typename T> class ThreadLocalFake {
  public:
    // Default: each thread's object is default-constructed
    ThreadLocalFake() : mValue() {}

    // With default: each thread's object is copy-constructed from defaultVal
    template <typename U>
    explicit ThreadLocalFake(const U &defaultVal) : mValue(defaultVal) {}

    // Access the thread-local instance (not actually thread-local in fake version)
    T &access() { return mValue; }
    const T &access() const { return mValue; }

    // Set the value (globally shared in fake version)
    void set(const T& value) {
        mValue = value;
    }

    // Convenience operators for "ThreadLocal<T> = x;"
    operator T &() { return access(); }
    operator const T &() const { return access(); }
    
    ThreadLocalFake &operator=(const T &v) {
        set(v);
        return *this;
    }

  private:
    T mValue;
};

} // namespace fl
