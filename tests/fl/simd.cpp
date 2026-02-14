// Test for fl::simd atomic operations

#include "fl/simd.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/fixed_point/s0x32x4.h"
#include "fl/fixed_point/s16x16x4.h"
#include "test.h"

using namespace fl;

//==============================================================================
// Atomic Load/Store Tests
//==============================================================================

FL_TEST_CASE("load_u8_16 loads 16 bytes correctly") {
    uint8_t src[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    uint8_t dst[16] = {0};

    auto vec = simd::load_u8_16(src);
    simd::store_u8_16(dst, vec);

    for (int i = 0; i < 16; ++i) {
        FL_REQUIRE(dst[i] == src[i]);
    }
}

FL_TEST_CASE("load_u32_4 loads 4 uint32_t correctly") {
    uint32_t src[4] = {0x12345678, 0xABCDEF00, 0xDEADBEEF, 0xCAFEBABE};
    uint32_t dst[4] = {0};

    auto vec = simd::load_u32_4(src);
    simd::store_u32_4(dst, vec);

    for (int i = 0; i < 4; ++i) {
        FL_REQUIRE(dst[i] == src[i]);
    }
}

FL_TEST_CASE("store_u8_16 stores 16 bytes correctly") {
    uint8_t buffer[32] = {0};  // Extra space to check bounds
    uint8_t pattern[16] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160};

    auto vec = simd::load_u8_16(pattern);
    simd::store_u8_16(&buffer[8], vec);  // Store in middle

    // Check pattern stored correctly
    for (int i = 0; i < 16; ++i) {
        FL_REQUIRE(buffer[8 + i] == pattern[i]);
    }

    // Check boundaries not overwritten
    for (int i = 0; i < 8; ++i) {
        FL_REQUIRE(buffer[i] == 0);
        FL_REQUIRE(buffer[24 + i] == 0);
    }
}

FL_TEST_CASE("store_u32_4 stores 4 uint32_t correctly") {
    uint32_t buffer[8] = {0};  // Extra space to check bounds
    uint32_t pattern[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};

    auto vec = simd::load_u32_4(pattern);
    simd::store_u32_4(&buffer[2], vec);  // Store in middle

    // Check pattern stored correctly
    for (int i = 0; i < 4; ++i) {
        FL_REQUIRE(buffer[2 + i] == pattern[i]);
    }

    // Check boundaries not overwritten
    FL_REQUIRE(buffer[0] == 0);
    FL_REQUIRE(buffer[1] == 0);
    FL_REQUIRE(buffer[6] == 0);
    FL_REQUIRE(buffer[7] == 0);
}

//==============================================================================
// Arithmetic Operation Tests
//==============================================================================

FL_TEST_CASE("add_sat_u8_16 adds without overflow") {
    uint8_t a[16] = {100, 150, 200, 255, 0, 50, 100, 150, 200, 255, 0, 50, 100, 150, 200, 255};
    uint8_t b[16] = { 50, 100, 150, 200, 0, 50, 100, 150, 200, 255, 0, 50, 100, 150, 200, 255};
    uint8_t dst[16];

    auto va = simd::load_u8_16(a);
    auto vb = simd::load_u8_16(b);
    auto result = simd::add_sat_u8_16(va, vb);
    simd::store_u8_16(dst, result);

    // Verify saturating addition
    FL_REQUIRE(dst[0] == 150);   // 100 + 50 = 150
    FL_REQUIRE(dst[1] == 250);   // 150 + 100 = 250
    FL_REQUIRE(dst[2] == 255);   // 200 + 150 = 350 -> saturate to 255
    FL_REQUIRE(dst[3] == 255);   // 255 + 200 = 455 -> saturate to 255
    FL_REQUIRE(dst[4] == 0);     // 0 + 0 = 0
    FL_REQUIRE(dst[5] == 100);   // 50 + 50 = 100
}

