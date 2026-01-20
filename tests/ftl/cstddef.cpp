#include "fl/stl/cstddef.h"
#include "fl/stl/cstddef.h"
#include "doctest.h"

using namespace fl;

///////////////////////////////////////////////////////////////////////////////
// Test suite for fl/cstddef.h
//
// This file tests the FastLED equivalents of stddef.h types:
// - fl::size_t
// - fl::ptrdiff_t
// - fl::nullptr_t
// - fl::max_align_t
// - FL_OFFSETOF macro
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("fl::size_t basic properties") {
    SUBCASE("size_t is unsigned") {
        fl::size_t zero = 0;
        fl::size_t one = 1;

        CHECK(zero < one);

        // size_t is unsigned, so decrementing 0 wraps around
        fl::size_t wrapped = zero - 1;
        CHECK(wrapped > zero);
    }

    SUBCASE("size_t can hold array sizes") {
        fl::size_t small = 10;
        fl::size_t medium = 1000;
        fl::size_t large = 100000;

        CHECK_EQ(small, 10);
        CHECK_EQ(medium, 1000);
        CHECK_EQ(large, 100000);
    }

    SUBCASE("size_t arithmetic") {
        fl::size_t a = 100;
        fl::size_t b = 50;

        CHECK_EQ(a + b, 150);
        CHECK_EQ(a - b, 50);
        CHECK_EQ(a * 2, 200);
        CHECK_EQ(a / 2, 50);
    }

    SUBCASE("size_t comparison") {
        fl::size_t a = 100;
        fl::size_t b = 200;
        fl::size_t c = 100;

        CHECK(a < b);
        CHECK(b > a);
        CHECK(a == c);
        CHECK(a != b);
        CHECK(a <= c);
        CHECK(a >= c);
    }
}

TEST_CASE("fl::ptrdiff_t basic properties") {
    SUBCASE("ptrdiff_t is signed") {
        fl::ptrdiff_t zero = 0;
        fl::ptrdiff_t positive = 100;
        fl::ptrdiff_t negative = -100;

        CHECK(positive > zero);
        CHECK(negative < zero);
        CHECK_EQ(positive + negative, 0);
    }

    SUBCASE("ptrdiff_t can represent pointer differences") {
        int arr[10];
        fl::ptrdiff_t diff = &arr[9] - &arr[0];

        CHECK_EQ(diff, 9);
    }

    SUBCASE("ptrdiff_t arithmetic") {
        fl::ptrdiff_t a = 100;
        fl::ptrdiff_t b = -50;

        CHECK_EQ(a + b, 50);
        CHECK_EQ(a - b, 150);
        CHECK_EQ(a * 2, 200);
        CHECK_EQ(a / 2, 50);
        CHECK_EQ(b * -1, 50);
    }

    SUBCASE("ptrdiff_t comparison") {
        fl::ptrdiff_t a = -100;
        fl::ptrdiff_t b = 100;
        fl::ptrdiff_t c = -100;

        CHECK(a < b);
        CHECK(b > a);
        CHECK(a == c);
        CHECK(a != b);
        CHECK(a <= c);
        CHECK(a >= c);
    }
}

TEST_CASE("fl::nullptr_t basic properties") {
    SUBCASE("nullptr_t can be assigned nullptr") {
        fl::nullptr_t n = nullptr;
        (void)n;  // Avoid unused variable warning

        // If this compiles, the test passes
        CHECK(true);
    }

    SUBCASE("nullptr_t can be compared with nullptr") {
        fl::nullptr_t n = nullptr;

        CHECK(n == nullptr);
        CHECK(nullptr == n);
    }

    SUBCASE("nullptr_t can be used to initialize pointers") {
        fl::nullptr_t n = nullptr;
        int* ptr = n;

        CHECK(ptr == nullptr);
    }

    SUBCASE("nullptr_t can be passed to functions expecting pointers") {
        auto check_null = [](int* p) -> bool {
            return p == nullptr;
        };

        fl::nullptr_t n = nullptr;
        CHECK(check_null(n));
    }
}

TEST_CASE("fl::max_align_t basic properties") {
    SUBCASE("max_align_t has sufficient alignment") {
        // max_align_t should have alignment suitable for any standard type
        fl::max_align_t m;
        (void)m;  // Avoid unused variable warning

        // Check that max_align_t is at least as large as its components
        CHECK(sizeof(fl::max_align_t) >= sizeof(long long));
        CHECK(sizeof(fl::max_align_t) >= sizeof(long double));
        CHECK(sizeof(fl::max_align_t) >= sizeof(void*));
    }

    SUBCASE("max_align_t alignment") {
        // Check alignment is at least as strict as long long
        CHECK(alignof(fl::max_align_t) >= alignof(long long));
        CHECK(alignof(fl::max_align_t) >= alignof(long double));
        CHECK(alignof(fl::max_align_t) >= alignof(void*));
    }

    SUBCASE("max_align_t can be used in arrays") {
        fl::max_align_t arr[10];
        (void)arr;  // Avoid unused variable warning

        CHECK_EQ(sizeof(arr), 10 * sizeof(fl::max_align_t));
    }

    SUBCASE("max_align_t union members can be accessed") {
        fl::max_align_t m;
        m.ll = 42;
        CHECK_EQ(m.ll, 42);

        m.p = nullptr;
        CHECK(m.p == nullptr);

        // Note: We don't check m.ld because it shares storage with other members
        // and would overwrite the previously set values
    }
}

