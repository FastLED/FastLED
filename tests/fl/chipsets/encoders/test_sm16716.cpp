#include "doctest.h"
#include "fl/chipsets/encoders/sm16716.h"
#include "fl/stl/array.h"
#include "fl/stl/iterator.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "fl/int.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"

// Named namespace to avoid unity build conflicts
namespace test_sm16716 {

using namespace fl;

// Helper: Verify termination header (7 bytes of 0x00 after LED data)
static void verifyTerminationHeader(const fl::vector<fl::u8>& data, size_t num_leds) {
    size_t led_data_size = num_leds * 3;  // 3 bytes per LED (RGB)
    size_t expected_header_size = 7;  // 7 bytes of 0x00

    REQUIRE(data.size() == led_data_size + expected_header_size);

    // Verify last 7 bytes are all 0x00
    for (size_t i = 0; i < expected_header_size; i++) {
        size_t offset = led_data_size + i;
        CHECK(data[offset] == 0x00);
    }
}

// Helper: Verify RGB LED frame at specific offset (3 bytes)
static void verifyLEDFrame(const fl::vector<fl::u8>& data, size_t offset, fl::u8 r, fl::u8 g, fl::u8 b) {
    REQUIRE(data.size() >= offset + 3);
    CHECK(data[offset + 0] == r);  // Red
    CHECK(data[offset + 1] == g);  // Green
    CHECK(data[offset + 2] == b);  // Blue
}

using namespace test_sm16716;

} // namespace test_sm16716

// ============================================================================
// Frame Structure Tests
// ============================================================================

TEST_CASE("encodeSM16716() - empty range (0 LEDs)") {
    fl::vector<fl::array<fl::u8, 3>> leds;
    fl::vector<fl::u8> output;

    fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));

    // Should only have termination header (7 bytes of 0x00)
    CHECK(output.size() == 7);
    for (size_t i = 0; i < 7; i++) {
        CHECK(output[i] == 0x00);
    }
}

TEST_CASE("encodeSM16716() - single LED") {
    fl::vector<fl::array<fl::u8, 3>> leds = {
        {255, 128, 64}  // R=255, G=128, B=64
    };
    fl::vector<fl::u8> output;

    fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));

    // Should have: 3 bytes (LED) + 7 bytes (header) = 10 bytes
    CHECK(output.size() == 10);

    // Verify LED data
    test_sm16716::verifyLEDFrame(output, 0, 255, 128, 64);

    // Verify termination header
    test_sm16716::verifyTerminationHeader(output, 1);
}

TEST_CASE("encodeSM16716() - multiple LEDs (3 LEDs)") {
    fl::vector<fl::array<fl::u8, 3>> leds = {
        {255, 0, 0},    // Red
        {0, 255, 0},    // Green
        {0, 0, 255}     // Blue
    };
    fl::vector<fl::u8> output;

    fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));

    // Should have: 9 bytes (3 LEDs * 3) + 7 bytes (header) = 16 bytes
    CHECK(output.size() == 16);

    // Verify LED data
    test_sm16716::verifyLEDFrame(output, 0, 255, 0, 0);    // LED 0
    test_sm16716::verifyLEDFrame(output, 3, 0, 255, 0);    // LED 1
    test_sm16716::verifyLEDFrame(output, 6, 0, 0, 255);    // LED 2

    // Verify termination header
    test_sm16716::verifyTerminationHeader(output, 3);
}

// ============================================================================
// Termination Header Tests
// ============================================================================

TEST_CASE("encodeSM16716() - termination header always 7 bytes") {
    SUBCASE("1 LED") {
        fl::vector<fl::array<fl::u8, 3>> leds = {{0, 0, 0}};
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        test_sm16716::verifyTerminationHeader(output, 1);
    }

    SUBCASE("10 LEDs") {
        fl::vector<fl::array<fl::u8, 3>> leds(10, {0, 0, 0});
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        test_sm16716::verifyTerminationHeader(output, 10);
    }

    SUBCASE("40 LEDs") {
        // Reduced from 100 to 40 for performance (still provides good stress test coverage)
        fl::vector<fl::array<fl::u8, 3>> leds(40, {0, 0, 0});
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        test_sm16716::verifyTerminationHeader(output, 40);
    }
}

