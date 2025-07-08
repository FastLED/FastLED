#pragma once

#include <stdlib.h>
#include <string.h>
#include "fl/inplacenew.h"
#include "fl/memset.h"
#include "fl/type_traits.h"
#include "fl/unused.h"
#include "fl/bit_cast.h"
#include "fl/stdint.h"

namespace fl {

void SetPSRamAllocator(void *(*alloc)(fl::size), void (*free)(void *));
void *PSRamAllocate(fl::size size, bool zero = true);
void PSRamDeallocate(void *ptr);
void Malloc(fl::size size);
void Free(void *ptr);

template <typename T> class PSRamAllocator {
  public:
    static T *Alloc(fl::size n) {
        void *ptr = PSRamAllocate(sizeof(T) * n, true);
        return fl::bit_cast_ptr<T>(ptr);
    }

    static void Free(T *p) {
        if (p == nullptr) {
            return;
        }
        PSRamDeallocate(p);
    }
};

// std compatible allocator.
template <typename T> class allocator {
  public:
    // Type definitions required by STL
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = fl::size;
    using difference_type = ptrdiff_t;
    
    // Rebind allocator to type U
    template <typename U>
    struct rebind {
        using other = allocator<U>;
    };
    
    // Default constructor
    allocator() noexcept {}
    
    // Copy constructor
    template <typename U>
    allocator(const allocator<U>&) noexcept {}
    
    // Destructor
    ~allocator() noexcept {}

    // Use this to allocate large blocks of memory for T.
    // This is useful for large arrays or objects that need to be allocated
    // in a single block.
    T* allocate(fl::size n) {
        if (n == 0) {
            return nullptr; // Handle zero allocation
        }
        void *ptr = malloc(sizeof(T) * n);
        if (ptr == nullptr) {
            return nullptr; // Handle allocation failure
        }
        fl::memset(ptr, 0, sizeof(T) * n); // Zero-initialize the memory
        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, fl::size n) {
        FASTLED_UNUSED(n);
        if (p == nullptr) {
            return; // Handle null pointer
        }
        free(p); // Free the allocated memory
    }
    
    // Construct an object at the specified address
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        if (p == nullptr) return;
        new(static_cast<void*>(p)) U(fl::forward<Args>(args)...);
    }
    
    // Destroy an object at the specified address
    template <typename U>
    void destroy(U* p) {
        if (p == nullptr) return;
        p->~U();
    }
};

template <typename T> class allocator_psram {
    public:
        // Type definitions required by STL
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = fl::size;
        using difference_type = ptrdiff_t;
    
        // Rebind allocator to type U
        template <typename U>
        struct rebind {
            using other = allocator_psram<U>;
        };
    
        // Default constructor
        allocator_psram() noexcept {}
    
        // Copy constructor
        template <typename U>
        allocator_psram(const allocator_psram<U>&) noexcept {}
    
        // Destructor
        ~allocator_psram() noexcept {}
    
        // Allocate memory for n objects of type T
        T* allocate(fl::size n) {
            return PSRamAllocator<T>::Alloc(n);
        }
    
        // Deallocate memory for n objects of type T
        void deallocate(T* p, fl::size n) {
            PSRamAllocator<T>::Free(p);
            FASTLED_UNUSED(n);
        }
    
        // Construct an object at the specified address
        template <typename U, typename... Args>
        void construct(U* p, Args&&... args) {
            if (p == nullptr) return;
            new(static_cast<void*>(p)) U(fl::forward<Args>(args)...);
        }
    
        // Destroy an object at the specified address
        template <typename U>
        void destroy(U* p) {
            if (p == nullptr) return;
            p->~U();
        }
};


