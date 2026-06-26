/// @file lpuart_driver.h
/// @brief Low-level LPUART WS2812 driver for IMXRT1062 (Teensy 4.x)
///
/// Drives a WS2812-class clockless LED strip via the LPUART hardware in
/// inverted-TX mode + eDMA. At 4 Mbps with TXINV=1 every wire byte takes
/// 2.5 us (10 wire bits at 250 ns each) and encodes 2 consecutive WS2812
/// bits:
///
///   wire bit 0     = start bit (forced HIGH by TXINV)
///   wire bits 1-4  = data bits b0..b3 (inverted on the wire)
///   wire bit 5     = data bit b4 (inverted)
///   wire bits 6-8  = data bits b5..b7 (inverted)
///   wire bit 9     = stop bit (forced LOW by TXINV)
///
/// Per WS2812B-V5 timing the two patterns we need on the wire are:
///   '0' bit = H L L L L  (250 ns HIGH + 1000 ns LOW)
///   '1' bit = H H H L L  (750 ns HIGH + 500 ns LOW)
///
/// Solving for the data byte:
///   (WS0, WS1) = (0,0) -> 0xEF
///   (WS0, WS1) = (0,1) -> 0x8F
///   (WS0, WS1) = (1,0) -> 0xEC
///   (WS0, WS1) = (1,1) -> 0x8C
///
/// One WS2812 byte (8 bits) therefore needs 4 UART bytes; 100 LEDs = 1200
/// UART bytes per frame. eDMA streams the encoded buffer to LPUARTn_DATA.

#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Per-pin descriptor populated by lpuart_lookup_pin().
struct LpuartPinInfo {
    u8 teensy_pin;          ///< Teensy digital pin number
    u8 lpuart_index;        ///< 1..8 -- which LPUARTn
    u8 mux_alt;             ///< IOMUXC ALT value (always 2 for LPUART TX on RT1062)
    volatile u32* mux_reg;  ///< IOMUXC SW_MUX_CTL register
    volatile u32* pad_reg;  ///< IOMUXC SW_PAD_CTL register
};

/// @brief Resolve a Teensy pin into LPUART pin info.
/// @return true if the pin maps to an LPUART TX peripheral on this board.
bool lpuart_lookup_pin(u8 teensy_pin, LpuartPinInfo* info) FL_NO_EXCEPT;

/// @brief Initialize the LPUART for WS2812 TX.
/// @param pin_info Pin descriptor from lpuart_lookup_pin().
/// @param reset_us Reset/latch time in microseconds (decides inter-frame gap).
bool lpuart_init(const LpuartPinInfo& pin_info, u32 reset_us) FL_NO_EXCEPT;

/// @brief Encode `num_pixel_bytes` raw WS2812 bytes into the UART byte
///        stream and start a DMA transmission.
/// @param pixel_data  Raw WS2812 byte array (R/G/B per LED, GRB order
///                    for WS2812 -- caller arranges).
/// @param num_pixel_bytes Number of raw bytes. UART buffer needed =
///        num_pixel_bytes * 4.
/// @return true if DMA was queued.
bool lpuart_show(const u8* pixel_data, u32 num_pixel_bytes) FL_NO_EXCEPT;

/// @brief Stream a PRE-ENCODED wave8 buffer to the LPUART. Bypasses the
///        internal encoder for callers (like ChannelEngineLPUART) that
///        do their own encoding into an externally-owned buffer.
/// @param encoded UART bytes already in wave8 form (4 per WS2812 byte).
/// @param num_uart_bytes Length in bytes.
bool lpuart_show_encoded(const u8* encoded, u32 num_uart_bytes) FL_NO_EXCEPT;

/// @brief Non-blocking completion check.
bool lpuart_is_done() FL_NO_EXCEPT;

/// @brief Block (bounded) until the current TX completes.
void lpuart_wait() FL_NO_EXCEPT;

/// @brief Tear down the LPUART + DMA channel + restore the pad.
void lpuart_deinit() FL_NO_EXCEPT;

/// @brief LPUART diagnostic snapshot (mirrors the FlexIO/ObjectFLED pattern).
struct LpuartDiagnostics {
    u32 baud;
    u32 ctrl;
    u32 stat;
    u32 fifo;
    u32 tcd_saddr;
    u32 tcd_daddr;
    u32 tcd_citer;
    u32 tcd_biter;
    u32 tcd_csr;
    u8 lpuart_index;
    u8 pin;
    bool initialized;
    bool dma_complete;
};

/// @brief Snapshot live LPUART + DMA TCD state (atomically gated).
void lpuart_read_diagnostics(LpuartDiagnostics* out) FL_NO_EXCEPT;

} // namespace fl
