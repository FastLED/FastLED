#include "fl/map_range.h"
#include "fl/geometry.h"
#include "doctest.h"
#include "fl/int.h"

using namespace fl;

TEST_CASE("fl::map_range - basic functionality") {
    SUBCASE("integer mapping") {
        // Map 0-100 to 0-1000
        CHECK_EQ(map_range(0, 0, 100, 0, 1000), 0);
        CHECK_EQ(map_range(50, 0, 100, 0, 1000), 500);
        CHECK_EQ(map_range(100, 0, 100, 0, 1000), 1000);
        CHECK_EQ(map_range(25, 0, 100, 0, 1000), 250);
        CHECK_EQ(map_range(75, 0, 100, 0, 1000), 750);
    }

    SUBCASE("negative range mapping") {
        // Map -100 to 100 to 0 to 200
        CHECK_EQ(map_range(-100, -100, 100, 0, 200), 0);
        CHECK_EQ(map_range(0, -100, 100, 0, 200), 100);
        CHECK_EQ(map_range(100, -100, 100, 0, 200), 200);
        CHECK_EQ(map_range(-50, -100, 100, 0, 200), 50);
    }

    SUBCASE("reverse mapping") {
        // Map 0-100 to 1000-0 (reversed output range)
        CHECK_EQ(map_range(0, 0, 100, 1000, 0), 1000);
        CHECK_EQ(map_range(50, 0, 100, 1000, 0), 500);
        CHECK_EQ(map_range(100, 0, 100, 1000, 0), 0);
    }

    SUBCASE("float to float mapping") {
        CHECK(doctest::Approx(map_range(0.0f, 0.0f, 1.0f, 0.0f, 100.0f)).epsilon(0.001) == 0.0f);
        CHECK(doctest::Approx(map_range(0.5f, 0.0f, 1.0f, 0.0f, 100.0f)).epsilon(0.001) == 50.0f);
        CHECK(doctest::Approx(map_range(1.0f, 0.0f, 1.0f, 0.0f, 100.0f)).epsilon(0.001) == 100.0f);
        CHECK(doctest::Approx(map_range(0.25f, 0.0f, 1.0f, 0.0f, 100.0f)).epsilon(0.001) == 25.0f);
    }

    SUBCASE("double to double mapping") {
        CHECK(doctest::Approx(map_range(0.0, 0.0, 1.0, 0.0, 100.0)).epsilon(0.001) == 0.0);
        CHECK(doctest::Approx(map_range(0.5, 0.0, 1.0, 0.0, 100.0)).epsilon(0.001) == 50.0);
        CHECK(doctest::Approx(map_range(1.0, 0.0, 1.0, 0.0, 100.0)).epsilon(0.001) == 100.0);
    }

    SUBCASE("int to float mapping") {
        CHECK(doctest::Approx(map_range(0, 0, 100, 0.0f, 1.0f)).epsilon(0.001) == 0.0f);
        CHECK(doctest::Approx(map_range(50, 0, 100, 0.0f, 1.0f)).epsilon(0.001) == 0.5f);
        CHECK(doctest::Approx(map_range(100, 0, 100, 0.0f, 1.0f)).epsilon(0.001) == 1.0f);
    }
}

TEST_CASE("fl::map_range - u8 specialization") {
    SUBCASE("basic u8 mapping") {
        CHECK_EQ(map_range<u8, u8>(0, 0, 255, 0, 100), 0);
        CHECK_EQ(map_range<u8, u8>(255, 0, 255, 0, 100), 100);
        CHECK_EQ(map_range<u8, u8>(127, 0, 255, 0, 100), 49);  // ~50
    }

    SUBCASE("u8 partial range") {
        CHECK_EQ(map_range<u8, u8>(0, 0, 100, 0, 255), 0);
        CHECK_EQ(map_range<u8, u8>(50, 0, 100, 0, 255), 127);
        CHECK_EQ(map_range<u8, u8>(100, 0, 100, 0, 255), 255);
    }

    SUBCASE("u8 clamping - underflow") {
        // The u8 specialization clamps to 0 if result would be negative
        u8 result = map_range<u8, u8>(10, 50, 100, 0, 255);
        CHECK(result >= 0);  // Should be clamped to minimum
    }

    SUBCASE("u8 clamping - overflow") {
        // The u8 specialization clamps to 255 if result would exceed 255
        u8 result = map_range<u8, u8>(200, 0, 100, 0, 255);
        CHECK_EQ(result, 255);
    }

    SUBCASE("u8 edge values") {
        CHECK_EQ(map_range<u8, u8>(0, 0, 0, 100, 200), 100);  // in_min == in_max
        CHECK_EQ(map_range<u8, u8>(5, 0, 0, 100, 200), 100);  // in_min == in_max
    }
}

