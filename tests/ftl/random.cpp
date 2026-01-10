#include "fl/stl/random.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/vector.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/int.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "fl/stl/type_traits.h"

using namespace fl;

TEST_CASE("fl::fl_random - Basic construction and seeding") {
    SUBCASE("Default constructor") {
        fl_random rng;
        // Should create a valid generator
        u32 val1 = rng();
        u32 val2 = rng();
        // Random values should be different (statistically)
        CHECK(val1 != val2);
    }

    SUBCASE("Constructor with explicit seed") {
        fl_random rng1(12345);
        fl_random rng2(12345);

        // Same seed should produce same sequence
        CHECK_EQ(rng1(), rng2());
        CHECK_EQ(rng1(), rng2());
        CHECK_EQ(rng1(), rng2());
    }

    SUBCASE("Different seeds produce different sequences") {
        fl_random rng1(111);
        fl_random rng2(222);

        u32 val1 = rng1();
        u32 val2 = rng2();

        // Different seeds should produce different values
        CHECK(val1 != val2);
    }
}

TEST_CASE("fl::fl_random - Seed management") {
    SUBCASE("set_seed changes state") {
        fl_random rng1(100);
        fl_random rng2(200);

        u32 val1 = rng1();
        u32 val2_first = rng2();

        // Reset rng2 to same seed as rng1 started with
        rng2.set_seed(100);
        u32 val2_reset = rng2();

        // After setting same seed, should produce same first value
        CHECK_EQ(val1, val2_reset);
        CHECK(val2_first != val2_reset);
    }

    SUBCASE("get_seed returns current seed") {
        u16 initial_seed = 5555;
        fl_random rng(initial_seed);

        CHECK_EQ(rng.get_seed(), initial_seed);

        // Generate some numbers
        rng();
        rng();

        // Seed should have changed after generating numbers
        CHECK(rng.get_seed() != initial_seed);
    }

    SUBCASE("add_entropy modifies seed") {
        fl_random rng(1000);
        u16 original_seed = rng.get_seed();

        rng.add_entropy(500);
        u16 new_seed = rng.get_seed();

        CHECK_EQ(new_seed, original_seed + 500);
    }
}

TEST_CASE("fl::fl_random - Basic 32-bit generation") {
    SUBCASE("operator() generates values in valid range") {
        fl_random rng(9999);

        for (int i = 0; i < 100; ++i) {
            u32 val = rng();
            // Should be in range [0, maximum()]
            CHECK(val >= fl_random::minimum());
            CHECK(val <= fl_random::maximum());
        }
    }

    SUBCASE("minimum() and maximum() constants") {
        CHECK_EQ(fl_random::minimum(), 0U);
        CHECK_EQ(fl_random::maximum(), 4294967295U);
    }

    SUBCASE("Generates well-distributed values") {
        fl_random rng(7777);

        // Generate many values and check they're reasonably distributed
        int low_count = 0;
        int high_count = 0;
        u32 midpoint = fl_random::maximum() / 2;

        for (int i = 0; i < 1000; ++i) {
            u32 val = rng();
            if (val < midpoint) {
                low_count++;
            } else {
                high_count++;
            }
        }

        // Should be roughly balanced (allow 30% deviation)
        CHECK(low_count > 300);
        CHECK(high_count > 300);
    }
}

TEST_CASE("fl::fl_random - Bounded 32-bit generation") {
    SUBCASE("operator()(n) generates values in [0, n)") {
        fl_random rng(4444);
        u32 bound = 100;

        for (int i = 0; i < 100; ++i) {
            u32 val = rng(bound);
            CHECK(val < bound);
        }
    }

    SUBCASE("operator()(n) with n=0 returns 0") {
        fl_random rng(3333);
        CHECK_EQ(rng(0), 0U);
    }

    SUBCASE("operator()(n) with n=1 returns 0") {
        fl_random rng(2222);
        CHECK_EQ(rng(1), 0U);
    }

    SUBCASE("operator()(min, max) generates values in [min, max)") {
        fl_random rng(8888);
        u32 min_val = 50;
        u32 max_val = 150;

        for (int i = 0; i < 100; ++i) {
            u32 val = rng(min_val, max_val);
            CHECK(val >= min_val);
            CHECK(val < max_val);
        }
    }

    SUBCASE("operator()(min, max) with min=max returns min") {
        fl_random rng(1111);
        u32 val = 42;
        CHECK_EQ(rng(val, val), val);
    }
}

