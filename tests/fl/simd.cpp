// Test for fl::simd atomic operations

#include "fl/simd.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "doctest.h"

using namespace fl;

//==============================================================================
// Atomic Load/Store Tests
//==============================================================================

TEST_CASE("load_u8_16 loads 16 bytes correctly") {
    uint8_t src[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    uint8_t dst[16] = {0};

    auto vec = simd::load_u8_16(src);
    simd::store_u8_16(dst, vec);

    for (int i = 0; i < 16; ++i) {
        REQUIRE(dst[i] == src[i]);
    }
}

TEST_CASE("load_u32_4 loads 4 uint32_t correctly") {
    uint32_t src[4] = {0x12345678, 0xABCDEF00, 0xDEADBEEF, 0xCAFEBABE};
    uint32_t dst[4] = {0};

    auto vec = simd::load_u32_4(src);
    simd::store_u32_4(dst, vec);

    for (int i = 0; i < 4; ++i) {
        REQUIRE(dst[i] == src[i]);
    }
}

TEST_CASE("store_u8_16 stores 16 bytes correctly") {
    uint8_t buffer[32] = {0};  // Extra space to check bounds
    uint8_t pattern[16] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160};

    auto vec = simd::load_u8_16(pattern);
    simd::store_u8_16(&buffer[8], vec);  // Store in middle

    // Check pattern stored correctly
    for (int i = 0; i < 16; ++i) {
        REQUIRE(buffer[8 + i] == pattern[i]);
    }

    // Check boundaries not overwritten
    for (int i = 0; i < 8; ++i) {
        REQUIRE(buffer[i] == 0);
        REQUIRE(buffer[24 + i] == 0);
    }
}

TEST_CASE("store_u32_4 stores 4 uint32_t correctly") {
    uint32_t buffer[8] = {0};  // Extra space to check bounds
    uint32_t pattern[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};

    auto vec = simd::load_u32_4(pattern);
    simd::store_u32_4(&buffer[2], vec);  // Store in middle

    // Check pattern stored correctly
    for (int i = 0; i < 4; ++i) {
        REQUIRE(buffer[2 + i] == pattern[i]);
    }

    // Check boundaries not overwritten
    REQUIRE(buffer[0] == 0);
    REQUIRE(buffer[1] == 0);
    REQUIRE(buffer[6] == 0);
    REQUIRE(buffer[7] == 0);
}

//==============================================================================
// Arithmetic Operation Tests
//==============================================================================

TEST_CASE("add_sat_u8_16 adds without overflow") {
    uint8_t a[16] = {100, 150, 200, 255, 0, 50, 100, 150, 200, 255, 0, 50, 100, 150, 200, 255};
    uint8_t b[16] = { 50, 100, 150, 200, 0, 50, 100, 150, 200, 255, 0, 50, 100, 150, 200, 255};
    uint8_t dst[16];

    auto va = simd::load_u8_16(a);
    auto vb = simd::load_u8_16(b);
    auto result = simd::add_sat_u8_16(va, vb);
    simd::store_u8_16(dst, result);

    // Verify saturating addition
    REQUIRE(dst[0] == 150);   // 100 + 50 = 150
    REQUIRE(dst[1] == 250);   // 150 + 100 = 250
    REQUIRE(dst[2] == 255);   // 200 + 150 = 350 -> saturate to 255
    REQUIRE(dst[3] == 255);   // 255 + 200 = 455 -> saturate to 255
    REQUIRE(dst[4] == 0);     // 0 + 0 = 0
    REQUIRE(dst[5] == 100);   // 50 + 50 = 100
}

TEST_CASE("add_sat_u8_16 handles edge cases") {
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
            REQUIRE(dst[i] == 0);
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
            REQUIRE(dst[i] == 255);
        }
    }
}

TEST_CASE("scale_u8_16 scales values correctly") {
    uint8_t src[16] = {0, 64, 128, 192, 255, 100, 200, 50, 10, 20, 30, 40, 60, 80, 120, 160};
    uint8_t dst[16];

    // Scale by 128 (0.5x)
    auto vec = simd::load_u8_16(src);
    auto result = simd::scale_u8_16(vec, 128);
    simd::store_u8_16(dst, result);

    REQUIRE(dst[0] == 0);      // 0 * 128/256 = 0
    REQUIRE(dst[1] == 32);     // 64 * 128/256 = 32
    REQUIRE(dst[2] == 64);     // 128 * 128/256 = 64
    REQUIRE(dst[3] == 96);     // 192 * 128/256 = 96
    REQUIRE(dst[4] == 127);    // 255 * 128/256 = 127
}

