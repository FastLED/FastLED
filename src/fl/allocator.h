#pragma once

#include <stdlib.h>
#include <string.h>
#include "fl/inplacenew.h"
#include "fl/memfill.h"
#include "fl/type_traits.h"
#include "fl/unused.h"
#include "fl/bit_cast.h"
#include "fl/stdint.h"
#include "fl/bitset.h"

#ifndef FASTLED_DEFAULT_SLAB_SIZE
#define FASTLED_DEFAULT_SLAB_SIZE 8
#endif

namespace fl {

// Test hooks for malloc/free operations
#if defined(FASTLED_TESTING)
// Interface class for malloc/free test hooks
class MallocFreeHook {
public:
    virtual ~MallocFreeHook() = default;
    virtual void onMalloc(void* ptr, fl::size size) = 0;
    virtual void onFree(void* ptr) = 0;
};

// Set test hooks for malloc and free operations
void SetMallocFreeHook(MallocFreeHook* hook);

// Clear test hooks (set to nullptr)
void ClearMallocFreeHook();
#endif

void SetPSRamAllocator(void *(*alloc)(fl::size), void (*free)(void *));
void *PSRamAllocate(fl::size size, bool zero = true);
void PSRamDeallocate(void *ptr);
void* Malloc(fl::size size);
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
        fl::size size = sizeof(T) * n;
        void *ptr = Malloc(size);
        if (ptr == nullptr) {
            return nullptr; // Handle allocation failure
        }
        fl::memfill(ptr, 0, sizeof(T) * n); // Zero-initialize the memory
        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, fl::size n) {
        FASTLED_UNUSED(n);
        if (p == nullptr) {
            return; // Handle null pointer
        }
        Free(p); // Free the allocated memory
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



// Slab allocator for fixed-size objects
// Optimized for frequent allocation/deallocation of objects of the same size
// Uses pre-allocated memory slabs with free lists to reduce fragmentation
template <typename T, fl::size SLAB_SIZE = FASTLED_DEFAULT_SLAB_SIZE>
class SlabAllocator {
private:


    static constexpr fl::size SLAB_BLOCK_SIZE = sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*);
    static constexpr fl::size BLOCKS_PER_SLAB = SLAB_SIZE;
    static constexpr fl::size SLAB_MEMORY_SIZE = SLAB_BLOCK_SIZE * BLOCKS_PER_SLAB;

    struct Slab {
        Slab* next;
        u8* memory;
        fl::size allocated_count;
        fl::bitset_fixed<BLOCKS_PER_SLAB> allocated_blocks;  // Track which blocks are allocated
        
        Slab() : next(nullptr), memory(nullptr), allocated_count(0) {}
        
        ~Slab() {
            if (memory) {
                free(memory);
            }
        }
    };

    Slab* slabs_;
    fl::size total_allocated_;
    fl::size total_deallocated_;

    Slab* createSlab() {
        Slab* slab = static_cast<Slab*>(malloc(sizeof(Slab)));
        if (!slab) {
            return nullptr;
        }
        
        // Use placement new to properly initialize the Slab
        new(slab) Slab();
        
        slab->memory = static_cast<u8*>(malloc(SLAB_MEMORY_SIZE));
        if (!slab->memory) {
            slab->~Slab();
            free(slab);
            return nullptr;
        }
        
        // Initialize all blocks in the slab as free
        slab->allocated_blocks.reset(); // All blocks start as free
        
        // Add slab to the slab list
        slab->next = slabs_;
        slabs_ = slab;
        
        return slab;
    }

    void* allocateFromSlab(fl::size n = 1) {
        // Try to find n contiguous free blocks in existing slabs
        for (Slab* slab = slabs_; slab; slab = slab->next) {
            void* ptr = findContiguousBlocks(slab, n);
            if (ptr) {
                return ptr;
            }
        }
        
        // No contiguous blocks found, create new slab if n fits
        if (n <= BLOCKS_PER_SLAB) {
            if (!createSlab()) {
                return nullptr; // Out of memory
            }
            
            // Try again with the new slab
            return findContiguousBlocks(slabs_, n);
        }
        
        // Request too large for slab, fall back to malloc
        return nullptr;
    }
    

    
    void* findContiguousBlocks(Slab* slab, fl::size n) {
        // Check if allocation is too large for this slab
        if (n > BLOCKS_PER_SLAB) {
            return nullptr;
        }
        
        // Use bitset's find_run to find n contiguous free blocks (false = free)
        fl::i32 start = slab->allocated_blocks.find_run(false, static_cast<fl::u32>(n));
        if (start >= 0) {
            // Mark blocks as allocated
            for (fl::size i = 0; i < n; ++i) {
                slab->allocated_blocks.set(static_cast<fl::u32>(start + i), true);
            }
            slab->allocated_count += n;
            total_allocated_ += n;
            
            // Return pointer to the first block
            return slab->memory + static_cast<fl::size>(start) * SLAB_BLOCK_SIZE;
        }
        
        return nullptr;
    }

    void deallocateToSlab(void* ptr, fl::size n = 1) {
        if (!ptr) {
            return;
        }
        
        // Find which slab this block belongs to
        for (Slab* slab = slabs_; slab; slab = slab->next) {
            u8* slab_start = slab->memory;
            u8* slab_end = slab_start + SLAB_MEMORY_SIZE;
            u8* block_ptr = fl::bit_cast_ptr<u8>(ptr);
            
            if (block_ptr >= slab_start && block_ptr < slab_end) {
                fl::size block_index = (block_ptr - slab_start) / SLAB_BLOCK_SIZE;
                
                // Mark blocks as free in the bitset
                for (fl::size i = 0; i < n; ++i) {
                    if (block_index + i < BLOCKS_PER_SLAB) {
                        slab->allocated_blocks.set(block_index + i, false);
                    }
                }
                
                slab->allocated_count -= n;
                total_deallocated_ += n;
                break;
            }
        }
    }

public:
    // Constructor
    SlabAllocator() : slabs_(nullptr), total_allocated_(0), total_deallocated_(0) {}
    
    // Destructor
    ~SlabAllocator() {
        cleanup();
    }
    
    // Non-copyable
    SlabAllocator(const SlabAllocator&) = delete;
    SlabAllocator& operator=(const SlabAllocator&) = delete;
    
    // Movable
    SlabAllocator(SlabAllocator&& other) noexcept 
        : slabs_(other.slabs_), total_allocated_(other.total_allocated_), total_deallocated_(other.total_deallocated_) {
        other.slabs_ = nullptr;
        other.total_allocated_ = 0;
        other.total_deallocated_ = 0;
    }
    
    SlabAllocator& operator=(SlabAllocator&& other) noexcept {
        if (this != &other) {
            cleanup();
            slabs_ = other.slabs_;
            total_allocated_ = other.total_allocated_;
            total_deallocated_ = other.total_deallocated_;
            other.slabs_ = nullptr;
            other.total_allocated_ = 0;
            other.total_deallocated_ = 0;
        }
        return *this;
    }

    T* allocate(fl::size n = 1) {
        if (n == 0) {
            return nullptr;
        }
        
        // Try to allocate from slab first
        void* ptr = allocateFromSlab(n);
        if (ptr) {
            fl::memfill(ptr, 0, sizeof(T) * n);
            return static_cast<T*>(ptr);
        }
        
        // Fall back to regular malloc for large allocations
        ptr = malloc(sizeof(T) * n);
        if (ptr) {
            fl::memfill(ptr, 0, sizeof(T) * n);
        }
        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, fl::size n = 1) {
        if (!ptr) {
            return;
        }
        
        // Try to deallocate from slab first
        bool found_in_slab = false;
        for (Slab* slab = slabs_; slab; slab = slab->next) {
            u8* slab_start = slab->memory;
            u8* slab_end = slab_start + SLAB_MEMORY_SIZE;
            u8* block_ptr = fl::bit_cast_ptr<u8>(static_cast<void*>(ptr));
            
            if (block_ptr >= slab_start && block_ptr < slab_end) {
                deallocateToSlab(ptr, n);
                found_in_slab = true;
                break;
            }
        }
        
        if (!found_in_slab) {
            // This was allocated with regular malloc
            free(ptr);
        }
    }

    // Get allocation statistics
    fl::size getTotalAllocated() const { return total_allocated_; }
    fl::size getTotalDeallocated() const { return total_deallocated_; }
    fl::size getActiveAllocations() const { return total_allocated_ - total_deallocated_; }
    
    // Get number of slabs
    fl::size getSlabCount() const {
        fl::size count = 0;
        for (Slab* slab = slabs_; slab; slab = slab->next) {
            ++count;
        }
        return count;
    }

    // Cleanup all slabs
    void cleanup() {
        while (slabs_) {
            Slab* next = slabs_->next;
            slabs_->~Slab();
            free(slabs_);
            slabs_ = next;
        }
        total_allocated_ = 0;
        total_deallocated_ = 0;
    }
};

