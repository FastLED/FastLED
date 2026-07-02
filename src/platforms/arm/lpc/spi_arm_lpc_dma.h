// IWYU pragma: private

#ifndef __INC_SPI_ARM_LPC_DMA_H
#define __INC_SPI_ARM_LPC_DMA_H

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fastspi_types.h"
#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/arm/lpc/lpc_dma_descriptor_table.h"
#include "platforms/arm/lpc/spi_arm_lpc.h"

// =============================================================================
// LPC845 SPI + DMA0 driver — async hardware-accelerated APA102/SK9822/WS2801
// =============================================================================
//
// Phase 1 of FastLED #3453. Drop-in peer of `ARMHardwareSPIOutput` from
// `spi_arm_lpc.h`. The polled driver writes one byte per `STAT.TXRDY` spin and
// blocks at the FastLED.show() boundary; this DMA driver hands the entire
// pixel frame to a DMA0 channel tied to the SPI block's TX request line, so
// the CPU is free in WFI during transmission.
//
// **Supported chips:** LPC845 only. **LPC804 is NOT supported** and never
// can be — LPC804 silicon has no DMA peripheral. See
// `agents/docs/peripheral-existence.md` "Historical anti-example: LPC804
// phantom DMA" for the empirical evidence (zero `DMA_Type` typedef, zero
// `DMA0_BASE` macro, zero `FSL_FEATURE_SOC_DMA_COUNT` in NXP's own
// `mcux-sdk/devices/LPC804/`; UM11065 has only 3 DMA references, all
// copy-paste leftovers from LPC845; address 0x50008000 is a reserved
// AHB slot on LPC804). The earlier gate widening in PRs #3499 / #3500 /
// framework-arduino-lpc8xx#35 was reverted after @phatpaul's
// [#3499 comment 4855252061](https://github.com/FastLED/FastLED/issues/3499#issuecomment-4855252061)
// surfaced the fabrication.
//
// **Status:** untested on silicon. Phase 1 of #3453 ships the register-level
// wiring per UM11029 §15 (SPI) and §17 (DMA) but has not been bench-validated
// on an LPC845-BRK — the AutoResearch echo path (#3300) is the gate for that.
// Every register access in this file should be re-checked against UM11029
// during bench bring-up; explicit TODO(3453) markers point at the spots most
// likely to need silicon-side adjustment.
//
// BUILD-TIME OPT-IN
// -----------------
// Activated when both are defined:
//   * `FL_IS_ARM_LPC_845`  (auto-detected from CMSIS device macros)
//   * `FASTLED_LPC_SPI_DMA`  (user-supplied, default off)
//
// When `FASTLED_LPC_SPI_DMA` is not set, `fastled_arm_lpc.h` continues
// to route SPI through the polled driver in `spi_arm_lpc.h`. Default
// LPC845 builds are unaffected.
//
// RESOURCES CONSUMED
// ------------------
//   * One DMA0 channel — `FASTLED_LPC_SPI_DMA_CHANNEL`. Default:
//     channel 4 (SPI0_TX per UM11029 Table 80). SPI1_TX is
//     channel 6; users targeting SPI1 override with
//     `-DFASTLED_LPC_SPI_DMA_CHANNEL=6`.
//   * The SPI0 or SPI1 peripheral block (chosen via `pSPIX` template arg).
//   * A SRAM-resident encoding buffer sized to the largest pixel frame. The
//     DMA can only stream halfword writes to TXDAT, so each pixel byte
//     widens into a `fl::u16` in this buffer. Owned by the driver instance,
//     allocated statically in v1 (tunable via `FASTLED_LPC_SPI_DMA_MAX_BYTES`).
//
// PIN ROUTING
// -----------
// Same convention as the polled driver: the user's `SystemInit` / board
// setup code is responsible for SWM routing of `SPIx_MOSI` / `SPIx_SCK` to
// the requested pins BEFORE `init()` is called. The driver does not touch
// IOCON or the Switch Matrix.
//
// REFERENCE
// ---------
//   * NXP UM11029 (LPC84x User Manual)
//     §15 — SPI peripheral (TXDATCTL/TXDAT/TXCTL register semantics)
//     §17 — DMA controller (descriptor layout, XFERCFG, INMUX,
//            ENABLESET/SETVALID/ACTIVE).
//   * `clockless_arm_lpc_pwm_dma.h` — established LPC845 DMA-engine
//     conventions in this repo (SYSAHBCLKCTRL0 power-up bit layout, the
//     PERIPHREQEN+HWTRIGEN CFG pattern, the SETVALID arming step).

