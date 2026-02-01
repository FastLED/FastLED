#pragma once

#include "fl/align.h"           // FL_ALIGN_AS_T macro for aligned storage
#include "fl/stl/new.h"         // Placement new operator
#include "fl/compiler_control.h" // FL_HAS_SANITIZER_LSAN macro

#if FL_HAS_SANITIZER_LSAN
#  include <sanitizer/lsan_interface.h>
#endif

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
//
// LSAN COMPATIBILITY: Uses __lsan::ScopedDisabler to prevent false positives.
// LSAN cannot properly trace through char[] storage to find heap allocations
// reachable from singleton members. The scoped disabler tells LSAN to ignore
// all allocations during singleton construction (both the placement new and
// any heap allocations in T's constructor).
template <typename T, int N = 0> class Singleton {
  public:
    static T &instance() {
        // Thread-safe initialization using C++11 magic statics
        static T* ptr = instanceInner();
        return *ptr;
    }

    static T *instanceRef() { return &instance(); }

    Singleton(const Singleton &) = delete;
    Singleton &operator=(const Singleton &) = delete;

  private:
    Singleton() = default;
    ~Singleton() = default;

    static T* instanceInner() {
        // Aligned char buffer storage - never destroyed
        // Use a struct wrapper to apply alignment attributes
        struct FL_ALIGN_AS_T(alignof(T)) AlignedStorage {
            char data[sizeof(T)];
        };

        // Static storage persists for program lifetime
        static AlignedStorage storage;

#if FL_HAS_SANITIZER_LSAN
        __lsan::ScopedDisabler disabler;  // Ignore all allocations in this scope
#endif

        // Placement new: construct instance in pre-allocated storage
        // INTENTIONAL: Destructor is NEVER called - this is a permanent leak
        T* ptr = new (&storage.data) T();
        return ptr;
    }
};

} // namespace fl
