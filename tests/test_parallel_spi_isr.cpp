/*
  FastLED — Parallel Soft-SPI ISR Unit Tests (Consolidated)
  ----------------------------------------------------------
  Tests the ISR engine with host simulation for various SPI configurations.
  Supports both manual tick and thread-based automatic ISR execution.

  Test matrix:
  - SPI widths: 1-way (Single), 2-way (Dual), 4-way (Quad), 8-way (Octo)
  - Execution modes: Manual tick, Thread-based auto-execution

  Each SPI width has its own setup function and test suite.
  Thread-based tests use automatic ISR execution, closer to hardware behavior.
  Manual tick tests require explicit tick() calls for deterministic testing.

  License: MIT (FastLED)
*/

// Note: FASTLED_SPI_HOST_SIMULATION defined via build system (meson.build)
#include "test.h"
#include "platforms/shared/spi_bitbang/spi_isr_engine.h"
#include "platforms/shared/spi_bitbang/host_sim.h"

#ifndef FASTLED_SPI_MANUAL_TICK
#include <thread>
#include <chrono>
#endif

#include <stdio.h>

#define TEST_DBG(...) printf("[TEST] " __VA_ARGS__)

namespace {

// ============================================================================
// Helper Functions
// ============================================================================

/* Helper: Initialize 1-way pin mapping */
void setup_single_spi_lut() {
    PinMaskEntry* lut = fl_spi_get_lut_array();

    // Map pins: GPIO0 for data, GPIO8 for clock
    uint32_t dataPinMask = 1u << 0;  // D0

    for (int v = 0; v < 256; v++) {
        // Only process bit 0 (upper 7 bits ignored)
        if (v & 1) {
            // Bit 0 is set - set data pin high
            lut[v].set_mask = dataPinMask;
            lut[v].clear_mask = 0;
        } else {
            // Bit 0 is clear - clear data pin low
            lut[v].set_mask = 0;
            lut[v].clear_mask = dataPinMask;
        }
    }

    fl_spi_set_clock_mask(1u << 8);
}

/* Helper: Initialize 2-way pin mapping */
void setup_dual_spi_lut() {
    PinMaskEntry* lut = fl_spi_get_lut_array();

    // Map pins: GPIO0-1 for data, GPIO8 for clock
    uint32_t dataPinMasks[2] = {
        1u << 0,  // D0
        1u << 1   // D1
    };

    for (int v = 0; v < 256; v++) {
        uint32_t setMask = 0;
        uint32_t clearMask = 0;

        for (int b = 0; b < 2; b++) {
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

/* Helper: Initialize 4-way pin mapping */
void setup_quad_spi_lut() {
    PinMaskEntry* lut = fl_spi_get_lut_array();

    // Map pins: GPIO0-3 for data, GPIO8 for clock
    uint32_t dataPinMasks[4] = {
        1u << 0,  // D0
        1u << 1,  // D1
        1u << 2,  // D2
        1u << 3   // D3
    };

    for (int v = 0; v < 256; v++) {
        uint32_t setMask = 0;
        uint32_t clearMask = 0;

        for (int b = 0; b < 4; b++) {
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

#ifdef FASTLED_SPI_MANUAL_TICK
/* Helper: Drive ISR until transfer completes (manual tick mode) */
void drive_isr_until_done(uint32_t max_ticks = 1000) {
    for (uint32_t i = 0; i < max_ticks; i++) {
        fl_spi_host_simulate_tick();

        if (!(fl_spi_status_flags() & FASTLED_STATUS_BUSY)) {
            return;  // Done
        }
    }
    FAIL("ISR did not complete within max_ticks");
}
#else
/* Helper: Wait for ISR to complete with timeout (thread mode) */
bool wait_for_completion(uint32_t timeout_ms = 100) {
    auto start = std::chrono::steady_clock::now();
    uint32_t initial_flags = fl_spi_status_flags();
    TEST_DBG("Waiting for ISR completion (timeout: %u ms), initial flags: 0x%x\n", timeout_ms, initial_flags);

    uint32_t iterations = 0;

    // First, wait for BUSY flag to be set (or DONE if immediate completion)
    // This handles the race where we check before ISR detects doorbell
    while (true) {
        uint32_t flags = fl_spi_status_flags();
        if (flags & (FASTLED_STATUS_BUSY | FASTLED_STATUS_DONE)) {
            // ISR has detected the work
            break;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(10));
        iterations++;

        // Timeout safety
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::milliseconds(timeout_ms)) {
            TEST_DBG("Timeout waiting for ISR to start after %u iterations, status flags: 0x%x\n", iterations, fl_spi_status_flags());
            return false;  // Timeout
        }
    }

    // Now wait for BUSY to clear (work completed)
    while (fl_spi_status_flags() & FASTLED_STATUS_BUSY) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        iterations++;

        if (iterations <= 5) {
            TEST_DBG("Wait iteration %u, status flags: 0x%x\n", iterations, fl_spi_status_flags());
        }

        // Timeout safety
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::milliseconds(timeout_ms)) {
            TEST_DBG("Timeout after %u iterations, status flags: 0x%x\n", iterations, fl_spi_status_flags());
            return false;  // Timeout
        }
    }

    TEST_DBG("ISR completed after %u iterations, status flags: 0x%x\n", iterations, fl_spi_status_flags());
    return true;  // Completed
}
#endif

}  // namespace

// ============================================================================
// 1-way Single-SPI Tests
// ============================================================================

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("single_spi_isr: basic 1-way transmission") {
#else
TEST_CASE("single_spi_isr_thread: automatic ISR execution") {
    TEST_DBG("=== Starting test: automatic ISR execution ===\n");
#endif
    setup_single_spi_lut();
    fl_gpio_sim_clear();

    // Prepare test data
    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    data[1] = 0x01;
    fl_spi_set_total_bytes(2);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_spi_reset_state();
    int ret = fl_spi_platform_isr_start(1600000);
    REQUIRE(ret == 0);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    TEST_DBG("Test data prepared: 2 bytes\n");
    TEST_DBG("Starting ISR platform...\n");
    int ret = fl_spi_platform_isr_start(1600000);
    REQUIRE(ret == 0);
    TEST_DBG("ISR platform started, ret=%d\n", ret);

    // Reset state AFTER starting ISR to synchronize doorbell counters
    fl_spi_reset_state();
    TEST_DBG("After reset_state, status flags: 0x%x\n", fl_spi_status_flags());

    TEST_DBG("Setting visibility delay and arming...\n");
    fl_spi_visibility_delay_us(10);
    TEST_DBG("Before arm, status flags: 0x%x\n", fl_spi_status_flags());
    fl_spi_arm();
    TEST_DBG("After arm, status flags: 0x%x\n", fl_spi_status_flags());

    bool completed = wait_for_completion();
    REQUIRE(completed);
    TEST_DBG("Checking completion flag...\n");
#endif

    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    uint32_t eventCount = fl_gpio_sim_get_event_count();
#ifndef FASTLED_SPI_MANUAL_TICK
    TEST_DBG("GPIO event count: %u\n", eventCount);
#endif
    CHECK(eventCount > 0);

#ifndef FASTLED_SPI_MANUAL_TICK
    TEST_DBG("Stopping ISR platform...\n");
#endif
    fl_spi_platform_isr_stop();
#ifndef FASTLED_SPI_MANUAL_TICK
    TEST_DBG("=== Test completed ===\n");
#endif
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("single_spi_isr: verify clock toggling") {
#else
TEST_CASE("single_spi_isr_thread: verify clock toggling") {
#endif
    setup_single_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x01;
    fl_spi_set_total_bytes(1);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion();
    REQUIRE(completed);
    fl_spi_platform_isr_stop();
#endif

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
#ifdef FASTLED_SPI_MANUAL_TICK
    // In manual tick mode, counts must match exactly
    CHECK(clockSetCount == clockClearCount);
#else
    // In thread mode, allow off-by-one due to race conditions during ISR stop
    CHECK((clockSetCount == clockClearCount || clockSetCount == clockClearCount + 1 || clockSetCount + 1 == clockClearCount));
#endif

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_spi_platform_isr_stop();
#endif
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("single_spi_isr: verify data patterns") {
#else
TEST_CASE("single_spi_isr_thread: verify data patterns") {
#endif
    setup_single_spi_lut();
    fl_gpio_sim_clear();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    data[1] = 0x01;
    fl_spi_set_total_bytes(2);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion();
    REQUIRE(completed);
#endif

    FL_GPIO_Event evt;
    bool foundDataSet = false;
    bool foundDataClear = false;
    uint32_t dataPinMask = 1u << 0;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0 && (evt.gpio_mask & dataPinMask)) {
            foundDataSet = true;
        }
        if (evt.event_type == 1 && (evt.gpio_mask & dataPinMask)) {
            foundDataClear = true;
        }
    }

    CHECK(foundDataSet);
    CHECK(foundDataClear);

    fl_spi_platform_isr_stop();
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("single_spi_isr: zero bytes transfer") {
#else
TEST_CASE("single_spi_isr_thread: zero bytes transfer") {
#endif
    setup_single_spi_lut();
    fl_gpio_sim_clear();

    fl_spi_set_total_bytes(0);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done(10);
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion(100);
    REQUIRE(completed);
#endif

    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount == 0);

    fl_spi_platform_isr_stop();
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("single_spi_isr: longer sequence") {
#else
TEST_CASE("single_spi_isr_thread: longer sequence") {
#endif
    setup_single_spi_lut();
    fl_gpio_sim_clear();

    uint8_t* data = fl_spi_get_data_array();
    for (int i = 0; i < 10; i++) {
        data[i] = (i % 2);
    }
    fl_spi_set_total_bytes(10);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion(500);
    REQUIRE(completed);
#endif

    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    uint32_t eventCount = fl_gpio_sim_get_event_count();
#ifdef FASTLED_SPI_MANUAL_TICK
    CHECK(eventCount >= 20);
#else
    CHECK(eventCount >= 20);
#endif

    fl_spi_platform_isr_stop();
}

// ============================================================================
// 2-way Dual-SPI Tests
// ============================================================================

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("dual_spi_isr: basic 2-way transmission") {
#else
TEST_CASE("dual_spi_isr_thread: basic 2-way transmission") {
#endif
    setup_dual_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    data[1] = 0x03;
    fl_spi_set_total_bytes(2);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    int ret = fl_spi_platform_isr_start(1600000);
    REQUIRE(ret == 0);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion();
    REQUIRE(completed);
#endif

    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount > 0);

    fl_spi_platform_isr_stop();
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("dual_spi_isr: verify clock toggling") {
#else
TEST_CASE("dual_spi_isr_thread: verify clock toggling") {
#endif
    setup_dual_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x01;
    fl_spi_set_total_bytes(1);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion();
    REQUIRE(completed);
    fl_spi_platform_isr_stop();
#endif

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
#ifdef FASTLED_SPI_MANUAL_TICK
    // In manual tick mode, counts must match exactly
    CHECK(clockSetCount == clockClearCount);
#else
    // In thread mode, allow off-by-one due to race conditions during ISR stop
    CHECK((clockSetCount == clockClearCount || clockSetCount == clockClearCount + 1 || clockSetCount + 1 == clockClearCount));
#endif

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_spi_platform_isr_stop();
#endif
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("dual_spi_isr: all patterns") {
#else
TEST_CASE("dual_spi_isr_thread: all four patterns") {
#endif
    setup_dual_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    data[1] = 0x01;
    data[2] = 0x02;
    data[3] = 0x03;
    fl_spi_set_total_bytes(4);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion(500);
    REQUIRE(completed);
#endif

    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount > 8);

    fl_spi_platform_isr_stop();
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("dual_spi_isr: zero bytes transfer") {
#else
TEST_CASE("dual_spi_isr_thread: zero bytes transfer") {
#endif
    setup_dual_spi_lut();

    fl_spi_set_total_bytes(0);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done(10);
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion(100);
    REQUIRE(completed);
#endif

    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount == 0);

    fl_spi_platform_isr_stop();
}

// ============================================================================
// 4-way Quad-SPI Tests
// ============================================================================

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("quad_spi_isr: basic 4-way transmission") {
#else
TEST_CASE("quad_spi_isr_thread: basic 4-way transmission") {
#endif
    setup_quad_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    data[1] = 0x0F;
    fl_spi_set_total_bytes(2);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    int ret = fl_spi_platform_isr_start(1600000);
    REQUIRE(ret == 0);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion();
    REQUIRE(completed);
#endif

    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount > 0);

    fl_spi_platform_isr_stop();
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("quad_spi_isr: verify clock toggling") {
#else
TEST_CASE("quad_spi_isr_thread: verify clock toggling") {
#endif
    setup_quad_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x05;
    fl_spi_set_total_bytes(1);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion();
    REQUIRE(completed);
    fl_spi_platform_isr_stop();
#endif

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
#ifdef FASTLED_SPI_MANUAL_TICK
    // In manual tick mode, counts must match exactly
    CHECK(clockSetCount == clockClearCount);
#else
    // In thread mode, allow off-by-one due to race conditions during ISR stop
    CHECK((clockSetCount == clockClearCount || clockSetCount == clockClearCount + 1 || clockSetCount + 1 == clockClearCount));
#endif

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_spi_platform_isr_stop();
#endif
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("quad_spi_isr: verify data pattern") {
#else
TEST_CASE("quad_spi_isr_thread: verify data pattern") {
#endif
    setup_quad_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x0A;
    fl_spi_set_total_bytes(1);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion();
    REQUIRE(completed);
    fl_spi_platform_isr_stop();
#endif

    FL_GPIO_Event evt;
    bool foundDataSet = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {
            if (evt.gpio_mask & 0x0F) {
                CHECK((evt.gpio_mask & 0x0F) == 0x0A);
                foundDataSet = true;
            }
        }
    }

    CHECK(foundDataSet);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_spi_platform_isr_stop();
#endif
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("quad_spi_isr: multiple byte sequence") {
#else
TEST_CASE("quad_spi_isr_thread: multiple byte sequence") {
#endif
    setup_quad_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    data[1] = 0x0F;
    data[2] = 0x0A;
    data[3] = 0x05;
    fl_spi_set_total_bytes(4);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion(500);
    REQUIRE(completed);
#endif

    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount > 8);

    fl_spi_platform_isr_stop();
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("quad_spi_isr: zero bytes transfer") {
#else
TEST_CASE("quad_spi_isr_thread: zero bytes transfer") {
#endif
    setup_quad_spi_lut();

    fl_spi_set_total_bytes(0);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done(10);
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion(100);
    REQUIRE(completed);
#endif

    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount == 0);

    fl_spi_platform_isr_stop();
}

// ============================================================================
// 8-way Octo-SPI Tests
// ============================================================================

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("octo_spi_isr: basic 8-way transmission") {
#else
TEST_CASE("octo_spi_isr_thread: basic 8-way transmission") {
#endif
    setup_octo_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    data[1] = 0xFF;
    fl_spi_set_total_bytes(2);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    int ret = fl_spi_platform_isr_start(1600000);
    REQUIRE(ret == 0);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion();
    REQUIRE(completed);
#endif

    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount > 0);

    fl_spi_platform_isr_stop();
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("octo_spi_isr: verify clock toggling") {
#else
TEST_CASE("octo_spi_isr_thread: verify clock toggling") {
#endif
    setup_octo_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x55;
    fl_spi_set_total_bytes(1);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion();
    REQUIRE(completed);
    fl_spi_platform_isr_stop();
#endif

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
#ifdef FASTLED_SPI_MANUAL_TICK
    // In manual tick mode, counts must match exactly
    CHECK(clockSetCount == clockClearCount);
#else
    // In thread mode, allow off-by-one due to race conditions during ISR stop
    CHECK((clockSetCount == clockClearCount || clockSetCount == clockClearCount + 1 || clockSetCount + 1 == clockClearCount));
#endif

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_spi_platform_isr_stop();
#endif
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("octo_spi_isr: verify data patterns") {
#else
TEST_CASE("octo_spi_isr_thread: verify data pattern 0xAA") {
#endif
    setup_octo_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0xAA;
    fl_spi_set_total_bytes(1);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion();
    REQUIRE(completed);
    fl_spi_platform_isr_stop();
#endif

    FL_GPIO_Event evt;
    bool foundDataSet = false;

    while (fl_gpio_sim_read_event(&evt)) {
        if (evt.event_type == 0) {
            if (evt.gpio_mask & 0xFF) {
                CHECK((evt.gpio_mask & 0xFF) == 0xAA);
                foundDataSet = true;
            }
        }
    }

    CHECK(foundDataSet);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_spi_platform_isr_stop();
#endif
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("octo_spi_isr: multiple byte sequence") {
#else
TEST_CASE("octo_spi_isr_thread: multiple byte sequence") {
#endif
    setup_octo_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    data[0] = 0x00;
    data[1] = 0xFF;
    data[2] = 0xAA;
    data[3] = 0x55;
    fl_spi_set_total_bytes(4);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done();
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion(500);
    REQUIRE(completed);
#endif

    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount > 8);

    fl_spi_platform_isr_stop();
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("octo_spi_isr: zero bytes transfer") {
#else
TEST_CASE("octo_spi_isr_thread: zero bytes transfer") {
#endif
    setup_octo_spi_lut();

    fl_spi_set_total_bytes(0);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done(10);
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion(100);
    REQUIRE(completed);
#endif

    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount == 0);

    fl_spi_platform_isr_stop();
}

#ifdef FASTLED_SPI_MANUAL_TICK
TEST_CASE("octo_spi_isr: long sequence") {
#else
TEST_CASE("octo_spi_isr_thread: long sequence") {
#endif
    setup_octo_spi_lut();

    uint8_t* data = fl_spi_get_data_array();
    for (int i = 0; i < 64; i++) {
        data[i] = static_cast<uint8_t>(i & 0xFF);
    }
    fl_spi_set_total_bytes(64);

#ifdef FASTLED_SPI_MANUAL_TICK
    fl_gpio_sim_clear();
    fl_spi_reset_state();
    fl_spi_platform_isr_start(1600000);
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    drive_isr_until_done(2000);
#else
    fl_spi_platform_isr_start(1600000);
    fl_spi_reset_state();
    fl_gpio_sim_clear();
    fl_spi_visibility_delay_us(10);
    fl_spi_arm();
    bool completed = wait_for_completion(3000);
    REQUIRE(completed);
#endif

    CHECK((fl_spi_status_flags() & FASTLED_STATUS_DONE) != 0);

    uint32_t eventCount = fl_gpio_sim_get_event_count();
    CHECK(eventCount >= 128);

    fl_spi_platform_isr_stop();
}
