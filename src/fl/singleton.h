#pragma once

#include "fl/align.h"           // FL_ALIGN_AS_T macro for aligned storage
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
//
// LSAN COMPATIBILITY: We use a two-level static design:
// 1. instanceInner() holds the aligned storage with placement new
// 2. instance() holds a static T* pointer initialized from instanceInner()
// This ensures LSAN can trace the typed T* pointer to find heap allocations
// reachable from the singleton, preventing false "direct leak" reports.
template <typename T, int N = 0> class Singleton {
  public:
    static T &instance() {
        // Thread-safe initialization using C++11 magic statics
        // The static T* pointer allows LSAN to trace heap allocations
        // reachable from the singleton instance.
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

        // Placement new: construct instance in pre-allocated storage
        // INTENTIONAL: Destructor is NEVER called - this is a permanent leak
        return new (&storage.data) T();
    }
};

} // namespace fl
