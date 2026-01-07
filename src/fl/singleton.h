#pragma once

#include "fl/align.h"           // FL_ALIGN_AS_T macro for aligned storage
#include "fl/stl/bit_cast.h"    // Safe type-punning via bit_cast_ptr
#include "fl/stl/new.h"          // Placement new operator

namespace fl {

// A templated singleton class, parameterized by the type of the singleton and
// an optional integer.
//
// IMPORTANT: Singleton instances are NEVER destroyed. This implementation uses
// aligned char buffer storage and placement new to construct the instance on
// first access, but intentionally never calls the destructor. This avoids:
// 1. Static destruction order fiasco (undefined order at program exit)
// 2. Unnecessary cleanup in long-lived embedded systems (which never exit)
// 3. Crashes from singleton dependencies during destruction
//
// The instance is constructed on first call to instance() and lives until
// process termination (or forever in embedded systems).
template <typename T, int N = 0> class Singleton {
  public:
    static T &instance() {
        // Thread-safe initialization using C++11 magic statics
        // The compiler and runtime ensure that the static local variable is initialized
        // exactly once, even if multiple threads call instance() simultaneously.
        // C++11 guarantees that if control enters the declaration concurrently while
        // the variable is being initialized, the concurrent execution shall wait for
        // completion of the initialization.
        //
        // Aligned char buffer storage - never destroyed
        // Use a struct wrapper to apply alignment attributes (alignas cannot be used directly on static variables)
        struct FL_ALIGN_AS_T(alignof(T)) AlignedStorage {
            char data[sizeof(T)];
        };

        // Use a struct with constructor to ensure initialization happens exactly once
        // This leverages C++11 magic statics for thread-safe initialization
        struct InitializedStorage {
            AlignedStorage storage;
            InitializedStorage() {
                // Placement new: construct instance in pre-allocated storage
                // INTENTIONAL: Destructor is NEVER called - this is a permanent leak
                new (&storage.data) T();
            }
        };

        static InitializedStorage init;

        // Safe type-punning from char* to T* using bit_cast_ptr
        return *fl::bit_cast_ptr<T>(&init.storage.data[0]);
    }

    static T *instanceRef() { return &instance(); }

    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;

  private:
    Singleton() = default;
    ~Singleton() = default;
};

} // namespace fl
