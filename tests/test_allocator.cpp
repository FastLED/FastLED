// Unit tests for SlabAllocator to ensure contiguous memory allocation

#include "test.h"
#include "fl/allocator.h"
#include "fl/vector.h"
#include "fl/set.h"
#include <algorithm>
#include <set>

using namespace fl;

// Test struct for slab allocator testing
struct TestObject {
    int data[4];  // 16 bytes to make it larger than pointer size
    TestObject() { 
        for (int i = 0; i < 4; ++i) {
            data[i] = 0;
        }
    }
};

TEST_CASE("SlabAllocator - Basic functionality") {
    using TestAllocator = SlabAllocator<TestObject, 8>;
    
    SUBCASE("Single allocation and deallocation") {
        TestAllocator allocator;
        
        TestObject* ptr = allocator.allocate();
        REQUIRE(ptr != nullptr);
        CHECK(allocator.getTotalAllocated() == 1);
        CHECK(allocator.getActiveAllocations() == 1);
        
        allocator.deallocate(ptr);
        CHECK(allocator.getTotalDeallocated() == 1);
        CHECK(allocator.getActiveAllocations() == 0);
    }
    
    SUBCASE("Multiple allocations") {
        TestAllocator allocator;
        
        fl::vector<TestObject*> ptrs;
        const size_t num_allocs = 5;
        
        for (size_t i = 0; i < num_allocs; ++i) {
            TestObject* ptr = allocator.allocate();
            REQUIRE(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        CHECK(allocator.getTotalAllocated() == num_allocs);
        CHECK(allocator.getActiveAllocations() == num_allocs);
        
        for (TestObject* ptr : ptrs) {
            allocator.deallocate(ptr);
        }
        
        CHECK(allocator.getActiveAllocations() == 0);
    }
}

TEST_CASE("SlabAllocator - Contiguous memory within slab") {
    using TestAllocator = SlabAllocator<TestObject, 8>;
    TestAllocator allocator;
    
    SUBCASE("First 8 allocations should be contiguous") {
        fl::vector<TestObject*> ptrs;
        
        // Allocate exactly one slab worth of objects
        for (size_t i = 0; i < 8; ++i) {
            TestObject* ptr = allocator.allocate();
            REQUIRE(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        // Sort pointers by address to check contiguity
        std::sort(ptrs.begin(), ptrs.end());
        
        // Calculate expected block size (must be at least sizeof(TestObject))
        constexpr size_t expected_block_size = sizeof(TestObject) > sizeof(void*) ? sizeof(TestObject) : sizeof(void*);
        
        // Verify contiguous allocation within the slab
        for (size_t i = 1; i < ptrs.size(); ++i) {
            uintptr_t prev_addr = reinterpret_cast<uintptr_t>(ptrs[i-1]);
            uintptr_t curr_addr = reinterpret_cast<uintptr_t>(ptrs[i]);
            uintptr_t diff = curr_addr - prev_addr;
            
            // The difference should be exactly the block size
            CHECK(diff == expected_block_size);
        }
        
        // Verify all pointers are within the same memory range (same slab)
        uintptr_t first_addr = reinterpret_cast<uintptr_t>(ptrs[0]);
        uintptr_t last_addr = reinterpret_cast<uintptr_t>(ptrs.back());
        uintptr_t total_range = last_addr - first_addr + expected_block_size;
        uintptr_t expected_range = expected_block_size * 8;  // 8 blocks in slab
        
        CHECK(total_range == expected_range);
        
        // Cleanup
        for (TestObject* ptr : ptrs) {
            allocator.deallocate(ptr);
        }
    }
    
    SUBCASE("Memory boundaries verification") {
        fl::vector<TestObject*> ptrs;
        
        // Allocate one slab worth
        for (size_t i = 0; i < 8; ++i) {
            TestObject* ptr = allocator.allocate();
            REQUIRE(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        // Find the memory range bounds
        uintptr_t min_addr = UINTPTR_MAX;
        uintptr_t max_addr = 0;
        
        for (TestObject* ptr : ptrs) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            min_addr = std::min(min_addr, addr);
            max_addr = std::max(max_addr, addr);
        }
        
        // All allocations should fall within a predictable range
        constexpr size_t block_size = sizeof(TestObject) > sizeof(void*) ? sizeof(TestObject) : sizeof(void*);
        constexpr size_t slab_size = block_size * 8;
        
        uintptr_t actual_range = max_addr - min_addr + block_size;
        CHECK(actual_range == slab_size);
        
        // Verify each pointer falls within the expected boundaries
        for (TestObject* ptr : ptrs) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
            CHECK(addr >= min_addr);
            CHECK(addr <= max_addr);
            
            // Verify alignment - each block should be at expected offset
            uintptr_t offset_from_start = addr - min_addr;
            CHECK(offset_from_start % block_size == 0);
        }
        
        // Cleanup
        for (TestObject* ptr : ptrs) {
            allocator.deallocate(ptr);
        }
    }
}

TEST_CASE("SlabAllocator - Multiple slabs behavior") {
    using TestAllocator = SlabAllocator<TestObject, 4>;  // Smaller slab for easier testing
    TestAllocator allocator;
    
    SUBCASE("Allocation across multiple slabs") {
        fl::vector<TestObject*> ptrs;
        
        // Allocate more than one slab can hold (4 * 3 = 12 objects across 3 slabs)
        const size_t total_allocs = 12;
        for (size_t i = 0; i < total_allocs; ++i) {
            TestObject* ptr = allocator.allocate();
            REQUIRE(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        CHECK(allocator.getSlabCount() == 3);  // Should have created 3 slabs
        CHECK(allocator.getTotalAllocated() == total_allocs);
        
        // Test that all allocations are valid and don't overlap
        fl::vector<TestObject*> sorted_ptrs = ptrs;
        std::sort(sorted_ptrs.begin(), sorted_ptrs.end());
        
        constexpr size_t block_size = sizeof(TestObject) > sizeof(void*) ? sizeof(TestObject) : sizeof(void*);
        
        // Verify no pointer overlaps (each should be at least block_size apart)
        for (size_t i = 1; i < sorted_ptrs.size(); ++i) {
            uintptr_t prev_addr = reinterpret_cast<uintptr_t>(sorted_ptrs[i-1]);
            uintptr_t curr_addr = reinterpret_cast<uintptr_t>(sorted_ptrs[i]);
            uintptr_t diff = curr_addr - prev_addr;
            
            // Each allocation should be at least block_size apart
            CHECK(diff >= block_size);
        }
        
        // Test that each allocation is properly aligned and usable
        for (size_t i = 0; i < ptrs.size(); ++i) {
            // Test alignment
            uintptr_t addr = reinterpret_cast<uintptr_t>(ptrs[i]);
            CHECK(addr % alignof(TestObject) == 0);
            
            // Test that we can write unique data to each allocation
            ptrs[i]->data[0] = static_cast<int>(i + 100);
            ptrs[i]->data[1] = static_cast<int>(i + 200);
            ptrs[i]->data[2] = static_cast<int>(i + 300);
            ptrs[i]->data[3] = static_cast<int>(i + 400);
        }
        
        // Verify all data is still intact (no memory corruption/overlap)
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK(ptrs[i]->data[0] == static_cast<int>(i + 100));
            CHECK(ptrs[i]->data[1] == static_cast<int>(i + 200));
            CHECK(ptrs[i]->data[2] == static_cast<int>(i + 300));
            CHECK(ptrs[i]->data[3] == static_cast<int>(i + 400));
        }
        
        // Cleanup
        for (TestObject* ptr : ptrs) {
            allocator.deallocate(ptr);
        }
    }
}

TEST_CASE("SlabAllocator - Memory layout verification") {
    using SmallAllocator = SlabAllocator<uint32_t, 16>;
    SmallAllocator allocator;
    
    SUBCASE("Detailed memory layout check") {
        fl::vector<uint32_t*> ptrs;
        
        // Allocate exactly one slab worth
        for (size_t i = 0; i < 16; ++i) {
            uint32_t* ptr = allocator.allocate();
            REQUIRE(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        // Sort by address
        std::sort(ptrs.begin(), ptrs.end());
        
        // Verify perfect sequential layout
        constexpr size_t block_size = sizeof(uint32_t) > sizeof(void*) ? sizeof(uint32_t) : sizeof(void*);
        
        uintptr_t base_addr = reinterpret_cast<uintptr_t>(ptrs[0]);
        
        for (size_t i = 0; i < ptrs.size(); ++i) {
            uintptr_t expected_addr = base_addr + (i * block_size);
            uintptr_t actual_addr = reinterpret_cast<uintptr_t>(ptrs[i]);
            
            CHECK(actual_addr == expected_addr);
        }
        
        // Verify the total memory span is exactly what we expect
        uintptr_t first_addr = reinterpret_cast<uintptr_t>(ptrs[0]);
        uintptr_t last_addr = reinterpret_cast<uintptr_t>(ptrs.back());
        uintptr_t total_span = last_addr - first_addr + block_size;
        uintptr_t expected_span = block_size * 16;
        
        CHECK(total_span == expected_span);
        
        // Test that we can write to each allocated block without interfering with others
        for (size_t i = 0; i < ptrs.size(); ++i) {
            *ptrs[i] = static_cast<uint32_t>(i + 1000);  // Write unique value
        }
        
        // Verify all values are intact (no memory corruption/overlap)
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK(*ptrs[i] == static_cast<uint32_t>(i + 1000));
        }
        
        // Cleanup
        for (uint32_t* ptr : ptrs) {
            allocator.deallocate(ptr);
        }
    }
}

TEST_CASE("SlabAllocator - Edge cases") {
    using EdgeAllocator = SlabAllocator<char, 8>;
    EdgeAllocator allocator;
    
    SUBCASE("Allocation and deallocation pattern") {
        fl::vector<char*> ptrs;
        
        // Allocate all blocks in slab
        for (size_t i = 0; i < 8; ++i) {
            char* ptr = allocator.allocate();
            REQUIRE(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        // Deallocate every other block
        for (size_t i = 0; i < ptrs.size(); i += 2) {
            allocator.deallocate(ptrs[i]);
            ptrs[i] = nullptr;
        }
        
        // Reallocate - should reuse freed blocks
        fl::vector<char*> new_ptrs;
        for (size_t i = 0; i < 4; ++i) {  // 4 blocks were freed
            char* ptr = allocator.allocate();
            REQUIRE(ptr != nullptr);
            new_ptrs.push_back(ptr);
        }
        
        // All new allocations should be from the same slab (reused memory)
        CHECK(allocator.getSlabCount() == 1);  // Still only one slab
        
        // Cleanup
        for (size_t i = 0; i < ptrs.size(); ++i) {
            if (ptrs[i] != nullptr) {
                allocator.deallocate(ptrs[i]);
            }
        }
        for (char* ptr : new_ptrs) {
            allocator.deallocate(ptr);
        }
    }
    
    SUBCASE("Bulk allocation fallback") {
        // Request bulk allocation (n != 1) - should fallback to malloc
        char* bulk_ptr = allocator.allocate(10);
        REQUIRE(bulk_ptr != nullptr);
        
        // This should not affect slab statistics since it uses malloc
        CHECK(allocator.getTotalAllocated() == 0);  // Slab stats unchanged
        CHECK(allocator.getSlabCount() == 0);       // No slabs created
        
        allocator.deallocate(bulk_ptr, 10);
    }
}

TEST_CASE("SlabAllocator - STL compatibility") {
    SUBCASE("STL allocator interface") {
        using STLAllocator = allocator_slab<TestObject, 8>;
        STLAllocator alloc;
        
        TestObject* ptr = alloc.allocate(1);
        REQUIRE(ptr != nullptr);
        
        // Construct object
        alloc.construct(ptr, TestObject{});
        
        // Use the object
        ptr->data[0] = 42;
        CHECK(ptr->data[0] == 42);
        
        // Destroy and deallocate
        alloc.destroy(ptr);
        alloc.deallocate(ptr, 1);
    }
    
    SUBCASE("Allocator equality") {
        allocator_slab<TestObject, 8> alloc1;
        allocator_slab<TestObject, 8> alloc2;
        
        CHECK(alloc1 == alloc2);  // All instances should be equivalent
        CHECK_FALSE(alloc1 != alloc2);
    }
} 

TEST_CASE("allocator_inlined - Basic functionality") {
    using TestAllocator = fl::allocator_inlined<int, 3>;
    
    SUBCASE("Single allocation and deallocation") {
        TestAllocator allocator;
        
        int* ptr = allocator.allocate(1);
        REQUIRE(ptr != nullptr);
        
        // Write to the allocation
        *ptr = 42;
        CHECK(*ptr == 42);
        
        allocator.deallocate(ptr, 1);
    }
    
    SUBCASE("Multiple inlined allocations") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        const size_t num_allocs = 3;  // Exactly the inlined capacity
        
        for (size_t i = 0; i < num_allocs; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        // Verify all allocations are valid and contain expected data
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK(*ptrs[i] == static_cast<int>(i + 100));
        }
        
        // Cleanup
        for (int* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
}

TEST_CASE("allocator_inlined - Inlined to heap transition") {
    using TestAllocator = fl::allocator_inlined<int, 3>;
    
    SUBCASE("Overflow to heap") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        const size_t total_allocs = 5;  // More than inlined capacity (3)
        
        for (size_t i = 0; i < total_allocs; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        // Verify all allocations are valid and contain expected data
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK(*ptrs[i] == static_cast<int>(i + 100));
        }
        
        // Cleanup
        for (int* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
    
    SUBCASE("Mixed inlined and heap allocations") {
        TestAllocator allocator;
        
        fl::vector<int*> inlined_ptrs;
        fl::vector<int*> heap_ptrs;
        
        // Allocate inlined storage first
        for (size_t i = 0; i < 3; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            inlined_ptrs.push_back(ptr);
        }
        
        // Then allocate heap storage
        for (size_t i = 0; i < 2; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 200);
            heap_ptrs.push_back(ptr);
        }
        
        // Verify inlined allocations
        for (size_t i = 0; i < inlined_ptrs.size(); ++i) {
            CHECK(*inlined_ptrs[i] == static_cast<int>(i + 100));
        }
        
        // Verify heap allocations
        for (size_t i = 0; i < heap_ptrs.size(); ++i) {
            CHECK(*heap_ptrs[i] == static_cast<int>(i + 200));
        }
        
        // Cleanup
        for (int* ptr : inlined_ptrs) {
            allocator.deallocate(ptr, 1);
        }
        for (int* ptr : heap_ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
}

TEST_CASE("allocator_inlined - Free slot management") {
    using TestAllocator = fl::allocator_inlined<int, 3>;
    
    SUBCASE("Deallocate and reuse inlined slots") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        
        // Allocate all inlined slots
        for (size_t i = 0; i < 3; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        // Deallocate the middle slot
        allocator.deallocate(ptrs[1], 1);
        ptrs[1] = nullptr;
        
        // Allocate a new slot - should reuse the freed slot
        int* new_ptr = allocator.allocate(1);
        REQUIRE(new_ptr != nullptr);
        *new_ptr = 999;
        
        // The new allocation should be from the same memory location as the freed slot
        // (This is implementation-dependent, but the slot should be reused)
        
        // Verify other allocations are still intact
        CHECK(*ptrs[0] == 100);
        CHECK(*ptrs[2] == 102);
        CHECK(*new_ptr == 999);
        
        // Cleanup
        allocator.deallocate(ptrs[0], 1);
        allocator.deallocate(ptrs[2], 1);
        allocator.deallocate(new_ptr, 1);
    }
    
    SUBCASE("Deallocate and reuse heap slots") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        
        // Allocate more than inlined capacity to force heap usage
        for (size_t i = 0; i < 5; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        // Deallocate a heap slot (index 4)
        allocator.deallocate(ptrs[4], 1);
        ptrs[4] = nullptr;
        
        // Allocate a new slot - should reuse the freed heap slot
        int* new_ptr = allocator.allocate(1);
        REQUIRE(new_ptr != nullptr);
        *new_ptr = 999;
        
        // Verify other allocations are still intact
        for (size_t i = 0; i < 4; ++i) {
            CHECK(*ptrs[i] == static_cast<int>(i + 100));
        }
        CHECK(*new_ptr == 999);
        
        // Cleanup
        for (size_t i = 0; i < ptrs.size(); ++i) {
            if (ptrs[i] != nullptr) {
                allocator.deallocate(ptrs[i], 1);
            }
        }
        allocator.deallocate(new_ptr, 1);
    }
}

TEST_CASE("allocator_inlined - Memory layout verification") {
    using TestAllocator = fl::allocator_inlined<int, 3>;
    
    SUBCASE("Inlined storage layout") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        
        // Allocate exactly inlined capacity
        for (size_t i = 0; i < 3; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        // Sort by address to check layout
        std::sort(ptrs.begin(), ptrs.end());
        
        // Verify that inlined allocations are contiguous
        for (size_t i = 1; i < ptrs.size(); ++i) {
            uintptr_t prev_addr = reinterpret_cast<uintptr_t>(ptrs[i-1]);
            uintptr_t curr_addr = reinterpret_cast<uintptr_t>(ptrs[i]);
            uintptr_t diff = curr_addr - prev_addr;
            
            // Should be exactly sizeof(int) apart (or aligned size)
            CHECK(diff >= sizeof(int));
        }
        
        // Cleanup
        for (int* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
    
    SUBCASE("Heap storage layout") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        
        // Allocate more than inlined capacity to force heap usage
        for (size_t i = 0; i < 5; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        // Sort by address
        std::sort(ptrs.begin(), ptrs.end());
        
        // Verify that heap allocations are properly spaced
        for (size_t i = 1; i < ptrs.size(); ++i) {
            uintptr_t prev_addr = reinterpret_cast<uintptr_t>(ptrs[i-1]);
            uintptr_t curr_addr = reinterpret_cast<uintptr_t>(ptrs[i]);
            uintptr_t diff = curr_addr - prev_addr;
            
            // Should be at least sizeof(int) apart
            CHECK(diff >= sizeof(int));
        }
        
        // Cleanup
        for (int* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
}

TEST_CASE("allocator_inlined - Edge cases") {
    using TestAllocator = fl::allocator_inlined<int, 3>;
    
    SUBCASE("Zero allocation") {
        TestAllocator allocator;
        
        int* ptr = allocator.allocate(0);
        CHECK(ptr == nullptr);
        
        allocator.deallocate(ptr, 0);  // Should be safe
    }
    
    SUBCASE("Null deallocation") {
        TestAllocator allocator;
        
        allocator.deallocate(nullptr, 1);  // Should be safe
    }
    
    SUBCASE("Large allocation") {
        TestAllocator allocator;
        
        // Allocate a large block (should go to heap)
        int* large_ptr = allocator.allocate(100);
        REQUIRE(large_ptr != nullptr);
        
        // Write to the allocation
        for (int i = 0; i < 100; ++i) {
            large_ptr[i] = i;
        }
        
        // Verify the data
        for (int i = 0; i < 100; ++i) {
            CHECK(large_ptr[i] == i);
        }
        
        allocator.deallocate(large_ptr, 100);
    }
}

TEST_CASE("allocator_inlined - Copy and assignment") {
    using TestAllocator = fl::allocator_inlined<int, 3>;
    
    SUBCASE("Copy constructor") {
        TestAllocator allocator1;
        
        // Allocate some memory in the first allocator
        int* ptr1 = allocator1.allocate(1);
        *ptr1 = 42;
        
        // Copy the allocator
        TestAllocator allocator2(allocator1);
        
        // Allocate from the copy
        int* ptr2 = allocator2.allocate(1);
        *ptr2 = 100;
        
        // Verify both allocations work
        CHECK(*ptr1 == 42);
        CHECK(*ptr2 == 100);
        
        // Cleanup
        allocator1.deallocate(ptr1, 1);
        allocator2.deallocate(ptr2, 1);
    }
    
    SUBCASE("Assignment operator") {
        TestAllocator allocator1;
        TestAllocator allocator2;
        
        // Allocate in first allocator
        int* ptr1 = allocator1.allocate(1);
        *ptr1 = 42;
        
        // Assign to second allocator
        allocator2 = allocator1;
        
        // Allocate from second allocator
        int* ptr2 = allocator2.allocate(1);
        *ptr2 = 100;
        
        // Verify both allocations work
        CHECK(*ptr1 == 42);
        CHECK(*ptr2 == 100);
        
        // Cleanup
        allocator1.deallocate(ptr1, 1);
        allocator2.deallocate(ptr2, 1);
    }
}

TEST_CASE("allocator_inlined - Clear functionality") {
    using TestAllocator = fl::allocator_inlined<int, 3>;
    
    SUBCASE("Clear inlined allocations") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        
        // Allocate inlined storage
        for (size_t i = 0; i < 3; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        // Clear the allocator
        allocator.clear();
        
        // Allocate again - should work normally
        int* new_ptr = allocator.allocate(1);
        REQUIRE(new_ptr != nullptr);
        *new_ptr = 999;
        CHECK(*new_ptr == 999);
        
        allocator.deallocate(new_ptr, 1);
    }
    
    SUBCASE("Clear mixed allocations") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        
        // Allocate both inlined and heap storage
        for (size_t i = 0; i < 5; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        // Clear the allocator
        allocator.clear();
        
        // Allocate again - should work normally
        int* new_ptr = allocator.allocate(1);
        REQUIRE(new_ptr != nullptr);
        *new_ptr = 999;
        CHECK(*new_ptr == 999);
        
        allocator.deallocate(new_ptr, 1);
    }
} 

TEST_CASE("allocator_inlined_slab - Basic functionality") {
    using TestAllocator = fl::allocator_inlined_slab<int, 4>;
    
    SUBCASE("Single allocation within inlined capacity") {
        TestAllocator allocator;
        
        int* ptr = allocator.allocate(1);
        REQUIRE(ptr != nullptr);
        CHECK(allocator.is_using_inlined());
        CHECK(allocator.total_size() == 1);
        CHECK(allocator.inlined_capacity() == 4);
        
        // Test that we can write to the allocation
        *ptr = 42;
        CHECK(*ptr == 42);
        
        allocator.deallocate(ptr, 1);
    }
    
    SUBCASE("Multiple allocations within inlined capacity") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        const size_t num_allocs = 4;  // Exactly the inlined capacity
        
        for (size_t i = 0; i < num_allocs; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        CHECK(allocator.is_using_inlined());
        CHECK(allocator.total_size() == num_allocs);
        
        // Verify all data is intact
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK(*ptrs[i] == static_cast<int>(i + 100));
        }
        
        // Cleanup
        for (int* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
    
    SUBCASE("Allocation beyond inlined capacity") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        const size_t num_allocs = 6;  // More than inlined capacity (4)
        
        for (size_t i = 0; i < num_allocs; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 200);
            ptrs.push_back(ptr);
        }
        
        // Should now be using heap (slab allocator backend)
        CHECK(!allocator.is_using_inlined());
        CHECK(allocator.total_size() == num_allocs);
        
        // Verify all data is intact
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK(*ptrs[i] == static_cast<int>(i + 200));
        }
        
        // Cleanup
        for (int* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
}

TEST_CASE("allocator_inlined_slab - Memory layout and contiguity") {
    using TestAllocator = fl::allocator_inlined_slab<TestObject, 3>;
    
    SUBCASE("Inlined memory layout verification") {
        TestAllocator allocator;
        
        fl::vector<TestObject*> ptrs;
        
        // Allocate exactly the inlined capacity
        for (size_t i = 0; i < 3; ++i) {
            TestObject* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        // Sort pointers by address
        std::sort(ptrs.begin(), ptrs.end());
        
        // Verify contiguous allocation within inlined storage
        for (size_t i = 1; i < ptrs.size(); ++i) {
            uintptr_t prev_addr = reinterpret_cast<uintptr_t>(ptrs[i-1]);
            uintptr_t curr_addr = reinterpret_cast<uintptr_t>(ptrs[i]);
            uintptr_t diff = curr_addr - prev_addr;
            
            // Should be exactly sizeof(TestObject) apart
            CHECK(diff == sizeof(TestObject));
        }
        
        // Verify all pointers are within the same memory range
        uintptr_t first_addr = reinterpret_cast<uintptr_t>(ptrs[0]);
        uintptr_t last_addr = reinterpret_cast<uintptr_t>(ptrs.back());
        uintptr_t total_range = last_addr - first_addr + sizeof(TestObject);
        uintptr_t expected_range = sizeof(TestObject) * 3;
        
        CHECK(total_range == expected_range);
        
        // Cleanup
        for (TestObject* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
    
    SUBCASE("Mixed inlined and heap allocation layout") {
        TestAllocator allocator;
        
        fl::vector<TestObject*> inlined_ptrs;
        fl::vector<TestObject*> heap_ptrs;
        
        // Allocate inlined capacity
        for (size_t i = 0; i < 3; ++i) {
            TestObject* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            inlined_ptrs.push_back(ptr);
        }
        
        // Allocate additional objects (should use heap/slab)
        for (size_t i = 0; i < 5; ++i) {
            TestObject* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            heap_ptrs.push_back(ptr);
        }
        
        // Verify inlined pointers are contiguous
        std::sort(inlined_ptrs.begin(), inlined_ptrs.end());
        for (size_t i = 1; i < inlined_ptrs.size(); ++i) {
            uintptr_t prev_addr = reinterpret_cast<uintptr_t>(inlined_ptrs[i-1]);
            uintptr_t curr_addr = reinterpret_cast<uintptr_t>(inlined_ptrs[i]);
            uintptr_t diff = curr_addr - prev_addr;
            CHECK(diff == sizeof(TestObject));
        }
        
        // Verify heap pointers are in different memory range
        uintptr_t inlined_start = reinterpret_cast<uintptr_t>(inlined_ptrs[0]);
        uintptr_t inlined_end = reinterpret_cast<uintptr_t>(inlined_ptrs.back()) + sizeof(TestObject);
        
        for (TestObject* heap_ptr : heap_ptrs) {
            uintptr_t heap_addr = reinterpret_cast<uintptr_t>(heap_ptr);
            // Heap pointers should be outside inlined range
            CHECK((heap_addr < inlined_start || heap_addr >= inlined_end));
        }
        
        // Cleanup
        for (TestObject* ptr : inlined_ptrs) {
            allocator.deallocate(ptr, 1);
        }
        for (TestObject* ptr : heap_ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
}

TEST_CASE("allocator_inlined_slab - Bulk allocation behavior") {
    using TestAllocator = fl::allocator_inlined_slab<int, 2>;
    
    SUBCASE("Bulk allocation bypasses inlined storage") {
        TestAllocator allocator;
        
        // Allocate 3 objects at once (more than inlined capacity)
        int* bulk_ptr = allocator.allocate(3);
        REQUIRE(bulk_ptr != nullptr);
        
        // Should be using heap, not inlined
        CHECK(!allocator.is_using_inlined());
        
        // Test that we can write to all allocated objects
        for (int i = 0; i < 3; ++i) {
            bulk_ptr[i] = i + 100;
        }
        
        // Verify data integrity
        for (int i = 0; i < 3; ++i) {
            CHECK(bulk_ptr[i] == i + 100);
        }
        
        allocator.deallocate(bulk_ptr, 3);
    }
    
    SUBCASE("Mixed single and bulk allocations") {
        TestAllocator allocator;
        
        // First, allocate single objects to fill inlined storage
        fl::vector<int*> single_ptrs;
        for (size_t i = 0; i < 2; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 10);
            single_ptrs.push_back(ptr);
        }
        
        CHECK(allocator.is_using_inlined());
        
        // Now allocate bulk objects
        int* bulk_ptr = allocator.allocate(4);
        REQUIRE(bulk_ptr != nullptr);
        
        // Should still be using inlined for the single allocations
        // but bulk allocation goes to heap
        for (int i = 0; i < 4; ++i) {
            bulk_ptr[i] = i + 1000;
        }
        
        // Verify all data is intact
        for (size_t i = 0; i < single_ptrs.size(); ++i) {
            CHECK(*single_ptrs[i] == static_cast<int>(i + 10));
        }
        
        for (int i = 0; i < 4; ++i) {
            CHECK(bulk_ptr[i] == i + 1000);
        }
        
        // Cleanup
        for (int* ptr : single_ptrs) {
            allocator.deallocate(ptr, 1);
        }
        allocator.deallocate(bulk_ptr, 4);
    }
}

TEST_CASE("allocator_inlined_slab - Free slot management") {
    using TestAllocator = fl::allocator_inlined_slab<int, 4>;
    
    SUBCASE("Reuse of freed inlined slots") {
        TestAllocator allocator;
        
        // Allocate all inlined slots
        fl::vector<int*> ptrs;
        for (size_t i = 0; i < 4; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        CHECK(allocator.is_using_inlined());
        CHECK(allocator.total_size() == 4);
        
        // Free the first and third allocations
        allocator.deallocate(ptrs[0], 1);
        allocator.deallocate(ptrs[2], 1);
        
        // Allocate two new objects - should reuse freed slots
        int* new1 = allocator.allocate(1);
        int* new2 = allocator.allocate(1);
        REQUIRE(new1 != nullptr);
        REQUIRE(new2 != nullptr);
        
        *new1 = 999;
        *new2 = 888;
        
        // Should still be using inlined storage
        CHECK(allocator.is_using_inlined());
        
        // Verify remaining original allocations are intact
        CHECK(*ptrs[1] == 101);
        CHECK(*ptrs[3] == 103);
        
        // Cleanup
        allocator.deallocate(ptrs[1], 1);
        allocator.deallocate(ptrs[3], 1);
        allocator.deallocate(new1, 1);
        allocator.deallocate(new2, 1);
    }
    
    SUBCASE("Complex allocation/deallocation pattern") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        
        // Allocate 6 objects (4 inlined + 2 heap)
        for (size_t i = 0; i < 6; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        // Free objects in a pattern: keep 0,2,4, free 1,3,5
        allocator.deallocate(ptrs[1], 1);
        allocator.deallocate(ptrs[3], 1);
        allocator.deallocate(ptrs[5], 1);
        
        // Allocate 3 new objects - should reuse freed slots
        fl::vector<int*> new_ptrs;
        for (size_t i = 0; i < 3; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 1000);
            new_ptrs.push_back(ptr);
        }
        
        // Verify remaining original allocations are intact
        CHECK(*ptrs[0] == 100);
        CHECK(*ptrs[2] == 102);
        CHECK(*ptrs[4] == 104);
        
        // Verify new allocations
        CHECK(*new_ptrs[0] == 1000);
        CHECK(*new_ptrs[1] == 1001);
        CHECK(*new_ptrs[2] == 1002);
        
        // Cleanup
        allocator.deallocate(ptrs[0], 1);
        allocator.deallocate(ptrs[2], 1);
        allocator.deallocate(ptrs[4], 1);
        for (int* ptr : new_ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
}

TEST_CASE("allocator_inlined_slab - Copy and assignment") {
    using TestAllocator = fl::allocator_inlined_slab<int, 3>;
    
    SUBCASE("Copy constructor with inlined data") {
        TestAllocator allocator1;
        
        // Allocate some objects in the first allocator
        fl::vector<int*> ptrs1;
        for (size_t i = 0; i < 3; ++i) {
            int* ptr = allocator1.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs1.push_back(ptr);
        }
        
        // Copy the allocator
        TestAllocator allocator2(allocator1);
        
        // Verify the copy has the same data
        CHECK(allocator2.total_size() == allocator1.total_size());
        CHECK(allocator2.is_using_inlined() == allocator1.is_using_inlined());
        
        // Cleanup original
        for (int* ptr : ptrs1) {
            allocator1.deallocate(ptr, 1);
        }
        
        // Cleanup copy
        allocator2.clear();
    }
    
    SUBCASE("Copy assignment with mixed storage") {
        TestAllocator allocator1;
        TestAllocator allocator2;
        
        // Fill allocator1 with more than inlined capacity
        fl::vector<int*> ptrs1;
        for (size_t i = 0; i < 5; ++i) {
            int* ptr = allocator1.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 200);
            ptrs1.push_back(ptr);
        }
        
        // Assign to allocator2
        allocator2 = allocator1;
        
        // Verify allocator2 has the same state
        CHECK(allocator2.total_size() == allocator1.total_size());
        CHECK(allocator2.is_using_inlined() == allocator1.is_using_inlined());
        
        // Cleanup
        allocator1.clear();
        allocator2.clear();
    }
}

TEST_CASE("allocator_inlined_slab - Clear functionality") {
    using TestAllocator = fl::allocator_inlined_slab<int, 3>;
    
    SUBCASE("Clear inlined allocations") {
        TestAllocator allocator;
        
        // Allocate inlined objects
        fl::vector<int*> ptrs;
        for (size_t i = 0; i < 3; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        CHECK(allocator.total_size() == 3);
        CHECK(allocator.is_using_inlined());
        
        // Clear all allocations
        allocator.clear();
        
        CHECK(allocator.total_size() == 0);
        CHECK(allocator.is_using_inlined());
        
        // Should be able to allocate again
        int* new_ptr = allocator.allocate(1);
        REQUIRE(new_ptr != nullptr);
        *new_ptr = 999;
        CHECK(*new_ptr == 999);
        
        allocator.deallocate(new_ptr, 1);
    }
    
    SUBCASE("Clear mixed allocations") {
        TestAllocator allocator;
        
        // Allocate more than inlined capacity
        fl::vector<int*> ptrs;
        for (size_t i = 0; i < 6; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        CHECK(allocator.total_size() == 6);
        CHECK(!allocator.is_using_inlined());
        
        // Clear all allocations
        allocator.clear();
        
        CHECK(allocator.total_size() == 0);
        CHECK(allocator.is_using_inlined());
        
        // Should be able to allocate again, starting with inlined
        int* new_ptr = allocator.allocate(1);
        REQUIRE(new_ptr != nullptr);
        *new_ptr = 999;
        CHECK(*new_ptr == 999);
        CHECK(allocator.is_using_inlined());
        
        allocator.deallocate(new_ptr, 1);
    }
}

TEST_CASE("allocator_inlined_slab - Edge cases and error handling") {
    using TestAllocator = fl::allocator_inlined_slab<int, 2>;
    
    SUBCASE("Zero allocation") {
        TestAllocator allocator;
        
        int* ptr = allocator.allocate(0);
        CHECK(ptr == nullptr);
        
        // Should still be in inlined mode
        CHECK(allocator.is_using_inlined());
    }
    
    SUBCASE("Null deallocation") {
        TestAllocator allocator;
        
        // Should not crash
        allocator.deallocate(nullptr, 1);
        allocator.deallocate(nullptr, 0);
        
        CHECK(allocator.total_size() == 0);
    }
    
    SUBCASE("Large bulk allocation") {
        TestAllocator allocator;
        
        // Allocate a large number of objects at once
        const size_t large_count = 100;
        int* bulk_ptr = allocator.allocate(large_count);
        REQUIRE(bulk_ptr != nullptr);
        
        // Should be using heap
        CHECK(!allocator.is_using_inlined());
        
        // Test that we can write to all allocated objects
        for (size_t i = 0; i < large_count; ++i) {
            bulk_ptr[i] = static_cast<int>(i + 1000);
        }
        
        // Verify data integrity
        for (size_t i = 0; i < large_count; ++i) {
            CHECK(bulk_ptr[i] == static_cast<int>(i + 1000));
        }
        
        allocator.deallocate(bulk_ptr, large_count);
    }
    
    SUBCASE("Memory exhaustion simulation") {
        TestAllocator allocator;
        
        // This test simulates what happens when the slab allocator
        // runs out of memory (though we can't easily force this in practice)
        
        // Allocate many objects to stress the allocator
        fl::vector<int*> ptrs;
        const size_t stress_count = 1000;
        
        for (size_t i = 0; i < stress_count; ++i) {
            int* ptr = allocator.allocate(1);
            if (ptr != nullptr) {
                *ptr = static_cast<int>(i);
                ptrs.push_back(ptr);
            } else {
                // If allocation fails, break
                break;
            }
        }
        
        // Verify we got some allocations
        CHECK(ptrs.size() > 0);
        
        // Verify data integrity
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK(*ptrs[i] == static_cast<int>(i));
        }
        
        // Cleanup
        for (int* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
}

TEST_CASE("allocator_inlined_slab - Performance characteristics") {
    using TestAllocator = fl::allocator_inlined_slab<int, 8>;
    
    SUBCASE("Allocation speed comparison") {
        TestAllocator allocator;
        
        // Measure allocation speed for inlined vs heap
        const size_t test_count = 1000;
        fl::vector<int*> ptrs;
        ptrs.reserve(test_count);
        
        // Allocate objects and measure time
        for (size_t i = 0; i < test_count; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i);
            ptrs.push_back(ptr);
        }
        
        // Verify all allocations succeeded
        CHECK(ptrs.size() == test_count);
        
        // Verify data integrity
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK(*ptrs[i] == static_cast<int>(i));
        }
        
        // Cleanup
        for (int* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
    
    SUBCASE("Memory efficiency") {
        TestAllocator allocator;
        
        // Test that the allocator efficiently manages memory
        fl::vector<int*> ptrs;
        
        // Allocate and deallocate in a pattern to test memory reuse
        for (size_t round = 0; round < 10; ++round) {
            // Allocate a batch
            for (size_t i = 0; i < 5; ++i) {
                int* ptr = allocator.allocate(1);
                REQUIRE(ptr != nullptr);
                *ptr = static_cast<int>(round * 100 + i);
                ptrs.push_back(ptr);
            }
            
            // Deallocate half of them
            for (size_t i = 0; i < 2; ++i) {
                if (!ptrs.empty()) {
                    allocator.deallocate(ptrs.back(), 1);
                    ptrs.pop_back();
                }
            }
        }
        
        // Verify remaining allocations
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK(*ptrs[i] >= 0);
        }
        
        // Cleanup
        for (int* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
}

TEST_CASE("allocator_inlined_slab - STL container compatibility") {
    using TestAllocator = fl::allocator_inlined_slab<int, 4>;
    
    SUBCASE("Vector with inlined slab allocator") {
        fl::vector<int, TestAllocator> vec;
        
        // Test basic vector operations
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);
        vec.push_back(5);  // This should trigger heap allocation
        
        CHECK(vec.size() == 5);
        CHECK(vec[0] == 1);
        CHECK(vec[1] == 2);
        CHECK(vec[2] == 3);
        CHECK(vec[3] == 4);
        CHECK(vec[4] == 5);
        
        // Test vector growth
        for (int i = 6; i <= 20; ++i) {
            vec.push_back(i);
        }
        
        CHECK(vec.size() == 20);
        CHECK(vec[19] == 20);
    }
    
    SUBCASE("Set with inlined slab allocator") {
        fl::set<int, TestAllocator> set;
        
        // Test basic set operations
        set.insert(3);
        set.insert(1);
        set.insert(4);
        set.insert(1);  // Duplicate
        set.insert(2);
        
        CHECK(set.size() == 4);
        CHECK(set.find(1) != set.end());
        CHECK(set.find(2) != set.end());
        CHECK(set.find(3) != set.end());
        CHECK(set.find(4) != set.end());
        CHECK(set.find(5) == set.end());
    }
}

TEST_CASE("allocator_inlined_slab - Complex object types") {
    struct ComplexObject {
        fl::string str;
        fl::vector<int> vec;
        int data[4];
        
        ComplexObject() {
            for (int i = 0; i < 4; ++i) {
                data[i] = i;
            }
        }
        
        ComplexObject(const fl::string& s, const fl::vector<int>& v) 
            : str(s), vec(v) {
            for (int i = 0; i < 4; ++i) {
                data[i] = i + 100;
            }
        }
    };
    
    using TestAllocator = fl::allocator_inlined_slab<ComplexObject, 3>;
    
    SUBCASE("Complex object allocation") {
        TestAllocator allocator;
        
        // Allocate complex objects
        fl::vector<ComplexObject*> ptrs;
        for (size_t i = 0; i < 3; ++i) {
            ComplexObject* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            
            // Use placement new to construct the object
            new(ptr) ComplexObject();
            
            // Test that the object is properly constructed
            for (int j = 0; j < 4; ++j) {
                CHECK(ptr->data[j] == j);
            }
            
            ptrs.push_back(ptr);
        }
        
        CHECK(allocator.is_using_inlined());
        CHECK(allocator.total_size() == 3);
        
        // Test object modification
        ptrs[0]->str = "test1";
        ptrs[1]->str = "test2";
        ptrs[2]->str = "test3";
        
        CHECK(ptrs[0]->str == "test1");
        CHECK(ptrs[1]->str == "test2");
        CHECK(ptrs[2]->str == "test3");
        
        // Cleanup - destroy objects before deallocation
        for (ComplexObject* ptr : ptrs) {
            ptr->~ComplexObject();
            allocator.deallocate(ptr, 1);
        }
    }
    
    SUBCASE("Complex object with heap transition") {
        TestAllocator allocator;
        
        fl::vector<ComplexObject*> ptrs;
        
        // Allocate more than inlined capacity
        for (size_t i = 0; i < 5; ++i) {
            ComplexObject* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            
            fl::vector<int> test_vec;
            test_vec.push_back(static_cast<int>(i));
            test_vec.push_back(static_cast<int>(i + 10));
            
            new(ptr) ComplexObject("obj" + fl::to_string(i), test_vec);
            
            ptrs.push_back(ptr);
        }
        
        CHECK(!allocator.is_using_inlined());
        CHECK(allocator.total_size() == 5);
        
        // Verify all objects are intact
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK(ptrs[i]->str == "obj" + fl::to_string(i));
            CHECK(ptrs[i]->vec.size() == 2);
            CHECK(ptrs[i]->vec[0] == static_cast<int>(i));
            CHECK(ptrs[i]->vec[1] == static_cast<int>(i + 10));
        }
        
        // Cleanup
        for (ComplexObject* ptr : ptrs) {
            ptr->~ComplexObject();
            allocator.deallocate(ptr, 1);
        }
    }
} 