FL_TEST_CASE("add_sat_u8_16 handles edge cases") {
    // All zeros
    {
        uint8_t a[16] = {0};
        uint8_t b[16] = {0};
        uint8_t dst[16];

        auto va = simd::load_u8_16(a);
        auto vb = simd::load_u8_16(b);
        auto result = simd::add_sat_u8_16(va, vb);
        simd::store_u8_16(dst, result);

        for (int i = 0; i < 16; ++i) {
            FL_REQUIRE(dst[i] == 0);
        }
    }

    // All max values
    {
        uint8_t a[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
        uint8_t b[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
        uint8_t dst[16];

        auto va = simd::load_u8_16(a);
        auto vb = simd::load_u8_16(b);
        auto result = simd::add_sat_u8_16(va, vb);
        simd::store_u8_16(dst, result);

        for (int i = 0; i < 16; ++i) {
            FL_REQUIRE(dst[i] == 255);
        }
    }
}

FL_TEST_CASE("scale_u8_16 scales values correctly") {
    uint8_t src[16] = {0, 64, 128, 192, 255, 100, 200, 50, 10, 20, 30, 40, 60, 80, 120, 160};
    uint8_t dst[16];

    // Scale by 128 (0.5x)
    auto vec = simd::load_u8_16(src);
    auto result = simd::scale_u8_16(vec, 128);
    simd::store_u8_16(dst, result);

    FL_REQUIRE(dst[0] == 0);      // 0 * 128/256 = 0
    FL_REQUIRE(dst[1] == 32);     // 64 * 128/256 = 32
    FL_REQUIRE(dst[2] == 64);     // 128 * 128/256 = 64
    FL_REQUIRE(dst[3] == 96);     // 192 * 128/256 = 96
    FL_REQUIRE(dst[4] == 127);    // 255 * 128/256 = 127
}

FL_TEST_CASE("scale_u8_16 handles identity scaling") {
    uint8_t src[16] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160};
    uint8_t dst[16];

    // Scale by 255 (should be ~1.0x)
    auto vec = simd::load_u8_16(src);
    auto result = simd::scale_u8_16(vec, 255);
    simd::store_u8_16(dst, result);

    // Values should be very close to original (within 1 due to rounding)
    for (int i = 0; i < 16; ++i) {
        FL_REQUIRE(dst[i] >= src[i] - 1);
        FL_REQUIRE(dst[i] <= src[i]);
    }
}

FL_TEST_CASE("scale_u8_16 handles zero scaling") {
    uint8_t src[16] = {100, 150, 200, 255, 50, 75, 125, 175, 10, 20, 30, 40, 60, 80, 120, 160};
    uint8_t dst[16];

    // Scale by 0 (should zero everything)
    auto vec = simd::load_u8_16(src);
    auto result = simd::scale_u8_16(vec, 0);
    simd::store_u8_16(dst, result);

    for (int i = 0; i < 16; ++i) {
        FL_REQUIRE(dst[i] == 0);
    }
}

FL_TEST_CASE("set1_u32_4 broadcasts value to all lanes") {
    uint32_t dst[4];
    uint32_t pattern = 0xDEADBEEF;

    auto vec = simd::set1_u32_4(pattern);
    simd::store_u32_4(dst, vec);

    for (int i = 0; i < 4; ++i) {
        FL_REQUIRE(dst[i] == pattern);
    }
}

FL_TEST_CASE("set1_u32_4 works with different patterns") {
    uint32_t dst[4];

    // Test with 0xFFFFFFFF
    {
        auto vec = simd::set1_u32_4(0xFFFFFFFF);
        simd::store_u32_4(dst, vec);
        for (int i = 0; i < 4; ++i) {
            FL_REQUIRE(dst[i] == 0xFFFFFFFF);
        }
    }

    // Test with 0x00000000
    {
        auto vec = simd::set1_u32_4(0x00000000);
        simd::store_u32_4(dst, vec);
        for (int i = 0; i < 4; ++i) {
            FL_REQUIRE(dst[i] == 0x00000000);
        }
    }

    // Test with 0xAAAA5555
    {
        auto vec = simd::set1_u32_4(0xAAAA5555);
        simd::store_u32_4(dst, vec);
        for (int i = 0; i < 4; ++i) {
            FL_REQUIRE(dst[i] == 0xAAAA5555);
        }
    }
}

FL_TEST_CASE("blend_u8_16 blends two vectors correctly") {
    uint8_t a[16] = {0, 0, 0, 0, 100, 100, 100, 100, 200, 200, 200, 200, 50, 75, 125, 150};
    uint8_t b[16] = {255, 255, 255, 255, 200, 200, 200, 200, 100, 100, 100, 100, 150, 175, 225, 250};
    uint8_t dst[16];

    // Blend with amount = 128 (0.5)
    auto va = simd::load_u8_16(a);
    auto vb = simd::load_u8_16(b);
    auto result = simd::blend_u8_16(va, vb, 128);
    simd::store_u8_16(dst, result);

    // Verify blend results: result = a + ((b - a) * 128) / 256
    FL_REQUIRE(dst[0] == 127);   // 0 + ((255 - 0) * 128) / 256 = 127
    FL_REQUIRE(dst[1] == 127);
    FL_REQUIRE(dst[4] == 150);   // 100 + ((200 - 100) * 128) / 256 = 150
    FL_REQUIRE(dst[8] == 150);   // 200 + ((100 - 200) * 128) / 256 = 150
    FL_REQUIRE(dst[12] == 100);  // 50 + ((150 - 50) * 128) / 256 = 100
}

FL_TEST_CASE("blend_u8_16 handles edge cases") {
    uint8_t a[16] = {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100};
    uint8_t b[16] = {200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200};
    uint8_t dst[16];

    // Blend with amount = 0 (should return all a)
    {
        auto va = simd::load_u8_16(a);
        auto vb = simd::load_u8_16(b);
        auto result = simd::blend_u8_16(va, vb, 0);
        simd::store_u8_16(dst, result);

        for (int i = 0; i < 16; ++i) {
            FL_REQUIRE(dst[i] == 100);
        }
    }

    // Blend with amount = 255 (should return almost all b)
    {
        auto va = simd::load_u8_16(a);
        auto vb = simd::load_u8_16(b);
        auto result = simd::blend_u8_16(va, vb, 255);
        simd::store_u8_16(dst, result);

        for (int i = 0; i < 16; ++i) {
            // 100 + ((200 - 100) * 255) / 256 = 100 + 99 = 199
            FL_REQUIRE(dst[i] == 199);
        }
    }
}

FL_TEST_CASE("blend_u8_16 handles blending extremes") {
    uint8_t a[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t b[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    uint8_t dst[16];

    // Blend 0 to 255 with various amounts
    {
        auto va = simd::load_u8_16(a);
        auto vb = simd::load_u8_16(b);

        // 25% blend
        auto result = simd::blend_u8_16(va, vb, 64);
        simd::store_u8_16(dst, result);
        for (int i = 0; i < 16; ++i) {
            FL_REQUIRE(dst[i] == 63);  // 0 + ((255 - 0) * 64) / 256 = 63
        }

        // 75% blend
        result = simd::blend_u8_16(va, vb, 192);
        simd::store_u8_16(dst, result);
        for (int i = 0; i < 16; ++i) {
            FL_REQUIRE(dst[i] == 191);  // 0 + ((255 - 0) * 192) / 256 = 191
        }
    }
}

//==============================================================================
// Composed Operations (Cache-Efficient Pipelines)
//==============================================================================

FL_TEST_CASE("composed operations: scale then add in single loop") {
    // Demonstrate cache-efficient pattern: scale then add in one pass
    uint8_t src[64];
    uint8_t other[64];
    uint8_t dst[64];

    // Initialize test data
    for (size_t i = 0; i < 64; ++i) {
        src[i] = static_cast<uint8_t>(i * 2);
        other[i] = static_cast<uint8_t>(100);
    }

    // Cache-efficient: load once, transform in registers, store once
    using namespace simd;
    size_t i = 0;
    for (; i + 16 <= 64; i += 16) {
        // Load 16 bytes
        auto v = load_u8_16(&src[i]);

        // Scale by 0.5 (in register)
        v = scale_u8_16(v, 128);

        // Load second operand
        auto w = load_u8_16(&other[i]);

        // Add with saturation (in register)
        v = add_sat_u8_16(v, w);

        // Store result
        store_u8_16(&dst[i], v);
    }

    // Handle remainder (if any)
    for (; i < 64; ++i) {
        uint8_t scaled = static_cast<uint8_t>((static_cast<uint16_t>(src[i]) * 128) >> 8);
        uint16_t sum = static_cast<uint16_t>(scaled) + static_cast<uint16_t>(other[i]);
        dst[i] = (sum > 255) ? 255 : static_cast<uint8_t>(sum);
    }

    // Verify results
    for (size_t j = 0; j < 64; ++j) {
        uint8_t expected_scaled = static_cast<uint8_t>((static_cast<uint16_t>(src[j]) * 128) >> 8);
        uint16_t expected_sum = static_cast<uint16_t>(expected_scaled) + static_cast<uint16_t>(other[j]);
        uint8_t expected = (expected_sum > 255) ? 255 : static_cast<uint8_t>(expected_sum);
        FL_REQUIRE(dst[j] == expected);
    }
}

FL_TEST_CASE("composed operations: pattern fill with set1 and store") {
    uint32_t buffer[64];

    // Use atomic operations to fill pattern
    using namespace simd;
    auto pattern = set1_u32_4(0xDEADBEEF);

    size_t i = 0;
    for (; i + 4 <= 64; i += 4) {
        store_u32_4(&buffer[i], pattern);
    }

    // Handle remainder (if any)
    for (; i < 64; ++i) {
        buffer[i] = 0xDEADBEEF;
    }

    // Verify
    for (size_t j = 0; j < 64; ++j) {
        FL_REQUIRE(buffer[j] == 0xDEADBEEF);
    }
}

FL_TEST_CASE("composed operations: multiple adds in sequence") {
    uint8_t a[16] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160};
    uint8_t b[16] = {5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80};
    uint8_t c[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    uint8_t dst[16];

    using namespace simd;

    // Load all operands
    auto va = load_u8_16(a);
    auto vb = load_u8_16(b);
    auto vc = load_u8_16(c);

    // Chain operations: (a + b) + c
    auto result = add_sat_u8_16(va, vb);
    result = add_sat_u8_16(result, vc);

    store_u8_16(dst, result);

    // Verify first few results
    FL_REQUIRE(dst[0] == 16);   // 10 + 5 + 1 = 16
    FL_REQUIRE(dst[1] == 32);   // 20 + 10 + 2 = 32
    FL_REQUIRE(dst[2] == 48);   // 30 + 15 + 3 = 48
}

//==============================================================================
// Int32 SIMD Operations (Compile Tests)
//==============================================================================

FL_TEST_CASE("xor_u32_4 compiles and executes") {
    uint32_t a[4] = {0xFFFFFFFF, 0xAAAAAAAA, 0x55555555, 0x12345678};
    uint32_t b[4] = {0x00000000, 0x55555555, 0xAAAAAAAA, 0x87654321};
    uint32_t dst[4];

    auto va = simd::load_u32_4(a);
    auto vb = simd::load_u32_4(b);
    auto result = simd::xor_u32_4(va, vb);
    simd::store_u32_4(dst, result);

    // Verify XOR results
    FL_REQUIRE(dst[0] == 0xFFFFFFFF);  // 0xFFFFFFFF ^ 0x00000000
    FL_REQUIRE(dst[1] == 0xFFFFFFFF);  // 0xAAAAAAAA ^ 0x55555555
    FL_REQUIRE(dst[2] == 0xFFFFFFFF);  // 0x55555555 ^ 0xAAAAAAAA
    FL_REQUIRE(dst[3] == 0x95511559);  // 0x12345678 ^ 0x87654321
}

FL_TEST_CASE("add_i32_4 compiles and executes") {
    uint32_t a[4] = {100, 200, 300, 400};
    uint32_t b[4] = {50, 100, 150, 200};
    uint32_t dst[4];

    auto va = simd::load_u32_4(a);
    auto vb = simd::load_u32_4(b);
    auto result = simd::add_i32_4(va, vb);
    simd::store_u32_4(dst, result);

    // Verify addition results
    FL_REQUIRE(dst[0] == 150);  // 100 + 50
    FL_REQUIRE(dst[1] == 300);  // 200 + 100
    FL_REQUIRE(dst[2] == 450);  // 300 + 150
    FL_REQUIRE(dst[3] == 600);  // 400 + 200
}

FL_TEST_CASE("add_i32_4 handles signed overflow") {
    // Test with values that would overflow if treated as unsigned
    uint32_t a[4] = {0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 100};  // Max positive, min negative, -1, 100
    uint32_t b[4] = {1, 0xFFFFFFFF, 1, 0xFFFFFFF6};             // 1, -1, 1, -10
    uint32_t dst[4];

    auto va = simd::load_u32_4(a);
    auto vb = simd::load_u32_4(b);
    auto result = simd::add_i32_4(va, vb);
    simd::store_u32_4(dst, result);

    // Verify signed arithmetic behavior
    FL_REQUIRE(dst[0] == 0x80000000);  // INT_MAX + 1 = INT_MIN (overflow)
    FL_REQUIRE(dst[1] == 0x7FFFFFFF);  // INT_MIN + (-1) = INT_MAX (underflow)
    FL_REQUIRE(dst[2] == 0);           // -1 + 1 = 0
    FL_REQUIRE(dst[3] == 90);          // 100 + (-10) = 90
}

FL_TEST_CASE("sub_i32_4 compiles and executes") {
    uint32_t a[4] = {300, 200, 100, 50};
    uint32_t b[4] = {100, 50, 25, 10};
    uint32_t dst[4];

    auto va = simd::load_u32_4(a);
    auto vb = simd::load_u32_4(b);
    auto result = simd::sub_i32_4(va, vb);
    simd::store_u32_4(dst, result);

    // Verify subtraction results
    FL_REQUIRE(dst[0] == 200);  // 300 - 100
    FL_REQUIRE(dst[1] == 150);  // 200 - 50
    FL_REQUIRE(dst[2] == 75);   // 100 - 25
    FL_REQUIRE(dst[3] == 40);   // 50 - 10
}

FL_TEST_CASE("sub_i32_4 handles signed underflow") {
    // Test with values that would underflow if treated as unsigned
    uint32_t a[4] = {100, 0, 0x80000000, 50};     // 100, 0, INT_MIN, 50
    uint32_t b[4] = {200, 1, 1, 0xFFFFFFFF};      // 200, 1, 1, -1
    uint32_t dst[4];

    auto va = simd::load_u32_4(a);
    auto vb = simd::load_u32_4(b);
    auto result = simd::sub_i32_4(va, vb);
    simd::store_u32_4(dst, result);

    // Verify signed arithmetic behavior
    FL_REQUIRE(dst[0] == static_cast<uint32_t>(-100));  // 100 - 200 = -100
    FL_REQUIRE(dst[1] == static_cast<uint32_t>(-1));    // 0 - 1 = -1
    FL_REQUIRE(dst[2] == 0x7FFFFFFF);                   // INT_MIN - 1 = INT_MAX (underflow)
    FL_REQUIRE(dst[3] == 51);                           // 50 - (-1) = 51
}

FL_TEST_CASE("mulhi_i32_4 compiles and executes") {
    // Test values chosen to produce known high 32 bits after >> 16
    // For Q16.16 fixed-point: (a * b) >> 16 gives Q16.16 multiplication result
    uint32_t a[4] = {0x00010000, 0x00020000, 0x00030000, 0x00040000};  // 1.0, 2.0, 3.0, 4.0 in Q16.16
    uint32_t b[4] = {0x00010000, 0x00010000, 0x00010000, 0x00010000};  // 1.0, 1.0, 1.0, 1.0 in Q16.16
    uint32_t dst[4];

    auto va = simd::load_u32_4(a);
    auto vb = simd::load_u32_4(b);
    auto result = simd::mulhi_i32_4(va, vb);
    simd::store_u32_4(dst, result);

    // Verify Q16.16 multiplication: (a * 1.0) >> 16 = a
    FL_REQUIRE(dst[0] == 0x00010000);  // 1.0 * 1.0 = 1.0
    FL_REQUIRE(dst[1] == 0x00020000);  // 2.0 * 1.0 = 2.0
    FL_REQUIRE(dst[2] == 0x00030000);  // 3.0 * 1.0 = 3.0
    FL_REQUIRE(dst[3] == 0x00040000);  // 4.0 * 1.0 = 4.0
}

FL_TEST_CASE("mulhi_i32_4 handles fractional multiplication") {
    // Test Q16.16 fractional multiplication
    uint32_t a[4] = {0x00008000, 0x00010000, 0x00018000, 0x00020000};  // 0.5, 1.0, 1.5, 2.0
    uint32_t b[4] = {0x00008000, 0x00008000, 0x00008000, 0x00008000};  // 0.5, 0.5, 0.5, 0.5
    uint32_t dst[4];

    auto va = simd::load_u32_4(a);
    auto vb = simd::load_u32_4(b);
    auto result = simd::mulhi_i32_4(va, vb);
    simd::store_u32_4(dst, result);

    // Verify Q16.16 multiplication: a * 0.5
    FL_REQUIRE(dst[0] == 0x00004000);  // 0.5 * 0.5 = 0.25
    FL_REQUIRE(dst[1] == 0x00008000);  // 1.0 * 0.5 = 0.5
    FL_REQUIRE(dst[2] == 0x0000C000);  // 1.5 * 0.5 = 0.75
    FL_REQUIRE(dst[3] == 0x00010000);  // 2.0 * 0.5 = 1.0
}

FL_TEST_CASE("mulhi_i32_4 vs scalar reference with signed values") {
    // Scalar reference implementation
    auto scalar_mulhi = [](int32_t a, int32_t b) -> int32_t {
        int64_t prod = static_cast<int64_t>(a) * static_cast<int64_t>(b);
        return static_cast<int32_t>(prod >> 16);
    };

    // Test with mix of positive, negative, and edge cases
    int32_t a_vals[4] = {1000, -2000, 65536, -65536};
    int32_t b_vals[4] = {5000, 6000, -7000, -8000};

    // Compute scalar reference
    int32_t expected[4];
    for (int i = 0; i < 4; i++) {
        expected[i] = scalar_mulhi(a_vals[i], b_vals[i]);
    }

    // Compute SIMD version
    auto va = simd::load_u32_4(reinterpret_cast<uint32_t*>(a_vals));
    auto vb = simd::load_u32_4(reinterpret_cast<uint32_t*>(b_vals));
    auto result = simd::mulhi_i32_4(va, vb);

    int32_t simd_result[4];
    simd::store_u32_4(reinterpret_cast<uint32_t*>(simd_result), result);

    // Verify SIMD matches scalar
    for (int i = 0; i < 4; i++) {
        FL_REQUIRE_EQ(simd_result[i], expected[i]);
    }
}

FL_TEST_CASE("mulhi_i32_4 comprehensive signed test") {
    // Scalar reference implementation
    auto scalar_mulhi = [](int32_t a, int32_t b) -> int32_t {
        int64_t prod = static_cast<int64_t>(a) * static_cast<int64_t>(b);
        return static_cast<int32_t>(prod >> 16);
    };

    // Test multiple cases systematically
    struct TestCase {
        int32_t a[4];
        int32_t b[4];
    };

    TestCase cases[] = {
        // Positive * Positive
        {{100, 1000, 10000, 100000}, {200, 2000, 20000, 200000}},
        // Negative * Positive
        {{-100, -1000, -10000, -100000}, {200, 2000, 20000, 200000}},
        // Positive * Negative
        {{100, 1000, 10000, 100000}, {-200, -2000, -20000, -200000}},
        // Negative * Negative
        {{-100, -1000, -10000, -100000}, {-200, -2000, -20000, -200000}},
        // Mixed signs
        {{100, -1000, 10000, -100000}, {-200, 2000, -20000, 200000}},
        // Edge cases
        {{0, -1, 1, 2147483647}, {0, -1, -1, 2}},
    };

    for (size_t test_idx = 0; test_idx < sizeof(cases) / sizeof(TestCase); test_idx++) {
        const TestCase& tc = cases[test_idx];

        // Compute scalar reference
        int32_t expected[4];
        for (int i = 0; i < 4; i++) {
            expected[i] = scalar_mulhi(tc.a[i], tc.b[i]);
        }

        // Compute SIMD version
        auto va = simd::load_u32_4(reinterpret_cast<const uint32_t*>(tc.a));
        auto vb = simd::load_u32_4(reinterpret_cast<const uint32_t*>(tc.b));
        auto result = simd::mulhi_i32_4(va, vb);

        int32_t simd_result[4];
        simd::store_u32_4(reinterpret_cast<uint32_t*>(simd_result), result);

        // Verify SIMD matches scalar
        for (int i = 0; i < 4; i++) {
            FL_REQUIRE_EQ(simd_result[i], expected[i]);
        }
    }
}

FL_TEST_CASE("srl_u32_4 compiles and executes") {
    uint32_t src[4] = {0x12345678, 0xABCDEF00, 0xFFFFFFFF, 0x80000000};
    uint32_t dst[4];

    auto vec = simd::load_u32_4(src);

    // Test shift by 16 (extract high 16 bits)
    auto result = simd::srl_u32_4(vec, 16);
    simd::store_u32_4(dst, result);

    FL_REQUIRE(dst[0] == 0x00001234);  // 0x12345678 >> 16
    FL_REQUIRE(dst[1] == 0x0000ABCD);  // 0xABCDEF00 >> 16
    FL_REQUIRE(dst[2] == 0x0000FFFF);  // 0xFFFFFFFF >> 16
    FL_REQUIRE(dst[3] == 0x00008000);  // 0x80000000 >> 16
}

FL_TEST_CASE("srl_u32_4 handles various shift amounts") {
    uint32_t src[4] = {0xFFFFFFFF, 0xAAAAAAAA, 0x55555555, 0x12345678};
    uint32_t dst[4];

    auto vec = simd::load_u32_4(src);

    // Test shift by 0 (identity)
    {
        auto result = simd::srl_u32_4(vec, 0);
        simd::store_u32_4(dst, result);
        FL_REQUIRE(dst[0] == 0xFFFFFFFF);
        FL_REQUIRE(dst[1] == 0xAAAAAAAA);
    }

    // Test shift by 8
    {
        auto result = simd::srl_u32_4(vec, 8);
        simd::store_u32_4(dst, result);
        FL_REQUIRE(dst[0] == 0x00FFFFFF);  // 0xFFFFFFFF >> 8
        FL_REQUIRE(dst[1] == 0x00AAAAAA);  // 0xAAAAAAAA >> 8
        FL_REQUIRE(dst[2] == 0x00555555);  // 0x55555555 >> 8
        FL_REQUIRE(dst[3] == 0x00123456);  // 0x12345678 >> 8
    }

    // Test shift by 31 (extract sign bit as unsigned)
    {
        auto result = simd::srl_u32_4(vec, 31);
        simd::store_u32_4(dst, result);
        FL_REQUIRE(dst[0] == 1);  // 0xFFFFFFFF >> 31
        FL_REQUIRE(dst[1] == 1);  // 0xAAAAAAAA >> 31
        FL_REQUIRE(dst[2] == 0);  // 0x55555555 >> 31
        FL_REQUIRE(dst[3] == 0);  // 0x12345678 >> 31
    }
}

FL_TEST_CASE("and_u32_4 compiles and executes") {
    uint32_t a[4] = {0xFFFFFFFF, 0xAAAAAAAA, 0x55555555, 0x12345678};
    uint32_t b[4] = {0x0000FFFF, 0xFFFF0000, 0xFFFFFFFF, 0x0F0F0F0F};
    uint32_t dst[4];

    auto va = simd::load_u32_4(a);
    auto vb = simd::load_u32_4(b);
    auto result = simd::and_u32_4(va, vb);
    simd::store_u32_4(dst, result);

    // Verify AND results
    FL_REQUIRE(dst[0] == 0x0000FFFF);  // 0xFFFFFFFF & 0x0000FFFF
    FL_REQUIRE(dst[1] == 0xAAAA0000);  // 0xAAAAAAAA & 0xFFFF0000
    FL_REQUIRE(dst[2] == 0x55555555);  // 0x55555555 & 0xFFFFFFFF
    FL_REQUIRE(dst[3] == 0x02040608);  // 0x12345678 & 0x0F0F0F0F
}

FL_TEST_CASE("and_u32_4 handles masking patterns") {
    uint32_t src[4] = {0x12345678, 0xABCDEF00, 0xDEADBEEF, 0xCAFEBABE};
    uint32_t dst[4];

    auto vec = simd::load_u32_4(src);

    // Test masking bottom 16 bits
    {
        auto mask = simd::set1_u32_4(0x0000FFFF);
        auto result = simd::and_u32_4(vec, mask);
        simd::store_u32_4(dst, result);

        FL_REQUIRE(dst[0] == 0x00005678);  // Extract low 16 bits
        FL_REQUIRE(dst[1] == 0x0000EF00);
        FL_REQUIRE(dst[2] == 0x0000BEEF);
        FL_REQUIRE(dst[3] == 0x0000BABE);
    }

    // Test masking bottom 6 bits
    {
        auto mask = simd::set1_u32_4(0x0000003F);
        auto result = simd::and_u32_4(vec, mask);
        simd::store_u32_4(dst, result);

        FL_REQUIRE(dst[0] == 0x00000038);  // 0x78 & 0x3F = 0x38
        FL_REQUIRE(dst[1] == 0x00000000);  // 0x00 & 0x3F = 0x00
        FL_REQUIRE(dst[2] == 0x0000002F);  // 0xEF & 0x3F = 0x2F
        FL_REQUIRE(dst[3] == 0x0000003E);  // 0xBE & 0x3F = 0x3E
    }

    // Test masking single bit
    {
        auto mask = simd::set1_u32_4(0x00000001);
        auto result = simd::and_u32_4(vec, mask);
        simd::store_u32_4(dst, result);

        FL_REQUIRE(dst[0] == 0);  // 0x78 is even
        FL_REQUIRE(dst[1] == 0);  // 0x00 is even
        FL_REQUIRE(dst[2] == 1);  // 0xEF is odd
        FL_REQUIRE(dst[3] == 0);  // 0xBE is even
    }
}

//==============================================================================
// SIMD Type Alignment Tests
//==============================================================================

FL_TEST_CASE("SIMD type alignment") {
    FL_SUBCASE("simd_u8x16 alignment") {
        FL_REQUIRE(alignof(simd::simd_u8x16) == 16);
        // Verify actual instances are aligned
        simd::simd_u8x16 vec;
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&vec) % 16 == 0);
    }

    FL_SUBCASE("simd_u32x4 alignment") {
        FL_REQUIRE(alignof(simd::simd_u32x4) == 16);
        simd::simd_u32x4 vec;
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&vec) % 16 == 0);
    }

    FL_SUBCASE("simd_f32x4 alignment") {
        FL_REQUIRE(alignof(simd::simd_f32x4) == 16);
        simd::simd_f32x4 vec;
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&vec) % 16 == 0);
    }

    FL_SUBCASE("array of SIMD types alignment") {
        // Arrays should maintain alignment
        simd::simd_u32x4 arr[4];
        for (int i = 0; i < 4; i++) {
            FL_REQUIRE(reinterpret_cast<uintptr_t>(&arr[i]) % 16 == 0);
        }
    }
}

