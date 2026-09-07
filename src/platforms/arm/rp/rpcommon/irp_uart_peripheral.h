#pragma once

// IWYU pragma: private

/// @file irp_uart_peripheral.h
/// @brief Host-testable asynchronous UART+DMA contract for RP2040/RP2350.

#include "fl/stl/cstddef.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

struct RpUartConfig {
    u8 uart_index = 0;
    u8 tx_pin = 0;
    u32 baud_rate = 0;
    u8 data_bits = 8;
    bool invert_tx = true;
};

/// DMA completion only means that the UART FIFO has accepted every source
/// byte.  `isWireBusy()` remains true while the PL011 shift register sends the
/// final start/data/stop bits, which is why it is deliberately separate.
/// Generic ceiling used when a backend cannot report its own clocking.
constexpr u32 kRpUartBaudCeiling = 6250000;

class IRpUartPeripheral {
  public:
    virtual ~IRpUartPeripheral() FL_NO_EXCEPT = default;

    virtual bool configure(const RpUartConfig& config) FL_NO_EXCEPT = 0;
    virtual u32 actualBaudRate() const FL_NO_EXCEPT = 0;

    /// Highest baud this backend can actually reach, or 0 when the UART is
    /// unusable (for example clk_peri is not running).
    ///
    /// The PL011 divides clk_peri by 16, so clk_peri bounds the achievable
    /// baud. Deliberately pure: a backend that silently reported the generic
    /// ceiling would let the engine accept timing the hardware cannot send.
    virtual u32 maxBaudRate() const FL_NO_EXCEPT = 0;
    virtual bool startTxDma(const u8* data, size_t size) FL_NO_EXCEPT = 0;
    virtual bool isDmaBusy() const FL_NO_EXCEPT = 0;
    virtual bool isWireBusy() const FL_NO_EXCEPT = 0;
    virtual bool hasError() const FL_NO_EXCEPT = 0;
    virtual u32 nowMicros() const FL_NO_EXCEPT = 0;
    virtual void abort() FL_NO_EXCEPT = 0;
    virtual void deinitialize() FL_NO_EXCEPT = 0;
};

}  // namespace fl