// Macro to specialize the Allocator for a specific type to use PSRam
// Usage: FL_USE_PSRAM_ALLOCATOR(MyClass)
#define FL_USE_PSRAM_ALLOCATOR(TYPE) \
template <> \
class allocator<TYPE> { \
  public: \
    using value_type = TYPE; \
    using pointer = TYPE*; \
    using const_pointer = const TYPE*; \
    using reference = TYPE&; \
    using const_reference = const TYPE&; \
    using size_type = fl::size; \
    using difference_type = ptrdiff_t; \
    \
    template <typename U> \
    struct rebind { \
        using other = allocator<U>; \
    }; \
    \
    allocator() noexcept {} \
    \
    template <typename U> \
    allocator(const allocator<U>&) noexcept {} \
    \
    ~allocator() noexcept {} \
    \
    TYPE *allocate(fl::size n) { \
        return fl::bit_cast_ptr<TYPE>(PSRamAllocate(sizeof(TYPE) * n, true)); \
    } \
    void deallocate(TYPE *p, fl::size n) { \
        if (p == nullptr) { \
            return; \
        } \
        PSRamDeallocate(p); \
    } \
    \
    template <typename U, typename... Args> \
    void construct(U* p, Args&&... args) { \
        if (p == nullptr) return; \
        new(static_cast<void*>(p)) U(fl::forward<Args>(args)...); \
    } \
    \
    template <typename U> \
    void destroy(U* p) { \
        if (p == nullptr) return; \
        p->~U(); \
    } \
};

// Slab allocator for fixed-size objects
// Optimized for frequent allocation/deallocation of objects of the same size
// Uses pre-allocated memory slabs with free lists to reduce fragmentation
template <typename T, fl::size SLAB_SIZE = 64>
class SlabAllocator {
private:
    struct FreeBlock {
        FreeBlock* next;
    };

    struct Slab {
        Slab* next;
        uint8_t* memory;
        fl::size allocated_count;
        
        Slab() : next(nullptr), memory(nullptr), allocated_count(0) {}
        
        ~Slab() {
            if (memory) {
                free(memory);
            }
        }
    };

    static constexpr fl::size BLOCK_SIZE = sizeof(T) > sizeof(FreeBlock*) ? sizeof(T) : sizeof(FreeBlock*);
    static constexpr fl::size BLOCKS_PER_SLAB = SLAB_SIZE;
    static constexpr fl::size SLAB_MEMORY_SIZE = BLOCK_SIZE * BLOCKS_PER_SLAB;

    static Slab* slabs_;
    static FreeBlock* free_list_;
    static fl::size total_allocated_;
    static fl::size total_deallocated_;

    static Slab* createSlab() {
        Slab* slab = static_cast<Slab*>(malloc(sizeof(Slab)));
        if (!slab) {
            return nullptr;
        }
        
        // Use placement new to properly initialize the Slab
        new(slab) Slab();
        
        slab->memory = static_cast<uint8_t*>(malloc(SLAB_MEMORY_SIZE));
        if (!slab->memory) {
            slab->~Slab();
            free(slab);
            return nullptr;
        }
        
        // Initialize all blocks in the slab as free
        for (fl::size i = 0; i < BLOCKS_PER_SLAB; ++i) {
            FreeBlock* block = fl::bit_cast_ptr<FreeBlock>(static_cast<void*>(slab->memory + i * BLOCK_SIZE));
            block->next = free_list_;
            free_list_ = block;
        }
        
        // Add slab to the slab list
        slab->next = slabs_;
        slabs_ = slab;
        
        return slab;
    }

    static void* allocateFromSlab() {
        if (!free_list_) {
            if (!createSlab()) {
                return nullptr; // Out of memory
            }
        }
        
        FreeBlock* block = free_list_;
        free_list_ = free_list_->next;
        ++total_allocated_;
        
        // Find which slab this block belongs to and increment its count
        for (Slab* slab = slabs_; slab; slab = slab->next) {
            uint8_t* slab_start = slab->memory;
            uint8_t* slab_end = slab_start + SLAB_MEMORY_SIZE;
            uint8_t* block_ptr = fl::bit_cast_ptr<uint8_t>(static_cast<void*>(block));
            
            if (block_ptr >= slab_start && block_ptr < slab_end) {
                ++slab->allocated_count;
                break;
            }
        }
        
        return block;
    }

