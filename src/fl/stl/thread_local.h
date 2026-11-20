#pragma once

#include "fl/stl/thread.h"
#if FASTLED_USE_THREAD_LOCAL
// IWYU pragma: begin_keep
#include <pthread.h>  // ok include
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep
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
// Each thread lazily gets its own heap-allocated T on first access().
//
// RAM: the container itself is only a pthread key plus one pointer. An
// explicit default is kept on the heap and only when one was supplied; with no
// default each thread's copy is value-initialized (T()). Storing an inline
// default T unconditionally doubled the cost of SingletonThreadLocal<Big>
// (see #5304).
template <typename T> class ThreadLocalReal {
  public:
    // Default: each thread's object is value-initialized
    ThreadLocalReal() FL_NO_EXCEPT { initializeKey(); }

    // With default: each thread's object is copy-constructed from defaultVal
    template <typename U>
    explicit ThreadLocalReal(const U &defaultVal) FL_NO_EXCEPT
        : mDefault(new ThreadStorage(defaultVal, 0)) {  // ok bare allocation
        initializeKey();
    }

    // Destructor - cleanup pthread key
    ~ThreadLocalReal() FL_NO_EXCEPT {
        if (mKeyInitialized) {
            // Manually clean up storage for the current thread.
            // pthread_key_delete does NOT call the cleanup function - it only
            // removes the key. Thread-specific data must be freed manually.
            ThreadStorage* storage = getStorage();
            if (storage) {
                pthread_setspecific(mKey, nullptr);
                delete storage;  // ok bare allocation
            }
            pthread_key_delete(mKey);
        }
        delete mDefault;  // ok bare allocation
    }

    // Copy constructor: copies the default (not any thread's current value)
    ThreadLocalReal(const ThreadLocalReal& other) FL_NO_EXCEPT
        : mDefault(cloneDefault(other.mDefault)) {
        initializeKey();
    }

    // Assignment operator: copies the default (key remains this instance's).
    // Threads that already accessed this instance keep their current values.
    // Not thread-safe: like construction, it must not race with another
    // thread's first access() (which reads the default).
    ThreadLocalReal& operator=(const ThreadLocalReal& other) FL_NO_EXCEPT {
        if (this != &other) {
            ThreadStorage* copy = cloneDefault(other.mDefault);
            delete mDefault;  // ok bare allocation
            mDefault = copy;
        }
        return *this;
    }

    // Access the thread-local instance
    T &access() FL_NO_EXCEPT {
        ThreadStorage* storage = getStorage();
        if (!storage) {
            storage = createStorage();
        }
        return storage->value;
    }

    const T &access() const FL_NO_EXCEPT {
        return const_cast<ThreadLocalReal *>(this)->access();
    }

    // Set the value for this thread
    void set(const T& value) FL_NO_EXCEPT {
        copyValue(access(), value);
    }

    // Convenience operators
    operator T &() FL_NO_EXCEPT { return access(); }
    operator const T &() const FL_NO_EXCEPT { return access(); }

    ThreadLocalReal &operator=(const T &v) FL_NO_EXCEPT {
        set(v);
        return *this;
    }

  private:
    // Helper function to copy values, specialized for arrays
    template<typename U>
    static void copyValue(U& dest, const U& src) FL_NO_EXCEPT {
        dest = src;  // Default behavior for non-array types
    }

    // Specialization for array types
    template<typename U, size_t N>
    static void copyValue(U (&dest)[N], const U (&src)[N]) FL_NO_EXCEPT {
        for (size_t i = 0; i < N; ++i) {
            copyValue(dest[i], src[i]);  // Recursively handle nested arrays
        }
    }

    // Per-thread storage; also used to hold the explicit default (if any).
    // Wrapping T in a struct lets the implicit copy constructor handle arrays.
    struct ThreadStorage {
        T value;
        ThreadStorage() FL_NO_EXCEPT : value() {}
        // Direct-initialize from a default of possibly different type. The
        // int tag keeps this from competing with the copy constructor.
        template <typename U>
        ThreadStorage(const U &v, int) FL_NO_EXCEPT : value(v) {}
    };

    static ThreadStorage *cloneDefault(const ThreadStorage *src) FL_NO_EXCEPT {
        return src ? new ThreadStorage(*src) : nullptr;  // ok bare allocation
    }

    // POSIX thread key for this instance
    pthread_key_t mKey;
    bool mKeyInitialized = false;
    // Heap-held default; nullptr means "value-initialize each thread's T".
    // A private, deep-copied owner (raw rather than fl::unique_ptr, whose
    // stored deleter would add a word to every instance).
    ThreadStorage *mDefault = nullptr;

    // Initialize the pthread key
    void initializeKey() FL_NO_EXCEPT {
        mKeyInitialized = pthread_key_create(&mKey, cleanupThreadStorage) == 0;
    }

    // Get thread-specific storage
    ThreadStorage* getStorage() const FL_NO_EXCEPT {
        if (!mKeyInitialized) {
            return nullptr;
        }
        return static_cast<ThreadStorage*>(pthread_getspecific(mKey));
    }

    // Create this thread's storage, copy-constructed from the default if one
    // was supplied, otherwise value-initialized.
    ThreadStorage* createStorage() FL_NO_EXCEPT {
        if (!mKeyInitialized) {
            return nullptr;
        }
        ThreadStorage *storage = mDefault
                                     ? new ThreadStorage(*mDefault)  // ok bare allocation
                                     : new ThreadStorage();  // ok bare allocation
        if (pthread_setspecific(mKey, storage) != 0) {
            delete storage;  // ok bare allocation
            return nullptr;
        }
        return storage;
    }

    // Cleanup function called when thread exits
    static void cleanupThreadStorage(void* data) FL_NO_EXCEPT {
        delete static_cast<ThreadStorage*>(data);  // ok bare allocation
    }
};

#endif // FASTLED_USE_THREAD_LOCAL

///////////////////// FAKE IMPLEMENTATION //////////////////////////////////////

template <typename T> class ThreadLocalFake {
  public:
    // Default: each thread's object is default-constructed
    ThreadLocalFake() FL_NO_EXCEPT : mValue() {}

    // With default: each thread's object is copy-constructed from defaultVal
    template <typename U>
    explicit ThreadLocalFake(const U &defaultVal) FL_NO_EXCEPT : mValue(defaultVal) {}

    // Access the thread-local instance (not actually thread-local in fake version)
    T &access() FL_NO_EXCEPT { return mValue; }
    const T &access() const FL_NO_EXCEPT { return mValue; }

    // Set the value (globally shared in fake version)
    void set(const T& value) FL_NO_EXCEPT {
        mValue = value;
    }

    // Convenience operators for "ThreadLocal<T> = x;"
    operator T &() FL_NO_EXCEPT { return access(); }
    operator const T &() const FL_NO_EXCEPT { return access(); }

    ThreadLocalFake &operator=(const T &v) FL_NO_EXCEPT {
        set(v);
        return *this;
    }

  private:
    T mValue;
};

} // namespace fl
