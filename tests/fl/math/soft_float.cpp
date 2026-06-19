// Unit tests for fl::soft_float -- integer-only IEEE-754 float<->double
// bit-codec. See FastLED #3022 / #3002.

#include "test.h"

#include "fl/math/soft_float.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/cstddef.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

static double widen_via_bitcodec(float v) {
    return fl::soft_float_to_double(v);
}

static double widen_via_native(float v) {
    return static_cast<double>(v);
}

static float narrow_via_bitcodec(double v) {
    return fl::soft_double_to_float(v);
}

static float narrow_via_native(double v) {
    return static_cast<float>(v);
}

// ---- widening: float -> double ----

FL_TEST_CASE("soft_float_to_double -- zero round-trip") {
    FL_CHECK_EQ(widen_via_bitcodec(0.0f), widen_via_native(0.0f));
    FL_CHECK_EQ(widen_via_bitcodec(-0.0f), widen_via_native(-0.0f));
    FL_CHECK_EQ(fl::bit_cast<u64>(widen_via_bitcodec(0.0f)),  0x0000000000000000ull);
    FL_CHECK_EQ(fl::bit_cast<u64>(widen_via_bitcodec(-0.0f)), 0x8000000000000000ull);
}

FL_TEST_CASE("soft_float_to_double -- well-known normals") {
    FL_CHECK_EQ(widen_via_bitcodec(1.0f), 1.0);
    FL_CHECK_EQ(widen_via_bitcodec(-1.0f), -1.0);
    FL_CHECK_EQ(widen_via_bitcodec(2.0f), 2.0);
    FL_CHECK_EQ(widen_via_bitcodec(0.5f), 0.5);
    FL_CHECK_EQ(widen_via_bitcodec(0.125f), 0.125);
    FL_CHECK_EQ(widen_via_bitcodec(3.14f), widen_via_native(3.14f));
}

FL_TEST_CASE("soft_float_to_double -- subnormal floats become normal doubles") {
    const u32 smallest_subnormal_bits = 0x00000001u;
    const float subnormal = fl::bit_cast<float>(smallest_subnormal_bits);

    const double via_codec = widen_via_bitcodec(subnormal);
    const double via_native = widen_via_native(subnormal);
    FL_CHECK_EQ(fl::bit_cast<u64>(via_codec), fl::bit_cast<u64>(via_native));

    const float largest_subnormal = fl::bit_cast<float>(0x007FFFFFu);
    FL_CHECK_EQ(fl::bit_cast<u64>(widen_via_bitcodec(largest_subnormal)),
                fl::bit_cast<u64>(widen_via_native(largest_subnormal)));
}

FL_TEST_CASE("soft_float_to_double -- infinities and NaN") {
    const float pos_inf = fl::bit_cast<float>(0x7F800000u);
    const float neg_inf = fl::bit_cast<float>(0xFF800000u);
    const float qnan    = fl::bit_cast<float>(0x7FC00000u);

    FL_CHECK_EQ(fl::bit_cast<u64>(widen_via_bitcodec(pos_inf)),
                0x7FF0000000000000ull);
    FL_CHECK_EQ(fl::bit_cast<u64>(widen_via_bitcodec(neg_inf)),
                0xFFF0000000000000ull);
    // NaN canonicalization matches the narrowing path.
    FL_CHECK_EQ(fl::bit_cast<u64>(widen_via_bitcodec(qnan)),
                0x7FF8000000000000ull);
}

FL_TEST_CASE("soft_float_to_double -- max / min finite") {
    const float max_finite = fl::bit_cast<float>(0x7F7FFFFFu);  // FLT_MAX
    const float min_normal = fl::bit_cast<float>(0x00800000u);  // FLT_MIN
    FL_CHECK_EQ(fl::bit_cast<u64>(widen_via_bitcodec(max_finite)),
                fl::bit_cast<u64>(widen_via_native(max_finite)));
    FL_CHECK_EQ(fl::bit_cast<u64>(widen_via_bitcodec(min_normal)),
                fl::bit_cast<u64>(widen_via_native(min_normal)));
}

// ---- narrowing: double -> float ----

FL_TEST_CASE("soft_double_to_float -- zero round-trip") {
    FL_CHECK_EQ(narrow_via_bitcodec(0.0), narrow_via_native(0.0));
    FL_CHECK_EQ(narrow_via_bitcodec(-0.0), narrow_via_native(-0.0));
}

FL_TEST_CASE("soft_double_to_float -- well-known normals") {
    FL_CHECK_EQ(narrow_via_bitcodec(1.0), 1.0f);
    FL_CHECK_EQ(narrow_via_bitcodec(0.5), 0.5f);
    FL_CHECK_EQ(narrow_via_bitcodec(0.125), 0.125f);
    // 3.14 widened then narrowed loses precision identically.
    FL_CHECK_EQ(narrow_via_bitcodec(3.14), narrow_via_native(3.14));
}

// ---- round-trip ----

FL_TEST_CASE("soft_float widen/narrow round-trip is identity") {
    const u32 sample_bits[] = {
        0x00000000u,  // +0
        0x80000000u,  // -0
        0x3F800000u,  // 1.0
        0xBF800000u,  // -1.0
        0x40490FDBu,  // pi (single)
        0x4048F5C3u,  // 3.14
        0x3DCCCCCDu,  // 0.1
        0x00000001u,  // smallest subnormal
        0x007FFFFFu,  // largest subnormal
        0x00800000u,  // FLT_MIN
        0x7F7FFFFFu,  // FLT_MAX
        0x7F800000u,  // +inf
        0xFF800000u,  // -inf
    };
    for (u32 bits : sample_bits) {
        const float f = fl::bit_cast<float>(bits);
        const double widened = fl::soft_float_to_double(f);
        const float narrowed = fl::soft_double_to_float(widened);
        FL_CHECK_EQ(fl::bit_cast<u32>(narrowed), bits);
    }
}

} // FL_TEST_FILE
