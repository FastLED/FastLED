#pragma once

/// @file memory_resource.h
/// @brief PMR-style polymorphic memory resource for type-erased allocation.
/// Replaces template-based allocators with runtime polymorphism.
/// Used by vector_basic to decouple allocation strategy from container type.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Polymorphic memory resource base class (PMR-style).
/// Concrete implementations wrap different allocation backends
/// (default heap, PSRAM, DMA, slab, etc.)
class memory_resource {
  public:
    virtual ~memory_resource() FL_NOEXCEPT = default;

    void* allocate(fl::size bytes) {
        if (bytes == 0) return nullptr;
        return do_allocate(bytes);
    }

    void deallocate(void* p, fl::size bytes) {
        if (!p) return;
        do_deallocate(p, bytes);
    }

    /// Try to resize in-place. Returns new pointer on success, nullptr on failure.
    /// On failure, caller must allocate-copy-deallocate manually.
    void* reallocate(void* p, fl::size old_bytes, fl::size new_bytes) {
        if (new_bytes == 0) {
            deallocate(p, old_bytes);
            return nullptr;
        }
        if (!p) return allocate(new_bytes);
        return do_reallocate(p, old_bytes, new_bytes);
    }

    bool is_equal(const memory_resource& other) const FL_NOEXCEPT {
        return do_is_equal(other);
    }

  protected:
    virtual void* do_allocate(fl::size bytes) = 0;
    virtual void do_deallocate(void* p, fl::size bytes) = 0;

    /// Default: returns nullptr (not supported). Override for realloc-capable resources.
    virtual void* do_reallocate(void* p, fl::size old_bytes, fl::size new_bytes);

    virtual bool do_is_equal(const memory_resource& other) const FL_NOEXCEPT {
        return this == &other;
    }
};

/// Get the default memory resource (wraps fl::Malloc / fl::Free / fl::realloc).
memory_resource* default_memory_resource() FL_NOEXCEPT;

/// Get the PSRAM memory resource (wraps PSRamAllocate / PSRamDeallocate).
memory_resource* psram_memory_resource();

} // namespace fl