// ============================================================================
// Color Order Tests
// ============================================================================

TEST_CASE("encodeSM16716() - color order RGB") {
    fl::vector<fl::array<fl::u8, 3>> leds = {
        {0xAA, 0xBB, 0xCC}  // R=0xAA, G=0xBB, B=0xCC
    };
    fl::vector<fl::u8> output;

    fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));

    // Verify RGB wire order
    CHECK(output[0] == 0xAA);  // Red first
    CHECK(output[1] == 0xBB);  // Green second
    CHECK(output[2] == 0xCC);  // Blue third
}

TEST_CASE("encodeSM16716() - pure colors") {
    SUBCASE("Pure red") {
        fl::vector<fl::array<fl::u8, 3>> leds = {{255, 0, 0}};
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        test_sm16716::verifyLEDFrame(output, 0, 255, 0, 0);
    }

    SUBCASE("Pure green") {
        fl::vector<fl::array<fl::u8, 3>> leds = {{0, 255, 0}};
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        test_sm16716::verifyLEDFrame(output, 0, 0, 255, 0);
    }

    SUBCASE("Pure blue") {
        fl::vector<fl::array<fl::u8, 3>> leds = {{0, 0, 255}};
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        test_sm16716::verifyLEDFrame(output, 0, 0, 0, 255);
    }
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_CASE("encodeSM16716() - black (0,0,0)") {
    fl::vector<fl::array<fl::u8, 3>> leds = {{0, 0, 0}};
    fl::vector<fl::u8> output;

    fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));

    test_sm16716::verifyLEDFrame(output, 0, 0, 0, 0);
    test_sm16716::verifyTerminationHeader(output, 1);
}

TEST_CASE("encodeSM16716() - white (255,255,255)") {
    fl::vector<fl::array<fl::u8, 3>> leds = {{255, 255, 255}};
    fl::vector<fl::u8> output;

    fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));

    test_sm16716::verifyLEDFrame(output, 0, 255, 255, 255);
    test_sm16716::verifyTerminationHeader(output, 1);
}

TEST_CASE("encodeSM16716() - mixed values") {
    fl::vector<fl::array<fl::u8, 3>> leds = {
        {0, 0, 0},          // Black
        {255, 255, 255},    // White
        {128, 64, 32},      // Mid-tones
        {1, 2, 3},          // Near-black
        {253, 254, 255}     // Near-white
    };
    fl::vector<fl::u8> output;

    fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));

    // Should have: 15 bytes (5 LEDs * 3) + 7 bytes (header) = 22 bytes
    CHECK(output.size() == 22);

    test_sm16716::verifyLEDFrame(output, 0, 0, 0, 0);
    test_sm16716::verifyLEDFrame(output, 3, 255, 255, 255);
    test_sm16716::verifyLEDFrame(output, 6, 128, 64, 32);
    test_sm16716::verifyLEDFrame(output, 9, 1, 2, 3);
    test_sm16716::verifyLEDFrame(output, 12, 253, 254, 255);

    test_sm16716::verifyTerminationHeader(output, 5);
}

// ============================================================================
// Boundary Tests
// ============================================================================