// LPC804 build gate: refuse compilation with a clear diagnostic if a user
// tries to opt into FASTLED_LPC_SPI_DMA on LPC804. LPC804 silicon has no
// DMA peripheral (see peripheral-existence.md); the earlier gate widening
// was reverted after that was empirically confirmed.
#if defined(FL_IS_ARM_LPC_804) && defined(FASTLED_LPC_SPI_DMA)
#error "FASTLED_LPC_SPI_DMA is not available on LPC804: LPC804 silicon has no DMA peripheral (per NXP mcux-sdk devices/LPC804/LPC804.h — zero DMA_Type / DMA0_BASE / FSL_FEATURE_SOC_DMA_COUNT; UM11065 has only DMA cross-references from ADC/DAC chapters, no DMA chapter). Use the polled SPI driver in spi_arm_lpc.h instead. See agents/docs/peripheral-existence.md for the empirical grep recipe."
#endif

#if defined(FL_IS_ARM_LPC) && defined(FL_IS_ARM_LPC_845) && \
    defined(FASTLED_LPC_SPI_DMA)

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

namespace fl {

// ----- Tuning macros -------------------------------------------------------

// DMA channel used for SPI TX. Each LPC845 DMA channel is hardwired to
// one peripheral request line — UM11029 §16.3.3 Table 296 ("DMA
// requests", p. 263):
//   ch 10 = SPI0_RX   ch 11 = SPI0_TX (default)
//   ch 12 = SPI1_RX   ch 13 = SPI1_TX — override with
//                             `-DFASTLED_LPC_SPI_DMA_CHANNEL=13`
// A previous revision defaulted to channel 4 citing a nonexistent
// "Table 80" — channel 4 is USART2_RX_DMA, whose request line never
// fires for SPI, so the armed channel stayed ACTIVE forever and the
// first transfer spun until the watchdog fired (#3580, silicon-bisected
// 2026-07-02). Explicit overrides must match Table 296 and be < 25.
#ifndef FASTLED_LPC_SPI_DMA_CHANNEL
#define FASTLED_LPC_SPI_DMA_CHANNEL 11
#endif

// Upper bound on encoded-buffer size in u16 elements. APA102 frames =
// 4 bytes/LED * LEDs + 8 start bytes + ceil(LEDs/16) end bytes; 384 LEDs
// fits comfortably in the default 2 KB. Override for larger strips:
//     -DFASTLED_LPC_SPI_DMA_MAX_BYTES=4096
#ifndef FASTLED_LPC_SPI_DMA_MAX_BYTES
#define FASTLED_LPC_SPI_DMA_MAX_BYTES 2048
#endif

// ----- Driver --------------------------------------------------------------

/// @brief LPC845 SPI + DMA0 async output for FastLED clocked strips.
///        LPC804 not supported (silicon has no DMA — see
///        peripheral-existence.md).
///
/// Same template surface as `ARMHardwareSPIOutput<>` (the polled peer);
/// users can swap in this template under a build flag without changing
/// `addLeds<>` call sites. The DMA path is engaged on bulk writes (the
/// `writeBytes` / `writeBytesValue` / `writePixels` family); single-byte
/// and single-bit helpers fall through to the polled SPI block because
/// arming a DMA descriptor for one byte costs more than the busy-wait.
///
/// @tparam _DATA_PIN  Pin index for MOSI (must be SWM-routed to SPIx_MOSI)
/// @tparam _CLOCK_PIN Pin index for SCK  (must be SWM-routed to SPIx_SCK)
/// @tparam _SPI_CLOCK_DIVIDER Approximate clock divider (host clock / this)
/// @tparam pSPIX      Peripheral base address (FL_LPC_SPI0_BASE or
///                    FL_LPC_SPI1_BASE)
template <u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_DIVIDER,
          u32 pSPIX = FL_LPC_SPI0_BASE>
class ARMHardwareSPIOutputDMA {
    Selectable *mPSelect;

