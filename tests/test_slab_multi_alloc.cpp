#include "doctest.h"
#include "fl/allocator.h"

using namespace fl;

TEST_CASE("SlabAllocator - Multi-allocation support") {
    SUBCASE("Small multi-allocation (3 objects)") {
        SlabAllocator<int, 8> allocator;
        
        // Allocate 3 objects at once
        int* ptr = allocator.allocate(3);
        REQUIRE(ptr != nullptr);
        
        // Write to all allocated objects
        for (int i = 0; i < 3; ++i) {
            ptr[i] = i + 100;
        }
        
        // Verify data integrity
        for (int i = 0; i < 3; ++i) {
            CHECK_EQ(ptr[i], i + 100);
        }
        
        // Check slab statistics
        CHECK_EQ(allocator.getTotalAllocated(), 3);
        CHECK_EQ(allocator.getSlabCount(), 1);
        
        allocator.deallocate(ptr, 3);
        CHECK_EQ(allocator.getTotalDeallocated(), 3);
    }
    
    SUBCASE("Medium multi-allocation (5 objects)") {
        SlabAllocator<int, 8> allocator;
        
        // Allocate 5 objects at once
        int* ptr = allocator.allocate(5);
        REQUIRE(ptr != nullptr);
        
        // Write to all allocated objects
        for (int i = 0; i < 5; ++i) {
            ptr[i] = i + 200;
        }
        
        // Verify data integrity
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ptr[i], i + 200);
        }
        
        allocator.deallocate(ptr, 5);
    }
    
    SUBCASE("Large multi-allocation fallback (100 objects)") {
        SlabAllocator<int, 8> allocator;
        
        // Allocate 100 objects - should fallback to malloc
        int* ptr = allocator.allocate(100);
        REQUIRE(ptr != nullptr);
        
        // Write to all allocated objects
        for (int i = 0; i < 100; ++i) {
            ptr[i] = i;
        }
        
        // Verify data integrity
        for (int i = 0; i < 100; ++i) {
            CHECK_EQ(ptr[i], i);
        }
        
        // Should not affect slab statistics since it uses malloc
        CHECK_EQ(allocator.getTotalAllocated(), 0);
        CHECK_EQ(allocator.getSlabCount(), 0);
        
        allocator.deallocate(ptr, 100);
    }
    
    SUBCASE("Mixed single and multi-allocations") {
        SlabAllocator<int, 8> allocator;
        
        // Allocate single objects first
        int* single1 = allocator.allocate(1);
        int* single2 = allocator.allocate(1);
        REQUIRE(single1 != nullptr);
        REQUIRE(single2 != nullptr);
        
        *single1 = 42;
        *single2 = 84;
        
        // Allocate multi-object
        int* multi = allocator.allocate(3);
        REQUIRE(multi != nullptr);
        
        for (int i = 0; i < 3; ++i) {
            multi[i] = i + 300;
        }
        
        // Verify all data is intact
        CHECK_EQ(*single1, 42);
        CHECK_EQ(*single2, 84);
        for (int i = 0; i < 3; ++i) {
            CHECK_EQ(multi[i], i + 300);
        }
        
        // Cleanup
        allocator.deallocate(single1, 1);
        allocator.deallocate(single2, 1);
        allocator.deallocate(multi, 3);
    }
    
    SUBCASE("Contiguous allocation verification") {
        SlabAllocator<int, 8> allocator;
        
        // Allocate 4 contiguous objects
        int* ptr = allocator.allocate(4);
        REQUIRE(ptr != nullptr);
        
        // Verify they are contiguous in memory
        for (int i = 1; i < 4; ++i) {
            ptrdiff_t diff = &ptr[i] - &ptr[i-1];
            CHECK_EQ(diff, 1);  // Should be exactly 1 int apart
        }
        
        allocator.deallocate(ptr, 4);
    }
} 