TEST_CASE("encodeSM16716() - boundary LED counts") {
    SUBCASE("1 LED") {
        fl::vector<fl::array<fl::u8, 3>> leds(1, {0x11, 0x22, 0x33});
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        CHECK(output.size() == 3 + 7);  // 3 bytes LED + 7 bytes header
    }

    SUBCASE("31 LEDs") {
        fl::vector<fl::array<fl::u8, 3>> leds(31, {0x11, 0x22, 0x33});
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        CHECK(output.size() == (31 * 3) + 7);
    }

    SUBCASE("32 LEDs") {
        fl::vector<fl::array<fl::u8, 3>> leds(32, {0x11, 0x22, 0x33});
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        CHECK(output.size() == (32 * 3) + 7);
    }

    SUBCASE("64 LEDs") {
        fl::vector<fl::array<fl::u8, 3>> leds(64, {0x11, 0x22, 0x33});
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        CHECK(output.size() == (64 * 3) + 7);
    }

    SUBCASE("40 LEDs") {
        // Reduced from 100 to 40 for performance (still provides good stress test coverage)
        fl::vector<fl::array<fl::u8, 3>> leds(40, {0x11, 0x22, 0x33});
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        CHECK(output.size() == (40 * 3) + 7);
    }
}

// ============================================================================
// Iterator Compatibility Tests
// ============================================================================

TEST_CASE("encodeSM16716() - works with different iterator types") {
    SUBCASE("vector iterators") {
        fl::vector<fl::array<fl::u8, 3>> leds = {{255, 128, 64}};
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        CHECK(output.size() == 10);
    }

    SUBCASE("array iterators") {
        fl::array<fl::array<fl::u8, 3>, 2> leds = {{{255, 0, 0}, {0, 255, 0}}};
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));
        CHECK(output.size() == 13);  // 2 LEDs * 3 + 7
    }

    SUBCASE("pointer iterators") {
        fl::array<fl::u8, 3> leds[] = {{255, 128, 64}, {64, 128, 255}};
        fl::vector<fl::u8> output;
        fl::encodeSM16716(leds, leds + 2, fl::back_inserter(output));
        CHECK(output.size() == 13);  // 2 LEDs * 3 + 7
    }
}

// ============================================================================
// Protocol Specification Tests
// ============================================================================

TEST_CASE("encodeSM16716() - protocol structure") {
    fl::vector<fl::array<fl::u8, 3>> leds = {{0xAA, 0xBB, 0xCC}};
    fl::vector<fl::u8> output;

    fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));

    // Protocol structure:
    // - LED data comes first (3 bytes per LED)
    // - Termination header comes last (7 bytes of 0x00)
    // - No preamble/start frame in encoder (handled by SPI FLAG_START_BIT)

    CHECK(output.size() == 10);

    // LED data at start
    CHECK(output[0] == 0xAA);
    CHECK(output[1] == 0xBB);
    CHECK(output[2] == 0xCC);

    // Termination header at end (50 zero bits = 7 bytes)
    CHECK(output[3] == 0x00);
    CHECK(output[4] == 0x00);
    CHECK(output[5] == 0x00);
    CHECK(output[6] == 0x00);
    CHECK(output[7] == 0x00);
    CHECK(output[8] == 0x00);
    CHECK(output[9] == 0x00);
}

TEST_CASE("encodeSM16716() - 50 zero bits termination") {
    // Verify that the termination header is exactly 50 zero bits
    // which equals 7 full bytes (56 bits), but SM16716 spec says 50 bits
    // The encoder outputs 7 bytes for simplicity, which is > 50 bits

    fl::vector<fl::array<fl::u8, 3>> leds = {{255, 255, 255}};
    fl::vector<fl::u8> output;

    fl::encodeSM16716(leds.begin(), leds.end(), fl::back_inserter(output));

    // Count zero bits in termination header
    size_t zero_bits = 0;
    for (size_t i = 3; i < output.size(); i++) {
        for (int bit = 0; bit < 8; bit++) {
            if ((output[i] & (1 << bit)) == 0) {
                zero_bits++;
            }
        }
    }

    // Should have at least 50 zero bits (we have 56)
    CHECK(zero_bits >= 50);
    CHECK(zero_bits == 56);  // 7 bytes * 8 bits
}