    // Encoding buffer for byte → halfword widening before each DMA kick.
    // Static so it costs SRAM only when the driver is instantiated; one
    // controller at a time owns it (mutually exclusive use — see header
    // comment).
    static u16 sEncodeBuf[FASTLED_LPC_SPI_DMA_MAX_BYTES];

    static inline SPI_Type* spi_block() __attribute__((always_inline)) {
        // C-style cast is the canonical MMIO base-address pattern; matches
        // the polled driver and the FastLED reinterpret_cast linter
        // exception for peripheral pointer macros.
        return (SPI_Type*)(pSPIX);  // MMIO base address
    }

public:
    ARMHardwareSPIOutputDMA() : mPSelect(nullptr) {}
    explicit ARMHardwareSPIOutputDMA(Selectable *pSelect) : mPSelect(pSelect) {}

    void setSelect(Selectable *pSelect) FL_NO_EXCEPT { mPSelect = pSelect; }

    /// Initialize SPI peripheral as master + DMA0 channel + INMUX wiring.
    /// Fully self-contained since #3580: the driver enables the SPI0/SPI1
    /// + SWM + DMA0 clocks in SYSCON, releases the SPI peripheral reset,
    /// and routes SCK/MOSI through the switch matrix to the template pins.
    /// (The previous "user's startup is responsible for clocks + SWM"
    /// contract was never honored by any startup path and reading the
    /// unclocked SPI block stalled the AHB bus forever.)
    void init() FL_NO_EXCEPT {
        FastPin<_DATA_PIN>::setOutput();
        FastPin<_CLOCK_PIN>::setOutput();

        // ----- SYSCON clock + reset + SWM routing --------------------------
        // Reading an UNCLOCKED LPC peripheral register stalls the AHB bus
        // forever (no bus fault — the load never retires). Silicon-diagnosed
        // as the #3580 wedge: breadcrumb bisect showed the first spi->STAT
        // read in waitFully() never returned; the WWDT warning-NMI fired 3 s
        // later. Nothing in the ACLPC startup or fbuild SystemInit enables
        // the SPI clocks, so the driver must own its own plumbing:
        // clock enable, reset release (active-low), and SWM pin routing.
        const bool is_spi0 = (pSPIX == FL_LPC_SPI0_BASE);
        SYSCON->SYSAHBCLKCTRL0 |=
            (is_spi0 ? SYSCON_SYSAHBCLKCTRL0_SPI0_MASK
                     : SYSCON_SYSAHBCLKCTRL0_SPI1_MASK) |
            SYSCON_SYSAHBCLKCTRL0_SWM_MASK;
        SYSCON->PRESETCTRL0 |=
            (is_spi0 ? SYSCON_PRESETCTRL0_SPI0_RST_N_MASK
                     : SYSCON_PRESETCTRL0_SPI1_RST_N_MASK);
        // Function-clock select. Separate from the AHB register clock:
        // FCLKSEL[9]=SPI0 / FCLKSEL[10]=SPI1 resets to "none", so the
        // shift engine has NO clock even though the register bus works.
        // Silicon signature (#3580): first DMA write accepted, SPI went
        // busy (STAT=0 — no TXRDY, no MSTIDLE) and froze mid-shift
        // forever. Select main_clk (0b001) — 24 MHz, matching the
        // divider math this driver documents.
        SYSCON->FCLKSEL[is_spi0 ? 9 : 10] = SYSCON_FCLKSEL_SEL(0x1u);
        // Route SCK + MOSI through the switch matrix to the template pins.
        // PINASSIGN fields are 8-bit pin indices (0xFF = unassigned):
        //   SPI0_SCK  = PINASSIGN3[31:24], SPI0_MOSI = PINASSIGN4[7:0]
        //   SPI1_SCK  = PINASSIGN5[23:16], SPI1_MOSI = PINASSIGN5[31:24]
        if (is_spi0) {
            SWM0->PINASSIGN.PINASSIGN3 =
                (SWM0->PINASSIGN.PINASSIGN3 & ~SWM_PINASSIGN3_SPI0_SCK_IO_MASK) |
                SWM_PINASSIGN3_SPI0_SCK_IO(_CLOCK_PIN);
            SWM0->PINASSIGN.PINASSIGN4 =
                (SWM0->PINASSIGN.PINASSIGN4 & ~SWM_PINASSIGN4_SPI0_MOSI_IO_MASK) |
                SWM_PINASSIGN4_SPI0_MOSI_IO(_DATA_PIN);
        } else {
            SWM0->PINASSIGN.PINASSIGN5 =
                (SWM0->PINASSIGN.PINASSIGN5 &
                 ~(SWM_PINASSIGN5_SPI1_SCK_IO_MASK |
                   SWM_PINASSIGN5_SPI1_MOSI_IO_MASK)) |
                SWM_PINASSIGN5_SPI1_SCK_IO(_CLOCK_PIN) |
                SWM_PINASSIGN5_SPI1_MOSI_IO(_DATA_PIN);
        }

        // ----- SPI peripheral configuration (same as polled driver) -------
        SPI_Type *spi = spi_block();
        spi->CFG = 0;
        u32 div = (_SPI_CLOCK_DIVIDER > 1) ? (_SPI_CLOCK_DIVIDER - 1) : 0;
        if (div > 0xFFF) div = 0xFFF;
        spi->DIV = div;
        spi->CFG = FL_LPC_SPI_CFG_ENABLE | FL_LPC_SPI_CFG_MASTER;

        // Latch sticky control so subsequent TXDAT writes inherit 8-bit
        // length + RXIGNORE. This lets the DMA stream write halfwords to
        // TXDAT without re-injecting the control bits per byte.
        // TODO(3453): verify TXCTL persistence semantics on real silicon.
        // UM11029 §15.6.5 specifies TXCTL applies to *all subsequent*
        // TXDAT writes until overwritten; assumed to also survive a
        // CFG-disable cycle, but bench-verify.
        // Deliberately NO sticky EOT: per-unit frame closure inserts a
        // ~1.25 µs SSEL gap after every byte (measured on silicon:
        // 4 MHz SCK degraded to 2.46 MHz effective). The stream instead
        // holds one open frame and waitFully() closes it in software
        // via STAT.ENDTRANSFER once the DMA drains — see waitFully().
        spi->TXCTL = FL_LPC_SPI_TXCTL_LEN_8BIT | FL_LPC_SPI_TXCTL_RXIGNORE;

        // Gate the TX DMA request line open. Silicon finding (#3580,
        // register snapshot 2026-07-02): with the channel armed
        // (CFGVALID=1), PERIPHREQEN=1 and SPI STAT.TXRDY=1, XFERCOUNT
        // never advanced — the SPI's TXRDY only reaches the DMA request
        // input when INTENSET.TXRDYEN is set (the interrupt-enable and
        // DMA-request paths share the gate on this IP). The NVIC-side
        // SPI IRQ stays disabled, so no ISR fires from this.
        spi->INTENSET = SPI_INTENSET_TXRDYEN_MASK;

        // ----- DMA0 power-up + channel configuration ---------------------
        // DMA clock enable, UM11029 §4.6.13 Table 41 bit 29.
        // Cross-checked against NXP mcux-sdk `SYSCON_SYSAHBCLKCTRL0_DMA_MASK`
        // = 0x20000000U (SHIFT = 29U). Shared with the PWM+DMA clockless
        // driver if both are gated on; the OR-set is idempotent.
        SYSCON->SYSAHBCLKCTRL0 |= SYSCON_SYSAHBCLKCTRL0_DMA_MASK;

        // Enable the controller (master enable, not yet channel-enabled).
        DMA0->CTRL = 1UL;

        // Own the descriptor table. SRAMBASE resets to 0 and the ACLPC
        // startup never programs it — arming a channel without this made
        // the DMA engine fetch descriptors from the flash vector table
        // and scribble RAM through garbage pointers (FastLED #3580).
        fl::lpc::ensureDmaSramBase();

        // Channel CFG. PERIPHREQEN=1 so the peripheral request line drives
        // descriptor consumption; HWTRIGEN=0 because SPI's TXRDY is a
        // request signal, not an edge-triggered HW trigger (matches LPC845
        // SDK SPI_TransferSendDMA path). TRIGPOL/TRIGTYPE/CHPRIORITY
        // defaulted to 0 (lowest priority — frame transmission tolerates it).
        // TODO(3453): the LPC845 SDK driver also sets BURSTPOWER to a
        // non-zero value for some peripherals. Default of 0 (1 transfer
        // per request) matches the per-byte TXRDY behavior of the SPI
        // block. Re-validate vs UM11029 §17.6.1.
        DMA0->CHANNEL[FASTLED_LPC_SPI_DMA_CHANNEL].CFG =
            (1UL << 0);  // PERIPHREQEN

        // Request wiring: every LPC845 DMA channel has a HARDWIRED
        // peripheral request input (UM11029 §16.3.3 Table 296) — SPI0_TX
        // is channel 11, SPI1_TX is channel 13, full map at the
        // FASTLED_LPC_SPI_DMA_CHANNEL definition above. No INMUX
        // programming is needed for peripheral requests; ITRIG_INMUX only
        // selects the optional hardware *trigger* source, which this
        // driver doesn't use (HWTRIGEN=0 — TXRDY paces via PERIPHREQEN).
        // The SPI block needs no DMA-enable bit of its own: per UM11029
        // §18.7.5 the TX request asserts whenever the transmitter can
        // accept data and the DMA channel's PERIPHREQEN is set.

        // Channel enabled but no descriptor armed yet — that happens in
        // each writeBytes/writePixels call.
        DMA0->COMMON[0].ENABLESET = (1UL << FASTLED_LPC_SPI_DMA_CHANNEL);
    }

