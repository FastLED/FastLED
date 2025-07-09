// Unit tests for allocator_inlined - Inlined storage with heap fallback allocator

#include "test.h"
#include "fl/allocator.h"
#include "fl/vector.h"

using namespace fl;

TEST_CASE("allocator_inlined - Basic functionality") {
    using TestAllocator = fl::allocator_inlined<int, 3>;
    
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
            CHECK_EQ(*ptrs[i], static_cast<int>(i + 100));
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
            CHECK_EQ(*inlined_ptrs[i], static_cast<int>(i + 100));
        }
        
        // Verify heap allocations
        for (size_t i = 0; i < heap_ptrs.size(); ++i) {
            CHECK_EQ(*heap_ptrs[i], static_cast<int>(i + 200));
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
        
        // Verify other allocations are still intact
        CHECK_EQ(*ptrs[0], 100);
        CHECK_EQ(*ptrs[2], 102);
        CHECK_EQ(*new_ptr, 999);
        
        // Cleanup
        allocator.deallocate(ptrs[0], 1);
        allocator.deallocate(ptrs[2], 1);
        allocator.deallocate(new_ptr, 1);
    }
}

TEST_CASE("allocator_inlined - Memory layout verification") {
    using TestAllocator = fl::allocator_inlined<int, 3>;
    
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

TEST_CASE("allocator_inlined - Edge cases") {
    using TestAllocator = fl::allocator_inlined<int, 3>;
    
    SUBCASE("Zero size allocation") {
        TestAllocator allocator;
        
        // Zero size allocation should return nullptr or be handled gracefully
        int* ptr = allocator.allocate(0);
        // Different implementations may handle this differently
        // Just ensure it doesn't crash
        if (ptr != nullptr) {
            allocator.deallocate(ptr, 0);
        }
    }
    
    SUBCASE("Null pointer deallocation") {
        TestAllocator allocator;
        
        // Should not crash
        allocator.deallocate(nullptr, 1);
    }
}

TEST_CASE("allocator_inlined - Clear functionality") {
    using TestAllocator = fl::allocator_inlined<int, 3>;
    
    SUBCASE("Clear after mixed allocations") {
        TestAllocator allocator;
        
        fl::vector<int*> ptrs;
        
        // Allocate more than inlined capacity
        for (size_t i = 0; i < 5; ++i) {
            int* ptr = allocator.allocate(1);
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<int>(i + 100);
            ptrs.push_back(ptr);
        }
        
        // Clear the allocator
        allocator.clear();
        
        // Should be able to allocate again after clear
        int* new_ptr = allocator.allocate(1);
        REQUIRE(new_ptr != nullptr);
        *new_ptr = 999;
        
        // Cleanup
        allocator.deallocate(new_ptr, 1);
    }
} 