FL_TEST_CASE("aligned SIMD load/store operations") {
    FL_SUBCASE("aligned load/store u8x16") {
        FL_ALIGNAS(16) u8 data[16];
        for (int i = 0; i < 16; i++) {
            data[i] = static_cast<u8>(i);
        }

        simd::simd_u8x16 vec = simd::load_u8_16(data);

        FL_ALIGNAS(16) u8 result[16];
        simd::store_u8_16(result, vec);

        for (int i = 0; i < 16; i++) {
            FL_REQUIRE(result[i] == data[i]);
        }
    }

    FL_SUBCASE("aligned load/store u32x4") {
        FL_ALIGNAS(16) u32 data[4] = {1, 2, 3, 4};

        simd::simd_u32x4 vec = simd::load_u32_4(data);

        FL_ALIGNAS(16) u32 result[4];
        simd::store_u32_4(result, vec);

        for (int i = 0; i < 4; i++) {
            FL_REQUIRE(result[i] == data[i]);
        }
    }

    FL_SUBCASE("unaligned load/store should still work") {
        // SIMD load/store functions should handle unaligned access
        u32 data[5] = {0, 1, 2, 3, 4};
        u32* unaligned = data + 1;  // Offset by 1 to ensure misalignment

        simd::simd_u32x4 vec = simd::load_u32_4(unaligned);

        u32 result[5] = {0, 0, 0, 0, 0};
        u32* unaligned_result = result + 1;
        simd::store_u32_4(unaligned_result, vec);

        for (int i = 0; i < 4; i++) {
            FL_REQUIRE(unaligned_result[i] == unaligned[i]);
        }
    }
}