TEST_CASE("FL_OFFSETOF macro") {
    struct SimpleStruct {
        char a;
        int b;
        double c;
    };

    SUBCASE("FL_OFFSETOF returns zero for first member") {
        fl::size_t offset = FL_OFFSETOF(SimpleStruct, a);
        CHECK_EQ(offset, 0);
    }

    SUBCASE("FL_OFFSETOF returns non-zero for subsequent members") {
        fl::size_t offset_b = FL_OFFSETOF(SimpleStruct, b);
        fl::size_t offset_c = FL_OFFSETOF(SimpleStruct, c);

        // b comes after a (which is 1 byte), but may be padded
        CHECK(offset_b > 0);

        // c comes after b (which is 4 bytes), but may be padded
        CHECK(offset_c > offset_b);
    }

    SUBCASE("FL_OFFSETOF correctly computes member offsets") {
        SimpleStruct s;

        // Compute expected offsets using pointer arithmetic
        fl::size_t expected_offset_a = reinterpret_cast<char*>(&s.a) - reinterpret_cast<char*>(&s);
        fl::size_t expected_offset_b = reinterpret_cast<char*>(&s.b) - reinterpret_cast<char*>(&s);
        fl::size_t expected_offset_c = reinterpret_cast<char*>(&s.c) - reinterpret_cast<char*>(&s);

        CHECK_EQ(FL_OFFSETOF(SimpleStruct, a), expected_offset_a);
        CHECK_EQ(FL_OFFSETOF(SimpleStruct, b), expected_offset_b);
        CHECK_EQ(FL_OFFSETOF(SimpleStruct, c), expected_offset_c);
    }

    SUBCASE("FL_OFFSETOF with nested structs") {
        struct Inner {
            int x;
            int y;
        };

        struct Outer {
            char a;
            Inner inner;
            double d;
        };

        fl::size_t offset_a = FL_OFFSETOF(Outer, a);
        fl::size_t offset_inner = FL_OFFSETOF(Outer, inner);
        fl::size_t offset_d = FL_OFFSETOF(Outer, d);

        CHECK_EQ(offset_a, 0);
        CHECK(offset_inner > 0);
        CHECK(offset_d > offset_inner);
    }

    SUBCASE("FL_OFFSETOF is compile-time constant") {
        // This test verifies that FL_OFFSETOF can be used in contexts
        // requiring compile-time constants
        constexpr fl::size_t offset = FL_OFFSETOF(SimpleStruct, b);
        static_assert(offset == FL_OFFSETOF(SimpleStruct, b), "FL_OFFSETOF should be constexpr");

        // Use the constexpr value
        CHECK(offset >= 0);
    }
}

TEST_CASE("fl::size_t and global size_t compatibility") {
    SUBCASE("fl::size_t and ::size_t are the same type") {
        // Both should refer to the same underlying type
        fl::size_t fl_size = 100;
        ::size_t global_size = fl_size;

        CHECK_EQ(global_size, 100);
        CHECK_EQ(sizeof(fl::size_t), sizeof(::size_t));
    }

    SUBCASE("can convert between fl::size_t and ::size_t") {
        fl::size_t fl_size = 42;
        ::size_t global_size = 84;

        global_size = fl_size;
        CHECK_EQ(global_size, 42);

        fl_size = global_size;
        CHECK_EQ(fl_size, 42);
    }
}

TEST_CASE("fl::ptrdiff_t and global ptrdiff_t compatibility") {
    SUBCASE("fl::ptrdiff_t and ::ptrdiff_t are the same type") {
        // Both should refer to the same underlying type
        fl::ptrdiff_t fl_diff = -100;
        ::ptrdiff_t global_diff = fl_diff;

        CHECK_EQ(global_diff, -100);
        CHECK_EQ(sizeof(fl::ptrdiff_t), sizeof(::ptrdiff_t));
    }

    SUBCASE("can convert between fl::ptrdiff_t and ::ptrdiff_t") {
        fl::ptrdiff_t fl_diff = 42;
        ::ptrdiff_t global_diff = -84;

        global_diff = fl_diff;
        CHECK_EQ(global_diff, 42);

        fl_diff = global_diff;
        CHECK_EQ(fl_diff, 42);
    }
}

