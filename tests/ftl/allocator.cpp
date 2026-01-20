#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "fl/stl/type_traits.h"
#include "fl/int.h"

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

    SUBCASE("allocator_realloc has both capabilities") {
        // allocator_realloc should support both reallocate() and allocate_at_least()
        static_assert(allocator_traits<allocator_realloc<int>>::has_reallocate_v,
                     "allocator_realloc should support reallocate()");
        static_assert(allocator_traits<allocator_realloc<int>>::has_allocate_at_least_v,
                     "allocator_realloc should support allocate_at_least()");
        CHECK(true);  // Compile-time checks passed
    }

    SUBCASE("base allocator<T> has allocate_at_least") {
        // Standard allocator should have allocate_at_least() (with default implementation)
        static_assert(allocator_traits<allocator<int>>::has_allocate_at_least_v,
                     "allocator<T> should support allocate_at_least()");
        CHECK(true);
    }

    SUBCASE("base allocator<T> has default reallocate") {
        // Standard allocator has reallocate() that returns nullptr (no-op)
        static_assert(allocator_traits<allocator<int>>::has_reallocate_v,
                     "allocator<T> should have reallocate() method");
        CHECK(true);
    }

    SUBCASE("allocator_psram capabilities") {
        // PSRam allocator may or may not have the optional methods
        // Just verify the traits can be queried without errors
        (void)allocator_traits<allocator_psram<int>>::has_allocate_at_least_v;
        (void)allocator_traits<allocator_psram<int>>::has_reallocate_v;
        CHECK(true);  // Just checking it compiles
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

    SUBCASE("reallocate works for trivially copyable types") {
        allocator<int> alloc;
        int* ptr = alloc.allocate(5);

        // Initialize data
        for (int i = 0; i < 5; ++i) {
            ptr[i] = i * 10;
        }

        // Basic allocator's reallocate should work for trivially copyable types (like int)
        int* new_ptr = alloc.reallocate(ptr, 5, 10);
        CHECK(new_ptr != nullptr);

        // Verify data was preserved
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(new_ptr[i], i * 10);
        }

        // Verify new memory is zero-initialized
        for (int i = 5; i < 10; ++i) {
            CHECK_EQ(new_ptr[i], 0);
        }

        alloc.deallocate(new_ptr, 10);
    }

    SUBCASE("rebind allocator") {
        using int_allocator = allocator<int>;
        using double_allocator = int_allocator::rebind<double>::other;
        CHECK((fl::is_same<double_allocator, allocator<double>>::value));
    }
}