// STL-compatible slab allocator
template <typename T, fl::size SLAB_SIZE = FASTLED_DEFAULT_SLAB_SIZE>
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
    allocator_slab(const allocator_slab& other) noexcept {
        FASTLED_UNUSED(other);
    }

    // Copy assignment
    allocator_slab& operator=(const allocator_slab& other) noexcept {
        FASTLED_UNUSED(other);
        return *this;
    }

    // Template copy constructor
    template <typename U>
    allocator_slab(const allocator_slab<U, SLAB_SIZE>& other) noexcept {
        FASTLED_UNUSED(other);
    }

    // Destructor
    ~allocator_slab() noexcept {}

private:
    // Get the shared static allocator instance
    static SlabAllocator<T, SLAB_SIZE>& get_allocator() {
        static SlabAllocator<T, SLAB_SIZE> allocator;
        return allocator;
    }

public:
    // Allocate memory for n objects of type T
    T* allocate(fl::size n) {
        // Use a static allocator instance per type/size combination
        SlabAllocator<T, SLAB_SIZE>& allocator = get_allocator();
        return allocator.allocate(n);
    }

    // Deallocate memory for n objects of type T
    void deallocate(T* p, fl::size n) {
        // Use the same static allocator instance
        SlabAllocator<T, SLAB_SIZE>& allocator = get_allocator();
        allocator.deallocate(p, n);
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

    // Cleanup method to clean up the static slab allocator
    void cleanup() {
        // Access the same static allocator instance and clean it up
        static SlabAllocator<T, SLAB_SIZE> allocator;
        allocator.cleanup();
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

// Inlined allocator that stores the first N elements inline
// Falls back to the base allocator for additional elements
template <typename T, fl::size N, typename BaseAllocator = fl::allocator<T>>
class allocator_inlined {
private:

    // Inlined storage block
    struct InlinedStorage {
        alignas(T) u8 data[N * sizeof(T)];
        
        InlinedStorage() {
            fl::memfill(data, 0, sizeof(data));
        }
    };
    
    InlinedStorage m_inlined_storage;
    BaseAllocator m_base_allocator;
    fl::size m_inlined_used = 0;
    fl::bitset_fixed<N> m_free_bits;  // Track free slots for inlined memory only
    fl::size m_active_allocations = 0;  // Track current active allocations

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
        using other = allocator_inlined<U, N, typename BaseAllocator::template rebind<U>::other>;
    };

    // Default constructor
    allocator_inlined() noexcept = default;

    // Copy constructor
    allocator_inlined(const allocator_inlined& other) noexcept {
        // Copy inlined data
        m_inlined_used = other.m_inlined_used;
        for (fl::size i = 0; i < m_inlined_used; ++i) {
            new (&get_inlined_ptr()[i]) T(other.get_inlined_ptr()[i]);
        }
        
        // Copy free bits
        m_free_bits = other.m_free_bits;
        
        // Note: Heap allocations are not copied, only inlined data
        
        // Copy active allocations count
        m_active_allocations = other.m_active_allocations;
    }

    // Copy assignment
    allocator_inlined& operator=(const allocator_inlined& other) noexcept {
        if (this != &other) {
            clear();
            
            // Copy inlined data
            m_inlined_used = other.m_inlined_used;
            for (fl::size i = 0; i < m_inlined_used; ++i) {
                new (&get_inlined_ptr()[i]) T(other.get_inlined_ptr()[i]);
            }
            
            // Copy free bits
            m_free_bits = other.m_free_bits;
            
            // Note: Heap allocations are not copied, only inlined data
            
            // Copy active allocations count
            m_active_allocations = other.m_active_allocations;
        }
        return *this;
    }

    // Template copy constructor
    template <typename U>
    allocator_inlined(const allocator_inlined<U, N, typename BaseAllocator::template rebind<U>::other>& other) noexcept {
        FASTLED_UNUSED(other);
    }

    // Destructor
    ~allocator_inlined() noexcept {
        clear();
    }

    // Allocate memory for n objects of type T
    T* allocate(fl::size n) {
        if (n == 0) {
            return nullptr;
        }
        
        // For large allocations (n > 1), use base allocator directly
        if (n > 1) {
            T* ptr = m_base_allocator.allocate(n);
            if (ptr) {
                m_active_allocations += n;
            }
            return ptr;
        }
        
        // For single allocations, first try inlined memory
        // Find first free inlined slot
        fl::i32 free_slot = m_free_bits.find_first(false);
        if (free_slot >= 0 && static_cast<fl::size>(free_slot) < N) {
            // Mark the inlined slot as used
            m_free_bits.set(static_cast<fl::u32>(free_slot), true);
            
            // Update inlined usage tracking
            if (static_cast<fl::size>(free_slot) + 1 > m_inlined_used) {
                m_inlined_used = static_cast<fl::size>(free_slot) + 1;
            }
            m_active_allocations++;
            return &get_inlined_ptr()[static_cast<fl::size>(free_slot)];
        }
        
        // No inlined slots available, use heap allocation
        T* ptr = m_base_allocator.allocate(1);
        if (ptr) {
            m_active_allocations++;
        }
        return ptr;
    }

    // Deallocate memory for n objects of type T
    void deallocate(T* p, fl::size n) {
        if (!p || n == 0) {
            return;
        }
        
        // Check if this is inlined memory
        T* inlined_start = get_inlined_ptr();
        T* inlined_end = inlined_start + N;
        
        if (p >= inlined_start && p < inlined_end) {
            // This is inlined memory, mark slots as free
            fl::size slot_index = (p - inlined_start);
            for (fl::size i = 0; i < n; ++i) {
                if (slot_index + i < N) {
                    m_free_bits.set(slot_index + i, false); // Mark as free
                }
            }
            m_active_allocations -= n;
            return;
        }
        

        
        // Fallback to base allocator for heap allocations
        m_base_allocator.deallocate(p, n);
        m_active_allocations -= n;
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

    // Clear all allocated memory
    void clear() {
        // Destroy inlined objects
        for (fl::size i = 0; i < m_inlined_used; ++i) {
            get_inlined_ptr()[i].~T();
        }
        m_inlined_used = 0;
        m_free_bits.reset();
        m_active_allocations = 0;
        
        // Clean up the base allocator (for SlabAllocator, this clears slabs and free lists)
        cleanup_base_allocator();
    }

    // Get total allocated size
    fl::size total_size() const {
        return m_active_allocations;
    }

    // Get inlined capacity
    fl::size inlined_capacity() const {
        return N;
    }

    // Check if using inlined storage
    bool is_using_inlined() const {
        return m_active_allocations == m_inlined_used;
    }

private:
    T* get_inlined_ptr() {
        return reinterpret_cast<T*>(m_inlined_storage.data);
    }
    
    const T* get_inlined_ptr() const {
        return reinterpret_cast<const T*>(m_inlined_storage.data);
    }
    
    // SFINAE helper to detect if base allocator has cleanup() method
    template<typename U>
    static auto has_cleanup_impl(int) -> decltype(fl::declval<U>().cleanup(), fl::true_type{});
    
    template<typename U>
    static fl::false_type has_cleanup_impl(...);
    
    using has_cleanup = decltype(has_cleanup_impl<BaseAllocator>(0));
    
    // Call cleanup on base allocator if it has the method
    void cleanup_base_allocator() {
        cleanup_base_allocator_impl(has_cleanup{});
    }
    
    void cleanup_base_allocator_impl(fl::true_type) {
        m_base_allocator.cleanup();
    }
    
    void cleanup_base_allocator_impl(fl::false_type) {
        // Base allocator doesn't have cleanup method, do nothing
    }
    


    // Equality comparison
    bool operator==(const allocator_inlined& other) const noexcept {
        FASTLED_UNUSED(other);
        return true; // All instances are equivalent for now
    }

    bool operator!=(const allocator_inlined& other) const noexcept {
        return !(*this == other);
    }
};

// Inlined allocator that uses PSRam for heap allocation
template <typename T, fl::size N>
using allocator_inlined_psram = allocator_inlined<T, N, fl::allocator_psram<T>>;

// Inlined allocator that uses slab allocator for heap allocation
template <typename T, fl::size N, fl::size SLAB_SIZE = 8>
using allocator_inlined_slab_psram = allocator_inlined<T, N, fl::allocator_slab<T, SLAB_SIZE>>;


template <typename T, fl::size N>
using allocator_inlined_slab = allocator_inlined<T, N, fl::allocator_slab<T>>;

} // namespace fl
