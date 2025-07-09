#pragma once

#include <stdlib.h>
#include <string.h>
#include "fl/inplacenew.h"
#include "fl/memset.h"
#include "fl/type_traits.h"
#include "fl/unused.h"
#include "fl/bit_cast.h"
#include "fl/stdint.h"
#include "fl/bitset.h"

#ifndef FASTLED_DEFAULT_SLAB_SIZE
#define FASTLED_DEFAULT_SLAB_SIZE 8
#endif

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



// Slab allocator for fixed-size objects
// Optimized for frequent allocation/deallocation of objects of the same size
// Uses pre-allocated memory slabs with free lists to reduce fragmentation
template <typename T, fl::size SLAB_SIZE = FASTLED_DEFAULT_SLAB_SIZE>
class SlabAllocator {
private:
    struct FreeBlock {
        FreeBlock* next;
    };

    struct Slab {
        Slab* next;
        u8* memory;
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

    Slab* slabs_;
    FreeBlock* free_list_;
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

    void* allocateFromSlab() {
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
            u8* slab_start = slab->memory;
            u8* slab_end = slab_start + SLAB_MEMORY_SIZE;
            u8* block_ptr = fl::bit_cast_ptr<u8>(static_cast<void*>(block));
            
            if (block_ptr >= slab_start && block_ptr < slab_end) {
                ++slab->allocated_count;
                break;
            }
        }
        
        return block;
    }

