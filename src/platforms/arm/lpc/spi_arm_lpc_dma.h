// IWYU pragma: private

#ifndef __INC_SPI_ARM_LPC_DMA_H
#define __INC_SPI_ARM_LPC_DMA_H

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fastspi_types.h"
#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/arm/lpc/spi_arm_lpc.h"

// =============================================================================
// LPC8xx SPI + DMA0 driver — async hardware-accelerated APA102/SK9822/WS2801
// =============================================================================
//
// Phase 1 of FastLED #3453. Drop-in peer of `ARMHardwareSPIOutput` from
// `spi_arm_lpc.h`. The polled driver writes one byte per `STAT.TXRDY` spin and
// blocks at the FastLED.show() boundary; this DMA driver hands the entire
// pixel frame to a DMA0 channel tied to the SPI block's TX request line, so
// the CPU is free in WFI during transmission.
//
// **Supported chips:** LPC845 and LPC804. Both share the same LPC8xx SPI
// peripheral block (UM11029 §15 for LPC84x, UM11065 §11 for LPC80x — same
// TXDATCTL / TXDAT / TXCTL register semantics). The two chips differ on
// the DMA side: LPC845 has 25 channels with SPI0_TX at conventional
// channel 4, LPC804 has only 4 channels total with SPI0_TX at
// conventional channel 0. The driver picks the right default per-chip
// via the `FASTLED_LPC_SPI_DMA_CHANNEL` macro (overridable).
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
//   * `FL_IS_ARM_LPC_845` OR `FL_IS_ARM_LPC_804`  (auto-detected from
//                                                   CMSIS device macros)
//   * `FASTLED_LPC_SPI_DMA`  (user-supplied, default off)
//
// When `FASTLED_LPC_SPI_DMA` is not set, `fastled_arm_lpc.h` continues
// to route SPI through the polled driver in `spi_arm_lpc.h`. Default
// LPC845 / LPC804 builds are unaffected.
//
// LPC804 support requires the vendor CMSIS PAL from
// `FastLED/framework-arduino-lpc8xx` at commit `1179200a30` or newer
// (adds `DMA_Type` + `DMA0` peripheral declarations for LPC804 —
// FastLED/framework-arduino-lpc8xx#35 / FastLED/fbuild#916). That is
// picked up transparently once the fbuild release containing #916
// lands and `pyproject.toml` bumps the fbuild pin.
//
// RESOURCES CONSUMED
// ------------------
//   * One DMA0 channel — `FASTLED_LPC_SPI_DMA_CHANNEL`. Default:
//       - LPC845: channel 4 (SPI0_TX per UM11029 Table 80). SPI1_TX is
//         channel 6; users targeting SPI1 override with
//         `-DFASTLED_LPC_SPI_DMA_CHANNEL=6`.
//       - LPC804: channel 0 (SPI0_TX per UM11065 §12.4 DMA trigger
//         source table). LPC804 has only 4 DMA channels total and one
//         SPI (SPI0), so channel allocation is tight but does not
//         collide with any FastLED-side driver today (the PWM+DMA
//         clockless driver is LPC845-only).
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

#if defined(FL_IS_ARM_LPC) && \
    (defined(FL_IS_ARM_LPC_845) || defined(FL_IS_ARM_LPC_804)) && \
    defined(FASTLED_LPC_SPI_DMA)

// Clear diagnostic for the LPC804 opt-in when the toolchain is still on
// the pre-#35 vendor CMSIS PAL. Without `DMA0` the `DMA0->CHANNEL[…]`
// accesses below would fail with a cascade of "undeclared" errors that
// don't hint at the actual problem. This #error steers the user at the
// fbuild release that includes FastLED/framework-arduino-lpc8xx#35.
#if defined(FL_IS_ARM_LPC_804) && !defined(DMA0)
#error "FASTLED_LPC_SPI_DMA=1 was set on an LPC804 build, but <LPC804.h> does not declare DMA0. Requires the vendor CMSIS PAL from FastLED/framework-arduino-lpc8xx#35 (SHA 1179200a30 or newer). fbuild#916 bumps the pin — use a fbuild release that includes it (>= 2.3.16 depending on the release cut)."
#endif

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