TEST_CASE("scale_u8_16 handles identity scaling") {
    uint8_t src[16] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160};
    uint8_t dst[16];

    // Scale by 255 (should be ~1.0x)
    auto vec = simd::load_u8_16(src);
    auto result = simd::scale_u8_16(vec, 255);
    simd::store_u8_16(dst, result);

    // Values should be very close to original (within 1 due to rounding)
    for (int i = 0; i < 16; ++i) {
        REQUIRE(dst[i] >= src[i] - 1);
        REQUIRE(dst[i] <= src[i]);
    }
}

TEST_CASE("scale_u8_16 handles zero scaling") {
    uint8_t src[16] = {100, 150, 200, 255, 50, 75, 125, 175, 10, 20, 30, 40, 60, 80, 120, 160};
    uint8_t dst[16];

    // Scale by 0 (should zero everything)
    auto vec = simd::load_u8_16(src);
    auto result = simd::scale_u8_16(vec, 0);
    simd::store_u8_16(dst, result);

    for (int i = 0; i < 16; ++i) {
        REQUIRE(dst[i] == 0);
    }
}

TEST_CASE("set1_u32_4 broadcasts value to all lanes") {
    uint32_t dst[4];
    uint32_t pattern = 0xDEADBEEF;

    auto vec = simd::set1_u32_4(pattern);
    simd::store_u32_4(dst, vec);

    for (int i = 0; i < 4; ++i) {
        REQUIRE(dst[i] == pattern);
    }
}

TEST_CASE("set1_u32_4 works with different patterns") {
    uint32_t dst[4];

    // Test with 0xFFFFFFFF
    {
        auto vec = simd::set1_u32_4(0xFFFFFFFF);
        simd::store_u32_4(dst, vec);
        for (int i = 0; i < 4; ++i) {
            REQUIRE(dst[i] == 0xFFFFFFFF);
        }
    }

    // Test with 0x00000000
    {
        auto vec = simd::set1_u32_4(0x00000000);
        simd::store_u32_4(dst, vec);
        for (int i = 0; i < 4; ++i) {
            REQUIRE(dst[i] == 0x00000000);
        }
    }

    // Test with 0xAAAA5555
    {
        auto vec = simd::set1_u32_4(0xAAAA5555);
        simd::store_u32_4(dst, vec);
        for (int i = 0; i < 4; ++i) {
            REQUIRE(dst[i] == 0xAAAA5555);
        }
    }
}

TEST_CASE("blend_u8_16 blends two vectors correctly") {
    uint8_t a[16] = {0, 0, 0, 0, 100, 100, 100, 100, 200, 200, 200, 200, 50, 75, 125, 150};
    uint8_t b[16] = {255, 255, 255, 255, 200, 200, 200, 200, 100, 100, 100, 100, 150, 175, 225, 250};
    uint8_t dst[16];

    // Blend with amount = 128 (0.5)
    auto va = simd::load_u8_16(a);
    auto vb = simd::load_u8_16(b);
    auto result = simd::blend_u8_16(va, vb, 128);
    simd::store_u8_16(dst, result);

    // Verify blend results: result = a + ((b - a) * 128) / 256
    REQUIRE(dst[0] == 127);   // 0 + ((255 - 0) * 128) / 256 = 127
    REQUIRE(dst[1] == 127);
    REQUIRE(dst[4] == 150);   // 100 + ((200 - 100) * 128) / 256 = 150
    REQUIRE(dst[8] == 150);   // 200 + ((100 - 200) * 128) / 256 = 150
    REQUIRE(dst[12] == 100);  // 50 + ((150 - 50) * 128) / 256 = 100
}

TEST_CASE("blend_u8_16 handles edge cases") {
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
            REQUIRE(dst[i] == 100);
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
            REQUIRE(dst[i] == 199);
        }
    }
}

TEST_CASE("blend_u8_16 handles blending extremes") {
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
            REQUIRE(dst[i] == 63);  // 0 + ((255 - 0) * 64) / 256 = 63
        }

        // 75% blend
        result = simd::blend_u8_16(va, vb, 192);
        simd::store_u8_16(dst, result);
        for (int i = 0; i < 16; ++i) {
            REQUIRE(dst[i] == 191);  // 0 + ((255 - 0) * 192) / 256 = 191
        }
    }
}

//==============================================================================
// Composed Operations (Cache-Efficient Pipelines)
//==============================================================================

TEST_CASE("composed operations: scale then add in single loop") {
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
        REQUIRE(dst[j] == expected);
    }
}

TEST_CASE("composed operations: pattern fill with set1 and store") {
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
        REQUIRE(buffer[j] == 0xDEADBEEF);
    }
}

TEST_CASE("composed operations: multiple adds in sequence") {
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
    REQUIRE(dst[0] == 16);   // 10 + 5 + 1 = 16
    REQUIRE(dst[1] == 32);   // 20 + 10 + 2 = 32
    REQUIRE(dst[2] == 48);   // 30 + 15 + 3 = 48
}
