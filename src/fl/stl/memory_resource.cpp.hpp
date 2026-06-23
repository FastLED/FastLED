// IWYU pragma: private

#include "fl/stl/memory_resource.h"
#include "fl/stl/allocator.h"  // fl::Malloc, fl::Free, PSRamAllocate, PSRamDeallocate
#include "fl/stl/malloc.h"     // fl::realloc
#include "fl/stl/cstring.h"    // fl::memset
#include "fl/stl/noexcept.h"

namespace fl {

// Default implementation: reallocate not supported.
void* memory_resource::do_reallocate(void* p, fl::size old_bytes, fl::size new_bytes) {
    FASTLED_UNUSED(p);
    FASTLED_UNUSED(old_bytes);
    FASTLED_UNUSED(new_bytes);
    return nullptr;
}

// ======= Default Memory Resource (Malloc/Free/realloc) =======

namespace {

class DefaultMemoryResource : public memory_resource {
  protected:
    void* do_allocate(fl::size bytes) override {
        void* p = fl::Malloc(bytes);
        if (p) {
            fl::memset(p, 0, bytes);
        }
        return p;
    }

    void do_deallocate(void* p, fl::size bytes) override {
        FASTLED_UNUSED(bytes);
        fl::Free(p);
    }

    void* do_reallocate(void* p, fl::size old_bytes, fl::size new_bytes) override {
        void* result = fl::realloc(p, new_bytes);
        if (result && new_bytes > old_bytes) {
            // Zero-initialize newly allocated region
            fl::memset(static_cast<char*>(result) + old_bytes, 0, new_bytes - old_bytes);
        }
        return result;
    }

    bool do_is_equal(const memory_resource& other) const FL_NOEXCEPT override {
        return this == &other;
    }
};

class PSRamMemoryResource : public memory_resource {
  protected:
    void* do_allocate(fl::size bytes) override {
        return fl::PSRamAllocate(bytes, true);  // zero-initialized
    }

    void do_deallocate(void* p, fl::size bytes) override {
        FASTLED_UNUSED(bytes);
        fl::PSRamDeallocate(p);
    }

    // PSRAM does not support realloc — returns nullptr to trigger alloc-copy-free path
    // do_reallocate uses the base default (returns nullptr)

    bool do_is_equal(const memory_resource& other) const FL_NOEXCEPT override {
        return this == &other;
    }
};

} // anonymous namespace

memory_resource* default_memory_resource() FL_NOEXCEPT {
    static DefaultMemoryResource instance;
    return &instance;
}

memory_resource* psram_memory_resource() {
    static PSRamMemoryResource instance;
    return &instance;
}

} // namespace fl
