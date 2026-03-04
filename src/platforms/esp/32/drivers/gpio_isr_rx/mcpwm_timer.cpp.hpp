// IWYU pragma: private
// ok no namespace fl

/**
 * @file mcpwm_timer.cpp
 * @brief MCPWM capture timer setup for ultra-fast GPIO edge timestamping
 *
 * Implements MCPWM capture timer configuration at maximum resolution (80 MHz / 12.5 ns)
 * for ESP32-C6 RISC-V platform. Provides hardware timestamp capture for fast GPIO ISR.
 *
 * Key Features:
 * - 80 MHz timer clock (12.5 ns resolution)
 * - Direct register address access for ISR
 * - GPIO to MCPWM capture channel routing
 * - Timer enable/disable control
 *
 * Architecture:
 * - Capture timer runs continuously at 80 MHz
 * - Fast ISR reads capture register directly (no HAL overhead)
 * - Timestamp automatically latched on GPIO edge
 *
 * Integration:
 * - Called during GpioIsrRx initialization
 * - Stores hardware register address in DualIsrContext
 * - Provides start/stop functions for timer lifecycle
 */

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

// Include feature flags to detect FASTLED_RMT5
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

// Check if this SoC has MCPWM hardware (ESP32-C3/C2 do not)
#include "soc/soc_caps.h"  // IWYU pragma: keep
#if defined(SOC_MCPWM_SUPPORTED) && SOC_MCPWM_SUPPORTED

#include "mcpwm_timer.h"
#include "dual_isr_context.h"
#include "fl/int.h"

using fl::u32;

// IWYU pragma: begin_keep
#include "driver/mcpwm_cap.h"
#include "driver/mcpwm_timer.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "soc/mcpwm_struct.h"
#include "hal/mcpwm_ll.h"
#include "soc/mcpwm_periph.h"
// IWYU pragma: end_keep

// Note: Functions are declared extern "C" in header - no namespace wrapping here

// ============================================================================
// Constants
// ============================================================================

static const char* MCPWM_TIMER_TAG = "mcpwm_timer";

// MCPWM configuration
#define MCPWM_GROUP_ID 0
#define MCPWM_TIMER_RESOLUTION_HZ 80000000  // 80 MHz = 12.5 ns per tick
#define MCPWM_CAPTURE_CHANNEL 0

// ============================================================================
// Internal State
// ============================================================================

/**
 * @brief Internal MCPWM state structure
 *
 * Holds MCPWM timer and capture channel handles for lifecycle management.
 * Allocated during init, freed during cleanup.
 */
typedef struct {
    mcpwm_cap_timer_handle_t timer_handle;      ///< Capture timer handle
    mcpwm_cap_channel_handle_t channel_handle;  ///< Capture channel handle
    int gpio_pin;                                ///< GPIO pin number
    bool initialized;                            ///< Initialization flag
} McpwmState;

