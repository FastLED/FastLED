/*
  Test: Cross-Platform ISR API
  -----------------------------
  Tests the fl::isr API for timer-based interrupt handling.

  License: MIT (FastLED)
*/

#define FASTLED_INTERNAL
#include "fl/isr.h"
#include "__atomic/atomic.h"
#include "__atomic/support/c11.h"
#include "__chrono/duration.h"
#include "__thread/this_thread.h"
#include "doctest.h"
#include "isr_stub.hpp"

using namespace fl;
// Test counters
static std::atomic<int> g_isr_call_count{0};
static std::atomic<uint32_t> g_isr_user_data_value{0};

// =============================================================================
// Test Handlers
// =============================================================================

static void test_isr_handler(void* user_data) {
    g_isr_call_count++;
    if (user_data) {
        uint32_t* value = static_cast<uint32_t*>(user_data);
        g_isr_user_data_value = *value;
    }
}

// =============================================================================
// Test Cases
// =============================================================================

TEST_CASE("test_isr_platform_info") {
    const char* platform = isr::getPlatformName();
    REQUIRE(platform != nullptr);

    // Just call the functions to verify they exist and don't crash
    (void)isr::getMaxTimerFrequency();
    (void)isr::getMinTimerFrequency();
    (void)isr::getMaxPriority();
    (void)isr::requiresAssemblyHandler(1);
    (void)isr::requiresAssemblyHandler(5);
}

TEST_CASE("test_isr_timer_basic") {
    g_isr_call_count = 0;
    g_isr_user_data_value = 0;

    // Configure a 100 Hz timer (10ms period)
    isr::isr_config_t config;
    config.handler = test_isr_handler;
    config.user_data = nullptr;
    config.frequency_hz = 100;  // 100 Hz
    config.priority = isr::ISR_PRIORITY_MEDIUM;
    config.flags = isr::ISR_FLAG_IRAM_SAFE;

    isr::isr_handle_t handle;
    int result = isr::attachTimerHandler(config, &handle);

    REQUIRE(result == 0);
    REQUIRE(handle.is_valid());

    // Wait for ~50ms (should get ~5 calls at 100 Hz) - reduced from 200ms
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    int call_count = g_isr_call_count.load();

    // Allow significant tolerance for timing - stub implementation may vary
    // Expected ~5 calls, but allow 2-10 to account for system timing variations
    REQUIRE(call_count >= 2);
    REQUIRE(call_count <= 10);

    // Detach handler
    result = isr::detachHandler(handle);
    REQUIRE(result == 0);
    REQUIRE(!handle.is_valid());

    // Wait a bit and verify no more calls (reduced from 100ms to 20ms)
    int final_count = g_isr_call_count.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    int count_after_detach = g_isr_call_count.load();

    REQUIRE(final_count == count_after_detach);
}

TEST_CASE("test_isr_timer_user_data") {
    g_isr_call_count = 0;
    g_isr_user_data_value = 0;

    uint32_t test_value = 0x12345678;

    // Configure timer with user data
    isr::isr_config_t config;
    config.handler = test_isr_handler;
    config.user_data = &test_value;
    config.frequency_hz = 50;  // 50 Hz
    config.priority = isr::ISR_PRIORITY_LOW;
    config.flags = isr::ISR_FLAG_IRAM_SAFE;

    isr::isr_handle_t handle;
    int result = isr::attachTimerHandler(config, &handle);

    REQUIRE(result == 0);

    // Wait for a few calls (reduced from 100ms to 30ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // Verify user data was passed correctly
    REQUIRE(g_isr_user_data_value.load() == test_value);

    // Cleanup
    result = isr::detachHandler(handle);
    REQUIRE(result == 0);
}