    void inline select() FL_NO_EXCEPT __attribute__((always_inline)) {
        if (mPSelect != nullptr) { mPSelect->select(); }
    }

    void inline release() FL_NO_EXCEPT __attribute__((always_inline)) {
        waitFully();
        if (mPSelect != nullptr) { mPSelect->release(); }
    }

    void endTransaction() FL_NO_EXCEPT { release(); }

    // ----- Synchronous primitives (poll-mode fallback) --------------------
    //
    // These match the polled driver exactly. Arming a one-byte DMA
    // descriptor takes longer than the busy-wait; the DMA path engages
    // only for the bulk methods further below.

    static void wait() __attribute__((always_inline)) {
        while (!(spi_block()->STAT & FL_LPC_SPI_STAT_TXRDY)) {}
    }

    void waitFully() FL_NO_EXCEPT {
        // Drain any in-flight DMA stream first.
        waitDma();
        // Wait for the last byte to leave the transmit path, then close
        // the SPI frame in software. The DMA stream writes plain TXDAT
        // (no per-unit EOT — per-byte frame closure costs ~1.25 µs of
        // SSEL gap per byte, measured 4 MHz → 2.46 MHz effective on
        // silicon), so the master holds the frame open after the stream
        // drains and MSTIDLE would never re-assert. STAT.ENDTRANSFER is
        // the UM-documented software frame-close for exactly this case
        // (UM11029: "force an end to the current transfer").
        while (!(spi_block()->STAT & FL_LPC_SPI_STAT_TXRDY)) {}
        spi_block()->STAT = SPI_STAT_ENDTRANSFER_MASK;
        while (!(spi_block()->STAT & FL_LPC_SPI_STAT_MSTIDLE)) {}
    }

