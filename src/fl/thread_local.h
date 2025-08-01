#pragma once

#include "fl/thread.h"
#if FASTLED_USE_THREAD_LOCAL
#include "fl/hash_map.h"
#include <pthread.h>  // ok include
#include <memory>  // ok include
#endif

#if FASTLED_USE_THREAD_LOCAL
// Real thread-local implementation using POSIX threading
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
    ThreadLocalReal() : mDefaultValue(), mHasDefault(false) {
        initializeKey();
    }
    
    // With default: each thread's object is copy-constructed from defaultVal
    template <typename U>
    explicit ThreadLocalReal(const U &defaultVal) : mDefaultValue(defaultVal), mHasDefault(true) {
        initializeKey();
    }

    // Destructor - cleanup pthread key
    ~ThreadLocalReal() {
        if (mKeyInitialized) {
            pthread_key_delete(mKey);
        }
    }

    // Copy constructor
    ThreadLocalReal(const ThreadLocalReal& other) : mDefaultValue(other.mDefaultValue), mHasDefault(other.mHasDefault) {
        initializeKey();
    }

    // Assignment operator
    ThreadLocalReal& operator=(const ThreadLocalReal& other) {
        if (this != &other) {
            mDefaultValue = other.mDefaultValue;
            mHasDefault = other.mHasDefault;
            // Key remains the same for this instance
        }
        return *this;
    }

    // Access the thread-local instance
    T &access() { 
        ThreadStorage* storage = getStorage();
        if (!storage) {
            storage = createStorage();
        }
        if (mHasDefault && !storage->initialized) {
            copyValue(storage->value, mDefaultValue);
            storage->initialized = true;
        }
        return storage->value; 
    }
    
    const T &access() const { 
        ThreadStorage* storage = getStorage();
        if (!storage) {
            storage = createStorage();
        }
        if (mHasDefault && !storage->initialized) {
            copyValue(storage->value, mDefaultValue);
            storage->initialized = true;
        }
        return storage->value; 
    }

    // Set the value for this thread
    void set(const T& value) {
        copyValue(access(), value);
    }

    // Convenience operators
    operator T &() { return access(); }
    operator const T &() const { return access(); }
    
    ThreadLocalReal &operator=(const T &v) {
        set(v);
        return *this;
    }

  private:
    // Helper function to copy values, specialized for arrays
    template<typename U>
    static void copyValue(U& dest, const U& src) {
        dest = src;  // Default behavior for non-array types
    }

    // Specialization for array types
    template<typename U, size_t N>
    static void copyValue(U (&dest)[N], const U (&src)[N]) {
        for (size_t i = 0; i < N; ++i) {
            copyValue(dest[i], src[i]);  // Recursively handle nested arrays
        }
    }

    // Storage for thread-local data
    struct ThreadStorage {
        T value{};
        bool initialized = false;
    };
    
    // POSIX thread key for this instance
    pthread_key_t mKey;
    bool mKeyInitialized = false;
    T mDefaultValue{};
    bool mHasDefault = false;
    
    // Initialize the pthread key
    void initializeKey() {
        int result = pthread_key_create(&mKey, cleanupThreadStorage);
        if (result == 0) {
            mKeyInitialized = true;
        } else {
            // Handle error - for now just mark as not initialized
            mKeyInitialized = false;
        }
    }
    
    // Get thread-specific storage
    ThreadStorage* getStorage() const {
        if (!mKeyInitialized) {
            return nullptr;
        }
        return static_cast<ThreadStorage*>(pthread_getspecific(mKey));
    }
    
    // Create thread-specific storage
    ThreadStorage* createStorage() const {
        if (!mKeyInitialized) {
            return nullptr;
        }
        
        ThreadStorage* storage = new ThreadStorage();
        int result = pthread_setspecific(mKey, storage);
        if (result != 0) {
            delete storage;
            return nullptr;
        }
        return storage;
    }
    
    // Cleanup function called when thread exits
    static void cleanupThreadStorage(void* data) {
        if (data) {
            delete static_cast<ThreadStorage*>(data);
        }
    }
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
