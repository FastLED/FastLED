#include "test.h"
#include "fl/allocator.h"
#include "fl/vector.h"

using namespace fl;

// Test allocation_result struct
TEST_CASE("fl::allocation_result") {
    SUBCASE("basic construction") {
        allocation_result<int*, fl::size> result{nullptr, 0};
        CHECK_EQ(result.ptr, nullptr);
        CHECK_EQ(result.count, 0);

        int value = 42;
        allocation_result<int*, fl::size> result2{&value, 1};
        CHECK_EQ(result2.ptr, &value);
        CHECK_EQ(result2.count, 1);
    }
}

// Test allocator_traits
TEST_CASE("fl::allocator_traits") {
    SUBCASE("basic allocator traits") {
        using traits = allocator_traits<allocator<int>>;
        CHECK((fl::is_same<traits::value_type, int>::value));
        CHECK((fl::is_same<traits::pointer, int*>::value));
        CHECK((fl::is_same<traits::size_type, fl::size>::value));
    }

    SUBCASE("has_reallocate detection") {
        // allocator<T> does not have working reallocate
        constexpr bool has_reallocate_basic = allocator_traits<allocator<int>>::has_reallocate_v;
        CHECK_EQ(has_reallocate_basic, true);

        // allocator_realloc<T> has working reallocate
        constexpr bool has_reallocate_realloc = allocator_traits<allocator_realloc<int>>::has_reallocate_v;
        CHECK_EQ(has_reallocate_realloc, true);
    }

    SUBCASE("has_allocate_at_least detection") {
        // Both allocator and allocator_realloc have allocate_at_least
        constexpr bool has_alloc_at_least_basic = allocator_traits<allocator<int>>::has_allocate_at_least_v;
        CHECK_EQ(has_alloc_at_least_basic, true);

        constexpr bool has_alloc_at_least_realloc = allocator_traits<allocator_realloc<int>>::has_allocate_at_least_v;
        CHECK_EQ(has_alloc_at_least_realloc, true);
    }
}

// Test basic allocator
TEST_CASE("fl::allocator") {
    SUBCASE("allocate and deallocate") {
        allocator<int> alloc;

        // Allocate zero elements
        int* ptr0 = alloc.allocate(0);
        CHECK_EQ(ptr0, nullptr);

        // Allocate single element
        int* ptr1 = alloc.allocate(1);
        CHECK(ptr1 != nullptr);
        CHECK_EQ(*ptr1, 0);  // Should be zero-initialized
        alloc.deallocate(ptr1, 1);

        // Allocate multiple elements
        int* ptr10 = alloc.allocate(10);
        CHECK(ptr10 != nullptr);
        for (int i = 0; i < 10; ++i) {
            CHECK_EQ(ptr10[i], 0);  // All should be zero-initialized
        }
        alloc.deallocate(ptr10, 10);

        // Deallocate nullptr should be safe
        alloc.deallocate(nullptr, 0);
    }

    SUBCASE("construct and destroy") {
        allocator<int> alloc;
        int* ptr = alloc.allocate(1);
        CHECK(ptr != nullptr);

        // Construct value
        alloc.construct(ptr, 42);
        CHECK_EQ(*ptr, 42);

        // Destroy and deallocate
        alloc.destroy(ptr);
        alloc.deallocate(ptr, 1);

        // Construct/destroy with nullptr should be safe
        alloc.construct(static_cast<int*>(nullptr), 42);
        alloc.destroy(static_cast<int*>(nullptr));
    }

    SUBCASE("allocate_at_least") {
        allocator<int> alloc;

        // Zero allocation
        auto result0 = alloc.allocate_at_least(0);
        CHECK_EQ(result0.ptr, nullptr);
        CHECK_EQ(result0.count, 0);

        // Normal allocation
        auto result = alloc.allocate_at_least(10);
        CHECK(result.ptr != nullptr);
        CHECK_EQ(result.count, 10);  // Basic allocator returns exact count
        alloc.deallocate(result.ptr, result.count);
    }

    SUBCASE("reallocate returns nullptr") {
        allocator<int> alloc;
        int* ptr = alloc.allocate(5);

        // Basic allocator's reallocate should return nullptr (not supported)
        int* new_ptr = alloc.reallocate(ptr, 5, 10);
        CHECK_EQ(new_ptr, nullptr);

        alloc.deallocate(ptr, 5);
    }

    SUBCASE("rebind allocator") {
        using int_allocator = allocator<int>;
        using double_allocator = int_allocator::rebind<double>::other;
        CHECK((fl::is_same<double_allocator, allocator<double>>::value));
    }
}

