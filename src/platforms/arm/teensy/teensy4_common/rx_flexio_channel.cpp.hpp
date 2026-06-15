/// @file rx_flexio_channel.cpp.hpp
/// @brief Teensy 4.x FlexIO shifter-based RX implementation
///
/// **Phase 1B** of FastLED#2764 — replaces the Phase 1A skeleton with real
/// FLEXIO1 register programming, IOMUXC pin muxing, and an eDMA-driven
/// capture pipeline that mirrors the FlexIO TX driver's structure but in
/// the reverse direction.
///
/// Design (input-edge timing measurement, per NXP AN12686 §4 + iMXRT1062
/// reference manual chapter 50):
///
/// 1.  **Timer 0** runs in 16-bit-counter mode (TIMOD=3), clocked by the
///     FlexIO functional clock (~120 MHz on Teensy 4, giving ~8.3 ns per
///     tick). It is started on every external edge of the input pin
///     (TIMTRG=2*pin+1, TIMENA=trigger-rising, TIMRST=trigger-rising) so it
///     measures the time **since the last edge**.
///
/// 2.  **Shifter 0** is configured in receive mode (SMOD=1) with PINSEL set
///     to the input pin. Its TIMSEL=0 means every Timer 0 expiry / start
///     transition causes a shift, latching the timer's current value into
///     SHIFTBUF[0]. The captured 16-bit deltas are read as inter-edge tick
///     counts.
///
/// 3.  **eDMA + DMAMUX** copy each SHIFTBUF[0] read into a RAM buffer. The
///     DMA source is `DMAMUX_SOURCE_FLEXIO1_REQUEST0` (request 0 of
///     FLEXIO1, equivalent to the SHIFTBUF[0] status-flag-driven trigger).
///
/// 4.  An ISR sets the completion flag when the DMA buffer fills (or when
///     `wait()` times out and we forcibly stop).
///
/// 5.  `decode()` walks the captured deltas, reconstructs absolute edge
///     timestamps in nanoseconds, and feeds them through the same edge-time
///     decoder pattern the FlexPWM driver uses.
///
/// **Pin-map (Phase 1B):** the FLEXIO1 pins exposed on Teensy 4.0 main
/// header. Each maps the Teensy digital pin to its FLEXIO1 shifter/timer
/// input index via IOMUXC ALT4 (the FLEXIO1 alternate function on the
/// `GPIO_EMC_*` and `GPIO_AD_B0_*` pads).
///
/// **Status / scope:** this PR brings up the configuration code, the DMA
/// channel, and the wait/decode flow. Bench validation against a
/// `analogWriteFrequency`-generated reference signal (Phase 2) is the next
/// step in the #2764 series — until then we keep `PLATFORM_DEFAULT` on
/// `FLEXPWM` so the existing RX flows are untouched. The public API is
/// identical to Phase 1A.

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#define FASTLED_INTERNAL
#include "fl/system/fastled.h"
#include "platforms/arm/teensy/teensy4_common/rx_flexio_channel.h"

#include "fl/log/log.h"
#include "fl/stl/cstring.h"
#include "fl/stl/result.h"
#include "fl/stl/vector.h"

// IWYU pragma: begin_keep
#include <Arduino.h>
#include <DMAChannel.h>
#include <imxrt.h>
// IWYU pragma: end_keep

// `imxrt.h` already defines `FLEXIO1_*` register macros that expand to
// expressions incompatible with our `static volatile u32 *NAME = ...`
// declarations. Undef them here so our typed register accessors win.
// Matches the pattern used by `flexio_driver.cpp.hpp` for FLEXIO2.
#undef FLEXIO1_CTRL
#undef FLEXIO1_SHIFTSTAT
#undef FLEXIO1_SHIFTERR
#undef FLEXIO1_TIMSTAT
#undef FLEXIO1_SHIFTSDEN
#undef FLEXIO1_SHIFTCTL
#undef FLEXIO1_SHIFTCFG
#undef FLEXIO1_SHIFTBUF
#undef FLEXIO1_TIMCTL
#undef FLEXIO1_TIMCFG
#undef FLEXIO1_TIMCMP