TEST_CASE("fl::map_range - u16 specialization") {
    SUBCASE("basic u16 mapping") {
        CHECK_EQ(map_range<u16, u16>(0, 0, 65535, 0, 1000), 0);
        CHECK_EQ(map_range<u16, u16>(65535, 0, 65535, 0, 1000), 1000);
        CHECK_EQ(map_range<u16, u16>(32767, 0, 65535, 0, 1000), 499);  // ~500
    }

    SUBCASE("u16 partial range") {
        CHECK_EQ(map_range<u16, u16>(0, 0, 1000, 0, 65535), 0);
        CHECK_EQ(map_range<u16, u16>(500, 0, 1000, 0, 65535), 32767);
        CHECK_EQ(map_range<u16, u16>(1000, 0, 1000, 0, 65535), 65535);
    }

    SUBCASE("u16 overflow protection") {
        // The u16 specialization prevents overflow by promoting to u32
        u16 result = map_range<u16, u16>(60000, 0, 65535, 0, 65535);
        CHECK(result >= 0);
        CHECK(result <= 65535);
    }
}

TEST_CASE("fl::map_range - edge cases") {
    SUBCASE("exact min boundary") {
        // When value equals in_min, should return out_min exactly
        CHECK_EQ(map_range(100, 100, 200, 1000, 2000), 1000);
        CHECK_EQ(map_range(0.0f, 0.0f, 1.0f, 50.0f, 100.0f), 50.0f);
    }

    SUBCASE("exact max boundary") {
        // When value equals in_max, should return out_max exactly
        CHECK_EQ(map_range(200, 100, 200, 1000, 2000), 2000);
        CHECK_EQ(map_range(1.0f, 0.0f, 1.0f, 50.0f, 100.0f), 100.0f);
    }

    SUBCASE("single point range") {
        // When in_min == in_max, should return out_min
        CHECK_EQ(map_range(5, 5, 5, 100, 200), 100);
        CHECK_EQ(map_range(10, 5, 5, 100, 200), 100);
        CHECK(doctest::Approx(map_range(5.0f, 5.0f, 5.0f, 100.0f, 200.0f)).epsilon(0.001) == 100.0f);
    }

    SUBCASE("value out of input range") {
        // map_range doesn't clamp input, so values outside the range extrapolate
        CHECK_EQ(map_range(150, 0, 100, 0, 1000), 1500);  // Extrapolates beyond range
        CHECK_EQ(map_range(-50, 0, 100, 0, 1000), -500);  // Extrapolates below range
    }
}

TEST_CASE("fl::map_range_clamped") {
    SUBCASE("value within range") {
        CHECK_EQ(map_range_clamped(50, 0, 100, 0, 1000), 500);
        CHECK_EQ(map_range_clamped(25, 0, 100, 0, 1000), 250);
    }

    SUBCASE("value below range - clamped to min") {
        CHECK_EQ(map_range_clamped(-50, 0, 100, 0, 1000), 0);
        CHECK_EQ(map_range_clamped(-1, 0, 100, 0, 1000), 0);
    }

    SUBCASE("value above range - clamped to max") {
        CHECK_EQ(map_range_clamped(150, 0, 100, 0, 1000), 1000);
        CHECK_EQ(map_range_clamped(200, 0, 100, 0, 1000), 1000);
    }

    SUBCASE("float clamping") {
        CHECK(doctest::Approx(map_range_clamped(-0.5f, 0.0f, 1.0f, 0.0f, 100.0f)).epsilon(0.001) == 0.0f);
        CHECK(doctest::Approx(map_range_clamped(1.5f, 0.0f, 1.0f, 0.0f, 100.0f)).epsilon(0.001) == 100.0f);
        CHECK(doctest::Approx(map_range_clamped(0.5f, 0.0f, 1.0f, 0.0f, 100.0f)).epsilon(0.001) == 50.0f);
    }

    SUBCASE("u8 clamped") {
        CHECK_EQ(map_range_clamped<u8, u8>(200, 0, 100, 0, 255), 255);
        CHECK_EQ(map_range_clamped<u8, u8>(50, 0, 100, 0, 255), 127);
    }
}

