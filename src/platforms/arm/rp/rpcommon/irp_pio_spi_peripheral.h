#pragma once

// IWYU pragma: private

/// @file irp_pio_spi_peripheral.h
/// @brief Host-testable contract for PIO-backed SPI TX on RP2xxx.

#include "fl/stl/cstddef.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

struct RpPioSpiConfig {
    u8 mosi_pin = 0;
    u8 sck_pin = 0;
    u32 clock_hz = 0;
};

/// PIO SPI is the FLEX_IO fallback for SPI pin pairs which cannot use the
/// RP2040's fixed SPI0/SPI1 muxes. DMA completion alone is insufficient: the
/// final PIO bit must also return the clock to its idle-low state.
class IRpPioSpiPeripheral {
  public:
    virtual ~IRpPioSpiPeripheral() FL_NO_EXCEPT = default;

    virtual bool configure(const RpPioSpiConfig& config) FL_NO_EXCEPT = 0;
    virtual bool startTxDma(const u32* words, size_t word_count) FL_NO_EXCEPT = 0;
    virtual bool isDmaBusy() const FL_NO_EXCEPT = 0;
    virtual bool isTerminalComplete() const FL_NO_EXCEPT = 0;
    virtual bool hasError() const FL_NO_EXCEPT = 0;
    virtual u32 nowMicros() const FL_NO_EXCEPT = 0;
    virtual void abort() FL_NO_EXCEPT = 0;
    virtual void deinitialize() FL_NO_EXCEPT = 0;
};

}  // namespace fl
