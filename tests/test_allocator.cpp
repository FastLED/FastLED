// Unit tests for general allocator functionality and integration tests

#include "test.h"
#include "fl/allocator.h"
#include "fl/vector.h"

using namespace fl;

// Test struct for general allocator testing
struct TestObject {
    int data[4];  // 16 bytes to make it larger than pointer size
    TestObject() { 
        for (int i = 0; i < 4; ++i) {
            data[i] = 0;
        }
    }
};

TEST_CASE("Allocator - Integration tests") {
    SUBCASE("Different allocator types comparison") {
        // Test basic functionality across different allocator types
        
        // SlabAllocator
        SlabAllocator<int, 8> slab_alloc;
        int* slab_ptr = slab_alloc.allocate();
        REQUIRE(slab_ptr != nullptr);
        *slab_ptr = 100;
        CHECK_EQ(*slab_ptr, 100);
        slab_alloc.deallocate(slab_ptr);
        
        // allocator_inlined
        fl::allocator_inlined<int, 3> inlined_alloc;
        int* inlined_ptr = inlined_alloc.allocate(1);
        REQUIRE(inlined_ptr != nullptr);
        *inlined_ptr = 200;
        CHECK_EQ(*inlined_ptr, 200);
        inlined_alloc.deallocate(inlined_ptr, 1);
        
        // allocator_inlined_slab
        fl::allocator_inlined_slab<int, 3> inlined_slab_alloc;
        int* inlined_slab_ptr = inlined_slab_alloc.allocate(1);
        REQUIRE(inlined_slab_ptr != nullptr);
        *inlined_slab_ptr = 300;
        CHECK_EQ(*inlined_slab_ptr, 300);
        inlined_slab_alloc.deallocate(inlined_slab_ptr, 1);
    }
}

TEST_CASE("Allocator - Multi-allocation support") {
    SUBCASE("SlabAllocator multi-allocation") {
        SlabAllocator<int, 8> allocator;
        
        // Test single allocation
        int* single_ptr = allocator.allocate(1);
        REQUIRE(single_ptr != nullptr);
        *single_ptr = 42;
        CHECK_EQ(*single_ptr, 42);
        allocator.deallocate(single_ptr, 1);
        
        // Test multi-allocation (should work with our bitset implementation)
        int* multi_ptr = allocator.allocate(3);
        REQUIRE(multi_ptr != nullptr);
        
        // Initialize the multi-allocation
        for (int i = 0; i < 3; ++i) {
            multi_ptr[i] = i + 100;
        }
        
        // Verify the values
        for (int i = 0; i < 3; ++i) {
            CHECK_EQ(multi_ptr[i], i + 100);
        }
        
        allocator.deallocate(multi_ptr, 3);
    }
}

TEST_CASE("Allocator - Copy and move semantics") {
    SUBCASE("allocator_inlined copy constructor") {
        fl::allocator_inlined<int, 3> allocator1;
        
        // Allocate some memory in the first allocator
        int* ptr1 = allocator1.allocate(1);
        REQUIRE(ptr1 != nullptr);
        *ptr1 = 42;
        
        // Copy constructor
        fl::allocator_inlined<int, 3> allocator2(allocator1);
        
        // Original allocation should still be valid
        CHECK_EQ(*ptr1, 42);
        
        // New allocator should be independent
        int* ptr2 = allocator2.allocate(1);
        REQUIRE(ptr2 != nullptr);
        *ptr2 = 84;
        CHECK_EQ(*ptr2, 84);
        
        // Cleanup
        allocator1.deallocate(ptr1, 1);
        allocator2.deallocate(ptr2, 1);
    }
    
    SUBCASE("allocator_inlined_slab copy constructor") {
        fl::allocator_inlined_slab<int, 3> allocator1;
        
        // Allocate some memory in the first allocator
        int* ptr1 = allocator1.allocate(1);
        REQUIRE(ptr1 != nullptr);
        *ptr1 = 42;
        
        // Copy constructor
        fl::allocator_inlined_slab<int, 3> allocator2(allocator1);
        
        // Original allocation should still be valid
        CHECK_EQ(*ptr1, 42);
        
        // New allocator should be independent
        int* ptr2 = allocator2.allocate(1);
        REQUIRE(ptr2 != nullptr);
        *ptr2 = 84;
        CHECK_EQ(*ptr2, 84);
        
        // Cleanup
        allocator1.deallocate(ptr1, 1);
        allocator2.deallocate(ptr2, 1);
    }
}

TEST_CASE("Allocator - Performance and stress tests") {
    SUBCASE("SlabAllocator performance") {
        SlabAllocator<TestObject, 16> allocator;
        
        fl::vector<TestObject*> ptrs;
        const size_t num_allocs = 32;  // More than one slab
        
        // Allocate
        for (size_t i = 0; i < num_allocs; ++i) {
            TestObject* ptr = allocator.allocate();
            REQUIRE(ptr != nullptr);
            
            // Initialize data
            for (int j = 0; j < 4; ++j) {
                ptr->data[j] = static_cast<int>(i * 10 + j);
            }
            ptrs.push_back(ptr);
        }
        
        // Verify data integrity
        for (size_t i = 0; i < ptrs.size(); ++i) {
            for (int j = 0; j < 4; ++j) {
                CHECK_EQ(ptrs[i]->data[j], static_cast<int>(i * 10 + j));
            }
        }
        
        // Cleanup
        for (TestObject* ptr : ptrs) {
            allocator.deallocate(ptr);
        }
    }
} 
