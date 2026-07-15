#pragma once

// IWYU pragma: private

/// @file irp_spi_peripheral.h
/// @brief Host-testable contract for RP2040 PL022 SPI DMA TX.

#include "fl/stl/cstddef.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

struct RpSpiConfig {
    u8 spi_index = 0;
    u8 mosi_pin = 0;
    u8 miso_pin = 0;
    u8 sck_pin = 0;
    u32 clock_hz = 0;
    u8 data_bits = 8;
    bool cpol = false;
    bool cpha = false;
    bool msb_first = true;
};

/// PL022 receives one byte for every byte it clocks out.  Implementations
/// must drain RX with DMA as well as feeding TX, otherwise the RX FIFO can
/// overrun and stall a long LED frame even though FastLED is transmit-only.
class IRpSpiPeripheral {
  public:
    virtual ~IRpSpiPeripheral() FL_NO_EXCEPT = default;

    virtual bool configure(const RpSpiConfig& config) FL_NO_EXCEPT = 0;
    virtual u32 actualClockHz() const FL_NO_EXCEPT = 0;
    virtual bool startTxDma(const u8* data, size_t size) FL_NO_EXCEPT = 0;
    virtual bool isTxDmaBusy() const FL_NO_EXCEPT = 0;
    virtual bool isRxDmaBusy() const FL_NO_EXCEPT = 0;
    virtual bool isWireBusy() const FL_NO_EXCEPT = 0;
    virtual bool hasError() const FL_NO_EXCEPT = 0;
    virtual u32 nowMicros() const FL_NO_EXCEPT = 0;
    virtual void abort() FL_NO_EXCEPT = 0;
    virtual void deinitialize() FL_NO_EXCEPT = 0;
};

}  // namespace fl
