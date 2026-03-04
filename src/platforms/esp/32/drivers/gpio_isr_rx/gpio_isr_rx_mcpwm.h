/**
 * @file gpio_isr_rx_mcpwm.h
 * @brief Dual-ISR GPIO RX interface with MCPWM hardware timestamps
 *
 * This is the new implementation that uses the dual-ISR architecture:
 * - Fast ISR (RISC-V assembly): Captures edges with MCPWM timestamps
 * - Slow ISR (C, GPTimer 10µs): Processes buffer and applies filtering
 *
 * Factory function:
 * - GpioIsrRxMcpwm_create(int pin) - Creates MCPWM-based implementation
 */

#pragma once

#include "fl/stl/shared_ptr.h"  // IWYU pragma: keep
#include "fl/rx_device.h"

namespace fl {

// Forward declaration
class GpioIsrRx;

/**
 * @brief Create GPIO ISR RX instance with MCPWM hardware timestamps
 * @param pin GPIO pin number for receiving signals
 * @return Shared pointer to GpioIsrRx interface
 *
 * This factory creates the new dual-ISR implementation that uses:
 * - MCPWM timer for hardware timestamps (12.5 ns resolution)
 * - Fast assembly ISR for edge capture (<130 ns latency)
 * - Slow management ISR for filtering and timeout detection
 */
fl::shared_ptr<GpioIsrRx> GpioIsrRxMcpwm_create(int pin);

} // namespace fl
