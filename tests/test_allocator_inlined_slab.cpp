// Unit tests for allocator_inlined_slab - Inlined storage with slab fallback allocator

#include "test.h"
#include "fl/allocator.h"
#include "fl/vector.h"

using namespace fl;

TEST_CASE("allocator_inlined_slab - Basic functionality") {
    using TestAllocator = fl::allocator_inlined_slab<int, 3>;
    
    SUBCASE("Single allocation and deallocation") {
        TestAllocator allocator;
        
        int* ptr = allocator.allocate(1);
        REQUIRE(ptr != nullptr);
        
        // Write to the allocation
        *ptr = 42;
        CHECK_EQ(*ptr, 42);
        
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
            CHECK_EQ(*ptrs[i], static_cast<int>(i + 100));
        }
        
        // Cleanup
        for (int* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
}

TEST_CASE("allocator_inlined_slab - Memory layout") {
    using TestAllocator = fl::allocator_inlined_slab<int, 3>;
    
    SUBCASE("Basic memory layout") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        
        // Allocate inlined storage
        for (size_t i = 0; i < 3; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        // Verify all allocations are valid and contain expected data
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK_EQ(*ptrs[i], static_cast<int>(i + 100));
        }
        
        // Cleanup
        for (int* ptr : ptrs) {
            allocator.deallocate(ptr, 1);
        }
    }
}

TEST_CASE("allocator_inlined_slab - Edge cases") {
    using TestAllocator = fl::allocator_inlined_slab<int, 3>;
    
    SUBCASE("Null pointer deallocation") {
        TestAllocator allocator;
        
        // Should not crash
        allocator.deallocate(nullptr, 1);
    }
} 