    template <u8 BIT>
    inline static void writeBit(u8 b) FL_NO_EXCEPT {
        wait();
        SPI_Type *spi = spi_block();
        spi->TXDATCTL = ((b & (1u << BIT)) ? 1u : 0u) |
                        FL_LPC_SPI_TXCTL_RXIGNORE;
    }

    static void writeByte(u8 b) __attribute__((always_inline)) {
        wait();
        SPI_Type *spi = spi_block();
        spi->TXDATCTL = static_cast<u32>(b) | FL_LPC_SPI_TXCTL_LEN_8BIT |
                        FL_LPC_SPI_TXCTL_RXIGNORE;
    }

    static void writeWord(u16 w) __attribute__((always_inline)) {
        writeByte(static_cast<u8>(w >> 8));
        writeByte(static_cast<u8>(w & 0xFF));
    }

    // ----- Async DMA primitives ------------------------------------------

    /// True once the previous DMA stream completes (channel ACTIVE flag
    /// drops). Does NOT account for SPI master shift-register drain —
    /// callers that need the wire to fully idle should use `waitFully()`.
    static bool done() FL_NO_EXCEPT {
        const u32 mask = (1UL << FASTLED_LPC_SPI_DMA_CHANNEL);
        return (DMA0->COMMON[0].ACTIVE & mask) == 0u;
    }

