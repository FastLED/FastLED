// Unit tests for allocator_realloc and allocator traits
// Tests the new realloc() optimization pathway and allocate_at_least() interface

#include "test.h"
#include "fl/allocator.h"
#include "fl/vector.h"


TEST_CASE("Allocator Traits - Capability Detection") {
    SUBCASE("allocator_realloc has both capabilities") {
        // allocator_realloc should support both reallocate() and allocate_at_least()
        static_assert(fl::allocator_traits<fl::allocator_realloc<int>>::has_reallocate_v,
                     "allocator_realloc should support reallocate()");
        static_assert(fl::allocator_traits<fl::allocator_realloc<int>>::has_allocate_at_least_v,
                     "allocator_realloc should support allocate_at_least()");
        CHECK(true);  // Compile-time checks passed
    }

    SUBCASE("base allocator<T> has allocate_at_least") {
        // Standard allocator should have allocate_at_least() (with default implementation)
        static_assert(fl::allocator_traits<fl::allocator<int>>::has_allocate_at_least_v,
                     "allocator<T> should support allocate_at_least()");
        CHECK(true);
    }

    SUBCASE("base allocator<T> has default reallocate") {
        // Standard allocator has reallocate() that returns nullptr (no-op)
        static_assert(fl::allocator_traits<fl::allocator<int>>::has_reallocate_v,
                     "allocator<T> should have reallocate() method");
        CHECK(true);
    }

    SUBCASE("allocator_psram capabilities") {
        // PSRam allocator may or may not have the optional methods
        // Just verify the traits can be queried without errors
        (void)fl::allocator_traits<fl::allocator_psram<int>>::has_allocate_at_least_v;
        (void)fl::allocator_traits<fl::allocator_psram<int>>::has_reallocate_v;
        CHECK(true);  // Just checking it compiles
    }
}

TEST_CASE("allocator_realloc - Basic Functionality") {
    SUBCASE("Simple allocation and deallocation") {
        fl::allocator_realloc<int> alloc;
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
        fl::allocator_realloc<int> alloc;
        int* ptr = alloc.allocate(0);
        CHECK_EQ(ptr, nullptr);
    }

    SUBCASE("Multiple allocations") {
        fl::allocator_realloc<int> alloc;

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
}

TEST_CASE("allocator_realloc - allocate_at_least()") {
    SUBCASE("allocate_at_least returns >= requested size") {
        fl::allocator_realloc<int> alloc;

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
        fl::allocator_realloc<int> alloc;
        auto result = alloc.allocate_at_least(0);
        CHECK_EQ(result.ptr, nullptr);
        CHECK_EQ(result.count, 0);
    }
}

TEST_CASE("allocator_realloc - reallocate()") {
    SUBCASE("Reallocate to larger size") {
        fl::allocator_realloc<int> alloc;

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
        fl::allocator_realloc<int> alloc;

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
        fl::allocator_realloc<int> alloc;

        int* ptr = alloc.allocate(10);
        REQUIRE(ptr != nullptr);

        int* result = alloc.reallocate(ptr, 10, 0);
        CHECK_EQ(result, nullptr);
    }
}

TEST_CASE("Vector with allocator_realloc") {
    SUBCASE("Vector with allocator_realloc resizing") {
        fl::vector<int, fl::allocator_realloc<int>> vec;

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
        fl::vector<float, fl::allocator_realloc<float>> vec;

        for (float i = 0.0f; i < 50.0f; ++i) {
            vec.push_back(i * 1.5f);
        }

        REQUIRE_EQ(vec.size(), 50);
        for (int i = 0; i < 50; ++i) {
            CHECK_EQ(vec[i], static_cast<float>(i) * 1.5f);
        }
    }

    SUBCASE("Vector reserve and access") {
        fl::vector<int, fl::allocator_realloc<int>> vec;

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
}

TEST_CASE("Comparison: allocator_realloc vs standard allocator") {
    SUBCASE("Both produce same results") {
        // Standard allocator
        fl::vector<int, fl::allocator<int>> vec_standard;
        for (int i = 0; i < 100; ++i) {
            vec_standard.push_back(i);
        }

        // Realloc allocator
        fl::vector<int, fl::allocator_realloc<int>> vec_realloc;
        for (int i = 0; i < 100; ++i) {
            vec_realloc.push_back(i);
        }

        // Both should have same size and contents
        CHECK_EQ(vec_standard.size(), vec_realloc.size());
        for (fl::size i = 0; i < vec_standard.size(); ++i) {
            CHECK_EQ(vec_standard[i], vec_realloc[i]);
        }
    }
}

TEST_CASE("allocator_traits - Type trait queries") {
    SUBCASE("Runtime trait queries") {
        // These should compile to constexpr true/false
        constexpr bool realloc_has_reallocate =
            fl::allocator_traits<fl::allocator_realloc<int>>::has_reallocate_v;
        constexpr bool realloc_has_at_least =
            fl::allocator_traits<fl::allocator_realloc<int>>::has_allocate_at_least_v;

        CHECK(realloc_has_reallocate);
        CHECK(realloc_has_at_least);

        constexpr bool standard_has_at_least =
            fl::allocator_traits<fl::allocator<int>>::has_allocate_at_least_v;
        CHECK(standard_has_at_least);
    }
}

TEST_CASE("allocation_result structure") {
    SUBCASE("allocation_result with int") {
        fl::allocation_result<int*, fl::size> result;
        result.ptr = nullptr;
        result.count = 42;

        CHECK_EQ(result.count, 42);
        CHECK_EQ(result.ptr, nullptr);
    }

    SUBCASE("allocation_result from allocator_realloc") {
        fl::allocator_realloc<float> alloc;
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
