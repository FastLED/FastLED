// Unit tests for SlabAllocator - Core slab-based memory allocator

#include "test.h"
#include "fl/allocator.h"
#include "fl/vector.h"
#include <algorithm>

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
        CHECK_EQ(allocator.getTotalAllocated(), 1);
        CHECK_EQ(allocator.getActiveAllocations(), 1);
        
        allocator.deallocate(ptr);
        CHECK_EQ(allocator.getTotalDeallocated(), 1);
        CHECK_EQ(allocator.getActiveAllocations(), 0);
    }
    
    SUBCASE("Multiple allocations within single slab") {
        TestAllocator allocator;
        
        fl::vector<TestObject*> ptrs;
        const size_t num_allocs = 5;
        
        for (size_t i = 0; i < num_allocs; ++i) {
            TestObject* ptr = allocator.allocate();
            REQUIRE(ptr != nullptr);
            ptrs.push_back(ptr);
        }
        
        CHECK_EQ(allocator.getTotalAllocated(), num_allocs);
        CHECK_EQ(allocator.getActiveAllocations(), num_allocs);
        
        for (TestObject* ptr : ptrs) {
            allocator.deallocate(ptr);
        }
        
        CHECK_EQ(allocator.getActiveAllocations(), 0);
    }
}

TEST_CASE("SlabAllocator - Memory layout verification") {
    using SmallAllocator = SlabAllocator<uint32_t, 16>;
    SmallAllocator allocator;
    
    SUBCASE("Basic memory layout") {
        fl::vector<uint32_t*> ptrs;
        
        // Allocate several objects
        for (size_t i = 0; i < 8; ++i) {
            uint32_t* ptr = allocator.allocate();
            REQUIRE(ptr != nullptr);
            *ptr = static_cast<uint32_t>(i + 1000);  // Set unique value
            ptrs.push_back(ptr);
        }
        
        // Verify all allocations are valid and have correct values
        for (size_t i = 0; i < ptrs.size(); ++i) {
            CHECK_EQ(*ptrs[i], static_cast<uint32_t>(i + 1000));
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
    
    SUBCASE("Null pointer deallocation") {
        // Should not crash
        allocator.deallocate(nullptr);
        CHECK_EQ(allocator.getActiveAllocations(), 0);
    }
    
    SUBCASE("Allocation after cleanup") {
        char* ptr1 = allocator.allocate();
        REQUIRE(ptr1 != nullptr);
        
        allocator.cleanup();
        CHECK_EQ(allocator.getActiveAllocations(), 0);
        
        // Should be able to allocate again after cleanup
        char* ptr2 = allocator.allocate();
        REQUIRE(ptr2 != nullptr);
        
        allocator.deallocate(ptr2);
    }
    
    SUBCASE("Large block allocation exceeding slab size") {
        // Try to allocate more blocks than fit in a single slab
        // This should fall back to malloc and not crash
        char* large_ptr = allocator.allocate(10);  // 10 blocks, but slab only has 8
        REQUIRE(large_ptr != nullptr);
        
        // Verify we can use the memory
        for (int i = 0; i < 10; ++i) {
            large_ptr[i] = static_cast<char>(i);
        }
        
        // Verify the values
        for (int i = 0; i < 10; ++i) {
            CHECK_EQ(large_ptr[i], static_cast<char>(i));
        }
        
        allocator.deallocate(large_ptr, 10);
    }
    
    SUBCASE("Very large block allocation") {
        // Try to allocate a very large block that should definitely fall back to malloc
        char* huge_ptr = allocator.allocate(1000);  // 1000 blocks
        REQUIRE(huge_ptr != nullptr);
        
        // Verify we can use the memory
        for (int i = 0; i < 1000; ++i) {
            huge_ptr[i] = static_cast<char>(i % 256);
        }
        
        // Verify some values
        for (int i = 0; i < 100; ++i) {
            CHECK_EQ(huge_ptr[i], static_cast<char>(i % 256));
        }
        
        allocator.deallocate(huge_ptr, 1000);
    }
} 