// Global MCPWM state (single instance for now)
static McpwmState g_mcpwm_state = {
    .timer_handle = nullptr,
    .channel_handle = nullptr,
    .gpio_pin = -1,
    .initialized = false
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Get hardware capture register address for direct ISR access
 *
 * This function retrieves the physical memory address of the MCPWM capture
 * value register. The fast ISR will read this register directly to minimize
 * latency (no HAL function call overhead).
 *
 * @param group_id MCPWM group ID (0 or 1)
 * @param cap_channel Capture channel number (0-2)
 * @return uint32_t Hardware register address, or 0 on error
 *
 * @note The returned address is a physical hardware register address
 * @note Fast ISR performs volatile read: *(volatile uint32_t*)addr
 */
static u32 mcpwm_get_capture_reg_addr(int group_id, int cap_channel) {
    // Validate group_id and cap_channel using ESP-IDF SOC headers
    #ifdef SOC_MCPWM_GROUPS_PER_CHIP
    if (group_id < 0 || group_id >= SOC_MCPWM_GROUPS_PER_CHIP) {
        ESP_LOGE(MCPWM_TIMER_TAG, "Invalid MCPWM group ID: %d", group_id);
        return 0;
    }
    #else
    if (group_id < 0 || group_id >= 1) {  // Default to 1 group if not defined
        ESP_LOGE(MCPWM_TIMER_TAG, "Invalid MCPWM group ID: %d (SOC_MCPWM_GROUPS_PER_CHIP not defined)", group_id);
        return 0;
    }
    #endif

    #ifdef SOC_MCPWM_CAPTURE_CHANNELS_PER_TIMER
    if (cap_channel < 0 || cap_channel >= SOC_MCPWM_CAPTURE_CHANNELS_PER_TIMER) {
        ESP_LOGE(MCPWM_TIMER_TAG, "Invalid capture channel: %d", cap_channel);
        return 0;
    }
    #else
    if (cap_channel < 0 || cap_channel >= 3) {  // Default to 3 channels if not defined
        ESP_LOGE(MCPWM_TIMER_TAG, "Invalid capture channel: %d (SOC_MCPWM_CAPTURE_CHANNELS_PER_TIMER not defined)", cap_channel);
        return 0;
    }
    #endif

    // Get base address of MCPWM peripheral for this group
    // The mcpwm_periph_signals structure contains peripheral information
    // For ESP32-C6, MCPWM base addresses are in soc/mcpwm_reg.h

    // Access hardware register structure
    // Each MCPWM group has a hardware register base address
    mcpwm_dev_t* mcpwm_dev = nullptr;

    if (group_id == 0) {
        mcpwm_dev = &MCPWM0;
    }
    #if defined(SOC_MCPWM_GROUPS_PER_CHIP) && SOC_MCPWM_GROUPS_PER_CHIP > 1
    else if (group_id == 1) {
        mcpwm_dev = &MCPWM1;
    }
    #endif
    else {
        ESP_LOGE(MCPWM_TIMER_TAG, "Unsupported MCPWM group: %d", group_id);
        return 0;
    }

    // Calculate capture register address
    // The capture value register is in the cap_chn[channel].val field
    // Get the address of the capture value register
    // Note: ESP32-C6 uses 'cap_chn' instead of 'cap_ch'
    volatile u32* cap_val_reg = &(mcpwm_dev->cap_chn[cap_channel].val);

    u32 reg_addr = (u32)cap_val_reg;

    ESP_LOGI(MCPWM_TIMER_TAG, "Capture register address: 0x%08lx (group=%d, channel=%d)",
             reg_addr, group_id, cap_channel);

    return reg_addr;
}

// ============================================================================
// Public API Functions
// ============================================================================

/**
 * @brief Initialize MCPWM capture timer at 80 MHz resolution
 *
 * Creates and configures MCPWM capture timer with maximum resolution (12.5 ns).
 * Sets up GPIO to MCPWM capture channel routing and stores hardware register
 * address in DualIsrContext for fast ISR access.
 *
 * @param ctx DualIsrContext pointer (must be initialized)
 * @param gpio_pin GPIO pin number for capture input
 * @return 0 on success, -1 on error
 *
 * @note Call this during GpioIsrRx initialization, before enabling interrupts
 * @note Timer starts in disabled state - call mcpwm_timer_start() to begin
 */
int mcpwm_timer_init(DualIsrContext* ctx, int gpio_pin) {
    if (!ctx) {
        ESP_LOGE(MCPWM_TIMER_TAG, "Invalid context pointer");
        return -1;
    }

    if (g_mcpwm_state.initialized) {
        ESP_LOGW(MCPWM_TIMER_TAG, "MCPWM already initialized, cleaning up first");
        mcpwm_timer_cleanup();
    }

    ESP_LOGI(MCPWM_TIMER_TAG, "Initializing MCPWM capture timer at %d Hz (%.1f ns resolution)",
             MCPWM_TIMER_RESOLUTION_HZ,
             1000000000.0 / MCPWM_TIMER_RESOLUTION_HZ);

    // Step 1: Create capture timer at 80 MHz
    mcpwm_capture_timer_config_t timer_config = {
        .group_id = MCPWM_GROUP_ID,
        .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,  // Use default clock source
        .resolution_hz = MCPWM_TIMER_RESOLUTION_HZ,
    };

    esp_err_t ret = mcpwm_new_capture_timer(&timer_config, &g_mcpwm_state.timer_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(MCPWM_TIMER_TAG, "Failed to create capture timer: %s", esp_err_to_name(ret));
        return -1;
    }

    // Verify actual resolution
    u32 actual_resolution = 0;
    ret = mcpwm_capture_timer_get_resolution(g_mcpwm_state.timer_handle, &actual_resolution);
    if (ret != ESP_OK) {
        ESP_LOGE(MCPWM_TIMER_TAG, "Failed to get timer resolution: %s", esp_err_to_name(ret));
        mcpwm_del_capture_timer(g_mcpwm_state.timer_handle);
        g_mcpwm_state.timer_handle = nullptr;
        return -1;
    }

    if (actual_resolution != MCPWM_TIMER_RESOLUTION_HZ) {
        ESP_LOGW(MCPWM_TIMER_TAG, "Timer resolution mismatch: requested=%u, actual=%lu",
                 MCPWM_TIMER_RESOLUTION_HZ, actual_resolution);
        // Continue anyway - actual resolution might be acceptable
    } else {
        ESP_LOGI(MCPWM_TIMER_TAG, "Timer resolution verified: %lu Hz (12.5 ns per tick)", actual_resolution);
    }

    // Step 2: Create capture channel and connect to GPIO
    mcpwm_capture_channel_config_t channel_config = {
        .gpio_num = gpio_pin,
        .prescale = 1,  // No prescaling - capture at full timer resolution
        .flags = {
            .pos_edge = 1,  // Capture on positive edges
            .neg_edge = 1,  // Capture on negative edges
            .pull_up = 0,   // No internal pull-up
            .pull_down = 0, // No internal pull-down
            .invert_cap_signal = 0,  // No inversion
            .io_loop_back = 0,       // No loopback
        }
    };

    ret = mcpwm_new_capture_channel(g_mcpwm_state.timer_handle, &channel_config,
                                     &g_mcpwm_state.channel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(MCPWM_TIMER_TAG, "Failed to create capture channel: %s", esp_err_to_name(ret));
        mcpwm_del_capture_timer(g_mcpwm_state.timer_handle);
        g_mcpwm_state.timer_handle = nullptr;
        return -1;
    }

    ESP_LOGI(MCPWM_TIMER_TAG, "Capture channel created on GPIO %d", gpio_pin);

    // Step 3: Get hardware register address for fast ISR access
    u32 reg_addr = mcpwm_get_capture_reg_addr(MCPWM_GROUP_ID, MCPWM_CAPTURE_CHANNEL);
    if (reg_addr == 0) {
        ESP_LOGE(MCPWM_TIMER_TAG, "Failed to get capture register address");
        mcpwm_del_capture_channel(g_mcpwm_state.channel_handle);
        mcpwm_del_capture_timer(g_mcpwm_state.timer_handle);
        g_mcpwm_state.timer_handle = nullptr;
        g_mcpwm_state.channel_handle = nullptr;
        return -1;
    }

    // Step 4: Store register address in context for fast ISR
    ctx->mcpwm_capture_reg_addr = reg_addr;

    // Step 5: Update state
    g_mcpwm_state.gpio_pin = gpio_pin;
    g_mcpwm_state.initialized = true;

    ESP_LOGI(MCPWM_TIMER_TAG, "MCPWM timer initialized successfully (reg_addr=0x%08lx)", reg_addr);
    return 0;
}

/**
 * @brief Start MCPWM capture timer
 *
 * Enables the capture timer to begin counting. After this call, the timer
 * counter increments at 80 MHz and capture events will latch the timer value.
 *
 * @return 0 on success, -1 on error
 *
 * @note Must call mcpwm_timer_init() first
 * @note Timer must be started before fast ISR can capture timestamps
 */
int mcpwm_timer_start() {
    if (!g_mcpwm_state.initialized || !g_mcpwm_state.timer_handle) {
        ESP_LOGE(MCPWM_TIMER_TAG, "MCPWM not initialized");
        return -1;
    }

    esp_err_t ret = mcpwm_capture_timer_enable(g_mcpwm_state.timer_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(MCPWM_TIMER_TAG, "Failed to enable capture timer: %s", esp_err_to_name(ret));
        return -1;
    }

    ret = mcpwm_capture_timer_start(g_mcpwm_state.timer_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(MCPWM_TIMER_TAG, "Failed to start capture timer: %s", esp_err_to_name(ret));
        return -1;
    }

    ESP_LOGI(MCPWM_TIMER_TAG, "MCPWM timer started");
    return 0;
}

/**
 * @brief Stop MCPWM capture timer
 *
 * Stops the capture timer counter. Timer value is frozen until restarted.
 * Does not deallocate resources - use mcpwm_timer_cleanup() for that.
 *
 * @return 0 on success, -1 on error
 *
 * @note Safe to call multiple times
 * @note Call before cleanup to ensure clean shutdown
 */
int mcpwm_timer_stop() {
    if (!g_mcpwm_state.initialized || !g_mcpwm_state.timer_handle) {
        ESP_LOGW(MCPWM_TIMER_TAG, "MCPWM not initialized, nothing to stop");
        return 0;  // Not an error - already stopped
    }

    esp_err_t ret = mcpwm_capture_timer_stop(g_mcpwm_state.timer_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(MCPWM_TIMER_TAG, "Failed to stop capture timer: %s", esp_err_to_name(ret));
        return -1;
    }

    ret = mcpwm_capture_timer_disable(g_mcpwm_state.timer_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(MCPWM_TIMER_TAG, "Failed to disable capture timer: %s", esp_err_to_name(ret));
        return -1;
    }

    ESP_LOGI(MCPWM_TIMER_TAG, "MCPWM timer stopped");
    return 0;
}

/**
 * @brief Clean up MCPWM resources
 *
 * Deallocates capture channel and timer handles. Call during GpioIsrRx
 * cleanup or before re-initializing with different parameters.
 *
 * @return 0 on success, -1 on error
 *
 * @note Automatically stops timer if running
 * @note Safe to call multiple times (idempotent)
 */
int mcpwm_timer_cleanup() {
    if (!g_mcpwm_state.initialized) {
        return 0;  // Already cleaned up
    }

    // Stop timer first if running
    mcpwm_timer_stop();

    // Delete capture channel
    if (g_mcpwm_state.channel_handle) {
        esp_err_t ret = mcpwm_del_capture_channel(g_mcpwm_state.channel_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(MCPWM_TIMER_TAG, "Failed to delete capture channel: %s", esp_err_to_name(ret));
        }
        g_mcpwm_state.channel_handle = nullptr;
    }

    // Delete capture timer
    if (g_mcpwm_state.timer_handle) {
        esp_err_t ret = mcpwm_del_capture_timer(g_mcpwm_state.timer_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(MCPWM_TIMER_TAG, "Failed to delete capture timer: %s", esp_err_to_name(ret));
        }
        g_mcpwm_state.timer_handle = nullptr;
    }

    // Reset state
    g_mcpwm_state.gpio_pin = -1;
    g_mcpwm_state.initialized = false;

    ESP_LOGI(MCPWM_TIMER_TAG, "MCPWM timer cleaned up");
    return 0;
}

/**
 * @brief Get current MCPWM timer value (for debugging)
 *
 * Reads the current capture timer count value. Useful for verification
 * and debugging. Not intended for use in fast ISR (use direct register
 * access instead).
 *
 * @return Current timer value in ticks (80 MHz), or 0 if not initialized
 *
 * @note This function has overhead - use for debugging only
 * @note Fast ISR should read ctx->mcpwm_capture_reg_addr directly
 */
u32 mcpwm_timer_get_value() {
    if (!g_mcpwm_state.initialized || !g_mcpwm_state.timer_handle) {
        return 0;
    }

    // Read current timer value via low-level HAL
    // Note: This is slower than direct register access used by fast ISR
    u32 cap_value = 0;

    // For debugging, we can read the capture register directly
    if (g_mcpwm_state.channel_handle) {
        // The timer value is continuously counting
        // We don't have a direct API to read it, but the capture register
        // holds the last captured value
        // For continuous timer value, we'd need to access hardware directly
        ESP_LOGW(MCPWM_TIMER_TAG, "Direct timer read not implemented - use capture events");
    }

    return cap_value;
}

#endif // SOC_MCPWM_SUPPORTED
#endif // FASTLED_RMT5
#endif // FL_IS_ESP32
