/*
  Test: Cross-Platform ISR API
  -----------------------------
  Tests the fl::isr API for timer-based interrupt handling.

  License: MIT (FastLED)
*/

#define FASTLED_INTERNAL
#include "fl/isr.h"
#include "fl/stl/atomic.h"
#include "fl/stl/thread.h"
#include "doctest.h"
#include "isr_stub.hpp"

using namespace fl;
// Test counters
static fl::atomic<int> g_isr_call_count{0};
static fl::atomic<uint32_t> g_isr_user_data_value{0};

// =============================================================================
// Helper Functions for Timing-Dependent Tests
// =============================================================================

// Wait for a condition to become true with timeout using condition variables
// This is more efficient and reliable than sleep polling under heavy load
// Returns true if condition met, false if timeout
template<typename Predicate>
static bool wait_for_condition(Predicate pred, std::chrono::milliseconds timeout) { // okay std namespace
    if (pred()) {
        return true;  // Already satisfied
    }

    // Get access to the test synchronization primitives from the timer manager
    auto& manager = fl::isr::TimerThreadManager::instance();
    fl::unique_lock<fl::mutex> lock(manager.get_test_sync_mutex());

    auto deadline = std::chrono::steady_clock::now() + timeout; // okay std namespace

    while (!pred()) {
        auto now = std::chrono::steady_clock::now(); // okay std namespace
        if (now >= deadline) {
            return false;  // Timeout
        }

        // Wait on condition variable with remaining timeout
        // This will be notified whenever an ISR executes
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now); // okay std namespace
        manager.get_test_sync_cv().wait_for(lock, remaining);
    }

    return true;  // Condition met
}

// =============================================================================
// Test Handlers
// =============================================================================

static void test_isr_handler(void* user_data) {
    ++g_isr_call_count;
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

    // Wait for at least 2 calls (with 200ms timeout = 20 expected calls at 100 Hz)
    // This ensures the timer is working, even under heavy system load
    bool got_calls = wait_for_condition([](){ return g_isr_call_count.load() >= 2; },
                                        std::chrono::milliseconds(200)); // okay std namespace
    REQUIRE(got_calls);

    int call_count = g_isr_call_count.load();
    // Should have gotten at least 2 calls, upper bound is generous for slow systems
    REQUIRE(call_count >= 2);
    REQUIRE(call_count <= 25);  // At most 25 calls in 200ms at 100Hz (allowing overhead)

    // Detach handler - use release memory order to ensure visibility
    result = isr::detachHandler(handle);
    REQUIRE(result == 0);
    REQUIRE(!handle.is_valid());

    // Capture count immediately after detach
    int final_count = g_isr_call_count.load();

    // Wait up to 50ms to ensure no more ISR calls occur
    // Use condition variable wait to detect any unexpected calls immediately
    auto& manager = fl::isr::TimerThreadManager::instance();
    fl::unique_lock<fl::mutex> lock(manager.get_test_sync_mutex());

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50); // okay std namespace
    bool spurious_call = false;

    while (std::chrono::steady_clock::now() < deadline) { // okay std namespace
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>( // okay std namespace
            deadline - std::chrono::steady_clock::now()); // okay std namespace
        if (remaining.count() <= 0) break;

        manager.get_test_sync_cv().wait_for(lock, remaining);

        // Check if count changed (indicating a spurious ISR call after detach)
        if (g_isr_call_count.load() != final_count) {
            spurious_call = true;
            break;
        }
    }
    lock.unlock();

    int count_after_detach = g_isr_call_count.load();
    REQUIRE(!spurious_call);
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

    // Wait for user data to be set (timeout 200ms = 10 expected calls at 50 Hz)
    bool got_user_data = wait_for_condition(
        [test_value](){ return g_isr_user_data_value.load() == test_value; },
        std::chrono::milliseconds(200)); // okay std namespace

    // Verify user data was passed correctly
    REQUIRE(got_user_data);
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

    // Wait for at least one call (with 200ms timeout)
    bool got_calls = wait_for_condition([](){ return g_isr_call_count.load() > 0; },
                                        std::chrono::milliseconds(200)); // okay std namespace
    REQUIRE(got_calls);
    int count_before_disable = g_isr_call_count.load();
    REQUIRE(count_before_disable > 0);

    // Disable handler
    result = isr::disableHandler(handle);
    REQUIRE(result == 0);
    REQUIRE(!isr::isHandlerEnabled(handle));

    // Wait up to 50ms to ensure count stabilizes (no more ISR calls)
    // Use condition variable to detect any calls immediately
    auto& manager = fl::isr::TimerThreadManager::instance();
    fl::unique_lock<fl::mutex> lock(manager.get_test_sync_mutex());

    int count_after_disable = count_before_disable;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50); // okay std namespace

    while (std::chrono::steady_clock::now() < deadline) { // okay std namespace
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>( // okay std namespace
            deadline - std::chrono::steady_clock::now()); // okay std namespace
        if (remaining.count() <= 0) break;

        int current_count = g_isr_call_count.load();
        if (current_count != count_after_disable) {
            count_after_disable = current_count;
            // Count changed, wait a bit more to ensure it stabilizes
            deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(30); // okay std namespace
        }

        manager.get_test_sync_cv().wait_for(lock, remaining);
    }

    // Final stabilized count
    count_after_disable = g_isr_call_count.load();
    lock.unlock();

    // Re-enable handler
    result = isr::enableHandler(handle);
    REQUIRE(result == 0);
    REQUIRE(isr::isHandlerEnabled(handle));

    // Wait for at least one new call after re-enabling (with 200ms timeout)
    bool got_new_calls = wait_for_condition(
        [count_after_disable](){ return g_isr_call_count.load() > count_after_disable; },
        std::chrono::milliseconds(200)); // okay std namespace
    REQUIRE(got_new_calls);
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

    // Wait for at least one call to verify timer is firing (with 200ms timeout)
    bool got_calls = wait_for_condition([](){ return g_isr_call_count.load() > 0; },
                                        std::chrono::milliseconds(200)); // okay std namespace
    REQUIRE(got_calls);
    int count_enabled = g_isr_call_count.load();
    REQUIRE(count_enabled > 0);

    // Globally disable interrupts
    interruptsDisable();
    REQUIRE(interruptsDisabled());

    // Wait up to 50ms to ensure count stabilizes (no more ISR calls)
    // Use condition variable to detect any calls immediately
    auto& manager = fl::isr::TimerThreadManager::instance();
    fl::unique_lock<fl::mutex> lock(manager.get_test_sync_mutex());

    int count_disabled = count_enabled;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50); // okay std namespace

    while (std::chrono::steady_clock::now() < deadline) { // okay std namespace
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>( // okay std namespace
            deadline - std::chrono::steady_clock::now()); // okay std namespace
        if (remaining.count() <= 0) break;

        int current_count = g_isr_call_count.load();
        if (current_count != count_disabled) {
            count_disabled = current_count;
            // Count changed, wait a bit more to ensure it stabilizes
            deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(30); // okay std namespace
        }

        manager.get_test_sync_cv().wait_for(lock, remaining);
    }

    // Final stabilized count
    count_disabled = g_isr_call_count.load();
    lock.unlock();

    // Re-enable global interrupts
    interruptsEnable();
    REQUIRE(interruptsEnabled());

    // Wait for at least one new call after re-enabling (with 200ms timeout)
    bool got_new_calls = wait_for_condition(
        [count_disabled](){ return g_isr_call_count.load() > count_disabled; },
        std::chrono::milliseconds(200)); // okay std namespace
    REQUIRE(got_new_calls);
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