TEST_CASE("fl::fl_random - 8-bit generation") {
    SUBCASE("random8() generates values in [0, 255]") {
        fl_random rng(6666);

        for (int i = 0; i < 100; ++i) {
            u8 val = rng.random8();
            // All u8 values are valid, just check it compiles and runs
            (void)val;
        }
    }

    SUBCASE("random8(n) generates values in [0, n)") {
        fl_random rng(5555);
        u8 bound = 50;

        for (int i = 0; i < 100; ++i) {
            u8 val = rng.random8(bound);
            CHECK(val < bound);
        }
    }

    SUBCASE("random8(n) with n=0 returns 0") {
        fl_random rng(4444);
        CHECK_EQ(rng.random8(0), 0);
    }

    SUBCASE("random8(min, max) generates values in [min, max)") {
        fl_random rng(3333);
        u8 min_val = 10;
        u8 max_val = 50;

        for (int i = 0; i < 100; ++i) {
            u8 val = rng.random8(min_val, max_val);
            CHECK(val >= min_val);
            CHECK(val < max_val);
        }
    }

    SUBCASE("random8(min, max) with min=max returns min") {
        fl_random rng(2222);
        u8 val = 42;
        CHECK_EQ(rng.random8(val, val), val);
    }

    SUBCASE("random8() distribution") {
        fl_random rng(9999);

        // Check distribution
        int low_count = 0;
        int high_count = 0;

        for (int i = 0; i < 1000; ++i) {
            u8 val = rng.random8();
            if (val < 128) {
                low_count++;
            } else {
                high_count++;
            }
        }

        // Should be roughly balanced
        CHECK(low_count > 300);
        CHECK(high_count > 300);
    }
}

TEST_CASE("fl::fl_random - 16-bit generation") {
    SUBCASE("random16() generates values in [0, 65535]") {
        fl_random rng(7777);

        for (int i = 0; i < 100; ++i) {
            u16 val = rng.random16();
            // All u16 values are valid
            (void)val;
        }
    }

    SUBCASE("random16(n) generates values in [0, n)") {
        fl_random rng(6666);
        u16 bound = 1000;

        for (int i = 0; i < 100; ++i) {
            u16 val = rng.random16(bound);
            CHECK(val < bound);
        }
    }

    SUBCASE("random16(n) with n=0 returns 0") {
        fl_random rng(5555);
        CHECK_EQ(rng.random16(0), 0);
    }

    SUBCASE("random16(min, max) generates values in [min, max)") {
        fl_random rng(4444);
        u16 min_val = 100;
        u16 max_val = 500;

        for (int i = 0; i < 100; ++i) {
            u16 val = rng.random16(min_val, max_val);
            CHECK(val >= min_val);
            CHECK(val < max_val);
        }
    }

    SUBCASE("random16(min, max) with min=max returns min") {
        fl_random rng(3333);
        u16 val = 12345;
        CHECK_EQ(rng.random16(val, val), val);
    }

    SUBCASE("random16() distribution") {
        fl_random rng(8888);

        // Check distribution
        int low_count = 0;
        int high_count = 0;

        for (int i = 0; i < 1000; ++i) {
            u16 val = rng.random16();
            if (val < 32768) {
                low_count++;
            } else {
                high_count++;
            }
        }

        // Should be roughly balanced
        CHECK(low_count > 300);
        CHECK(high_count > 300);
    }
}