    /// Block until the previous DMA stream completes. Cheap to call when
    /// already done. CPU sleeps in WFI between checks; the DMA-done IRQ
    /// will wake it. (If the IRQ has been disabled or vectored elsewhere,
    /// WFI still wakes on any unmasked NVIC pending bit; worst case this
    /// loop polls at SysTick rate, which is still O(microseconds) on a
    /// 24 MHz core.)
    static void waitDma() FL_NO_EXCEPT {
        const u32 mask = (1UL << FASTLED_LPC_SPI_DMA_CHANNEL);
        while ((DMA0->COMMON[0].ACTIVE & mask) != 0u) {
            __asm volatile("wfi" ::: "memory");
        }
    }

    /// Arm and kick a DMA stream covering `len` bytes from `src`. The bytes
    /// are widened into the static u16 encode buffer; each transfer writes
    /// a halfword to TXDAT and the latched TXCTL bits drive an 8-bit SPI
    /// frame with RX ignored. Returns immediately (does NOT wait).
    static void kickDmaStream(const u8 *src, u32 len) FL_NO_EXCEPT {
        if (len == 0u) return;

        const u32 cap = FASTLED_LPC_SPI_DMA_MAX_BYTES;
        if (len > cap) {
            // TODO(3453): chunked-stream support for frames larger than the
            // encode buffer. For now clamp to capacity — clearly visible
            // at bench-validation time (output truncates), preferable to a
            // silent buffer overrun.
            len = cap;
        }

        // Wait out any prior stream before reusing the static buffer.
        waitDma();

        // Encode: u8[len] → u16[len], zero-extend each byte. The upper byte
        // of TXDAT is "don't care" because TXCTL.LEN is set to 7 (8-bit).
        for (u32 i = 0; i < len; ++i) {
            sEncodeBuf[i] = static_cast<u16>(src[i]);
        }

        // Configure descriptor + XFERCFG and arm. SETVALID flips
        // VALIDPENDING → VALID, arming the channel for the next TXRDY
        // trigger. Never SETVALID an unarmed descriptor (see
        // configureChannelDescriptor's guard).
        if (configureChannelDescriptor(len)) {
            DMA0->COMMON[0].SETVALID = (1UL << FASTLED_LPC_SPI_DMA_CHANNEL);
        }
    }

    // ----- Bulk synchronous API (poll-mode-compatible wrappers) ----------

    static void writeBytesValueRaw(u8 value, int len) FL_NO_EXCEPT {
        if (len <= 0) return;

        // Fast path: small repeats stay polled to avoid the encode overhead.
        if (len <= 16) {
            while (len--) { writeByte(value); }
            return;
        }

        const u32 cap = FASTLED_LPC_SPI_DMA_MAX_BYTES;
        u32 remaining = static_cast<u32>(len);
        while (remaining > 0) {
            const u32 chunk = remaining > cap ? cap : remaining;
            for (u32 i = 0; i < chunk; ++i) {
                sEncodeBuf[i] = static_cast<u16>(value);
            }
            waitDma();
            if (configureChannelDescriptor(chunk)) {
                DMA0->COMMON[0].SETVALID = (1UL << FASTLED_LPC_SPI_DMA_CHANNEL);
                waitDma();
            }
            remaining -= chunk;
        }
    }

    void writeBytesValue(u8 value, int len) FL_NO_EXCEPT {
        select();
        writeBytesValueRaw(value, len);
        waitFully();
        release();
    }