TEST_CASE("test_isr_timer_enable_disable") {
    g_isr_call_count = 0;

    // Configure timer
    isr::isr_config_t config;
    config.handler = test_isr_handler;
    config.user_data = nullptr;
    config.frequency_hz = 100;  // 100 Hz
    config.priority = isr::ISR_PRIORITY_MEDIUM;
    config.flags = isr::ISR_FLAG_IRAM_SAFE;

    isr::isr_handle_t handle;
    int result = isr::attachTimerHandler(config, &handle);
    REQUIRE(result == 0);

    // Wait for some calls (reduced from 100ms to 30ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    int count_before_disable = g_isr_call_count.load();
    REQUIRE(count_before_disable > 0);

    // Disable handler
    result = isr::disableHandler(handle);
    REQUIRE(result == 0);
    REQUIRE(!isr::isHandlerEnabled(handle));

    // Small delay to ensure any in-flight handler call completes (reduced from 20ms to 5ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    int count_after_disable_immediate = g_isr_call_count.load();

    // Wait longer and verify no new calls (reduced from 100ms to 30ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    int count_after_disable = g_isr_call_count.load();
    REQUIRE(count_after_disable_immediate == count_after_disable);

    // Re-enable handler
    result = isr::enableHandler(handle);
    REQUIRE(result == 0);
    REQUIRE(isr::isHandlerEnabled(handle));

    // Wait and verify new calls (reduced from 100ms to 30ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    int count_after_enable = g_isr_call_count.load();
    REQUIRE(count_after_enable > count_after_disable);

    // Cleanup
    result = isr::detachHandler(handle);
    REQUIRE(result == 0);
}

TEST_CASE("test_isr_error_handling") {
    // Test null handler
    isr::isr_config_t config;
    config.handler = nullptr;  // Invalid
    config.frequency_hz = 100;

    isr::isr_handle_t handle;
    int result = isr::attachTimerHandler(config, &handle);
    REQUIRE(result != 0);
    REQUIRE(!handle.is_valid());

    // Test zero frequency
    config.handler = test_isr_handler;
    config.frequency_hz = 0;  // Invalid
    result = isr::attachTimerHandler(config, &handle);
    REQUIRE(result != 0);
    REQUIRE(!handle.is_valid());

    // Test invalid handle operations
    isr::isr_handle_t invalid_handle;
    result = isr::detachHandler(invalid_handle);
    REQUIRE(result != 0);

    result = isr::enableHandler(invalid_handle);
    REQUIRE(result != 0);

    result = isr::disableHandler(invalid_handle);
    REQUIRE(result != 0);

    REQUIRE(!isr::isHandlerEnabled(invalid_handle));
}

TEST_CASE("test_interrupts_global_state") {
    // Interrupts should start enabled
    REQUIRE(interruptsEnabled());
    REQUIRE(!interruptsDisabled());

    // Disable interrupts
    interruptsDisable();
    REQUIRE(interruptsDisabled());
    REQUIRE(!interruptsEnabled());

    // Re-enable interrupts
    interruptsEnable();
    REQUIRE(interruptsEnabled());
    REQUIRE(!interruptsDisabled());
}

TEST_CASE("test_interrupts_global_disable_blocks_isr") {
    g_isr_call_count = 0;

    // Ensure interrupts are enabled initially
    interruptsEnable();

    // Configure timer
    isr::isr_config_t config;
    config.handler = test_isr_handler;
    config.user_data = nullptr;
    config.frequency_hz = 100;  // 100 Hz
    config.priority = isr::ISR_PRIORITY_MEDIUM;
    config.flags = isr::ISR_FLAG_IRAM_SAFE;

    isr::isr_handle_t handle;
    int result = isr::attachTimerHandler(config, &handle);
    REQUIRE(result == 0);

    // Wait and verify timer is firing (reduced from 100ms to 30ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    int count_enabled = g_isr_call_count.load();
    REQUIRE(count_enabled > 0);

    // Globally disable interrupts
    interruptsDisable();
    REQUIRE(interruptsDisabled());

    // Small delay to ensure any in-flight handler call completes (reduced from 20ms to 5ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    int count_after_disable_immediate = g_isr_call_count.load();

    // Wait and verify NO new calls (global interrupts disabled) - reduced from 150ms to 50ms
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int count_disabled = g_isr_call_count.load();
    REQUIRE(count_after_disable_immediate == count_disabled);

    // Re-enable global interrupts
    interruptsEnable();
    REQUIRE(interruptsEnabled());

    // Wait and verify new calls resume (reduced from 100ms to 30ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    int count_reenabled = g_isr_call_count.load();
    REQUIRE(count_reenabled > count_disabled);

    // Cleanup
    result = isr::detachHandler(handle);
    REQUIRE(result == 0);
}

// =============================================================================
// Test Main
// =============================================================================
// Note: setup() and loop() removed - not needed for this test