TEST_CASE("fl::fl_random - Reproducibility") {
    SUBCASE("Same seed produces identical sequences") {
        fl_random rng1(54321);
        fl_random rng2(54321);

        // Test operator()
        for (int i = 0; i < 20; ++i) {
            CHECK_EQ(rng1(), rng2());
        }
    }

    SUBCASE("Same seed produces identical random8 sequences") {
        fl_random rng1(11111);
        fl_random rng2(11111);

        for (int i = 0; i < 20; ++i) {
            CHECK_EQ(rng1.random8(), rng2.random8());
        }
    }

    SUBCASE("Same seed produces identical random16 sequences") {
        fl_random rng1(22222);
        fl_random rng2(22222);

        for (int i = 0; i < 20; ++i) {
            CHECK_EQ(rng1.random16(), rng2.random16());
        }
    }

    SUBCASE("Resetting seed produces identical sequence again") {
        fl_random rng(33333);

        fl::vector<u32> first_sequence;
        for (int i = 0; i < 10; ++i) {
            first_sequence.push_back(rng());
        }

        // Reset to same seed
        rng.set_seed(33333);

        for (int i = 0; i < 10; ++i) {
            CHECK_EQ(rng(), first_sequence[i]);
        }
    }
}

TEST_CASE("fl::fl_random - Edge cases") {
    SUBCASE("Maximum bound values") {
        fl_random rng(9999);

        // Test with maximum u8 bound
        u8 val8 = rng.random8(255);
        CHECK(val8 < 255);

        // Test with maximum u16 bound
        u16 val16 = rng.random16(65535);
        CHECK(val16 < 65535);

        // Test with large u32 bound
        u32 val32 = rng(4294967295U);
        CHECK(val32 < 4294967295U);
    }

    SUBCASE("Alternating method calls") {
        fl_random rng(12121);

        // Mix different methods to ensure they all work together
        u32 v1 = rng();
        u8 v2 = rng.random8();
        u16 v3 = rng.random16();
        u32 v4 = rng(100);
        u8 v5 = rng.random8(50);
        u16 v6 = rng.random16(1000);

        // Just ensure they compile and run without crashing
        (void)v1; (void)v2; (void)v3; (void)v4; (void)v5; (void)v6;
    }
}

TEST_CASE("fl::fl_random - Integration with algorithms") {
    SUBCASE("Can be used with fl::shuffle") {
        fl_random rng(7777);

        fl::vector<int> vec;
        for (int i = 0; i < 10; ++i) {
            vec.push_back(i);
        }

        fl::vector<int> original = vec;

        // Shuffle using our random generator
        fl::shuffle(vec.begin(), vec.end(), rng);

        // Vector should have same elements but likely different order
        // With 10 elements, probability of same order is 1/10! which is tiny
        // But we can't guarantee it's different, so just verify size is same
        CHECK_EQ(vec.size(), original.size());

        // Verify all elements are still present
        fl::sort(vec.begin(), vec.end());
        fl::sort(original.begin(), original.end());
        CHECK(vec == original);
    }

    SUBCASE("Multiple independent generators") {
        fl_random rng1(100);
        fl_random rng2(200);
        fl_random rng3(100); // Same as rng1

        // rng1 and rng3 should produce same values
        CHECK_EQ(rng1(), rng3());

        // rng2 should produce different values
        u32 val2 = rng2();
        CHECK(rng1() != val2); // Very likely to be different
    }
}

TEST_CASE("fl::default_random - Global instance") {
    SUBCASE("default_random() returns valid generator") {
        fl_random& rng = default_random();

        u32 val1 = rng();
        u32 val2 = rng();

        // Should generate different values
        CHECK(val1 != val2);
    }

    SUBCASE("default_random() returns same instance") {
        fl_random& rng1 = default_random();
        fl_random& rng2 = default_random();

        // Should be the same object
        CHECK_EQ(&rng1, &rng2);
    }

    SUBCASE("Can use default_random with algorithms") {
        fl::vector<int> vec;
        for (int i = 0; i < 5; ++i) {
            vec.push_back(i);
        }

        // Should compile and run without errors
        fl::shuffle(vec.begin(), vec.end(), default_random());

        CHECK_EQ(vec.size(), 5U);
    }
}

TEST_CASE("fl::fl_random - Type traits") {
    SUBCASE("result_type is u32") {
        CHECK(fl::is_same<fl_random::result_type, u32>::value);
    }

    SUBCASE("Constexpr minimum and maximum") {
        // These should be usable in constant expressions
        static_assert(fl_random::minimum() == 0, "minimum should be 0");
        static_assert(fl_random::maximum() == 4294967295U, "maximum should be u32 max");
    }
}