TEST_CASE("type sizes and relationships") {
    SUBCASE("size_t is large enough to hold any array index") {
        // size_t should be able to represent the size of the largest possible array
        CHECK(sizeof(fl::size_t) >= sizeof(unsigned int));
    }

    SUBCASE("ptrdiff_t is large enough to hold pointer differences") {
        // ptrdiff_t should be the signed version of size_t
        // It should be able to represent the difference between any two pointers
        CHECK(sizeof(fl::ptrdiff_t) >= sizeof(int));

        // size_t and ptrdiff_t should have the same size (typically)
        // Note: This may not be true on all platforms, but is common
        CHECK(sizeof(fl::size_t) == sizeof(fl::ptrdiff_t));
    }

    SUBCASE("max_align_t provides strictest alignment") {
        // max_align_t should have the strictest alignment requirement
        // It should be suitable for any standard scalar type
        CHECK(alignof(fl::max_align_t) >= alignof(char));
        CHECK(alignof(fl::max_align_t) >= alignof(int));
        CHECK(alignof(fl::max_align_t) >= alignof(long));
        CHECK(alignof(fl::max_align_t) >= alignof(long long));
        CHECK(alignof(fl::max_align_t) >= alignof(float));
        CHECK(alignof(fl::max_align_t) >= alignof(double));
        CHECK(alignof(fl::max_align_t) >= alignof(void*));
    }
}

TEST_CASE("practical usage scenarios") {
    SUBCASE("size_t for array indexing") {
        constexpr fl::size_t array_size = 100;
        int arr[array_size];

        for (fl::size_t i = 0; i < array_size; ++i) {
            arr[i] = static_cast<int>(i);
        }

        CHECK_EQ(arr[0], 0);
        CHECK_EQ(arr[50], 50);
        CHECK_EQ(arr[99], 99);
    }

    SUBCASE("ptrdiff_t for pointer arithmetic") {
        int arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        int* start = arr;
        int* end = arr + 10;

        fl::ptrdiff_t count = end - start;
        CHECK_EQ(count, 10);

        int* middle = start + (count / 2);
        CHECK_EQ(*middle, 5);
    }

    SUBCASE("nullptr_t for null pointer semantics") {
        auto find_value = [](int* arr, fl::size_t size, int value) -> int* {
            for (fl::size_t i = 0; i < size; ++i) {
                if (arr[i] == value) {
                    return &arr[i];
                }
            }
            return nullptr;
        };

        int arr[5] = {10, 20, 30, 40, 50};

        int* found = find_value(arr, 5, 30);
        CHECK(found != nullptr);
        CHECK_EQ(*found, 30);

        int* not_found = find_value(arr, 5, 99);
        CHECK(not_found == nullptr);
    }

    SUBCASE("FL_OFFSETOF for struct introspection") {
        struct Point {
            int x;
            int y;
            int z;
        };

        // Calculate total size using offsetof and sizeof last member
        fl::size_t offset_z = FL_OFFSETOF(Point, z);
        fl::size_t expected_min_size = offset_z + sizeof(int);

        // Actual size may be larger due to padding
        CHECK(sizeof(Point) >= expected_min_size);
    }

    SUBCASE("max_align_t for aligned storage") {
        // Use max_align_t to create maximally-aligned storage
        fl::max_align_t storage[10];

        // This storage can be used for any type with placement new
        void* ptr = &storage[0];
        CHECK(ptr != nullptr);

        // Check that the storage is properly aligned
        CHECK(reinterpret_cast<fl::size_t>(ptr) % alignof(fl::max_align_t) == 0);
    }
}

TEST_CASE("edge cases and boundary conditions") {
    SUBCASE("size_t with zero") {
        fl::size_t zero = 0;
        CHECK_EQ(zero, 0);

        // Decrementing zero wraps around (unsigned behavior)
        fl::size_t wrapped = zero - 1;
        CHECK(wrapped > 0);
    }

    SUBCASE("ptrdiff_t with zero") {
        fl::ptrdiff_t zero = 0;
        CHECK_EQ(zero, 0);
        CHECK_EQ(-zero, 0);
    }

    SUBCASE("nullptr comparisons") {
        int* p1 = nullptr;
        int* p2 = nullptr;
        int value = 42;
        int* p3 = &value;

        CHECK(p1 == p2);
        CHECK(p1 == nullptr);
        CHECK(p2 == nullptr);
        CHECK(p3 != nullptr);
        CHECK(p3 != p1);
    }

    SUBCASE("FL_OFFSETOF with single-member struct") {
        struct Single {
            int value;
        };

        fl::size_t offset = FL_OFFSETOF(Single, value);
        CHECK_EQ(offset, 0);
    }
}

TEST_CASE("type conversions and casts") {
    SUBCASE("size_t to ptrdiff_t conversion") {
        fl::size_t s = 100;
        fl::ptrdiff_t d = static_cast<fl::ptrdiff_t>(s);

        CHECK_EQ(d, 100);
    }

    SUBCASE("ptrdiff_t to size_t conversion (positive values)") {
        fl::ptrdiff_t d = 100;
        fl::size_t s = static_cast<fl::size_t>(d);

        CHECK_EQ(s, 100);
    }

    SUBCASE("nullptr_t to pointer conversion") {
        fl::nullptr_t n = nullptr;
        int* ptr = static_cast<int*>(n);

        CHECK(ptr == nullptr);
    }
}