    void deallocateToSlab(void* ptr) {
        if (!ptr) {
            return;
        }
        
        // Find which slab this block belongs to and decrement its count
        for (Slab* slab = slabs_; slab; slab = slab->next) {
            u8* slab_start = slab->memory;
            u8* slab_end = slab_start + SLAB_MEMORY_SIZE;
            u8* block_ptr = fl::bit_cast_ptr<u8>(ptr);
            
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
    // Constructor
    SlabAllocator() : slabs_(nullptr), free_list_(nullptr), total_allocated_(0), total_deallocated_(0) {}
    
    // Destructor
    ~SlabAllocator() {
        cleanup();
    }
    
    // Non-copyable
    SlabAllocator(const SlabAllocator&) = delete;
    SlabAllocator& operator=(const SlabAllocator&) = delete;
    
    // Movable
    SlabAllocator(SlabAllocator&& other) noexcept 
        : slabs_(other.slabs_), free_list_(other.free_list_), 
          total_allocated_(other.total_allocated_), total_deallocated_(other.total_deallocated_) {
        other.slabs_ = nullptr;
        other.free_list_ = nullptr;
        other.total_allocated_ = 0;
        other.total_deallocated_ = 0;
    }
    
    SlabAllocator& operator=(SlabAllocator&& other) noexcept {
        if (this != &other) {
            cleanup();
            slabs_ = other.slabs_;
            free_list_ = other.free_list_;
            total_allocated_ = other.total_allocated_;
            total_deallocated_ = other.total_deallocated_;
            other.slabs_ = nullptr;
            other.free_list_ = nullptr;
            other.total_allocated_ = 0;
            other.total_deallocated_ = 0;
        }
        return *this;
    }

    T* allocate(fl::size n = 1) {
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

    void deallocate(T* ptr, fl::size n = 1) {
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
        free_list_ = nullptr;
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

    // Allocate memory for n objects of type T
    T* allocate(fl::size n) {
        // Use a static allocator instance per type/size combination
        static SlabAllocator<T, SLAB_SIZE> allocator;
        return allocator.allocate(n);
    }

    // Deallocate memory for n objects of type T
    void deallocate(T* p, fl::size n) {
        // Use the same static allocator instance
        static SlabAllocator<T, SLAB_SIZE> allocator;
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
            fl::memset(data, 0, sizeof(data));
        }
    };
    
    InlinedStorage m_inlined_storage;
    BaseAllocator m_base_allocator;
    fl::size m_inlined_used = 0;
    fl::bitset_fixed<N> m_inlined_free_bits;  // Track free slots in inlined storage
    T* m_heap_data = nullptr;
    fl::size m_heap_capacity = 0;
    fl::size m_heap_used = 0;

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
        m_inlined_free_bits = other.m_inlined_free_bits;
        
        // Copy heap data if any
        if (other.m_heap_used > 0) {
            m_heap_capacity = other.m_heap_capacity;
            m_heap_data = m_base_allocator.allocate(m_heap_capacity);
            m_heap_used = other.m_heap_used;
            for (fl::size i = 0; i < m_heap_used; ++i) {
                m_base_allocator.construct(&m_heap_data[i], other.m_heap_data[i]);
            }
        }
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
            m_inlined_free_bits = other.m_inlined_free_bits;
            
            // Copy heap data if any
            if (other.m_heap_used > 0) {
                m_heap_capacity = other.m_heap_capacity;
                m_heap_data = m_base_allocator.allocate(m_heap_capacity);
                m_heap_used = other.m_heap_used;
                for (fl::size i = 0; i < m_heap_used; ++i) {
                    m_base_allocator.construct(&m_heap_data[i], other.m_heap_data[i]);
                }
            }
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
        
        // First try to find n consecutive free slots in inlined storage
        if (n == 1) {
            // For single allocation, find first free bit
            for (fl::size i = 0; i < N; ++i) {
                if (m_inlined_free_bits.test(i)) {
                    m_inlined_free_bits.reset(i);
                    return &get_inlined_ptr()[i];
                }
            }
        }
        
        // If we can fit in remaining inlined storage
        if (m_inlined_used + n <= N) {
            T* ptr = &get_inlined_ptr()[m_inlined_used];
            m_inlined_used += n;
            return ptr;
        }
        
        // We need heap storage - initialize heap if this is the first time
        if (m_heap_data == nullptr) {
            fl::size initial_capacity = n * 2; // Start with some extra capacity
            m_heap_data = m_base_allocator.allocate(initial_capacity);
            m_heap_capacity = initial_capacity;
            m_heap_used = 0;
        }
        
        // If we need more heap storage
        if (m_heap_used + n > m_heap_capacity) {
            fl::size new_capacity = m_heap_capacity * 2;
            if (new_capacity < m_heap_used + n) {
                new_capacity = m_heap_used + n;
            }
            
            T* new_heap_data = m_base_allocator.allocate(new_capacity);
            
            // Move existing heap data
            for (fl::size i = 0; i < m_heap_used; ++i) {
                m_base_allocator.construct(&new_heap_data[i], fl::move(m_heap_data[i]));
                m_base_allocator.destroy(&m_heap_data[i]);
            }
            
            m_base_allocator.deallocate(m_heap_data, m_heap_capacity);
            m_heap_data = new_heap_data;
            m_heap_capacity = new_capacity;
        }
        
        T* ptr = &m_heap_data[m_heap_used];
        m_heap_used += n;
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
                    m_inlined_free_bits.set(slot_index + i);
                }
            }
            return;
        }
        
        // This is heap memory, deallocate through base allocator
        if (p >= m_heap_data && p < m_heap_data + m_heap_capacity) {
            // Mark as deallocated
            m_heap_used -= n;
            return;
        }
        
        // Fallback to base allocator for external allocations
        m_base_allocator.deallocate(p, n);
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
        m_inlined_free_bits.reset();
        
        // Destroy heap objects
        for (fl::size i = 0; i < m_heap_used; ++i) {
            m_base_allocator.destroy(&m_heap_data[i]);
        }
        
        if (m_heap_data) {
            m_base_allocator.deallocate(m_heap_data, m_heap_capacity);
            m_heap_data = nullptr;
        }
        m_heap_capacity = 0;
        m_heap_used = 0;
    }

    // Get total allocated size
    fl::size total_size() const {
        return m_inlined_used + m_heap_used;
    }

    // Get inlined capacity
    fl::size inlined_capacity() const {
        return N;
    }

    // Check if using inlined storage
    bool is_using_inlined() const {
        return m_heap_used == 0;
    }

private:
    T* get_inlined_ptr() {
        return reinterpret_cast<T*>(m_inlined_storage.data);
    }
    
    const T* get_inlined_ptr() const {
        return reinterpret_cast<const T*>(m_inlined_storage.data);
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