// Test allocator_realloc
TEST_CASE("fl::allocator_realloc") {
    SUBCASE("basic allocation") {
        allocator_realloc<int> alloc;

        // Allocate and verify zero-initialization
        int* ptr = alloc.allocate(5);
        CHECK(ptr != nullptr);
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ptr[i], 0);
        }
        alloc.deallocate(ptr, 5);
    }

    SUBCASE("allocate_at_least with growth factor") {
        allocator_realloc<int> alloc;

        // Should return 1.5x requested
        auto result = alloc.allocate_at_least(10);
        CHECK(result.ptr != nullptr);
        CHECK_EQ(result.count, 15);  // 10 * 1.5 = 15
        alloc.deallocate(result.ptr, result.count);
    }

    SUBCASE("reallocate functionality") {
        allocator_realloc<int> alloc;

        // Allocate initial memory
        int* ptr = alloc.allocate(5);
        CHECK(ptr != nullptr);
        ptr[0] = 1;
        ptr[1] = 2;
        ptr[2] = 3;
        ptr[3] = 4;
        ptr[4] = 5;

        // Reallocate to larger size
        int* new_ptr = alloc.reallocate(ptr, 5, 10);
        CHECK(new_ptr != nullptr);

        // Original data should be preserved
        CHECK_EQ(new_ptr[0], 1);
        CHECK_EQ(new_ptr[1], 2);
        CHECK_EQ(new_ptr[2], 3);
        CHECK_EQ(new_ptr[3], 4);
        CHECK_EQ(new_ptr[4], 5);

        // New elements should be zero-initialized
        CHECK_EQ(new_ptr[5], 0);
        CHECK_EQ(new_ptr[6], 0);

        alloc.deallocate(new_ptr, 10);
    }

    SUBCASE("reallocate to zero") {
        allocator_realloc<int> alloc;
        int* ptr = alloc.allocate(5);

        // Reallocate to zero should free memory
        int* new_ptr = alloc.reallocate(ptr, 5, 0);
        CHECK_EQ(new_ptr, nullptr);
    }
}

// Test allocator_psram
TEST_CASE("fl::allocator_psram") {
    SUBCASE("basic allocation") {
        allocator_psram<int> alloc;

        // Note: PSRam may not be available in test environment
        // These tests verify the interface works
        int* ptr = alloc.allocate(5);
        if (ptr != nullptr) {
            alloc.deallocate(ptr, 5);
        }
    }

    SUBCASE("allocate_at_least") {
        allocator_psram<int> alloc;
        auto result = alloc.allocate_at_least(10);
        if (result.ptr != nullptr) {
            CHECK_EQ(result.count, 10);  // PSRam allocator returns exact count
            alloc.deallocate(result.ptr, result.count);
        }
    }

    SUBCASE("reallocate not supported") {
        allocator_psram<int> alloc;
        int* ptr = alloc.allocate(5);
        if (ptr != nullptr) {
            // PSRam allocator doesn't support reallocate
            int* new_ptr = alloc.reallocate(ptr, 5, 10);
            CHECK_EQ(new_ptr, nullptr);
            alloc.deallocate(ptr, 5);
        }
    }
}