FL_TEST_CASE("struct with SIMD member alignment") {
    FL_SUBCASE("struct with SIMD members") {
        struct TestStruct {
            simd::simd_u32x4 a;
            simd::simd_u32x4 b;
            i32 scalar;
        };

        // Struct itself should be 16-byte aligned due to members
        FL_REQUIRE(alignof(TestStruct) == 16);

        TestStruct s;
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&s) % 16 == 0);
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&s.a) % 16 == 0);
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&s.b) % 16 == 0);
    }
}

FL_TEST_CASE("fixed-point SIMD type alignment") {
    FL_SUBCASE("s0x32x4 alignment") {
        // SIMD fixed-point types should be 16-byte aligned (contains simd_u32x4)
        FL_REQUIRE(alignof(s0x32x4) == 16);
        s0x32x4 vec = s0x32x4::set1(s0x32::from_raw(1073741824));  // 0.5 in Q31
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&vec) % 16 == 0);
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&vec.raw) % 16 == 0);
    }

    FL_SUBCASE("s16x16x4 alignment") {
        FL_REQUIRE(alignof(s16x16x4) == 16);
        s16x16x4 vec = s16x16x4::set1(s16x16::from_raw(32768));  // 0.5 in Q16.16
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&vec) % 16 == 0);
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&vec.raw) % 16 == 0);
    }

    FL_SUBCASE("array of fixed-point SIMD types alignment") {
        s0x32x4 arr[4];
        for (int i = 0; i < 4; i++) {
            FL_REQUIRE(reinterpret_cast<uintptr_t>(&arr[i]) % 16 == 0);
        }
    }

    FL_SUBCASE("struct with fixed-point SIMD members") {
        struct TestStruct {
            s0x32x4 a;
            s16x16x4 b;
        };

        FL_REQUIRE(alignof(TestStruct) == 16);

        TestStruct s;
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&s) % 16 == 0);
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&s.a) % 16 == 0);
        FL_REQUIRE(reinterpret_cast<uintptr_t>(&s.b) % 16 == 0);
    }
}