namespace fl {

// ----- Tuning macros -------------------------------------------------------

// DMA channel used for SPI TX. Defaults are chip-specific:
//
//   LPC845 (UM11029 Table 80):
//     SPI0_TX = channel 4
//     SPI1_TX = channel 6
//     Users targeting SPI1 override with `-DFASTLED_LPC_SPI_DMA_CHANNEL=6`.
//
//   LPC804 (UM11065 §12.4 DMA trigger source table):
//     SPI0_TX = channel 0
//     No SPI1 on LPC804.
//
// The channel-count difference reflects the underlying DMA hardware:
// LPC845's DMA0 has 25 channels; LPC804's DMA0 has only 4 (channels
// 0..3). Explicit overrides must satisfy the chip's channel limit or
// the DMA controller silently rejects the descriptor arm.
#ifndef FASTLED_LPC_SPI_DMA_CHANNEL
#if defined(FL_IS_ARM_LPC_804)
#define FASTLED_LPC_SPI_DMA_CHANNEL 0
#else
#define FASTLED_LPC_SPI_DMA_CHANNEL 4
#endif
#endif

// Upper bound on encoded-buffer size in u16 elements. APA102 frames =
// 4 bytes/LED * LEDs + 8 start bytes + ceil(LEDs/16) end bytes; 384 LEDs
// fits comfortably in the default 2 KB. Override for larger strips:
//     -DFASTLED_LPC_SPI_DMA_MAX_BYTES=4096
#ifndef FASTLED_LPC_SPI_DMA_MAX_BYTES
#define FASTLED_LPC_SPI_DMA_MAX_BYTES 2048
#endif

// ----- Driver --------------------------------------------------------------

/// @brief LPC8xx (LPC845 / LPC804) SPI + DMA0 async output for FastLED
///        clocked strips. LPC804 support gated on the vendor CMSIS PAL
///        from FastLED/framework-arduino-lpc8xx#35 (SHA `1179200a30`+).
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
    /// The user's startup code is responsible for:
    ///   (a) enabling the SPI clock in SYSCON->SYSAHBCLKCTRL[11] / [12]
    ///       (SPI0 / SPI1) on LPC845
    ///   (b) routing MOSI/SCK through SWM to the requested pins
    /// The driver enables SYSAHBCLKCTRL0 bit 29 for DMA0 itself.
    void init() FL_NO_EXCEPT {
        FastPin<_DATA_PIN>::setOutput();
        FastPin<_CLOCK_PIN>::setOutput();

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
        spi->TXCTL = FL_LPC_SPI_TXCTL_LEN_8BIT | FL_LPC_SPI_TXCTL_RXIGNORE;

        // ----- DMA0 power-up + channel configuration ---------------------
        // SYSAHBCLKCTRL0 bit 29 = DMA. Already shared with the PWM-DMA
        // clockless driver if both are gated on; the OR-set is idempotent.
        // TODO(3453): verify SYSCON->SYSAHBCLKCTRL0 bit 29 against
        // UM11029 §4.6.13.
        SYSCON->SYSAHBCLKCTRL0 |= (1UL << 29);

        // Enable the controller (master enable, not yet channel-enabled).
        DMA0->CTRL = 1UL;

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

        // INMUX wiring for SPI_TX request → DMA channel. LPC845 has 25
        // DMA channels, the first 16 of which have dedicated trigger
        // sources (no INMUX needed); SPI0_TX = channel 4 default per
        // UM11029 Table 80. For SPI1_TX (channel 6) the same default
        // wiring applies. Channels 16-24 use ITRIG_INMUX; this driver
        // assumes the user picks a channel in 0..15.
        // TODO(3453): if the user overrides FASTLED_LPC_SPI_DMA_CHANNEL
        // to 16..24, this path needs to also program DMA0->ITRIG_INMUX.

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
        // Drain any in-flight DMA stream first; then wait for the SPI
        // master to settle.
        waitDma();
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

        // Configure descriptor + XFERCFG and arm.
        configureChannelDescriptor(len);

        // SETVALID flips VALIDPENDING → VALID, arming the channel for the
        // next TXRDY trigger.
        DMA0->COMMON[0].SETVALID = (1UL << FASTLED_LPC_SPI_DMA_CHANNEL);
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
            configureChannelDescriptor(chunk);
            DMA0->COMMON[0].SETVALID = (1UL << FASTLED_LPC_SPI_DMA_CHANNEL);
            waitDma();
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
            configureChannelDescriptor(chunk);
            DMA0->COMMON[0].SETVALID = (1UL << FASTLED_LPC_SPI_DMA_CHANNEL);
            // Block between chunks — the static encode buffer is needed
            // for the next slice. Real async (no inter-chunk drain) needs
            // double-buffering, tracked at #3453 Phase 1 follow-up.
            waitDma();
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
    // TODO(3453): SRAMBASE allocation. The PWM-DMA driver punts to "SDK
    // clock_config.h conventional address"; this driver does the same in
    // v1 — the user's framework startup is expected to have programmed
    // DMA0->SRAMBASE before FastLED.show() runs. A future revision should
    // own a `static alignas(256) Descriptor[25]` block here.
    static void configureChannelDescriptor(u32 count) FL_NO_EXCEPT {
        if (count == 0u) return;

        const u32 ch = FASTLED_LPC_SPI_DMA_CHANNEL;
        const u32 src_end = reinterpret_cast<u32>(&sEncodeBuf[count - 1u]); // ok reinterpret cast - LPC DMA descriptor requires raw address
        const u32 dst_end = reinterpret_cast<u32>(&spi_block()->TXDAT); // ok reinterpret cast - LPC DMA descriptor requires raw address

        // Descriptor write. UM11029 §17.4.3: the descriptor table is at
        // SRAMBASE; channel N's descriptor occupies bytes [16*N, 16*N+15].
        const u32 srambase = DMA0->SRAMBASE;
        if (srambase != 0u) {
            volatile u32 *descr = reinterpret_cast<volatile u32*>(srambase + 16u * ch); // ok reinterpret cast - SRAMBASE + channel offset = raw descriptor table address
            // descr[0] is the XFERCFG-shadow word; the live XFERCFG is
            // the per-channel register. UM11029 wires both ways.
            descr[1] = src_end;  // SRCEND
            descr[2] = dst_end;  // DSTEND
            descr[3] = 0u;       // LINK = no chained descriptor
        }
        // If SRAMBASE is unprogrammed (== 0), the hardware will fault on
        // first descriptor fetch. TODO(3453): refuse to kick if SRAMBASE
        // is zero (defensive guard once the rest of the path stabilises).

        // XFERCFG. UM11029 §17.5:
        //   bit  0    CFGVALID
        //   bit  1    RELOAD = 0  (one-shot)
        //   bit  2    SWTRIG = 0  (hardware-triggered by SPI TXRDY)
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
            (1UL << 4) |          // SETINTA
            (1UL << 8) |          // WIDTH = 16-bit
            (1UL << 12) |         // SRCINC = +1 halfword
            (0UL << 14) |         // DSTINC = no
            (((count - 1u) & 0x3FFu) << 16);
        DMA0->CHANNEL[ch].XFERCFG = xfercfg;
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