TEST_CASE("fl::map_range - vec2 specialization") {
    SUBCASE("basic vec2 mapping") {
        vec2<float> out_min = {0.0f, 0.0f};
        vec2<float> out_max = {100.0f, 200.0f};

        auto result = map_range(0.0f, 0.0f, 1.0f, out_min, out_max);
        CHECK(doctest::Approx(result.x).epsilon(0.001) == 0.0f);
        CHECK(doctest::Approx(result.y).epsilon(0.001) == 0.0f);

        result = map_range(1.0f, 0.0f, 1.0f, out_min, out_max);
        CHECK(doctest::Approx(result.x).epsilon(0.001) == 100.0f);
        CHECK(doctest::Approx(result.y).epsilon(0.001) == 200.0f);

        result = map_range(0.5f, 0.0f, 1.0f, out_min, out_max);
        CHECK(doctest::Approx(result.x).epsilon(0.001) == 50.0f);
        CHECK(doctest::Approx(result.y).epsilon(0.001) == 100.0f);
    }

    SUBCASE("vec2 with different component ranges") {
        vec2<float> out_min = {-100.0f, -200.0f};
        vec2<float> out_max = {100.0f, 200.0f};

        // When input is at midpoint (0), output should be midpoint of range
        auto result = map_range(0.0f, -10.0f, 10.0f, out_min, out_max);
        CHECK(doctest::Approx(result.x).epsilon(0.1) == 0.0f);
        CHECK(doctest::Approx(result.y).epsilon(0.1) == 0.0f);

        // When input is at min, output should be out_min
        result = map_range(-10.0f, -10.0f, 10.0f, out_min, out_max);
        CHECK(doctest::Approx(result.x).epsilon(0.1) == -100.0f);
        CHECK(doctest::Approx(result.y).epsilon(0.1) == -200.0f);

        // When input is at max, output should be out_max
        result = map_range(10.0f, -10.0f, 10.0f, out_min, out_max);
        CHECK(doctest::Approx(result.x).epsilon(0.1) == 100.0f);
        CHECK(doctest::Approx(result.y).epsilon(0.1) == 200.0f);
    }

    SUBCASE("vec2 single point range") {
        vec2<float> out_min = {10.0f, 20.0f};
        vec2<float> out_max = {100.0f, 200.0f};

        auto result = map_range(5.0f, 5.0f, 5.0f, out_min, out_max);
        CHECK(doctest::Approx(result.x).epsilon(0.001) == 10.0f);
        CHECK(doctest::Approx(result.y).epsilon(0.001) == 20.0f);
    }
}

TEST_CASE("fl::map_range - float equality handling") {
    SUBCASE("float boundary equality uses epsilon") {
        // The map_range_detail::equals function uses FL_ALMOST_EQUAL_FLOAT
        // This ensures that values very close to boundaries are treated correctly
        float epsilon = 0.0000001f;
        float result = map_range(1.0f + epsilon, 1.0f, 2.0f, 100.0f, 200.0f);
        // Should be very close to 100.0f if epsilon comparison works
        CHECK(result >= 99.9f);
        CHECK(result <= 100.1f);
    }

    SUBCASE("double boundary equality uses epsilon") {
        double epsilon = 0.00000000001;
        double result = map_range(1.0 + epsilon, 1.0, 2.0, 100.0, 200.0);
        CHECK(result >= 99.99);
        CHECK(result <= 100.01);
    }
}

TEST_CASE("fl::map_range - different type combinations") {
    SUBCASE("i16 to i32") {
        i16 in_val = 100;
        i32 result = map_range(in_val, i16(0), i16(1000), i32(0), i32(1000000));
        CHECK_EQ(result, 100000);
    }

    SUBCASE("u32 to u32") {
        u32 result = map_range(500000u, 0u, 1000000u, 0u, 100u);
        CHECK_EQ(result, 50u);
    }

    SUBCASE("float to int") {
        int result = map_range(0.5f, 0.0f, 1.0f, 0, 100);
        CHECK_EQ(result, 50);
    }
}