// Test SlabAllocator
TEST_CASE("fl::SlabAllocator") {
    SUBCASE("basic allocation") {
        SlabAllocator<int, 8> alloc;

        int* ptr = alloc.allocate(1);
        CHECK(ptr != nullptr);
        CHECK_EQ(*ptr, 0);  // Zero-initialized
        alloc.deallocate(ptr, 1);
    }

    SUBCASE("multiple allocations in same slab") {
        SlabAllocator<int, 8> alloc;

        int* ptr1 = alloc.allocate(1);
        int* ptr2 = alloc.allocate(1);
        int* ptr3 = alloc.allocate(1);

        CHECK(ptr1 != nullptr);
        CHECK(ptr2 != nullptr);
        CHECK(ptr3 != nullptr);
        CHECK(ptr1 != ptr2);
        CHECK(ptr2 != ptr3);

        *ptr1 = 1;
        *ptr2 = 2;
        *ptr3 = 3;

        CHECK_EQ(*ptr1, 1);
        CHECK_EQ(*ptr2, 2);
        CHECK_EQ(*ptr3, 3);

        alloc.deallocate(ptr1, 1);
        alloc.deallocate(ptr2, 1);
        alloc.deallocate(ptr3, 1);
    }

    SUBCASE("allocation and deallocation statistics") {
        SlabAllocator<int, 8> alloc;

        CHECK_EQ(alloc.getTotalAllocated(), 0);
        CHECK_EQ(alloc.getTotalDeallocated(), 0);
        CHECK_EQ(alloc.getActiveAllocations(), 0);

        int* ptr1 = alloc.allocate(2);
        CHECK_EQ(alloc.getTotalAllocated(), 2);
        CHECK_EQ(alloc.getActiveAllocations(), 2);

        int* ptr2 = alloc.allocate(3);
        CHECK_EQ(alloc.getTotalAllocated(), 5);
        CHECK_EQ(alloc.getActiveAllocations(), 5);

        alloc.deallocate(ptr1, 2);
        CHECK_EQ(alloc.getTotalDeallocated(), 2);
        CHECK_EQ(alloc.getActiveAllocations(), 3);

        alloc.deallocate(ptr2, 3);
        CHECK_EQ(alloc.getTotalDeallocated(), 5);
        CHECK_EQ(alloc.getActiveAllocations(), 0);
    }

    SUBCASE("cleanup clears statistics") {
        SlabAllocator<int, 8> alloc;

        int* ptr = alloc.allocate(3);
        (void)ptr;  // Intentionally unused, just testing statistics
        CHECK_EQ(alloc.getTotalAllocated(), 3);

        alloc.cleanup();
        CHECK_EQ(alloc.getTotalAllocated(), 0);
        CHECK_EQ(alloc.getTotalDeallocated(), 0);
        CHECK_EQ(alloc.getActiveAllocations(), 0);
    }

    SUBCASE("move constructor") {
        SlabAllocator<int, 8> alloc1;
        int* ptr = alloc1.allocate(2);
        CHECK_EQ(alloc1.getTotalAllocated(), 2);

        SlabAllocator<int, 8> alloc2(fl::move(alloc1));
        CHECK_EQ(alloc2.getTotalAllocated(), 2);
        CHECK_EQ(alloc1.getTotalAllocated(), 0);  // Moved-from is reset

        alloc2.deallocate(ptr, 2);
    }

    SUBCASE("move assignment") {
        SlabAllocator<int, 8> alloc1;
        int* ptr = alloc1.allocate(2);
        CHECK_EQ(alloc1.getTotalAllocated(), 2);

        SlabAllocator<int, 8> alloc2;
        alloc2 = fl::move(alloc1);
        CHECK_EQ(alloc2.getTotalAllocated(), 2);
        CHECK_EQ(alloc1.getTotalAllocated(), 0);  // Moved-from is reset

        alloc2.deallocate(ptr, 2);
    }

    SUBCASE("large allocation fallback") {
        SlabAllocator<int, 8> alloc;

        // Allocate more than slab size
        int* ptr = alloc.allocate(20);
        CHECK(ptr != nullptr);

        // Should not appear in slab statistics
        // (falls back to regular malloc)

        alloc.deallocate(ptr, 20);
    }

    SUBCASE("deallocate nullptr is safe") {
        SlabAllocator<int, 8> alloc;
        alloc.deallocate(nullptr, 1);  // Should not crash
    }

    SUBCASE("slab count tracking") {
        SlabAllocator<int, 4> alloc;  // Small slab size

        CHECK_EQ(alloc.getSlabCount(), 0);

        int* ptr1 = alloc.allocate(1);
        CHECK_EQ(alloc.getSlabCount(), 1);  // First slab created

        int* ptr2 = alloc.allocate(1);
        CHECK_EQ(alloc.getSlabCount(), 1);  // Still using first slab

        int* ptr3 = alloc.allocate(1);
        int* ptr4 = alloc.allocate(1);
        CHECK_EQ(alloc.getSlabCount(), 1);  // Still using first slab (4 elements)

        int* ptr5 = alloc.allocate(1);
        CHECK_EQ(alloc.getSlabCount(), 2);  // Second slab needed

        alloc.deallocate(ptr1, 1);
        alloc.deallocate(ptr2, 1);
        alloc.deallocate(ptr3, 1);
        alloc.deallocate(ptr4, 1);
        alloc.deallocate(ptr5, 1);
    }
}