    static void deallocateToSlab(void* ptr) {
        if (!ptr) {
            return;
        }
        
        // Find which slab this block belongs to and decrement its count
        for (Slab* slab = slabs_; slab; slab = slab->next) {
            uint8_t* slab_start = slab->memory;
            uint8_t* slab_end = slab_start + SLAB_MEMORY_SIZE;
            uint8_t* block_ptr = fl::bit_cast_ptr<uint8_t>(ptr);
            
            if (block_ptr >= slab_start && block_ptr < slab_end) {
                --slab->allocated_count;
                break;
            }
        }
        
        FreeBlock* block = fl::bit_cast_ptr<FreeBlock>(ptr);
        block->next = free_list_;
        free_list_ = block;
        ++total_deallocated_;
    }

public:
    static T* allocate(fl::size n = 1) {
        if (n != 1) {
            // Slab allocator only supports single object allocation
            // Fall back to regular malloc for bulk allocations
            void* ptr = malloc(sizeof(T) * n);
            if (ptr) {
                fl::memset(ptr, 0, sizeof(T) * n);
            }
            return static_cast<T*>(ptr);
        }
        
        void* ptr = allocateFromSlab();
        if (ptr) {
            fl::memset(ptr, 0, sizeof(T));
        }
        return static_cast<T*>(ptr);
    }

    static void deallocate(T* ptr, fl::size n = 1) {
        if (!ptr) {
            return;
        }
        
        if (n != 1) {
            // This was allocated with regular malloc
            free(ptr);
            return;
        }
        
        deallocateToSlab(ptr);
    }

    // Get allocation statistics
    static fl::size getTotalAllocated() { return total_allocated_; }
    static fl::size getTotalDeallocated() { return total_deallocated_; }
    static fl::size getActiveAllocations() { return total_allocated_ - total_deallocated_; }
    
    // Get number of slabs
    static fl::size getSlabCount() {
        fl::size count = 0;
        for (Slab* slab = slabs_; slab; slab = slab->next) {
            ++count;
        }
        return count;
    }

    // Cleanup all slabs (call at program exit)
    static void cleanup() {
        while (slabs_) {
            Slab* next = slabs_->next;
            slabs_->~Slab();
            free(slabs_);
            slabs_ = next;
        }
        free_list_ = nullptr;
        total_allocated_ = 0;
        total_deallocated_ = 0;
    }
};

// Static member definitions
template <typename T, fl::size SLAB_SIZE>
typename SlabAllocator<T, SLAB_SIZE>::Slab* SlabAllocator<T, SLAB_SIZE>::slabs_ = nullptr;

template <typename T, fl::size SLAB_SIZE>
typename SlabAllocator<T, SLAB_SIZE>::FreeBlock* SlabAllocator<T, SLAB_SIZE>::free_list_ = nullptr;

template <typename T, fl::size SLAB_SIZE>
fl::size SlabAllocator<T, SLAB_SIZE>::total_allocated_ = 0;

template <typename T, fl::size SLAB_SIZE>
fl::size SlabAllocator<T, SLAB_SIZE>::total_deallocated_ = 0;

// STL-compatible slab allocator
template <typename T, fl::size SLAB_SIZE = 64>
class allocator_slab {
public:
    // Type definitions required by STL
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = fl::size;
    using difference_type = ptrdiff_t;

    // Rebind allocator to type U
    template <typename U>
    struct rebind {
        using other = typename fl::conditional<
            fl::is_same<U, void>::value,
            allocator_slab<char, SLAB_SIZE>,
            allocator_slab<U, SLAB_SIZE>
        >::type;
    };

    // Default constructor
    allocator_slab() noexcept {}

    // Copy constructor
    template <typename U>
    allocator_slab(const allocator_slab<U, SLAB_SIZE>&) noexcept {}

    // Destructor
    ~allocator_slab() noexcept {}

    // Allocate memory for n objects of type T
    T* allocate(fl::size n) {
        return SlabAllocator<T, SLAB_SIZE>::allocate(n);
    }

    // Deallocate memory for n objects of type T
    void deallocate(T* p, fl::size n) {
        SlabAllocator<T, SLAB_SIZE>::deallocate(p, n);
    }

    // Construct an object at the specified address
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        if (p == nullptr) return;
        new(static_cast<void*>(p)) U(fl::forward<Args>(args)...);
    }

    // Destroy an object at the specified address
    template <typename U>
    void destroy(U* p) {
        if (p == nullptr) return;
        p->~U();
    }

    // Equality comparison
    bool operator==(const allocator_slab& other) const noexcept {
        FASTLED_UNUSED(other);
        return true; // All instances are equivalent
    }

    bool operator!=(const allocator_slab& other) const noexcept {
        return !(*this == other);
    }
};

} // namespace fl
