/// @file flexio_driver.h
/// @brief Low-level FlexIO2 WS2812 driver for IMXRT1062 (Teensy 4.x)
///
/// Uses a 1-shifter + 1-timer architecture (post-#3415 Round-5 rewrite):
/// - Shifter 0: Transmit mode, LSB-first, PINCFG=3 drives the output pin
///   directly from shifter[0]. DMA writes pre-encoded 32-bit words
///   (4 FlexIO bits per WS2812 bit) into SHIFTBUF on each shifter-empty
///   event.
/// - Timer 0: Dual 8-bit baud mode. Lower byte = baud divider
///   (kFlexIOBaudDiv=18 -> 317 ns per FlexIO shift). Upper byte = bit
///   count (63 -> 32 bits per shifter word). Triggered by shifter
///   status flag (TRGSEL=1, TRGPOL=1 active-low) so the timer runs
///   while the shifter has data and stops when it empties.
///
/// The earlier 4-timer + Timer-output-OR design described in
/// RESEARCH.md §8 was abandoned in #3415 because the timer-conditional
/// gating it required isn't actually expressible in FlexIO's TRGSEL
/// encoding.
///
/// Important: the `t0h_ns`, `t1h_ns`, and `period_ns` parameters of
/// flexio_init() are currently IGNORED; the driver hard-codes
/// WS2812B nominal timing via the baud divider and the 4-bit
/// `0xE`/`0x1` nibble encoder. See #3416 FX-MED-1.

#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief FlexIO2 pin mapping entry
struct FlexIOPinInfo {
    u8 teensy_pin;       ///< Teensy digital pin number
    u8 flexio_pin;       ///< FlexIO2 pin number (for PINSEL fields)
    volatile u32* mux_reg;  ///< IOMUXC mux register address
    volatile u32* pad_reg;  ///< IOMUXC pad control register address
};

/// @brief Look up FlexIO2 pin info for a Teensy pin
/// @param teensy_pin Teensy digital pin number
/// @param[out] info Filled with pin mapping if found
/// @return true if pin has a FlexIO2 mapping, false otherwise
bool flexio_lookup_pin(u8 teensy_pin, FlexIOPinInfo* info) FL_NO_EXCEPT;

/// @brief Initialize FlexIO2 hardware for WS2812 waveform generation
/// @param pin_info Pin mapping info (from flexio_lookup_pin)
/// @param t0h_ns T0H timing in nanoseconds (high time for '0' bit)
/// @param t1h_ns T1H timing in nanoseconds (high time for '1' bit)
/// @param period_ns Total bit period in nanoseconds
/// @param reset_us Reset/latch time in microseconds
/// @return true on success, false on failure
bool flexio_init(const FlexIOPinInfo& pin_info, u32 t0h_ns, u32 t1h_ns,
                 u32 period_ns, u32 reset_us) FL_NO_EXCEPT;

/// @brief Start DMA transfer of pixel data to FlexIO2
/// @param pixel_data Pointer to encoded pixel bytes (must be 32-bit aligned)
/// @param num_bytes Number of bytes to transmit
/// @return true if DMA transfer started, false on error
bool flexio_show(const u8* pixel_data, u32 num_bytes) FL_NO_EXCEPT;

/// @brief Check if FlexIO2 DMA transfer is complete
/// @return true if transfer is done (or no transfer active), false if still running
bool flexio_is_done() FL_NO_EXCEPT;

/// @brief Block until FlexIO2 DMA transfer completes
void flexio_wait() FL_NO_EXCEPT;

/// @brief Shut down FlexIO2 and release resources
void flexio_deinit() FL_NO_EXCEPT;

/// @brief FlexIO2 hardware state snapshot for bring-up diagnostics.
/// Filled by flexio_read_diagnostics() with the live register values so
/// the autoresearch RPC can report whether the peripheral is actually
/// running, what its shifter/timer/DMA state is, and what the TCD looks
/// like after the last flexio_show().
struct FlexIODiagnostics {
    u32 ctrl;
    u32 shiftstat;
    u32 shifterr;
    u32 timstat;
    u32 shiftsden;
    u32 shiftctl0;
    u32 shiftcfg0;
    u32 timctl0;
    u32 timcfg0;
    u32 timcmp0;
    u32 ccm_ccgr3;
    u32 ccm_cscmr2;
    u32 ccm_cs1cdr;
    u32 muxRegValue;
    u32 padRegValue;
    u32 tcd_saddr;
    u32 tcd_daddr;
    u32 tcd_citer;
    u32 tcd_biter;
    u32 tcd_csr;
    bool initialized;
    bool dmaComplete;
};

/// @brief Snapshot the FlexIO2 register state for the most recent pin.
void flexio_read_diagnostics(FlexIODiagnostics* out) FL_NO_EXCEPT;

} // namespace fl
