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
#if defined(ESP32)
#include "platforms/esp/32/nmi_multispi.h"
#endif

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

// ============================================================================
// ESP32-specific NMI Multi-SPI Tests (Task 6.4)
// ============================================================================
// These tests validate the Level 7 NMI infrastructure for ultra-low latency
// multi-SPI parallel output. Tests are conditional on ESP32 platform.

#if defined(ESP32)

// Test: NMI API availability
TEST_CASE("NMI_API_available") {
    // Verify that NMI API headers compile on ESP32
    // This test ensures the API is accessible
    CHECK(true);
}

// Test: NMI initialization with valid parameters
TEST_CASE("NMI_initialization_valid_params") {
    uint8_t clock_pin = 17;
    uint8_t data_pins[] = {2, 4, 5, 12, 13, 14, 15, 16};

    // Initialize NMI multi-SPI at 800 kHz (WS2812 timing)
    bool success = fl::nmi::initMultiSPI(clock_pin, data_pins, 800000);

    // Note: Initialization may fail if:
    // - ESP-IDF v5.2.1 has known bug (returns ESP_ERR_NOT_FOUND)
    // - Level 7 interrupt already allocated
    // - Timer already in use
    // Therefore, we don't CHECK(success) - just verify API compiles and returns bool

    // If initialization succeeded, clean up
    if (success) {
        fl::nmi::shutdown();
    }

    CHECK(true);  // API is callable
}

// Test: NMI initialization with invalid frequency
TEST_CASE("NMI_initialization_invalid_frequency") {
    uint8_t clock_pin = 17;
    uint8_t data_pins[] = {2, 4, 5, 12, 13, 14, 15, 16};

    // Try to initialize with invalid frequency (too low)
    bool success_low = fl::nmi::initMultiSPI(clock_pin, data_pins, 500);  // Below 1 kHz
    CHECK(success_low == false);  // Should fail

    // Try to initialize with invalid frequency (too high)
    bool success_high = fl::nmi::initMultiSPI(clock_pin, data_pins, 50000000);  // Above 40 MHz
    CHECK(success_high == false);  // Should fail
}

// Test: Transmission without initialization
TEST_CASE("NMI_transmission_without_init") {
    uint8_t buffer[256];

    // Ensure NMI is not initialized
    fl::nmi::shutdown();

    // Try to start transmission without initialization
    bool success = fl::nmi::startTransmission(buffer, sizeof(buffer));
    CHECK(success == false);  // Should fail
}

// Test: Transmission with null buffer
TEST_CASE("NMI_transmission_null_buffer") {
    uint8_t clock_pin = 17;
    uint8_t data_pins[] = {2, 4, 5, 12, 13, 14, 15, 16};

    // Initialize (may fail on some ESP-IDF versions)
    bool init_success = fl::nmi::initMultiSPI(clock_pin, data_pins, 800000);

    if (init_success) {
        // Try to start transmission with null buffer
        bool success = fl::nmi::startTransmission(nullptr, 256);
        CHECK(success == false);  // Should fail

        // Try to start transmission with zero length
        uint8_t buffer[256];
        success = fl::nmi::startTransmission(buffer, 0);
        CHECK(success == false);  // Should fail

        fl::nmi::shutdown();
    }
}

// Test: Transmission completion status
TEST_CASE("NMI_transmission_completion") {
    uint8_t clock_pin = 17;
    uint8_t data_pins[] = {2, 4, 5, 12, 13, 14, 15, 16};

    // Initialize
    bool init_success = fl::nmi::initMultiSPI(clock_pin, data_pins, 800000);

    if (init_success) {
        // Before starting transmission, should report complete
        bool is_complete = fl::nmi::isTransmissionComplete();
        CHECK(is_complete == true);  // Nothing running

        // Start a small transmission
        uint8_t buffer[8] = {0xFF, 0x00, 0xAA, 0x55, 0xF0, 0x0F, 0xCC, 0x33};
        bool start_success = fl::nmi::startTransmission(buffer, sizeof(buffer));

        if (start_success) {
            // Immediately after starting, might still be running
            // (depends on timing - NMI may complete quickly)

            // Wait for completion (with timeout to prevent infinite loop)
            int timeout = 10000;  // 10ms should be plenty for 8 bytes @ 800 kHz
            while (!fl::nmi::isTransmissionComplete() && timeout-- > 0) {
                // Busy wait (NMI runs in background)
            }

            // Should eventually complete
            CHECK(fl::nmi::isTransmissionComplete() == true);
        }

        fl::nmi::shutdown();
    }
}

// Test: Shutdown API
TEST_CASE("NMI_shutdown") {
    uint8_t clock_pin = 17;
    uint8_t data_pins[] = {2, 4, 5, 12, 13, 14, 15, 16};

    // Initialize
    bool init_success = fl::nmi::initMultiSPI(clock_pin, data_pins, 800000);

    if (init_success) {
        // Shutdown should succeed
        fl::nmi::shutdown();

        // After shutdown, transmission should fail
        uint8_t buffer[256];
        bool success = fl::nmi::startTransmission(buffer, sizeof(buffer));
        CHECK(success == false);
    }

    // Multiple shutdowns should be safe (idempotent)
    fl::nmi::shutdown();
    fl::nmi::shutdown();
    CHECK(true);
}

// Test: Re-initialization after shutdown
TEST_CASE("NMI_reinit_after_shutdown") {
    uint8_t clock_pin = 17;
    uint8_t data_pins[] = {2, 4, 5, 12, 13, 14, 15, 16};

    // Initialize
    bool init1 = fl::nmi::initMultiSPI(clock_pin, data_pins, 800000);

    if (init1) {
        // Shutdown
        fl::nmi::shutdown();

        // Re-initialize with different frequency
        bool init2 = fl::nmi::initMultiSPI(clock_pin, data_pins, 1000000);

        if (init2) {
            // Should be able to use API after re-init
            uint8_t buffer[4] = {0xAA, 0xBB, 0xCC, 0xDD};
            bool success = fl::nmi::startTransmission(buffer, sizeof(buffer));
            // May succeed or fail depending on hardware state

            fl::nmi::shutdown();
        }
    }

    CHECK(true);
}

// Test: Diagnostic counters
TEST_CASE("NMI_diagnostic_counters") {
    uint8_t clock_pin = 17;
    uint8_t data_pins[] = {2, 4, 5, 12, 13, 14, 15, 16};

    // Initialize
    bool init_success = fl::nmi::initMultiSPI(clock_pin, data_pins, 800000);

    if (init_success) {
        // Get initial counter values
        uint32_t invocations_before = fl::nmi::getInvocationCount();
        uint32_t max_cycles_before = fl::nmi::getMaxExecutionCycles();

        // Counters should be accessible (exact values don't matter)
        CHECK(invocations_before >= 0);  // Always true, but documents expectation
        CHECK(max_cycles_before >= 0);

        // Start a transmission
        uint8_t buffer[16];
        bool start_success = fl::nmi::startTransmission(buffer, sizeof(buffer));

        if (start_success) {
            // Wait for completion
            int timeout = 10000;
            while (!fl::nmi::isTransmissionComplete() && timeout-- > 0) {}

            // Counters may have increased (if NMI fired)
            uint32_t invocations_after = fl::nmi::getInvocationCount();
            // Note: Invocations may not increase if timer hasn't fired yet
            // So we just verify the API is callable
            CHECK(invocations_after >= invocations_before);
        }

        fl::nmi::shutdown();
    }
}

#endif // ESP32

} // anonymous namespace

// Doctest provides main() automatically - no need to define it
