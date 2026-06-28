/// @file objectfled_spi_mode.h
/// @brief ObjectFLED SPI-mode hardware helpers (paired with the clockless
/// ObjectFLED driver in OjectFLED.cpp.hpp). The clockless mode owns
/// QTimer4 + XBAR1 + 3 eDMA channels writing to GPIO1_DR_SET/CLEAR (via
/// the IOMUXC_GPR_GPR26 GPIO6->GPIO1 remap). This SPI mode owns
/// FlexPWM2_SM0 + 1 eDMA channel writing whole 32-bit words to GPIO6_DR
/// directly. Both modes serialize through ObjectFLEDDmaManager::acquire()
/// so they never run concurrently on the same GPIO6 bank.
///
/// The unified ChannelEngineObjectFLED chooses between the two modes per
/// channel based on `ChannelData::isSpi()`. See #3428 and
/// `src/fl/channels/README.md` -> "Rule: Parallel-IO peripherals -- one
/// engine for both clockless and SPI modes".

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Routing info for an (MOSI, SCLK) pin pair on GPIO6.
struct ObjectFLEDSPIPinInfo {
    u8  mosi_pin;          ///< Teensy MOSI pin number
    u8  sclk_pin;          ///< Teensy SCLK pin number
    u8  mosi_bit;          ///< GPIO6 bit index for MOSI (0..31)
    u8  sclk_bit;          ///< GPIO6 bit index for SCLK (0..31)
    u32 mosi_mask;         ///< (1u << mosi_bit)
    u32 sclk_mask;         ///< (1u << sclk_bit)
    volatile u32* mosi_mux_reg;  ///< IOMUXC pad-mux register
    volatile u32* mosi_pad_reg;  ///< IOMUXC pad-control register
    volatile u32* sclk_mux_reg;
    volatile u32* sclk_pad_reg;
};

/// Look up a (MOSI, SCLK) pin pair. Both pins MUST be GPIO6-resident
/// (Teensy 4.0: pins 0, 1, 14-27 ; Teensy 4.1 adds 38-41). Returns false
/// on aliasing, bit-mask conflict, or wrong GPIO bank.
bool objectfled_spi_lookup_pins(u8 mosi_pin, u8 sclk_pin,
                                ObjectFLEDSPIPinInfo* info) FL_NO_EXCEPT;

/// Initialize FlexPWM2_SM0 to fire DMA at 2*clock_hz. `clock_hz` is clamped
/// to [100 kHz, 25 MHz]; returns false on clock_hz == 0. If already
/// initialized, deinit()s first.
bool objectfled_spi_init(const ObjectFLEDSPIPinInfo& pin_info,
                         u32 clock_hz) FL_NO_EXCEPT;

/// Begin a DMA-driven SPI transmit of `num_bytes` (max 64) from `buffer`,
/// MSB-first, Mode 0. Returns false if not initialized or arguments
/// invalid. Caller should `objectfled_spi_wait()` before re-arming.
bool objectfled_spi_show(const u8* buffer, u32 num_bytes) FL_NO_EXCEPT;

/// Block until the in-flight DMA completes (50 ms bounded timeout +
/// 5 ms shifter drain). Force-recovers on timeout.
void objectfled_spi_wait() FL_NO_EXCEPT;

/// Stop FlexPWM2, free DMA, restore pad mux. Releases the shared
/// ObjectFLEDDmaManager slot so clockless mode can take over.
void objectfled_spi_deinit() FL_NO_EXCEPT;

/// @brief Diagnostic snapshot of the DMA + QTimer3 + XBAR1 register state
/// after the most recent `objectfled_spi_show()` + `objectfled_spi_wait()`.
/// Used by `objectfledSpiSelfTest` RPC to surface why DMA didn't complete
/// (without scope-on-wire). Populated by `objectfled_spi_read_diagnostics`.
struct ObjectFLEDSPIDiagnostics {
    u32 dma_erq;          ///< eDMA ERQ register (channel-N bit shows if our channel is enabled)
    u32 dma_int;          ///< eDMA INT register (channel-N bit shows major-loop done)
    u32 dma_err;          ///< eDMA ERR register (channel-N bit shows error)
    u32 dma_es;           ///< eDMA ES (error status detail)
    u32 dma_channel;      ///< Our DMA channel index (0..31)
    u32 dma_citer;        ///< TCD.CITER -- counts down each minor loop; 0 = transfer done
    u32 dma_biter;        ///< TCD.BITER -- the initial value CITER was loaded from
    u32 dma_dlastsga;     ///< TCD.DLASTSGA -- destination last-address-after-major-loop
    u16 tmr3_cntr0;       ///< QTimer3 ch0 counter (should advance after enable)
    u16 tmr3_csctrl0;     ///< QTimer3 ch0 compare-status-and-control (TCF1 = compare-1 fired)
    u16 tmr3_sctrl0;      ///< QTimer3 ch0 status (TCF = timer-compare-flag)
    u16 tmr3_ctrl0;       ///< QTimer3 ch0 control (CM=count mode; should be nonzero when running)
    u16 tmr3_enbl;        ///< QTimer3 enable register (bit 0 = ch0 enabled)
    u16 xbar1_ctrl1;      ///< XBARA1 CTRL1 (slot 1 = our edge-detect for req95)
    bool dma_complete;    ///< sSpiDmaComplete flag (set by ISR)
    bool initialized;     ///< sSpiInitialized
};

/// @brief Read current DMA/QTimer3/XBAR1 register state into `out`.
/// Safe to call any time; no side effects on hardware state.
void objectfled_spi_read_diagnostics(ObjectFLEDSPIDiagnostics* out) FL_NO_EXCEPT;

}  // namespace fl

#endif  // FL_IS_TEENSY_4X