    template <class D>
    void writeBytes(FASTLED_REGISTER u8 *data, int len,
                    void *context = nullptr) FL_NO_EXCEPT {
        if (len <= 0) { release(); return; }

        select();
        // If no per-byte adjuster is in play, hand the buffer straight to
        // DMA. The `DATA_NOP` partial specialisation collapses D::adjust to
        // an identity; the compiler folds the encode loop, so this branch
        // is mostly an optimization for the templated-adjuster case.
        const u32 cap = FASTLED_LPC_SPI_DMA_MAX_BYTES;
        u32 remaining = static_cast<u32>(len);
        u8 *src = data;
        while (remaining > 0) {
            const u32 chunk = remaining > cap ? cap : remaining;
            waitDma();
            for (u32 i = 0; i < chunk; ++i) {
                sEncodeBuf[i] = static_cast<u16>(D::adjust(src[i]));
            }
            if (configureChannelDescriptor(chunk)) {
                DMA0->COMMON[0].SETVALID = (1UL << FASTLED_LPC_SPI_DMA_CHANNEL);
                // Block between chunks — the static encode buffer is needed
                // for the next slice. Real async (no inter-chunk drain) needs
                // double-buffering, tracked at #3453 Phase 1 follow-up.
                waitDma();
            }
            src += chunk;
            remaining -= chunk;
        }
        D::postBlock(len, context);
        waitFully();
        release();
    }

    void writeBytes(FASTLED_REGISTER u8 *data, int len) {
        writeBytes<DATA_NOP>(data, len);
    }

    /// Bracketed pixel-controller path used for APA102 / SK9822 / WS2801.
    /// APA102's per-LED start bit comes from `FLAG_START_BIT`; that bit
    /// stays on the polled `writeBit<0>(1)` path because it's a single
    /// SPI frame per LED and not worth DMA-streaming.
    template <u8 FLAGS, class D, EOrder RGB_ORDER>
    void writePixels(PixelController<RGB_ORDER> pixels,
                     void *context = nullptr) FL_NO_EXCEPT {
        int len = pixels.mLen;

        select();
        while (pixels.has(1)) {
            if (FLAGS & FLAG_START_BIT) {
                writeBit<0>(1);
                writeByte(D::adjust(pixels.loadAndScale0()));
                writeByte(D::adjust(pixels.loadAndScale1()));
                writeByte(D::adjust(pixels.loadAndScale2()));
            } else {
                writeByte(D::adjust(pixels.loadAndScale0()));
                writeByte(D::adjust(pixels.loadAndScale1()));
                writeByte(D::adjust(pixels.loadAndScale2()));
            }
            pixels.advanceData();
            pixels.stepDithering();
        }
        D::postBlock(len, context);
        // TODO(3453): the writePixels path above falls through to the
        // single-byte writeByte() helpers; it does NOT engage the DMA
        // stream. A proper DMA writePixels needs a packed-frame buffer
        // (re-encode pixel-controller output into a flat u8[] then call
        // kickDmaStream) — this lands in Phase 1 follow-up once the
        // single-byte path is bench-validated. For v1, ship the API
        // compatible with the polled driver and let writeBytes() be the
        // bulk-DMA path (APA102 driver chains it inside the controller).
        waitFully();
        release();
    }

