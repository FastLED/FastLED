#pragma once

// ok no namespace fl

/**
 * @file mcpwm_timer.h
 * @brief MCPWM capture timer interface for ultra-fast GPIO edge timestamping
 *
 * Public API for MCPWM timer lifecycle management:
 * - Initialization at 80 MHz (12.5 ns resolution)
 * - Start/stop timer control
 * - Cleanup and resource deallocation
 *
 * Usage Pattern:
 * ```cpp
 * DualIsrContext ctx;
 * mcpwm_timer_init(&ctx, GPIO_NUM_5);  // Init with GPIO pin
 * mcpwm_timer_start();                 // Start counting
 * // ... fast ISR reads ctx.mcpwm_capture_reg_addr ...
 * mcpwm_timer_stop();                  // Stop when done
 * mcpwm_timer_cleanup();               // Free resources
 * ```
 */

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

// Check if this SoC has MCPWM hardware (ESP32-C3/C2 do not)
#include "soc/soc_caps.h"  // IWYU pragma: keep
#if defined(SOC_MCPWM_SUPPORTED) && SOC_MCPWM_SUPPORTED

#include "platforms/esp/32/drivers/gpio_isr_rx/dual_isr_context.h"
#include "fl/stl/stdint.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"

#ifdef __cplusplus
extern "C" {
#endif

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
 * @note Stores hardware register address in ctx->mcpwm_capture_reg_addr
 */
int mcpwm_timer_init(DualIsrContext* ctx, int gpio_pin) FL_NOEXCEPT;

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
int mcpwm_timer_start() FL_NOEXCEPT;

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
int mcpwm_timer_stop() FL_NOEXCEPT;

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
int mcpwm_timer_cleanup() FL_NOEXCEPT;

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
uint32_t mcpwm_timer_get_value() FL_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif // SOC_MCPWM_SUPPORTED
#endif // FL_IS_ESP32
