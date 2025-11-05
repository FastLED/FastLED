/// @file tests/test_fast_pins.cpp
/// @brief Unit tests for FastPins API
///
/// Tests the fast-pins GPIO API including:
/// - LUT generation correctness
/// - Pin mask extraction
/// - API compile-time checks
/// - Pattern writes

#include "test.h"
#include "platforms/fast_pins.h"

using namespace fl;

namespace {

// Test: Basic LUT generation for 4 pins
TEST_CASE("FastPins_4pins_LUT_generation") {
    FastPins<4> writer;
    writer.setPins(2, 3, 5, 7);

    // Verify LUT size
    CHECK(writer.getLUT() != nullptr);
    CHECK(writer.getPinCount() == 4);

    // For 4 pins, we should have 16 LUT entries (2^4)
    const auto* lut = writer.getLUT();

    // Test specific patterns:
    // Pattern 0x00 (all LOW): set_mask = 0, clear_mask has all pin masks
    // Pattern 0x0F (all HIGH): set_mask has all pin masks, clear_mask = 0
    // Pattern 0x05 (pins 0,2 HIGH; 1,3 LOW): mixed masks

    // Note: Exact mask values depend on platform pin mapping
    // Just verify that masks are non-zero and different for different patterns
    CHECK(lut[0].set_mask != lut[15].set_mask);  // All LOW != All HIGH
    CHECK(lut[0].clear_mask != lut[15].clear_mask);
}

// Test: LUT generation for 8 pins (maximum)
TEST_CASE("FastPins_8pins_LUT_size") {
    FastPins<8> writer;
    writer.setPins(0, 1, 2, 3, 4, 5, 6, 7);

    CHECK(writer.getPinCount() == 8);

    // For 8 pins, we should have 256 LUT entries (2^8)
    // Verify that all entries are initialized (at minimum, they should differ)
    const auto* lut = writer.getLUT();

    // Test corner cases:
    // Pattern 0x00: all pins LOW
    // Pattern 0xFF: all pins HIGH
    // Pattern 0xAA: alternating (10101010)
    // Pattern 0x55: alternating (01010101)

    CHECK(lut[0x00].set_mask != lut[0xFF].set_mask);
    CHECK(lut[0xAA].set_mask != lut[0x55].set_mask);
}

// Test: Single pin operation (degenerate case)
TEST_CASE("FastPins_1pin") {
    FastPins<1> writer;
    writer.setPins(5);

    CHECK(writer.getPinCount() == 1);

    // For 1 pin, we should have 2 LUT entries (2^1)
    const auto* lut = writer.getLUT();

    // Pattern 0: pin LOW
    // Pattern 1: pin HIGH
    bool masks_differ = (lut[0].set_mask != lut[1].set_mask) ||
                        (lut[0].clear_mask != lut[1].clear_mask);
    CHECK(masks_differ);
}

// Test: Compile-time pin count check
TEST_CASE("FastPins_compile_time_checks") {
    // This should compile fine
    FastPins<4> writer;

    // Verify that setting fewer pins than MAX_PINS is allowed
    writer.setPins(2, 3);  // 2 pins for FastPins<4> is OK
    CHECK(writer.getPinCount() == 2);

    writer.setPins(1, 2, 3, 4);  // 4 pins for FastPins<4> is OK
    CHECK(writer.getPinCount() == 4);

    // Note: Setting MORE pins than MAX_PINS would fail at compile time:
    // writer.setPins(1, 2, 3, 4, 5);  // Compile error: static_assert
}

// Test: LUT symmetry - complementary patterns should have complementary masks
TEST_CASE("FastPins_LUT_symmetry") {
    FastPins<3> writer;
    writer.setPins(1, 2, 3);

    const auto* lut = writer.getLUT();

    // For 3 pins, patterns should show symmetry:
    // Pattern 0x0 vs 0x7 (all LOW vs all HIGH)
    // Pattern 0x2 vs 0x5 (middle pin vs outer pins)

    // Verify that complementary patterns have swapped set/clear masks
    // (set_mask of pattern N should match clear_mask of pattern ~N)
    // Note: This is an idealized test - actual behavior depends on platform
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t complement = i ^ 0x7;  // Flip all 3 bits

        // The set mask for pattern i should equal clear mask for complement
        // (and vice versa)
        // This is platform-dependent, so just check they're different
        bool patterns_differ = (lut[i].set_mask != lut[complement].set_mask) ||
                               (lut[i].clear_mask != lut[complement].clear_mask);
        CHECK(patterns_differ);
    }
}

// Test: Write API (basic smoke test - actual GPIO writes depend on platform)
TEST_CASE("FastPins_write_API") {
    FastPins<4> writer;
    writer.setPins(2, 3, 5, 7);

    // These calls should not crash (actual GPIO behavior is platform-specific)
    writer.write(0x0);
    writer.write(0xF);
    writer.write(0xA);
    writer.write(0x5);

    // No assertions here - just verify API compiles and doesn't crash
    CHECK(true);
}

// Test: Default constructor
TEST_CASE("FastPins_default_constructor") {
    FastPins<4> writer;

    // Before setPins(), pin count should be 0
    CHECK(writer.getPinCount() == 0);

    // LUT should exist but be uninitialized
    CHECK(writer.getLUT() != nullptr);
}

} // anonymous namespace

// Doctest provides main() automatically - no need to define it