// Test allocator_realloc
TEST_CASE("fl::allocator_realloc") {
    SUBCASE("Simple allocation and deallocation") {
        allocator_realloc<int> alloc;
        int* ptr = alloc.allocate(10);
        REQUIRE(ptr != nullptr);

        // Write test values
        for (int i = 0; i < 10; ++i) {
            ptr[i] = i * 100;
        }

        // Verify values
        for (int i = 0; i < 10; ++i) {
            CHECK_EQ(ptr[i], i * 100);
        }

        alloc.deallocate(ptr, 10);
    }

    SUBCASE("Zero allocation returns nullptr") {
        allocator_realloc<int> alloc;
        int* ptr = alloc.allocate(0);
        CHECK_EQ(ptr, nullptr);
    }

    SUBCASE("Multiple allocations") {
        allocator_realloc<int> alloc;

        int* ptr1 = alloc.allocate(5);
        int* ptr2 = alloc.allocate(3);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr1 != ptr2);

        ptr1[0] = 111;
        ptr2[0] = 222;

        CHECK_EQ(ptr1[0], 111);
        CHECK_EQ(ptr2[0], 222);

        alloc.deallocate(ptr1, 5);
        alloc.deallocate(ptr2, 3);
    }

    SUBCASE("allocate_at_least returns >= requested size") {
        allocator_realloc<int> alloc;

        auto result = alloc.allocate_at_least(10);
        REQUIRE(result.ptr != nullptr);
        REQUIRE(result.count >= 10);

        // Should have allocated at least 1.5x (or 15) for 10 elements
        // Actually trying for (3 * 10) / 2 = 15
        CHECK(result.count >= 10);

        // Verify we can write to the extra allocated space
        for (fl::size i = 0; i < result.count; ++i) {
            result.ptr[i] = static_cast<int>(i);
        }

        alloc.deallocate(result.ptr, result.count);
    }

    SUBCASE("allocate_at_least with zero returns empty result") {
        allocator_realloc<int> alloc;
        auto result = alloc.allocate_at_least(0);
        CHECK_EQ(result.ptr, nullptr);
        CHECK_EQ(result.count, 0);
    }

    SUBCASE("Reallocate to larger size") {
        allocator_realloc<int> alloc;

        // Initial allocation
        int* ptr = alloc.allocate(5);
        REQUIRE(ptr != nullptr);

        for (int i = 0; i < 5; ++i) {
            ptr[i] = i * 10;
        }

        // Reallocate to larger size
        int* new_ptr = alloc.reallocate(ptr, 5, 15);

        if (new_ptr) {  // reallocate() might return nullptr if it fails
            // Verify old data is preserved
            for (int i = 0; i < 5; ++i) {
                CHECK_EQ(new_ptr[i], i * 10);
            }

            // Write new data to expanded region
            for (int i = 5; i < 15; ++i) {
                new_ptr[i] = i * 10;
            }

            // Verify new data
            for (int i = 5; i < 15; ++i) {
                CHECK_EQ(new_ptr[i], i * 10);
            }

            alloc.deallocate(new_ptr, 15);
        } else {
            // reallocate() not supported, fall back to manual allocation
            alloc.deallocate(ptr, 5);
        }
    }

    SUBCASE("Reallocate to smaller size") {
        allocator_realloc<int> alloc;

        int* ptr = alloc.allocate(20);
        REQUIRE(ptr != nullptr);

        for (int i = 0; i < 20; ++i) {
            ptr[i] = i;
        }

        // Reallocate to smaller size
        int* new_ptr = alloc.reallocate(ptr, 20, 10);

        if (new_ptr) {
            // Verify data is preserved
            for (int i = 0; i < 10; ++i) {
                CHECK_EQ(new_ptr[i], i);
            }
            alloc.deallocate(new_ptr, 10);
        } else {
            alloc.deallocate(ptr, 20);
        }
    }

    SUBCASE("Reallocate to zero size") {
        allocator_realloc<int> alloc;

        int* ptr = alloc.allocate(10);
        REQUIRE(ptr != nullptr);

        int* result = alloc.reallocate(ptr, 10, 0);
        CHECK_EQ(result, nullptr);
    }

    SUBCASE("Vector with allocator_realloc resizing") {
        fl::vector<int, allocator_realloc<int>> vec;

        // Push back elements to trigger multiple reallocations
        for (int i = 0; i < 100; ++i) {
            vec.push_back(i);
        }

        // Verify all elements are correct
        REQUIRE_EQ(vec.size(), 100);
        for (int i = 0; i < 100; ++i) {
            CHECK_EQ(vec[i], i);
        }
    }

    SUBCASE("Vector with POD types benefits from realloc") {
        fl::vector<float, allocator_realloc<float>> vec;

        for (float i = 0.0f; i < 50.0f; ++i) {
            vec.push_back(i * 1.5f);
        }

        REQUIRE_EQ(vec.size(), 50);
        for (int i = 0; i < 50; ++i) {
            CHECK_EQ(vec[i], static_cast<float>(i) * 1.5f);
        }
    }

    SUBCASE("Vector reserve and access") {
        fl::vector<int, allocator_realloc<int>> vec;

        vec.reserve(100);
        REQUIRE(vec.capacity() >= 100);

        for (int i = 0; i < 50; ++i) {
            vec.push_back(i);
        }

        CHECK_EQ(vec.size(), 50);
        for (int i = 0; i < 50; ++i) {
            CHECK_EQ(vec[i], i);
        }
    }

    SUBCASE("Comparison: allocator_realloc vs standard allocator") {
        // Standard allocator
        fl::vector<int, allocator<int>> vec_standard;
        for (int i = 0; i < 100; ++i) {
            vec_standard.push_back(i);
        }

        // Realloc allocator
        fl::vector<int, allocator_realloc<int>> vec_realloc;
        for (int i = 0; i < 100; ++i) {
            vec_realloc.push_back(i);
        }

        // Both should have same size and contents
        CHECK_EQ(vec_standard.size(), vec_realloc.size());
        for (fl::size i = 0; i < vec_standard.size(); ++i) {
            CHECK_EQ(vec_standard[i], vec_realloc[i]);
        }
    }

    SUBCASE("Runtime trait queries") {
        // These should compile to constexpr true/false
        constexpr bool realloc_has_reallocate =
            allocator_traits<allocator_realloc<int>>::has_reallocate_v;
        constexpr bool realloc_has_at_least =
            allocator_traits<allocator_realloc<int>>::has_allocate_at_least_v;

        CHECK(realloc_has_reallocate);
        CHECK(realloc_has_at_least);

        constexpr bool standard_has_at_least =
            allocator_traits<allocator<int>>::has_allocate_at_least_v;
        CHECK(standard_has_at_least);
    }

    SUBCASE("allocation_result from allocator_realloc") {
        allocator_realloc<float> alloc;
        auto result = alloc.allocate_at_least(20);

        CHECK_NE(result.ptr, nullptr);
        CHECK(result.count >= 20);

        // Should be able to construct objects in the result
        for (fl::size i = 0; i < result.count; ++i) {
            alloc.construct(&result.ptr[i], static_cast<float>(i) * 3.14f);
        }

        // Verify constructed values
        for (fl::size i = 0; i < result.count; ++i) {
            CHECK_EQ(result.ptr[i], static_cast<float>(i) * 3.14f);
        }

        alloc.deallocate(result.ptr, result.count);
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

    SUBCASE("memory layout verification") {
        SlabAllocator<uint32_t, 16> alloc;

        fl::vector<uint32_t*> ptrs;

        // Allocate several objects
        for (size_t i = 0; i < 8; ++i) {
            uint32_t* ptr = alloc.allocate();
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
            alloc.deallocate(ptr);
        }
    }

    SUBCASE("allocation after cleanup") {
        SlabAllocator<char, 8> alloc;

        char* ptr1 = alloc.allocate();
        REQUIRE(ptr1 != nullptr);

        alloc.cleanup();
        CHECK_EQ(alloc.getActiveAllocations(), 0);

        // Should be able to allocate again after cleanup
        char* ptr2 = alloc.allocate();
        REQUIRE(ptr2 != nullptr);

        alloc.deallocate(ptr2);
    }

    SUBCASE("large block allocation exceeding slab size") {
        SlabAllocator<char, 8> alloc;

        // Try to allocate more blocks than fit in a single slab
        // This should fall back to malloc and not crash
        char* large_ptr = alloc.allocate(10);  // 10 blocks, but slab only has 8
        REQUIRE(large_ptr != nullptr);

        // Verify we can use the memory
        for (int i = 0; i < 10; ++i) {
            large_ptr[i] = static_cast<char>(i);
        }

        // Verify the values
        for (int i = 0; i < 10; ++i) {
            CHECK_EQ(large_ptr[i], static_cast<char>(i));
        }

        alloc.deallocate(large_ptr, 10);
    }

    SUBCASE("very large block allocation") {
        SlabAllocator<char, 8> alloc;

        // Try to allocate a very large block that should definitely fall back to malloc
        char* huge_ptr = alloc.allocate(1000);  // 1000 blocks
        REQUIRE(huge_ptr != nullptr);

        // Verify we can use the memory
        for (int i = 0; i < 1000; ++i) {
            huge_ptr[i] = static_cast<char>(i % 256);
        }

        // Verify some values
        for (int i = 0; i < 100; ++i) {
            CHECK_EQ(huge_ptr[i], static_cast<char>(i % 256));
        }

        alloc.deallocate(huge_ptr, 1000);
    }

    SUBCASE("small multi-allocation (3 objects)") {
        SlabAllocator<int, 8> alloc;

        // Allocate 3 objects at once
        int* ptr = alloc.allocate(3);
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
        CHECK_EQ(alloc.getTotalAllocated(), 3);
        CHECK_EQ(alloc.getSlabCount(), 1);

        alloc.deallocate(ptr, 3);
        CHECK_EQ(alloc.getTotalDeallocated(), 3);
    }

    SUBCASE("medium multi-allocation (5 objects)") {
        SlabAllocator<int, 8> alloc;

        // Allocate 5 objects at once
        int* ptr = alloc.allocate(5);
        REQUIRE(ptr != nullptr);

        // Write to all allocated objects
        for (int i = 0; i < 5; ++i) {
            ptr[i] = i + 200;
        }

        // Verify data integrity
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(ptr[i], i + 200);
        }

        alloc.deallocate(ptr, 5);
    }

    SUBCASE("large multi-allocation fallback (100 objects)") {
        SlabAllocator<int, 8> alloc;

        // Allocate 100 objects - should fallback to malloc
        int* ptr = alloc.allocate(100);
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
        CHECK_EQ(alloc.getTotalAllocated(), 0);
        CHECK_EQ(alloc.getSlabCount(), 0);

        alloc.deallocate(ptr, 100);
    }

    SUBCASE("mixed single and multi-allocations") {
        SlabAllocator<int, 8> alloc;

        // Allocate single objects first
        int* single1 = alloc.allocate(1);
        int* single2 = alloc.allocate(1);
        REQUIRE(single1 != nullptr);
        REQUIRE(single2 != nullptr);

        *single1 = 42;
        *single2 = 84;

        // Allocate multi-object
        int* multi = alloc.allocate(3);
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
        alloc.deallocate(single1, 1);
        alloc.deallocate(single2, 1);
        alloc.deallocate(multi, 3);
    }

    SUBCASE("contiguous allocation verification") {
        SlabAllocator<int, 8> alloc;

        // Allocate 4 contiguous objects
        int* ptr = alloc.allocate(4);
        REQUIRE(ptr != nullptr);

        // Verify they are contiguous in memory
        for (int i = 1; i < 4; ++i) {
            ptrdiff_t diff = &ptr[i] - &ptr[i-1];
            CHECK_EQ(diff, 1);  // Should be exactly 1 int apart
        }

        alloc.deallocate(ptr, 4);
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

// Test allocator_inlined - Comprehensive tests from test_allocator_inlined.cpp
TEST_CASE("fl::allocator_inlined - Basic functionality") {
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

TEST_CASE("fl::allocator_inlined - Inlined to heap transition") {
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

TEST_CASE("fl::allocator_inlined - Free slot management") {
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

TEST_CASE("fl::allocator_inlined - Memory layout verification") {
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

TEST_CASE("fl::allocator_inlined - Edge cases") {
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

TEST_CASE("fl::allocator_inlined - Clear functionality") {
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

    SUBCASE("multiple inlined allocations") {
        allocator_inlined_slab<int, 3> alloc;

        fl::vector<int*> ptrs;
        const size_t num_allocs = 3;  // Exactly the inlined capacity

        for (size_t i = 0; i < num_allocs; ++i) {
            int* ptr = alloc.allocate(1);
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
            alloc.deallocate(ptr, 1);
        }
    }

    SUBCASE("memory layout verification") {
        allocator_inlined_slab<int, 3> alloc;

        fl::vector<int*> ptrs;

        // Allocate inlined storage
        for (size_t i = 0; i < 3; ++i) {
            int* ptr = alloc.allocate(1);
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
            alloc.deallocate(ptr, 1);
        }
    }

    SUBCASE("null pointer deallocation") {
        allocator_inlined_slab<int, 3> alloc;

        // Should not crash
        alloc.deallocate(nullptr, 1);
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


// Global variables to track hook calls
static fl::vector_inlined<void*, 1000> gMallocCalls;
static fl::vector_inlined<fl::size, 1000> gMallocSizes;
static fl::vector_inlined<void*, 1000> gFreeCalls;

// Test hook implementation class
class TestMallocFreeHook : public fl::MallocFreeHook {
public:
    void onMalloc(void* ptr, fl::size size) override {
        gMallocCalls.push_back(ptr);
        gMallocSizes.push_back(size);
    }
    
    void onFree(void* ptr) override {
        gFreeCalls.push_back(ptr);
    }
};

// Helper function to clear tracking data
static void ClearTrackingData() {
    gMallocCalls.clear();
    gMallocSizes.clear();
    gFreeCalls.clear();
    // fl::SetMallocFreeHook(nullptr);
}

TEST_CASE("Malloc/Free Test Hooks - Basic functionality") {
    // Clear any previous tracking data
    ClearTrackingData();
    
    SUBCASE("Set and clear hooks") {
        // Create hook instance
        TestMallocFreeHook hook;
        
        // Set hook
        fl::SetMallocFreeHook(&hook);
        
        // Clear hook
        fl::ClearMallocFreeHook();
        
        // Test that hooks are cleared by doing allocations that shouldn't trigger callbacks
        ClearTrackingData();
        
        void* ptr1 = fl::PSRamAllocate(100);
        void* ptr2 = fl::PSRamAllocate(200);
        
        CHECK(gMallocCalls.empty());
        CHECK(gMallocSizes.empty());
        
        fl::PSRamDeallocate(ptr1);
        fl::PSRamDeallocate(ptr2);
        
        CHECK(gFreeCalls.empty());
    }
    
    SUBCASE("Malloc hook is called after allocation") {
        TestMallocFreeHook hook;
        fl::SetMallocFreeHook(&hook);
        
        ClearTrackingData();
        
        // Test fl::PSRamAllocate
        void* ptr1 = fl::PSRamAllocate(100);
        REQUIRE(ptr1 != nullptr);
        
        CHECK(gMallocCalls.size() == 1);
        CHECK(gMallocCalls[0] == ptr1);
        CHECK(gMallocSizes.size() == 1);
        CHECK(gMallocSizes[0] == 100);
        
        // Test Malloc function
        ClearTrackingData();
        fl::Malloc(200);
        
        CHECK(gMallocCalls.size() == 1);
        CHECK(gMallocSizes[0] == 200);
        
        // Cleanup
        fl::PSRamDeallocate(ptr1);
        fl::ClearMallocFreeHook();
    }
    
    SUBCASE("Free hook is called before deallocation") {
        TestMallocFreeHook hook;
        fl::SetMallocFreeHook(&hook);
        
        ClearTrackingData();
        
        // Allocate some memory
        void* ptr1 = fl::PSRamAllocate(100);
        void* ptr2 = fl::PSRamAllocate(200);
        
        // Clear malloc tracking for this test
        ClearTrackingData();
        
        // Test fl::PSRamDeallocate
        fl::PSRamDeallocate(ptr1);
        
        CHECK(gFreeCalls.size() == 1);
        CHECK(gFreeCalls[0] == ptr1);
        
        // Test Free function
        ClearTrackingData();
        fl::Free(ptr2);
        
        CHECK(gFreeCalls.size() == 1);
        CHECK(gFreeCalls[0] == ptr2);
        
        fl::ClearMallocFreeHook();
    }
    
    SUBCASE("Both hooks work together") {
        TestMallocFreeHook hook;
        fl::SetMallocFreeHook(&hook);
        
        ClearTrackingData();
        
        // Allocate memory
        void* ptr1 = fl::PSRamAllocate(150);
        void* ptr2 = fl::PSRamAllocate(250);
        
        CHECK(gMallocCalls.size() == 2);
        CHECK(gMallocSizes.size() == 2);
        CHECK(gMallocCalls[0] == ptr1);
        CHECK(gMallocCalls[1] == ptr2);
        CHECK(gMallocSizes[0] == 150);
        CHECK(gMallocSizes[1] == 250);
        
        // Clear malloc tracking for free test
        gMallocCalls.clear();
        gMallocSizes.clear();
        
        // Deallocate memory
        fl::PSRamDeallocate(ptr1);
        fl::PSRamDeallocate(ptr2);
        
        CHECK(gFreeCalls.size() == 2);
        CHECK(gFreeCalls[0] == ptr1);
        CHECK(gFreeCalls[1] == ptr2);
        
        // Verify malloc tracking wasn't affected by free operations
        CHECK(gMallocCalls.empty());
        CHECK(gMallocSizes.empty());
        
        fl::ClearMallocFreeHook();
    }
    
    SUBCASE("Null pointer handling") {
        TestMallocFreeHook hook;
        fl::SetMallocFreeHook(&hook);
        
        ClearTrackingData();
        
        // Test that hooks are not called for null pointer free
        fl::Free(nullptr);
        
        CHECK(gFreeCalls.empty());
        
        // Test that hooks are not called for zero-size allocation
        void* ptr = fl::PSRamAllocate(0);
        if (ptr == nullptr) {
            CHECK(gMallocCalls.empty());
            CHECK(gMallocSizes.empty());
        }
        
        fl::ClearMallocFreeHook();
    }
    
    SUBCASE("Hook replacement") {
        // Create initial hook
        TestMallocFreeHook hook1;
        fl::SetMallocFreeHook(&hook1);
        
        ClearTrackingData();
        
        void* ptr = fl::PSRamAllocate(100);
        
        CHECK(gMallocCalls.size() == 1);
        
        // Replace with different hook
        fl::vector<void*> newMallocCalls;
        fl::vector<fl::size> newMallocSizes;
        fl::vector<void*> newFreeCalls;
        
        class NewTestHook : public fl::MallocFreeHook {
        public:
            NewTestHook(fl::vector<void*>& mallocCalls, fl::vector<fl::size>& mallocSizes, fl::vector<void*>& freeCalls)
                : mMallocCalls(mallocCalls), mMallocSizes(mallocSizes), mFreeCalls(freeCalls) {}
            
            void onMalloc(void* ptr, fl::size size) override {
                mMallocCalls.push_back(ptr);
                mMallocSizes.push_back(size);
            }
            
            void onFree(void* ptr) override {
                mFreeCalls.push_back(ptr);
            }
            
        private:
            fl::vector<void*>& mMallocCalls;
            fl::vector<fl::size>& mMallocSizes;
            fl::vector<void*>& mFreeCalls;
        };
        
        NewTestHook hook2(newMallocCalls, newMallocSizes, newFreeCalls);
        fl::SetMallocFreeHook(&hook2);
        
        void* ptr2 = fl::PSRamAllocate(200);
        
        // Original hook should not be called
        CHECK(gMallocCalls.size() == 1);
        CHECK(gMallocSizes.size() == 1);
        
        // New hook should be called
        CHECK(newMallocCalls.size() == 1);
        CHECK(newMallocSizes.size() == 1);
        CHECK(newMallocCalls[0] == ptr2);
        CHECK(newMallocSizes[0] == 200);
        
        // Cleanup
        fl::PSRamDeallocate(ptr);
        fl::PSRamDeallocate(ptr2);
        fl::ClearMallocFreeHook();
    }
}

TEST_CASE("Malloc/Free Test Hooks - Integration with allocators") {
    TestMallocFreeHook hook;
    fl::SetMallocFreeHook(&hook);
    
    SUBCASE("Standard allocator integration") {
        ClearTrackingData();
        
        fl::allocator<int> alloc;
        
        // Allocate using standard allocator
        int* ptr = alloc.allocate(5);
        REQUIRE(ptr != nullptr);
        
        // Hook should be called
        CHECK(gMallocCalls.size() == 1);
        CHECK(gMallocCalls[0] == ptr);
        CHECK(gMallocSizes[0] == sizeof(int) * 5);
        
        // Deallocate
        gMallocCalls.clear();
        gMallocSizes.clear();
        
        alloc.deallocate(ptr, 5);
        
        CHECK(gFreeCalls.size() == 1);
        CHECK(gFreeCalls[0] == ptr);
    }
    
    SUBCASE("PSRAM allocator integration") {
        ClearTrackingData();
        
        fl::allocator_psram<int> alloc;
        
        // Allocate using PSRAM allocator
        int* ptr = alloc.allocate(3);
        REQUIRE(ptr != nullptr);
        
        // Hook should be called
        CHECK(gMallocCalls.size() == 1);
        CHECK(gMallocCalls[0] == ptr);
        CHECK(gMallocSizes[0] == sizeof(int) * 3);
        
        // Deallocate
        gMallocCalls.clear();
        gMallocSizes.clear();
        
        alloc.deallocate(ptr, 3);
        
        CHECK(gFreeCalls.size() == 1);
        CHECK(gFreeCalls[0] == ptr);
    }
    
    fl::ClearMallocFreeHook();
}