// Test allocator_slab (STL-compatible wrapper)
TEST_CASE("fl::allocator_slab") {
    SUBCASE("basic allocation") {
        allocator_slab<int, 8> alloc;

        int* ptr = alloc.allocate(1);
        CHECK(ptr != nullptr);
        alloc.deallocate(ptr, 1);
    }

    SUBCASE("construct and destroy") {
        allocator_slab<int, 8> alloc;

        int* ptr = alloc.allocate(1);
        alloc.construct(ptr, 42);
        CHECK_EQ(*ptr, 42);

        alloc.destroy(ptr);
        alloc.deallocate(ptr, 1);
    }

    SUBCASE("equality comparison") {
        allocator_slab<int, 8> alloc1;
        allocator_slab<int, 8> alloc2;

        CHECK(alloc1 == alloc2);
        CHECK(!(alloc1 != alloc2));
    }

    SUBCASE("rebind allocator") {
        using int_alloc = allocator_slab<int, 8>;
        using double_alloc = int_alloc::rebind<double>::other;
        CHECK((fl::is_same<double_alloc, allocator_slab<double, 8>>::value));
    }

    SUBCASE("copy constructor and assignment") {
        allocator_slab<int, 8> alloc1;
        allocator_slab<int, 8> alloc2(alloc1);
        allocator_slab<int, 8> alloc3;
        alloc3 = alloc1;

        // Should compile and work without issues
        int* ptr1 = alloc1.allocate(1);
        int* ptr2 = alloc2.allocate(1);
        alloc1.deallocate(ptr1, 1);
        alloc3.deallocate(ptr2, 1);
    }
}

