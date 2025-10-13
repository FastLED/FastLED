/*
  FastLED — Parallel Soft-SPI ISR Unit Tests (8-way Octo-SPI)
  ------------------------------------------------------------
  Tests the ISR engine with host simulation for 8-way Octo-SPI.
  Verifies GPIO event capture and ISR behavior without hardware.
  This is the full 8-bit parallel variant with maximum parallelism.

  License: MIT (FastLED)
*/

// Note: FASTLED_SPI_HOST_SIMULATION defined via build system (meson.build)
#include "test.h"
#include "platforms/esp/32/parallel_spi/fl_parallel_spi_isr_rv.h"
#include "platforms/esp/32/parallel_spi/fl_parallel_spi_host_sim.h"

namespace {

/* Helper: Initialize 8-way pin mapping */
void setup_octo_spi_lut() {
    PinMaskEntry* lut = fl_spi_get_lut_array();

    // Map pins: GPIO0-7 for data, GPIO8 for clock
    uint32_t dataPinMasks[8] = {
        1u << 0,  // D0
        1u << 1,  // D1
        1u << 2,  // D2
        1u << 3,  // D3
        1u << 4,  // D4
        1u << 5,  // D5
        1u << 6,  // D6
        1u << 7   // D7
    };

    for (int v = 0; v < 256; v++) {
        uint32_t setMask = 0;
        uint32_t clearMask = 0;

        for (int b = 0; b < 8; b++) {
            if (v & (1 << b)) {
                setMask |= dataPinMasks[b];
            } else {
                clearMask |= dataPinMasks[b];
            }
        }

        lut[v].set_mask = setMask;
        lut[v].clear_mask = clearMask;
    }

    fl_spi_set_clock_mask(1u << 8);
}

/* Helper: Drive ISR until transfer completes */
void drive_isr_until_done(uint32_t max_ticks = 1000) {
    for (uint32_t i = 0; i < max_ticks; i++) {
        fl_spi_host_simulate_tick();

        if (!(fl_spi_status_flags() & FASTLED_STATUS_BUSY)) {
            return;  // Done
        }
    }
    FAIL("ISR did not complete within max_ticks");
}

}  // namespace

