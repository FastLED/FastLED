#pragma once

#include "fl/stl/align.h"           // FL_ALIGN_AS_T macro for aligned storage
#include "fl/stl/new.h"         // Placement new operator  // IWYU pragma: keep
#include "fl/stl/compiler_control.h" // FL_HAS_SANITIZER_LSAN macro
#include "fl/stl/thread_local.h"
#include "fl/stl/noexcept.h"

#if FL_HAS_SANITIZER_LSAN
#  include <sanitizer/lsan_interface.h>
#endif

namespace fl {

namespace detail {

// Process-wide singleton registry for cross-DLL singleton sharing.
// On Windows, inline functions with static locals create per-DLL copies.
// This registry ensures all DLLs share the same singleton instance.
// Defined in singleton.cpp.hpp.
void* singleton_registry_get(const char* key) FL_NO_EXCEPT;
void singleton_registry_set(const char* key, void* value) FL_NO_EXCEPT;

} // namespace detail

// Internal singleton — NO registry lookup. Simple static storage + placement
// new. For use in .cpp.hpp files only, where each compilation unit has exactly
// one definition and cross-DLL sharing is handled by the .cpp.hpp include
// pattern itself.
//
// IMPORTANT: Singleton instances are NEVER destroyed. This implementation uses
// aligned char buffer storage and placement new to construct the instance on
// first access, but intentionally never calls the destructor. This avoids:
// 1. Static destruction order fiasco (undefined order at program exit)
// 2. Unnecessary cleanup in long-lived embedded systems (which never exit)
// 3. Crashes from singleton dependencies during destruction
//
// LSAN COMPATIBILITY: Uses __lsan::ScopedDisabler to prevent false positives.
template <typename T, int N = 0> class Singleton {
  public:
    static T &instance() FL_NO_EXCEPT {
        // Aligned char buffer storage — never destroyed
        struct FL_ALIGN_AS_T(alignof(T)) AlignedStorage {
            char data[sizeof(T)];
        };

        static AlignedStorage storage;
        static T* ptr = nullptr;
        if (!ptr) {
#if FL_HAS_SANITIZER_LSAN
            __lsan::ScopedDisabler disabler;
#endif
            ptr = new (&storage.data) T();
        }
        return *ptr;
    }

    static T *instanceRef() FL_NO_EXCEPT { return &instance(); }

    Singleton(const Singleton &) FL_NO_EXCEPT = delete;
    Singleton &operator=(const Singleton &) FL_NO_EXCEPT = delete;

  private:
    Singleton() FL_NO_EXCEPT = default;
    ~Singleton() FL_NO_EXCEPT = default;
};

// Cross-DLL singleton — WITH FL_PRETTY_FUNCTION registry. For use in header
// files where cross-DLL sharing matters. On Windows, inline functions with
// static locals create per-DLL copies; the registry prevents this.
//
// IMPORTANT: SingletonShared instances are NEVER destroyed (same rationale as
// Singleton above).
//
// CROSS-DLL SAFETY: Uses a process-wide registry to ensure all DLLs in a
// process share the same singleton instance.
//
// LSAN COMPATIBILITY: Uses __lsan::ScopedDisabler to prevent false positives.
template <typename T, int N = 0> class SingletonShared {
  public:
    static T &instance() FL_NO_EXCEPT {
        // Check the process-wide registry first (handles cross-DLL sharing).
        // FL_PRETTY_FUNCTION produces a unique string per template instantiation
        // (includes template parameters in the signature).
        void* existing = detail::singleton_registry_get(FL_PRETTY_FUNCTION);
        if (existing) {
            return *static_cast<T*>(existing);
        }
        // First time for this type — create and register
        T* ptr = instanceInner();
        detail::singleton_registry_set(FL_PRETTY_FUNCTION, ptr);
        return *ptr;
    }

    static T *instanceRef() FL_NO_EXCEPT { return &instance(); }

    SingletonShared(const SingletonShared &) FL_NO_EXCEPT = delete;
    SingletonShared &operator=(const SingletonShared &) FL_NO_EXCEPT = delete;

  private:
    SingletonShared() FL_NO_EXCEPT = default;
    ~SingletonShared() FL_NO_EXCEPT = default;

    static T* instanceInner() FL_NO_EXCEPT {
        // Aligned char buffer storage — never destroyed
        struct FL_ALIGN_AS_T(alignof(T)) AlignedStorage {
            char data[sizeof(T)];
        };

        static AlignedStorage storage;

#if FL_HAS_SANITIZER_LSAN
        __lsan::ScopedDisabler disabler;  // Ignore all allocations in this scope
#endif

        T* ptr = new (&storage.data) T();
        return ptr;
    }
};

// Thread-local singleton — combines Singleton and ThreadLocal patterns.
// Each thread gets its own T instance, but the ThreadLocal<T> container itself
// is a process-wide singleton (never destroyed, same rationale as Singleton).
//
// Replaces the common pattern:
//   T& get() { static ThreadLocal<T> tl; return tl.access(); }
// with:
//   T& get() { return SingletonThreadLocal<T>::instance(); }
//
// LSAN COMPATIBILITY: Uses __lsan::ScopedDisabler to prevent false positives.
template <typename T, int N = 0> class SingletonThreadLocal {
  public:
    static T &instance() FL_NO_EXCEPT {
        // Aligned char buffer storage — never destroyed
        struct FL_ALIGN_AS_T(alignof(ThreadLocal<T>)) AlignedStorage {
            char data[sizeof(ThreadLocal<T>)];
        };

        static AlignedStorage storage;
        static ThreadLocal<T>* ptr = nullptr;
        if (!ptr) {
            ptr = new (&storage.data) ThreadLocal<T>();
        }
        // The ThreadLocal container is never destroyed (singleton pattern).
        // Each thread's createStorage() allocation is intentionally permanent.
        // Suppress LSAN false positives for per-thread storage allocations.
#if FL_HAS_SANITIZER_LSAN
        __lsan::ScopedDisabler disabler;
#endif
        return ptr->access();
    }

    static T *instanceRef() FL_NO_EXCEPT { return &instance(); }

    SingletonThreadLocal(const SingletonThreadLocal &) FL_NO_EXCEPT = delete;
    SingletonThreadLocal &operator=(const SingletonThreadLocal &) FL_NO_EXCEPT = delete;

  private:
    SingletonThreadLocal() FL_NO_EXCEPT = default;
    ~SingletonThreadLocal() FL_NO_EXCEPT = default;
};

} // namespace fl