    static void finalizeTransmission() FL_NO_EXCEPT {}

private:
    // Program the DMA channel's source-end pointer + XFERCFG for a stream
    // of `count` halfwords from sEncodeBuf to SPI->TXDAT.
    //
    // LPC845 DMA descriptor layout (UM11029 §17.4): 16-byte block in SRAM
    // at SRAMBASE + 16*channel. Fields:
    //     +0  XFERCFG (written via DMA0->CHANNEL[ch].XFERCFG, NOT in descr.)
    //     +4  SRCEND  — pointer to LAST byte/halfword/word of source
    //     +8  DSTEND  — pointer to LAST byte/halfword/word of destination
    //     +12 LINK    — next descriptor (0 for one-shot)
    //
    // For a non-incrementing destination (DSTINC=0), DSTEND points at the
    // single destination address. For source-increment (SRCINC=+1
    // halfword), SRCEND points at the address of the LAST halfword in
    // the source buffer, NOT one past it.
    //
    // SRAMBASE ownership: init() calls `fl::lpc::ensureDmaSramBase()` so
    // the shared 25-channel table (lpc_dma_descriptor_table.h) is armed
    // before any kick. The srambase==0 early-return below is a defensive
    // guard for mis-sequenced callers (kick before init): arming a
    // channel with SRAMBASE=0 makes the engine fetch its descriptor from
    // the FLASH VECTOR TABLE at address 0 and DMA-write through garbage
    // pointers — silicon-diagnosed as `.bss` corruption + PC jumping into
    // a static object (FastLED #3580).
    //
    // Returns true if the descriptor was written and XFERCFG armed;
    // callers must NOT set SETVALID when this returns false.
    static bool configureChannelDescriptor(u32 count) FL_NO_EXCEPT {
        if (count == 0u) return false;

        const u32 ch = FASTLED_LPC_SPI_DMA_CHANNEL;
        const u32 src_end = reinterpret_cast<u32>(&sEncodeBuf[count - 1u]); // ok reinterpret cast - LPC DMA descriptor requires raw address
        const u32 dst_end = reinterpret_cast<u32>(&spi_block()->TXDAT); // ok reinterpret cast - LPC DMA descriptor requires raw address

        // Descriptor write. UM11029 §17.4.3: the descriptor table is at
        // SRAMBASE; channel N's descriptor occupies bytes [16*N, 16*N+15].
        const u32 srambase = DMA0->SRAMBASE;
        if (srambase == 0u) {
            return false;  // kick before init() — refuse to arm (see above)
        }
        volatile u32 *descr = reinterpret_cast<volatile u32*>(srambase + 16u * ch); // ok reinterpret cast - SRAMBASE + channel offset = raw descriptor table address
        // descr[0] is the XFERCFG-shadow word; the live XFERCFG is
        // the per-channel register. UM11029 wires both ways.
        descr[1] = src_end;  // SRCEND
        descr[2] = dst_end;  // DSTEND
        descr[3] = 0u;       // LINK = no chained descriptor

        // XFERCFG. UM11029 §16.6.19 / §16.7.1:
        //   bit  0    CFGVALID
        //   bit  1    RELOAD = 0  (one-shot)
        //   bit  2    SWTRIG = 1  — REQUIRED. "Triggered" and "paced" are
        //             separate states: PERIPHREQEN makes the SPI TXRDY
        //             request PACE individual transfers, but the channel
        //             only services requests once it has been TRIGGERED
        //             (CTLSTAT.TRIG=1). With HWTRIGEN=0 nothing triggers
        //             it in hardware, so software must. Silicon-diagnosed
        //             (#3580 register snapshot): CFGVALID=1, PERIPHREQEN=1,
        //             TXRDY=1, yet CTLSTAT.TRIG=0 and XFERCOUNT frozen at
        //             full — armed forever, zero transfers, until SWTRIG.
        //   bit  4    SETINTA = 1 (interrupt-A fires at end-of-transfer;
        //                          NVIC vector defaults to disabled, so
        //                          this is harmless unless the user
        //                          explicitly enables DMA0_IRQn)
        //   bit  8-9  WIDTH  = 01b (16-bit halfword)
        //   bit 12-13 SRCINC = 01b (+1 halfword)
        //   bit 14-15 DSTINC = 00b (no-inc, every write hits TXDAT)
        //   bit 16-25 XFERCOUNT = count-1
        const u32 xfercfg =
            (1UL << 0) |          // CFGVALID
            (1UL << 2) |          // SWTRIG — see block comment above
            (1UL << 4) |          // SETINTA
            (1UL << 8) |          // WIDTH = 16-bit
            (1UL << 12) |         // SRCINC = +1 halfword
            (0UL << 14) |         // DSTINC = no
            (((count - 1u) & 0x3FFu) << 16);
        DMA0->CHANNEL[ch].XFERCFG = xfercfg;
        return true;
    }
};

// Static encoding buffer definition. C++11 requires out-of-class definition
// for non-inline static data members; templated dummy form mirrors the
// PWM-DMA clockless driver's static-buffer pattern.
template <u8 D, u8 C, u32 DIV, u32 P>
u16 ARMHardwareSPIOutputDMA<D, C, DIV, P>::sEncodeBuf[FASTLED_LPC_SPI_DMA_MAX_BYTES];

}  // namespace fl

FL_DISABLE_WARNING_POP

#endif  // FL_IS_ARM_LPC && FL_IS_ARM_LPC_845 && FASTLED_LPC_SPI_DMA

#endif  // __INC_SPI_ARM_LPC_DMA_H