TEST_CASE("octo_spi_isr: basic 8-way transmission") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Prepare test data: 0x00, 0xFF
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;  // All data pins low (00000000)
    data[1] = 0xFF;  // All data pins high (11111111)
    fl_spi_set_total_bytes(2);

    // Start ISR
    int ret = fl_spi_platform_isr_start(1600000);
    REQUIRE(ret == 0);

    // Arm transfer
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    // Drive ISR
    drive_isr_until_done();

    // Verify transfer completed
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    // Inspect ring buffer
    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount > 0);

    // Stop ISR
    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: verify clock toggling") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Single byte transmission
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x55;  // 01010101 pattern
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify clock toggles (should see clock mask in both SET and CLEAR events)
    FL_GPIO_Event evt;
    uint32_t clockSetCount = 0;
    uint32_t clockClearCount = 0;
    uint32_t clockMask = 1u << 8;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0 && (evt.gpio_mask & clockMask)) {
            clockSetCount++;
        }
        if (evt.event_type == 1 && (evt.gpio_mask & clockMask)) {
            clockClearCount++;
        }
    }

    CHECK(clockSetCount > 0);
    CHECK(clockClearCount > 0);
    CHECK(clockSetCount == clockClearCount);  // Balanced clock

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: verify data pattern 0xAA") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test pattern: 0xAA = 10101010 binary
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0xAA;
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify data pins match pattern (bits 1, 3, 5, 7 should be set)
    FL_GPIO_Event evt;
    bool foundDataSet = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {  // SET event
            if (evt.gpio_mask & 0xFF) {  // Data pins (8 bits)
                CHECK((evt.gpio_mask & 0xFF) == 0xAA);
                foundDataSet = true;
            }
        }
    }

    CHECK(foundDataSet);

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: verify data pattern 0x55") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test pattern: 0x55 = 01010101 binary
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x55;
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify data pins match pattern (bits 0, 2, 4, 6 should be set)
    FL_GPIO_Event evt;
    bool foundDataSet = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {  // SET event
            if (evt.gpio_mask & 0xFF) {  // Data pins
                CHECK((evt.gpio_mask & 0xFF) == 0x55);
                foundDataSet = true;
            }
        }
    }

    CHECK(foundDataSet);

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: multiple byte sequence") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test sequence: 0x00, 0xFF, 0xAA, 0x55
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    data[1] = 0xFF;
    data[2] = 0xAA;
    data[3] = 0x55;
    fl_spi_set_total_bytes(4);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify all bytes transmitted (should have 4 * 2 phases = 8 clock cycles)
    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount > 8);  // At least SET+CLEAR per phase

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: zero bytes transfer") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // No data to send
    fl_spi_set_total_bytes(0);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    // ISR should immediately complete with no data
    drive_isr_until_done(10);

    // Should be done immediately
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    // No events should be generated
    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount == 0);

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: all ones pattern") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with all ones (0xFF)
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0xFF;
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify all data pins were set (all 8 bits)
    FL_GPIO_Event evt;
    bool foundAllOnes = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {  // SET event
            if ((evt.gpio_mask & 0xFF) == 0xFF) {
                foundAllOnes = true;
            }
        }
    }

    CHECK(foundAllOnes);

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: all zeros pattern") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with all zeros (0x00)
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    fl_spi_set_total_bytes(1);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Verify all data pins were cleared
    FL_GPIO_Event evt;
    bool foundDataClear = false;
    bool foundDataSet = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0 && (evt.gpio_mask & 0xFF)) {
            foundDataSet = true;
        }
        if (evt.event_type == 1 && (evt.gpio_mask & 0xFF)) {
            foundDataClear = true;
        }
    }

    CHECK(foundDataClear);
    CHECK_FALSE(foundDataSet);  // Should NOT set data pins for 0x00

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: alternating pattern") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with alternating 0xAA/0x55 pattern
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0xAA;  // 10101010
    data[1] = 0x55;  // 01010101
    data[2] = 0xAA;  // 10101010
    data[3] = 0x55;  // 01010101
    fl_spi_set_total_bytes(4);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Should complete successfully
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: sequential byte values") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with sequential values: 0x00, 0x01, 0x02, ..., 0x0F
    uint8_t* data = fl_spi_get_data_array();
    for (int i = 0; i < 16; i++) {
        data[i] = static_cast<uint8_t>(i);
    }
    fl_spi_set_total_bytes(16);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Should complete successfully
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    // Should have many events (16 bytes * 2 phases * 2+ events per phase)
    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount >= 32);

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: long sequence") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with a longer sequence (64 bytes)
    uint8_t* data = fl_spi_get_data_array();
    for (int i = 0; i < 64; i++) {
        data[i] = static_cast<uint8_t>(i & 0xFF);  // Pattern repeats every 256
    }
    fl_spi_set_total_bytes(64);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done(1500);  // Need more ticks for 64 bytes

    // Should complete successfully
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    // Should have many events (64 bytes * 2 phases * 2+ events per phase)
    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount >= 128);

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: max bytes transfer") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with maximum bytes (256)
    uint8_t* data = fl_spi_get_data_array();
    for (int i = 0; i < 256; i++) {
        data[i] = static_cast<uint8_t>(i);  // Each byte equals its index
    }
    fl_spi_set_total_bytes(256);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done(2000);  // Need more ticks for 256 bytes

    // Should complete successfully
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: boundary values") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with boundary values: 0x00, 0x01, 0x7F, 0x80, 0xFE, 0xFF
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;  // Min value
    data[1] = 0x01;  // Min+1
    data[2] = 0x7F;  // Max signed 7-bit
    data[3] = 0x80;  // High bit only
    data[4] = 0xFE;  // Max-1
    data[5] = 0xFF;  // Max value
    fl_spi_set_total_bytes(6);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Should complete successfully
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    fl_spi_platform_isr_stop();
}

TEST_CASE("octo_spi_isr: power of two patterns") {
    setup_octo_spi_lut();
    fl_gpio_sim_clear();
    fl_spi_reset_state();

    // Test with powers of 2: 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
    uint8_t* data = fl_spi_get_data_array();
    for (int i = 0; i < 8; i++) {
        data[i] = static_cast<uint8_t>(1 << i);
    }
    fl_spi_set_total_bytes(8);

    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();

    drive_isr_until_done();

    // Should complete successfully
    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    fl_spi_platform_isr_stop();
}