// Test allocator_inlined
TEST_CASE("fl::allocator_inlined") {
    SUBCASE("basic inlined allocation") {
        allocator_inlined<int, 4> alloc;

        CHECK_EQ(alloc.inlined_capacity(), 4);
        CHECK_EQ(alloc.total_size(), 0);

        // Allocate within inlined capacity
        int* ptr1 = alloc.allocate(1);
        CHECK(ptr1 != nullptr);
        CHECK_EQ(alloc.total_size(), 1);

        int* ptr2 = alloc.allocate(1);
        CHECK(ptr2 != nullptr);
        CHECK_EQ(alloc.total_size(), 2);

        alloc.deallocate(ptr1, 1);
        alloc.deallocate(ptr2, 1);
        CHECK_EQ(alloc.total_size(), 0);
    }

    SUBCASE("heap fallback for large allocations") {
        allocator_inlined<int, 4> alloc;

        // Allocate more than inlined capacity
        int* ptr = alloc.allocate(10);
        CHECK(ptr != nullptr);
        CHECK_EQ(alloc.total_size(), 10);

        alloc.deallocate(ptr, 10);
        CHECK_EQ(alloc.total_size(), 0);
    }

    SUBCASE("zero allocation") {
        allocator_inlined<int, 4> alloc;
        int* ptr = alloc.allocate(0);
        CHECK_EQ(ptr, nullptr);
    }

    SUBCASE("deallocate nullptr is safe") {
        allocator_inlined<int, 4> alloc;
        alloc.deallocate(nullptr, 1);  // Should not crash
    }

    SUBCASE("clear method") {
        allocator_inlined<int, 4> alloc;

        int* ptr1 = alloc.allocate(1);
        int* ptr2 = alloc.allocate(1);
        (void)ptr1;  // Intentionally unused, testing statistics
        (void)ptr2;  // Intentionally unused, testing statistics
        CHECK_EQ(alloc.total_size(), 2);

        alloc.clear();
        CHECK_EQ(alloc.total_size(), 0);
    }

    SUBCASE("construct and destroy") {
        allocator_inlined<int, 4> alloc;

        int* ptr = alloc.allocate(1);
        alloc.construct(ptr, 99);
        CHECK_EQ(*ptr, 99);

        alloc.destroy(ptr);
        alloc.deallocate(ptr, 1);
    }

    SUBCASE("copy constructor") {
        allocator_inlined<int, 4> alloc1;
        int* ptr1 = alloc1.allocate(1);
        alloc1.construct(ptr1, 42);
        CHECK_EQ(*ptr1, 42);

        allocator_inlined<int, 4> alloc2(alloc1);
        (void)alloc2;  // Copy should work, though heap allocations aren't copied

        alloc1.destroy(ptr1);
        alloc1.deallocate(ptr1, 1);
    }

    SUBCASE("copy assignment") {
        allocator_inlined<int, 4> alloc1;
        int* ptr1 = alloc1.allocate(1);
        alloc1.construct(ptr1, 42);

        allocator_inlined<int, 4> alloc2;
        alloc2 = alloc1;

        alloc1.destroy(ptr1);
        alloc1.deallocate(ptr1, 1);
    }

    SUBCASE("inlined slot reuse") {
        allocator_inlined<int, 4> alloc;

        // Allocate and deallocate to create free slot
        int* ptr1 = alloc.allocate(1);
        alloc.deallocate(ptr1, 1);

        // Next allocation should reuse the freed slot
        int* ptr2 = alloc.allocate(1);
        CHECK_EQ(ptr1, ptr2);  // Should be same address

        alloc.deallocate(ptr2, 1);
    }
}

// Test allocator_inlined_psram alias
TEST_CASE("fl::allocator_inlined_psram") {
    SUBCASE("type alias verification") {
        using expected = allocator_inlined<int, 4, allocator_psram<int>>;
        using actual = allocator_inlined_psram<int, 4>;
        CHECK((fl::is_same<expected, actual>::value));
    }
}

// Test allocator_inlined_slab alias
TEST_CASE("fl::allocator_inlined_slab") {
    SUBCASE("type alias verification") {
        using expected = allocator_inlined<int, 4, allocator_slab<int>>;
        using actual = allocator_inlined_slab<int, 4>;
        CHECK((fl::is_same<expected, actual>::value));
    }

    SUBCASE("basic usage") {
        allocator_inlined_slab<int, 4> alloc;

        int* ptr = alloc.allocate(1);
        CHECK(ptr != nullptr);
        alloc.deallocate(ptr, 1);
    }
}

// Integration test with fl::vector
TEST_CASE("fl::allocator integration with vector") {
    SUBCASE("vector with default allocator") {
        fl::vector<int, allocator<int>> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);

        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
    }

    SUBCASE("vector with realloc allocator") {
        fl::vector<int, allocator_realloc<int>> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);

        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
    }

    SUBCASE("vector with slab allocator") {
        fl::vector<int, allocator_slab<int, 8>> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);

        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
    }

    SUBCASE("vector with inlined allocator") {
        fl::vector<int, allocator_inlined<int, 4>> vec;

        // Add elements within inlined capacity
        vec.push_back(1);
        vec.push_back(2);

        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);

        // Add more elements to trigger heap allocation
        vec.push_back(3);
        vec.push_back(4);
        vec.push_back(5);

        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[4], 5);
    }
}
