// Unit tests for SlabAllocator to ensure contiguous memory allocation

#include "test.h"
#include "fl/allocator.h"
#include "fl/vector.h"
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