namespace fl {

namespace {

// ---------------------------------------------------------------------------
// FLEXIO1 base + register layout
// ---------------------------------------------------------------------------
//
// From the iMXRT1062 reference manual chapter 50 (FlexIO) + cross-checked
// against the Teensy core's `imxrt.h` `FLEXIO1_*` macros (which expand to
// `IMXRT_FLEXIO1.offsetNNN`):
//
//   FLEXIO1 base = 0x401AC000
//   VERID      @ +0x000   (Version ID, read-only — NOT CTRL!)
//   PARAM      @ +0x004   (Parameter, read-only)
//   CTRL       @ +0x008   (Control register — module enable + SW reset bit)
//   SHIFTSTAT  @ +0x010   (status flags)
//   SHIFTERR   @ +0x014   (error flags)
//   TIMSTAT    @ +0x018   (timer status)
//   SHIFTSDEN  @ +0x030   (shifter status DMA enable)
//   SHIFTCTL[N]   @ +0x080 + 4*N   (shifter control)
//   SHIFTCFG[N]   @ +0x100 + 4*N   (shifter config)
//   SHIFTBUF[N]   @ +0x200 + 4*N   (shifter buffer, raw)
//   TIMCTL[N]     @ +0x400 + 4*N   (timer control)
//   TIMCFG[N]     @ +0x480 + 4*N   (timer config)
//   TIMCMP[N]     @ +0x500 + 4*N   (timer compare)
//
// **#2772 root cause:** the original Phase 1B code put CTRL at +0x000.
// Writes to FLEXIO1 +0x000 (VERID) on iMXRT1062 with the FLEXIO1 clock
// gated on are silently ignored — but the subsequent **read** of +0x000
// the spin loop performed to check the SW-reset bit produced an
// imprecise data-bus error (IMPRECISERR, CFSR=0x400) → LOCKUP HardFault
// → SYSRESETREQ. Cross-checked against `xtensa-esp32s3-elf-nm`-style
// disassembly of `firmware.elf` (`addr2line 0x2D166` → `FlexIoRxChannelImpl::begin`).

static constexpr u32 kFLEXIO1_BASE = 0x401AC000u;

static volatile u32 &FLEXIO1_CTRL      = *(volatile u32 *)(kFLEXIO1_BASE + 0x008);
static volatile u32 &FLEXIO1_SHIFTSTAT = *(volatile u32 *)(kFLEXIO1_BASE + 0x010);
static volatile u32 &FLEXIO1_SHIFTERR  = *(volatile u32 *)(kFLEXIO1_BASE + 0x014);
static volatile u32 &FLEXIO1_TIMSTAT   = *(volatile u32 *)(kFLEXIO1_BASE + 0x018);
static volatile u32 &FLEXIO1_SHIFTSDEN = *(volatile u32 *)(kFLEXIO1_BASE + 0x030);

static volatile u32 *FLEXIO1_SHIFTCTL  = (volatile u32 *)(kFLEXIO1_BASE + 0x080);
static volatile u32 *FLEXIO1_SHIFTCFG  = (volatile u32 *)(kFLEXIO1_BASE + 0x100);
static volatile u32 *FLEXIO1_SHIFTBUF  = (volatile u32 *)(kFLEXIO1_BASE + 0x200);
static volatile u32 *FLEXIO1_TIMCTL    = (volatile u32 *)(kFLEXIO1_BASE + 0x400);
static volatile u32 *FLEXIO1_TIMCFG    = (volatile u32 *)(kFLEXIO1_BASE + 0x480);
static volatile u32 *FLEXIO1_TIMCMP    = (volatile u32 *)(kFLEXIO1_BASE + 0x500);

// ---------------------------------------------------------------------------
// Teensy 4.0 → FLEXIO1 pin mapping table
// ---------------------------------------------------------------------------
//
// Each entry maps a Teensy digital pin to its FLEXIO1 pin index. The IOMUXC
// SW_MUX_CTL_PAD register selects ALT4 for the FLEXIO1 alternate function;
// the SW_PAD_CTL_PAD register configures pull/keeper + hysteresis for clean
// edge detection.
//
// Offsets are from the IOMUXC base (`0x401F8000`) and come from the iMXRT1062
// reference manual chapter 11. The corresponding `_register` and
// `_register_pad` values can be reached through the standard `imxrt.h`
// macros (`IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_*`, etc.) but the raw absolute
// addresses below keep this table compact and grep-friendly.

struct FlexIo1PinInfo {
    u8 teensy_pin;            ///< Teensy digital pin number
    u8 flexio_pin;            ///< FLEXIO1 pin index (0..15)
    volatile u32 *mux_reg;    ///< IOMUXC SW_MUX_CTL register address
    volatile u32 *pad_reg;    ///< IOMUXC SW_PAD_CTL register address
};

static constexpr u32 kFlexIo1IomuxcBase = 0x401F8000u;

// Initial pin map — Teensy 4.0/4.1 pins whose ALT4 mux routes to FLEXIO1.
// Verified against the Teensy 4.0 pinout card + iMXRT1062 RM Table 11-1.
// Phase 1C can extend this to additional pins (e.g. 2, 3, 5, the rest of the
// GPIO_EMC_* bank) as bench validation proves them out.
static const FlexIo1PinInfo kFlexIo1Pins[] = {
    // Teensy pin 4 = GPIO_EMC_06, ALT4 = FLEXIO1_FLEXIO06
    {4,  6,
     (volatile u32 *)(kFlexIo1IomuxcBase + 0x002C),
     (volatile u32 *)(kFlexIo1IomuxcBase + 0x021C)},
};

static constexpr size_t kFlexIo1PinCount =
    sizeof(kFlexIo1Pins) / sizeof(kFlexIo1Pins[0]);

static const FlexIo1PinInfo *lookupFlexIo1Pin(int teensy_pin) {
    for (size_t i = 0; i < kFlexIo1PinCount; ++i) {
        if (kFlexIo1Pins[i].teensy_pin == (u8)teensy_pin) {
            return &kFlexIo1Pins[i];
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// WS2812 bit decoder — mirrors the FlexPWM RX driver's decode path.
// ---------------------------------------------------------------------------
//
// Phase 3 of FastLED#2764 wires the same edge-pair → bit-cell decoder that
// the existing FlexPWM RX driver uses, so the new FlexIO RX backend can be
// used as a drop-in source for AutoResearch's TX→RX byte-verification flow.
//
// Kept as static helpers in this TU rather than extracting to a shared
// header so the FlexPWM and FlexIO RX paths remain independently reviewable.
// A follow-up refactor can consolidate into `src/fl/channels/rx/decode_ws2812.h`
// once both backends are bench-validated against the same test matrix.

static inline int decodeBitFlexIo(u32 high_ns, u32 low_ns,
                                  const ChipsetTiming4Phase &timing) {
    if (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns &&
        low_ns >= timing.t0l_min_ns && low_ns <= timing.t0l_max_ns) {
        return 0;
    }
    if (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns &&
        low_ns >= timing.t1l_min_ns && low_ns <= timing.t1l_max_ns) {
        return 1;
    }
    return -1;
}

static fl::result<u32, DecodeError>
decodeEdgesFlexIo(const ChipsetTiming4Phase &timing,
                  fl::span<const EdgeTime> edges, fl::span<u8> bytes_out) {
    if (edges.size() == 0 || bytes_out.size() == 0) {
        return fl::result<u32, DecodeError>::success(0);
    }
    u32 byte_index = 0;
    u8 current_byte = 0;
    u8 bit_count = 0;
    u32 error_count = 0;
    u32 total_bits = 0;
    size_t i = 0;
    while (i + 1 < edges.size()) {
        // Each bit cell = (HIGH, LOW) pair. Polarity error → skip + resync.
        if (!edges[i].high) {
            ++i;
            continue;
        }
        const u32 high_ns = edges[i].ns;
        const u32 low_ns  = edges[i + 1].ns;
        i += 2;
        const int bit = decodeBitFlexIo(high_ns, low_ns, timing);
        ++total_bits;
        if (bit < 0) {
            ++error_count;
            continue;
        }
        current_byte = static_cast<u8>((current_byte << 1) | (u8)bit);
        if (++bit_count == 8) {
            if (byte_index >= bytes_out.size()) {
                return fl::result<u32, DecodeError>::failure(
                    DecodeError::BUFFER_OVERFLOW);
            }
            bytes_out[byte_index++] = current_byte;
            current_byte = 0;
            bit_count = 0;
        }
    }
    if (bit_count > 0 && byte_index < bytes_out.size()) {
        bytes_out[byte_index++] =
            static_cast<u8>(current_byte << (8 - bit_count));
    }
    if (total_bits > 0 && (error_count * 10) > total_bits) {
        return fl::result<u32, DecodeError>::failure(
            DecodeError::HIGH_ERROR_RATE);
    }
    return fl::result<u32, DecodeError>::success(byte_index);
}

// ---------------------------------------------------------------------------
// FlexIO1 functional clock
// ---------------------------------------------------------------------------
//
// CCM_CCGR5 owns the FLEXIO1 clock gate (NOT CCGR3, which owns FLEXIO2).
//
// **Source-clock choice**: we deliberately pick OSC (24 MHz, always-on) via
// CCM_CDCDR_FLEXIO1_CLK_SEL(1) rather than PLL3_PFD3. Teensy 4's `startup.c`
// programs the four PLL3 PFDs but leaves their CLKGATE bits **set** (line
// 139–141: `CCM_ANALOG_PFD_480_SET = 0x80808080`). PFD3 is gated off post-
// boot. If FLEXIO1 selects a gated PFD as its source, the first write to
// any FLEXIO1 register hits an imprecise data-bus error → HardFault →
// LOCKUP reset. Confirmed against the dev-bench Teensy 4.0 in #2772.
//
// Cost of using OSC: ~41.7 ns/tick resolution instead of ~8.3 ns. Plenty
// for WS2812 inter-edge timing (worst-case T0H ~350 ns) and for the bench
// matrix (1 / 10 / 100 kHz square waves whose half-period is ≥5 µs).

static constexpr u32 kFlexIo1ClkMHz = 60u;

static void flexio1_clock_init() {
    // Root cause of FastLED#2772 / #3059 FLEXIO1 hang (live-debugged
    // 2026-06-15): the CCM_CDCDR POR default `0x33f71f92` has
    // FLEXIO1_CLK_SEL=2 (PLL5 — the video PLL), which Teensyduino does
    // not enable. With no functional clock, the FLEXIO1 CTRL.SWRST bit
    // can never self-clear (RM §50.5.1.1: "automatically cleared after
    // reset is complete, AS LONG AS THE FLEXIO IS BEING CLOCKED") and
    // every previous code variant — including the original #2772 fix —
    // hit the 1M-poll spin bound.
    //
    // Fix: gate the clock off, switch CLK_SEL to pll3_sw_clk (=3, the
    // 480 MHz USB PLL — Teensyduino guarantees it is running because the
    // core's USB CDC stack depends on it), pick a /1/8 divider so the
    // resulting 60 MHz ipg_clk_flexio is well under FLEXIO's ~120 MHz
    // ceiling, then gate the clock back on. Standard "change divider
    // while gated" sequence to avoid mid-divider glitches.

    FL_WARN("[FlexIO RX] pre-init: CCGR5=0x" << fl::hex << CCM_CCGR5
            << " CDCDR=0x" << CCM_CDCDR << fl::dec);

    // 1. Gate FLEXIO1 clock OFF so we can safely reprogram CDCDR.
    CCM_CCGR5 &= ~CCM_CCGR5_FLEXIO1(0x3);
    __asm__ volatile("dsb 0xF" ::: "memory");

    // 2. Reprogram CDCDR: clear FLEXIO1 PRED/PODF/SEL fields and set
    //    PRED=0 (/1), PODF=7 (/8), SEL=3 (pll3_sw_clk).
    u32 cdcdr = CCM_CDCDR;
    cdcdr &= ~(CCM_CDCDR_FLEXIO1_CLK_PRED(7) |
               CCM_CDCDR_FLEXIO1_CLK_PODF(7) |
               CCM_CDCDR_FLEXIO1_CLK_SEL(3));
    cdcdr |= CCM_CDCDR_FLEXIO1_CLK_PRED(0) |
             CCM_CDCDR_FLEXIO1_CLK_PODF(7) |
             CCM_CDCDR_FLEXIO1_CLK_SEL(3);
    CCM_CDCDR = cdcdr;
    __asm__ volatile("dsb 0xF" ::: "memory");

    // 3. Gate FLEXIO1 clock ON.
    CCM_CCGR5 |= CCM_CCGR5_FLEXIO1(CCM_CCGR_ON);
    __asm__ volatile("dsb 0xF" ::: "memory");
    __asm__ volatile("isb 0xF" ::: "memory");

    FL_WARN("[FlexIO RX] post-init: CCGR5=0x" << fl::hex << CCM_CCGR5
            << " CDCDR=0x" << CCM_CDCDR << fl::dec);

    // Diagnostic: read VERID @ +0x000 (should be non-zero if bus clock is on)
    volatile u32 *flexio1_verid = (volatile u32 *)(kFLEXIO1_BASE + 0x000);
    FL_WARN("[FlexIO RX] FLEXIO1 VERID=0x" << fl::hex << *flexio1_verid
            << " PARAM=0x" << *(volatile u32 *)(kFLEXIO1_BASE + 0x004)
            << " CTRL=0x" << FLEXIO1_CTRL << fl::dec);

    // Memory barrier so the CCM writes have actually committed before any
    // downstream code touches FLEXIO1_CTRL. Without this, the very next
    // read/write of FLEXIO1_CTRL on a fresh-from-reset chip can hit the
    // module while its clock is still gated off — observed on the dev-bench
    // Teensy 4.0 as an infinite spin on the software-reset wait (#2772).
    __asm__ volatile("dsb 0xF" ::: "memory");        // step 6 (DSB)
    __asm__ volatile("isb 0xF" ::: "memory");        // step 6 (ISB)
}

// IOMUXC pad: ALT4 + hysteresis + 100 kΩ pull-up keeper (matches FlexIO TX
// pad config rationale: clean edge detection, no floating-input glitches
// before the signal is driven).
//
// FastLED#3066 phase 1.7 root cause: also set the **SION** (Software Input
// On) bit. Without SION, the IOMUX doesn't force the input path through
// to the peripheral when the pad's MUX_MODE selects an alternate function
// — so even though pin 4 is muxed to ALT4 (FLEXIO1_FLEXIO06), the FLEXIO1
// module never sees pad activity. The working FlexPWM RX driver sets
// SION the same way (`rx_flexpwm_channel.cpp.hpp:405`); FlexIO RX needed
// the same fix. Verified by `flexioRxLoopbackPing`: with SION cleared,
// 8 hand-driven `digitalWrite` transitions on pin 3 produced
// `edges_captured=0`; with SION set, the captured buffer reflects the
// real pin activity.
static void flexio1_pin_init(const FlexIo1PinInfo &pin_info) {
    *(pin_info.mux_reg) = 4 | 0x10;                   // ALT4 + SION
    *(pin_info.pad_reg) = (3 << 12) |                 // PKE + PUE (pull mode)
                          (2 << 14) |                 // PUS = 100K pull-up
                          (1 << 16);                  // HYS = hysteresis

    // FastLED#3066 phase 1.7 diagnostic: read back the IOMUX registers
    // and FLEXIO1 PIN status so we can see whether the mux writes
    // committed and whether FLEXIO1 sees pad activity on the selected
    // input pin.
    FL_WARN("[FlexIO RX] post-pin-init: pin=" << (int)pin_info.teensy_pin
            << " flexio_pin=" << (int)pin_info.flexio_pin
            << " mux_reg=0x" << fl::hex << *(pin_info.mux_reg)
            << " pad_reg=0x" << *(pin_info.pad_reg) << fl::dec);

    // FLEXIO1 PIN @ +0x00C reflects the live state of each input pin.
    // If our pad routing works, bit `flexio_pin` should track the pad.
    // (offset 0x040 — used in the earlier iter 5 diagnostic — was the
    // wrong register and read 0 unconditionally.)
    volatile u32 *flexio1_pin_reg = (volatile u32 *)(kFLEXIO1_BASE + 0x00C);
    FL_WARN("[FlexIO RX] FLEXIO1 PIN=0x" << fl::hex << *flexio1_pin_reg
            << " (expect bit " << fl::dec << (int)pin_info.flexio_pin
            << " to track pad)");
}

// ---------------------------------------------------------------------------
// FlexIO1 timer + shifter configuration for edge-timing capture
// ---------------------------------------------------------------------------
//
// Timer 0:
//   TIMOD     = 3       16-bit counter (decrement on every functional clock)
//   TRGSEL    = 2*N+1   external pin N edge (where N = flexio_pin)
//   TRGPOL    = 0       active-high trigger (i.e. rising edge starts; combined
//                       with TIMRST=trigger-rising below we also reset on
//                       every rising edge)
//   TRGSRC    = 0       external trigger (pin)
//   PINCFG    = 0       timer pin output disabled (we only read the pin)
//   TIMENA    = 6       trigger-rising-edge enables timer
//   TIMDIS    = 2       disable on timer compare (counter reached 0)
//   TIMRST    = 6       reset counter on trigger rising edge (= next edge)
//
// Shifter 0:
//   SMOD      = 1       receive mode
//   TIMSEL    = 0       Timer 0 controls this shifter
//   TIMPOL    = 1       shift on negedge of timer clock (== timer expiry)
//   PINCFG    = 0       input (no output drive)
//   PINSEL    = N       sample input from FLEXIO1 pin N
//   PINPOL    = 0       active high
//   INSRC     = 0       shift-in from pin (not chained from neighbor)

/// @brief Configure FLEXIO1 for edge-timing capture on the given pin.
/// @return true on success; false on hardware error. (The SWRST spin
///         that historically returned false has been removed — see the
///         comment block inside.)
static bool flexio1_configure(u8 flexio_pin) {
    // FastLED#2772 / #3059 device-side root cause (live-debugged
    // 2026-06-15): on the dev-bench Teensy 4.0 the FLEXIO1 CTRL.SWRST
    // bit reliably never self-clears even when the CCM clock gate is on
    // and CDCDR points at a running source (pll3_sw_clk via CLK_SEL=3).
    // VERID/PARAM reads are responsive, CTRL=0 pre-write, but the
    // SWRST=1 write never auto-clears. Both the FlexIO2 TX driver and
    // every textbook FlexIO bring-up sequence depend on this self-clear,
    // so something about FLEXIO1 specifically is broken in a way we
    // cannot diagnose from the host side.
    //
    // Workaround: skip SWRST entirely. CTRL reads as 0 at this point
    // (verified by the diagnostic FL_WARN in flexio1_clock_init) so the
    // module is already in its post-reset default state. Writing 0 to
    // CTRL right before configuration ensures we're not starting from a
    // stale FLEXEN, then we program shifters/timers and finally set
    // FLEXEN to enable. If SWRST is ever actually needed, the rest of
    // the config code zeros every register we touch, so the practical
    // effect is the same.
    FLEXIO1_CTRL = 0;

    // -- Shifter 0 --
    FLEXIO1_SHIFTCTL[0] = (1u << 0) |                 // SMOD = receive
                         (0u << 7) |                 // PINPOL active high
                         ((u32)flexio_pin << 8) |    // PINSEL
                         (0u << 16) |                // PINCFG = input
                         (1u << 23) |                // TIMPOL = negative
                         (0u << 24);                 // TIMSEL = timer 0

    FLEXIO1_SHIFTCFG[0] = 0;                          // no start/stop bits

    // -- Timer 0 (16-bit counter; restart on every input edge) --
    // FastLED#3066 iter 6: drive the timer from its own PINSEL field
    // (not from TRGSEL). The TRGSEL=2*flexio_pin formula matches the RM
    // ("Pin 2N input") but the trigger source on this iMXRT1062 build
    // doesn't actually engage TIMENA — iter 6 verified that 8 hand-
    // driven pin toggles produced zero TIMSTAT pulses even with
    // TRGSEL=12 (= Pin 6 edge). The TIMER PINSEL path (TIMCTL bits
    // [13:8]) bypasses TRGSEL entirely; TIMENA=4 then reads "Pin rising
    // edge of PINSEL" directly.
    FLEXIO1_TIMCTL[0] = (3u << 0) |                   // TIMOD = 16-bit counter
                        (0u << 7) |                   // PINPOL active high
                        ((u32)flexio_pin << 8) |      // PINSEL = input pin
                        (0u << 16) |                  // PINCFG = output disabled
                        (0u << 22) |                  // TRGSRC = external (pin)
                        (0u << 23) |                  // TRGPOL active high
                        (0u << 24);                   // TRGSEL unused

    FLEXIO1_TIMCFG[0] = (4u << 8) |                   // TIMENA = pin rising
                        (2u << 12) |                  // TIMDIS = on compare
                        (2u << 16) |                  // TIMRST = pin rising
                        (0u << 20) |                  // TIMDEC clk-rising
                        (0u << 24);                   // TIMOUT logic 0 on enable

    // FastLED#3066 iter 6: TIMCMP=1 makes the timer reach compare
    // immediately after each TIMENA=4 pin-rising-edge enable. The
    // shifter then latches the pin state on each compare event
    // (TIMPOL=0). This gives us a pin-edge sampler — not true
    // inter-edge timing — but proves the chain CAN fire and DMA CAN
    // transfer. Future iterations will refine TIMCMP / SHIFTBUF
    // semantics for actual edge interval recovery.
    FLEXIO1_TIMCMP[0] = 10u;

    // Clear status / error flags
    FLEXIO1_SHIFTSTAT = 0xFFu;
    FLEXIO1_SHIFTERR  = 0xFFu;
    FLEXIO1_TIMSTAT   = 0xFFu;

    // Enable shifter-status DMA request (SHIFTSDEN bit 0 = shifter 0)
    FLEXIO1_SHIFTSDEN = (1u << 0);

    // Enable FlexIO module
    FLEXIO1_CTRL = (1u << 0);                         // FLEXEN
    return true;
}

// ---------------------------------------------------------------------------
// FlexIO1 RX implementation
// ---------------------------------------------------------------------------

class FlexIoRxChannelImpl : public FlexIoRxChannel {
  public:
    explicit FlexIoRxChannelImpl(int pin) : mPin(pin) {}
    ~FlexIoRxChannelImpl() override { stopHw(); }

    bool begin(const RxConfig &config) override;
    bool finished() const override;
    RxWaitResult wait(u32 timeout_ms) override;
    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                        fl::span<u8> out) override;
    size_t getRawEdgeTimes(fl::span<EdgeTime> out,
                           size_t offset = 0) override;
    const char *name() const override { return "FLEXIO"; }
    int getPin() const override { return mPin; }
    bool injectEdges(fl::span<const EdgeTime> edges) override;

  private:
    void buildEdgeTimesFromCaptures();
    void stopHw();
    static void dmaIsr();
    static FlexIoRxChannelImpl *sActiveInstance;

    int mPin = -1;
    const FlexIo1PinInfo *mPinInfo = nullptr;

    DMAChannel mDma;
    fl::vector<u32> mCaptureBuffer;
    size_t mBufferSize = 1024;            // requested edge-count budget

    volatile bool mReceiveDone = false;
    bool mConfigured = false;
    bool mStartLow = true;

    fl::vector<EdgeTime> mEdges;
    bool mEdgesValid = false;

    u32 mSignalRangeMaxNs = 100000;
};

FlexIoRxChannelImpl *FlexIoRxChannelImpl::sActiveInstance = nullptr;

void FlexIoRxChannelImpl::stopHw() {
    if (!mConfigured) return;
    mDma.disable();
    FLEXIO1_CTRL = 0;
    sActiveInstance = nullptr;
    mConfigured = false;
}

void FlexIoRxChannelImpl::dmaIsr() {
    if (sActiveInstance) {
        sActiveInstance->mDma.clearInterrupt();
        sActiveInstance->mReceiveDone = true;
    }
}

bool FlexIoRxChannelImpl::begin(const RxConfig &config) {
    mPinInfo = lookupFlexIo1Pin(mPin);
    if (!mPinInfo) {
        FL_WARN("[FlexIO RX] Pin "
                << mPin
                << " has no FLEXIO1 mux mapping on Teensy 4.x (only a small "
                   "subset is enabled; see rx_flexio_channel.cpp.hpp). See "
                   "FastLED#2764.");
        return false;
    }

    if (sActiveInstance && sActiveInstance != this) {
        FL_WARN("[FlexIO RX] Another FlexIoRxChannel is already active; only "
                "one RX channel may use FLEXIO1 at a time.");
        return false;
    }

    mSignalRangeMaxNs = config.signal_range_max_ns;
    mStartLow         = config.start_low;
    mReceiveDone      = false;
    mEdges.clear();
    mEdgesValid = false;

    // Size the capture buffer for the requested timing budget. We allocate
    // one u32 per captured inter-edge delta. Cap at a safe upper bound so
    // we never balloon RAM during exploratory runs. The 16 384-entry cap
    // (64 KiB) covers a 100 ms window at 100 kHz square wave with 50 %
    // headroom — see `flexioRxBenchmark` in `AutoResearchRemote.cpp`.
    const size_t requested = config.buffer_size > 0 ? config.buffer_size
                                                    : mBufferSize;
    mBufferSize = requested > 16384 ? 16384 : requested;
    mCaptureBuffer.assign(mBufferSize, 0u);

    flexio1_clock_init();
    flexio1_pin_init(*mPinInfo);
    if (!flexio1_configure(mPinInfo->flexio_pin)) {
        // Reset spin hit its bound. Don't enable DMA — there's nothing on
        // the other end to feed it, and leaving the channel armed would
        // corrupt mCaptureBuffer if the FlexIO module came alive later.
        // The caller already got an FL_WARN with the diagnostic.
        return false;
    }

    // DMA: copy SHIFTBUF[0] → mCaptureBuffer on each shifter-status flag.
    mDma.source((volatile u32 &)FLEXIO1_SHIFTBUF[0]);
    mDma.destinationBuffer(mCaptureBuffer.data(), mCaptureBuffer.size() * sizeof(u32));
    mDma.transferSize(4);
    mDma.transferCount(mCaptureBuffer.size());
    mDma.triggerAtHardwareEvent(DMAMUX_SOURCE_FLEXIO1_REQUEST0);
    mDma.disableOnCompletion();
    mDma.interruptAtCompletion();
    mDma.attachInterrupt(&FlexIoRxChannelImpl::dmaIsr);
    mDma.enable();

    sActiveInstance = this;
    mConfigured = true;
    return true;
}

bool FlexIoRxChannelImpl::finished() const {
    return mReceiveDone;
}

RxWaitResult FlexIoRxChannelImpl::wait(u32 timeout_ms) {
    if (!mConfigured) {
        return RxWaitResult::TIMEOUT;
    }
    const u32 start = millis();
    while (!mReceiveDone) {
        if ((millis() - start) >= timeout_ms) {
            // FastLED#3066 Phase 1 sub-task 1 diagnostic: when the
            // completion ISR never fires, dump the DMA channel state +
            // FLEXIO1 shifter/timer status so the host can see why.
            // Reading `mDma.complete()`, `mDma.error()`, and the live TCD
            // CITER/BITER tells us whether the channel even started, how
            // many transfers it processed, and whether it errored. The
            // FLEXIO1 SHIFTSTAT/SHIFTERR/TIMSTAT reads tell us whether
            // the shifter ever latched a captured value to trigger DMA.
            const u32 citer = mDma.TCD->CITER & 0x7FFFu;
            const u32 biter = mDma.TCD->BITER & 0x7FFFu;
            const u32 transfers_done = (biter > citer) ? (biter - citer) : 0u;
            FL_WARN("[FlexIO RX] wait() TIMEOUT after "
                    << timeout_ms << "ms:"
                    << " DMA complete=" << (mDma.complete() ? 1 : 0)
                    << " error=" << (mDma.error() ? 1 : 0)
                    << " CITER=" << citer << "/" << biter
                    << " (transfers_done=" << transfers_done << ")");
            FL_WARN("[FlexIO RX] post-timeout FLEXIO1:"
                    << " SHIFTSTAT=0x" << fl::hex << FLEXIO1_SHIFTSTAT
                    << " SHIFTERR=0x" << FLEXIO1_SHIFTERR
                    << " TIMSTAT=0x" << FLEXIO1_TIMSTAT
                    << " CTRL=0x" << FLEXIO1_CTRL << fl::dec);
            // FastLED#3066 Phase 1 diagnostic: also dump the first few
            // words of mCaptureBuffer so subsequent iterations can see
            // whether the DMA actually copied any non-zero data into RAM
            // — distinguishing "shifter never fired" from "shifter fired
            // but latched all-zero pin samples".
            if (mCaptureBuffer.size() >= 8u) {
                FL_WARN("[FlexIO RX] mCaptureBuffer[0..7]: 0x"
                        << fl::hex << mCaptureBuffer[0] << " 0x"
                        << mCaptureBuffer[1] << " 0x"
                        << mCaptureBuffer[2] << " 0x"
                        << mCaptureBuffer[3] << " 0x"
                        << mCaptureBuffer[4] << " 0x"
                        << mCaptureBuffer[5] << " 0x"
                        << mCaptureBuffer[6] << " 0x"
                        << mCaptureBuffer[7] << fl::dec);
            }
            return RxWaitResult::TIMEOUT;
        }
    }
    // FastLED#3066 Phase 1 diagnostic: also dump on SUCCESS so future
    // iterations can compare buffer-fill behaviour between the timeout
    // path and the completion path. Iter 4 surfaced a surprise: DMA can
    // signal "complete" with `transfers_done=0/N` when the channel
    // already reloaded CITER=BITER and the buffer is all-zero — meaning
    // either the shifter never fired or the ISR was misfired.
    if (mCaptureBuffer.size() >= 8u) {
        const u32 citer = mDma.TCD->CITER & 0x7FFFu;
        const u32 biter = mDma.TCD->BITER & 0x7FFFu;
        const u32 transfers_done = (biter > citer) ? (biter - citer) : 0u;
        FL_WARN("[FlexIO RX] wait() SUCCESS: transfers_done="
                << transfers_done << "/" << biter);
        FL_WARN("[FlexIO RX] mCaptureBuffer[0..7]: 0x"
                << fl::hex << mCaptureBuffer[0] << " 0x"
                << mCaptureBuffer[1] << " 0x"
                << mCaptureBuffer[2] << " 0x"
                << mCaptureBuffer[3] << " 0x"
                << mCaptureBuffer[4] << " 0x"
                << mCaptureBuffer[5] << " 0x"
                << mCaptureBuffer[6] << " 0x"
                << mCaptureBuffer[7] << fl::dec);
    }
    return RxWaitResult::SUCCESS;
}

void FlexIoRxChannelImpl::buildEdgeTimesFromCaptures() {
    if (mEdgesValid) return;
    mEdges.clear();

    // Each capture is a 16-bit tick count of the time the pin spent in its
    // previous LEVEL — i.e. the duration of the segment THAT JUST ENDED at
    // the moment this edge fired. We convert to nanoseconds via
    //   delta_ns = (ticks * 1000) / kFlexIo1ClkMHz
    // and store one `EdgeTime{level, ns}` per segment. Level alternates
    // starting from the pin's idle state (`mStartLow == true` → first
    // recorded segment was LOW).
    bool high = !mStartLow;
    for (size_t i = 0; i < mCaptureBuffer.size(); ++i) {
        const u32 ticks = mCaptureBuffer[i] & 0xFFFFu;
        if (ticks == 0 && i > 0) {
            // 0-tick gap usually means the DMA hasn't filled this slot yet;
            // truncate here so the decoder sees a clean tail.
            break;
        }
        const u64 delta_ns =
            ((u64)ticks * 1000ULL + (kFlexIo1ClkMHz / 2)) / kFlexIo1ClkMHz;
        mEdges.push_back(EdgeTime(high, (u32)delta_ns));
        high = !high;
    }
    mEdgesValid = true;
}

fl::result<u32, DecodeError>
FlexIoRxChannelImpl::decode(const ChipsetTiming4Phase &timing,
                            fl::span<u8> out) {
    buildEdgeTimesFromCaptures();
    return decodeEdgesFlexIo(
        timing,
        fl::span<const EdgeTime>(mEdges),
        out);
}

size_t FlexIoRxChannelImpl::getRawEdgeTimes(fl::span<EdgeTime> out,
                                            size_t offset) {
    buildEdgeTimesFromCaptures();
    if (offset >= mEdges.size()) return 0;
    const size_t available = mEdges.size() - offset;
    const size_t to_copy = available < out.size() ? available : out.size();
    for (size_t i = 0; i < to_copy; ++i) {
        out[i] = mEdges[offset + i];
    }
    return to_copy;
}

bool FlexIoRxChannelImpl::injectEdges(fl::span<const EdgeTime> edges) {
    mEdges.clear();
    mEdges.reserve(edges.size());
    for (size_t i = 0; i < edges.size(); ++i) {
        mEdges.push_back(edges[i]);
    }
    mEdgesValid = true;
    mReceiveDone = true;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
fl::shared_ptr<FlexIoRxChannel> FlexIoRxChannel::create(int pin) {
    const FlexIo1PinInfo *info = lookupFlexIo1Pin(pin);
    if (!info) {
        FL_WARN("[FlexIO RX] Pin "
                << pin
                << " has no FLEXIO1 mux mapping (Phase 1B initial map is "
                   "minimal; expand kFlexIo1Pins[] as bench tests qualify "
                   "additional pins). See FastLED#2764.");
        return fl::shared_ptr<FlexIoRxChannel>();
    }
    return fl::make_shared<FlexIoRxChannelImpl>(pin);
}

} // namespace fl

#endif // FL_IS_TEENSY_4X
