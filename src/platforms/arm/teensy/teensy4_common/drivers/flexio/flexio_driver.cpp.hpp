// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/flexio/flexio_driver.h"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/iflexio_peripheral.h"

#include "fl/log/log.h"
#include "fl/stl/cstring.h"

// IWYU pragma: begin_keep
#include <Arduino.h>
#include <imxrt.h>
#include <DMAChannel.h>
// IWYU pragma: end_keep

// The Teensy framework defines FLEXIO2_* as macros that expand to struct member
// accesses (e.g. IMXRT_FLEXIO2.offset010). We need to #undef them so we can
// define our own register accessors that support array indexing.
#undef FLEXIO2_CTRL
#undef FLEXIO2_SHIFTSTAT
#undef FLEXIO2_SHIFTERR
#undef FLEXIO2_TIMSTAT
#undef FLEXIO2_SHIFTSDEN
#undef FLEXIO2_SHIFTCTL
#undef FLEXIO2_SHIFTCFG
#undef FLEXIO2_SHIFTBUF
#undef FLEXIO2_SHIFTBUFBIS
#undef FLEXIO2_TIMCTL
#undef FLEXIO2_TIMCFG
#undef FLEXIO2_TIMCMP

namespace fl {

// ============================================================================
// FlexIO2 Pin Mapping Table (Teensy 4.0/4.1 → FlexIO2 pins)
// From RESEARCH.md §11
// ============================================================================

struct FlexIOPinEntry {
    u8 teensy_pin;
    u8 flexio_pin;
    u32 mux_reg_offset;  // Offset from IOMUXC base for SW_MUX_CTL
    u32 pad_reg_offset;  // Offset from IOMUXC base for SW_PAD_CTL
};

// IOMUXC base address
static constexpr u32 kIOMUXC_BASE = 0x401F8000;

// FlexIO2 pin mapping entries. Offsets are byte offsets from IOMUXC
// base 0x401F8000 and are cross-checked against Teensyduino imxrt.h
// macros (IOMUXC_SW_MUX_CTL_PAD_GPIO_B*_** / IOMUXC_SW_PAD_CTL_PAD_*).
// The previous table was off by exactly +0x10 on every entry, so the
// per-pin mux register write went to a different pad and the requested
// TX pin was never actually switched to ALT4 (FlexIO2). The pin stayed
// in its boot/GPIO default alt mode and the FlexIO shifter, while
// running internally, drove an unrouted internal signal. This was the
// real root cause of zero_capture across rounds 1-6 -- not register
// values, not clock, not DMA, not sequencing.
static constexpr FlexIOPinEntry kFlexIOPins[] = {
    {10, 0,  0x013C, 0x032C},  // GPIO_B0_00
    {12, 1,  0x0140, 0x0330},  // GPIO_B0_01
    {11, 2,  0x0144, 0x0334},  // GPIO_B0_02
    {13, 3,  0x0148, 0x0338},  // GPIO_B0_03
    { 6, 10, 0x0164, 0x0354},  // GPIO_B0_10
    { 9, 11, 0x0168, 0x0358},  // GPIO_B0_11
    {32, 12, 0x016C, 0x035C},  // GPIO_B0_12
    { 8, 16, 0x017C, 0x036C},  // GPIO_B1_00
    { 7, 17, 0x0180, 0x0370},  // GPIO_B1_01
};
static constexpr int kNumFlexIOPins = sizeof(kFlexIOPins) / sizeof(kFlexIOPins[0]);

bool flexio_lookup_pin(u8 teensy_pin, FlexIOPinInfo* info) {
    for (int i = 0; i < kNumFlexIOPins; i++) {
        if (kFlexIOPins[i].teensy_pin == teensy_pin) {
            info->teensy_pin = teensy_pin;
            info->flexio_pin = kFlexIOPins[i].flexio_pin;
            info->mux_reg = (volatile u32*)(kIOMUXC_BASE + kFlexIOPins[i].mux_reg_offset);
            info->pad_reg = (volatile u32*)(kIOMUXC_BASE + kFlexIOPins[i].pad_reg_offset);
            return true;
        }
    }
    return false;
}

// #3416 FX-CRIT-1: enforce at compile time that this driver targets
// only FlexIO2 (the kFLEXIO2_BASE / FLEXIO2_* register pointers are
// hard-coded). FlexIO2 on i.MX RT1062 has 8 shifters (indices 0-7);
// any pin table entry above flexio_pin=17 is suspicious because the
// physical pad routing on Teensy 4.x maxes at FLEXIO2_FLEXIO17.
// Anything above that suggests a copy-paste from a FlexIO3 reference
// where pin indices go higher -- which would silently write to the
// FlexIO2 register block for a pad electrically routed to FlexIO3.
static constexpr bool flexio_pin_table_is_flexio2_only() {
    for (int i = 0; i < kNumFlexIOPins; i++) {
        if (kFlexIOPins[i].flexio_pin > 17) return false;
    }
    return true;
}
static_assert(flexio_pin_table_is_flexio2_only(),
              "kFlexIOPins entries must target FlexIO2 pads only "
              "(flexio_pin <= 17). See #3416 FX-CRIT-1.");

// ============================================================================
// FlexIO2 Register Access Helpers
// ============================================================================

static constexpr u32 kFLEXIO2_BASE = 0x401B0000;

// #3410 Round-2 audit: CTRL is at offset 0x008, NOT 0x000.
// Offset 0x000 is VERID (read-only Version ID per i.MX RT1062 FlexIO RM).
// The previous +0x000 address was writing to a read-only register, so
// FLEXEN / FASTACC / SWRST never actually got set, and subsequent FlexIO
// register writes (which require the module to be clocked AND enabled)
// triggered IMPRECISERR. addr2line on the crash addresses pointed at
// flexio_init's call to flexio_configure_hw -- consistent with FLEXIO2_CTRL
// writes silently failing.
static volatile u32& FLEXIO2_CTRL     = *(volatile u32*)(kFLEXIO2_BASE + 0x008);
static volatile u32& FLEXIO2_SHIFTSTAT = *(volatile u32*)(kFLEXIO2_BASE + 0x010);
static volatile u32& FLEXIO2_SHIFTERR  = *(volatile u32*)(kFLEXIO2_BASE + 0x014);
static volatile u32& FLEXIO2_TIMSTAT   = *(volatile u32*)(kFLEXIO2_BASE + 0x018);
static volatile u32& FLEXIO2_SHIFTSDEN = *(volatile u32*)(kFLEXIO2_BASE + 0x030);

static volatile u32* FLEXIO2_SHIFTCTL  = (volatile u32*)(kFLEXIO2_BASE + 0x080);
static volatile u32* FLEXIO2_SHIFTCFG  = (volatile u32*)(kFLEXIO2_BASE + 0x100);
static volatile u32* FLEXIO2_SHIFTBUF  = (volatile u32*)(kFLEXIO2_BASE + 0x200);
static volatile u32* FLEXIO2_SHIFTBUFBIS = (volatile u32*)(kFLEXIO2_BASE + 0x380);

static volatile u32* FLEXIO2_TIMCTL    = (volatile u32*)(kFLEXIO2_BASE + 0x400);
static volatile u32* FLEXIO2_TIMCFG    = (volatile u32*)(kFLEXIO2_BASE + 0x480);
static volatile u32* FLEXIO2_TIMCMP    = (volatile u32*)(kFLEXIO2_BASE + 0x500);

// ============================================================================
// Module State
// ============================================================================

static DMAChannel* sDmaChannel = nullptr;
static volatile bool sDmaComplete = true;
static bool sInitialized = false;
static u8 sFlexIOPin = 0;
static u32 sLatchCycles = 0;
static FlexIOPinInfo sCurrentPinInfo{};

static constexpr u32 kMaxPixelBytes = 4096;
DMAMEM static u32 sPixelBuffer[kMaxPixelBytes / 4] __attribute__((aligned(32)));

static volatile u32 sDmaErrorCount = 0;
static volatile u32 sLastDmaEs = 0;

static void flexio_dma_isr() {
    // Guard: deinit() can delete sDmaChannel while a pending IRQ is still
    // queued at the NVIC. Without this check the next IRQ vector services
    // a deleted channel and dereferences a freed pointer (IMPRECISERR
    // data bus fault observed during FlexIO bring-up, #3410).
    if (sDmaChannel != nullptr) {
        sDmaChannel->clearInterrupt();
    }
    // #3416 FX-HIGH-6: ensure the clearInterrupt() write to DMA_CINT
    // reaches the eDMA before we return. On Cortex-M7 a posted store can
    // still be in the write buffer when the ISR returns, allowing the
    // NVIC to immediately re-fire the same vector before the interrupt
    // status actually clears. Matches Teensyduino FlexSerial.cpp:512.
    asm volatile("dsb" ::: "memory");
    sDmaComplete = true;
}

// #3416 FX-HIGH-5: DMA_ES (eDMA Error Status) is now sampled in
// flexio_read_diagnostics() and surfaced via the JSON-RPC diag dump.
// Installing a dedicated error-vector handler would require taking
// IRQ_DMA_ERROR globally (shared with Audio/Serial), so we expose
// the state passively instead.

// ============================================================================
// Clock Configuration
// ============================================================================

static void flexio_clock_init() {
    // #3410 Round-2 audit: Match Teensyduino FlexIO_t4::setClockSettings()
    // exactly: gate OFF -> set CSCMR2 (source) -> set CS1CDR (dividers)
    // -> gate ON. The previous order (CS1CDR before CSCMR2) put dividers
    // on a not-yet-routed clock source and the FlexIO module never
    // started clocking -- causing register writes in flexio_configure_hw
    // to IMPRECISERR (decoded via addr2line to FlexIOPeripheralReal::init).
    CCM_CCGR3 &= ~CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);

    CCM_CSCMR2 = (CCM_CSCMR2 & ~CCM_CSCMR2_FLEXIO2_CLK_SEL(3)) |
                 CCM_CSCMR2_FLEXIO2_CLK_SEL(3);

    CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_FLEXIO2_CLK_PRED(7) |
                                   CCM_CS1CDR_FLEXIO2_CLK_PODF(7))) |
                 CCM_CS1CDR_FLEXIO2_CLK_PRED(1) |
                 CCM_CS1CDR_FLEXIO2_CLK_PODF(1);

    // #3416 FX-MED-3: barrier so CCM_CS1CDR write commits before we
    // enable the clock gate. Without this, the CCM may still be
    // propagating the new divider when the gate is opened, letting the
    // FlexIO module see one or two ticks of the previous (or
    // undefined) clock divider before settling.
    asm volatile("dsb" ::: "memory");
    (void)CCM_CS1CDR;

    CCM_CCGR3 |= CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);
}

// ============================================================================
// Pin Muxing
// ============================================================================

// IOMUXC pad drive strength for the FlexIO TX pin (DSE field, bits 3-5).
// Larger DSE -> stronger driver -> faster edges; smaller DSE -> softer
// edges that behave like a built-in series-termination resistor.
//
// Empirical sweep (Teensy 4.1 pin 8 -> pin 22 loopback, jumper wire,
// 100 LEDs x 4 patterns, with park-pin-LOW fix in place):
//   DSE=1 (~150 ohm)  -> 0.9% byte corruption
//   DSE=3 (~50 ohm)   -> 0.8% byte corruption (default)
//   DSE=6 (~25 ohm)   -> 0.9% byte corruption
// All three converge to the same RX-side noise floor; varying drive
// strength did NOT improve things. The residual errors are receiver-
// side capture dropout (raw_sample shows ~200 us mid-frame HIGH gaps)
// rather than edge ringing. Define is exposed for hardware-specific
// tuning on different boards / wire lengths.
//   Values: 0 = output disabled, 1..7 = R0/N drive
#ifndef FL_FLEXIO_TX_DSE
#define FL_FLEXIO_TX_DSE 3
#endif

// IOMUXC pad SPEED field (bits 6-7). 0 = 50 MHz, 1 = 100 MHz, 2 = 150 MHz,
// 3 = 200 MHz. Lower SPEED also softens the slew rate slightly.
#ifndef FL_FLEXIO_TX_SPEED
#define FL_FLEXIO_TX_SPEED 0
#endif

static void flexio_pin_init(const FlexIOPinInfo& pin_info) {
    // #3410 Round 5: mux value MUST include the SION bit (0x10) per
    // Teensyduino FlexIO_t4 library reference (flex2_hardware uses 0x14
    // = ALT4 + SION). Without SION the IOMUXC input path is disabled
    // and the FlexIO peripheral cannot drive the pin in alt mode --
    // root cause of zero_capture across all prior bring-up rounds
    // (#3411, #3412, #3413, Round-4 experiments). All other register
    // settings were correct; this single missing bit kept the pad in a
    // gated state.
    *(pin_info.mux_reg) = 4 | 0x10;  // ALT4 + SION
    *(pin_info.pad_reg) =
        ((FL_FLEXIO_TX_DSE & 0x7u) << 3) |
        ((FL_FLEXIO_TX_SPEED & 0x3u) << 6);
}

// #3410 Round 7 follow-up: between flexio_show() calls the FlexIO
// peripheral leaves the line in an undefined state (the receiver
// observed an extended HIGH between frames). The decoder then treats
// that long HIGH as an invalid first edge-pair and silently advances
// past it, eating the FIRST WS2812 bit of the next frame. The visible
// symptom is byte_1 = 0xFE for expected 0xFF, byte_n's bits all shifted
// left by 1, etc.
//
// Fix: BEFORE re-enabling FLEXEN for a new frame, briefly switch the pad
// to ALT5 (GPIO5) output and drive it LOW for >=60 us so the receiver
// captures a clean LOW idle. Then restore ALT4 | SION immediately
// before the shifter loads its first word.
static void flexio_pin_park_low(const FlexIOPinInfo& pin_info) {
    // ALT5 == GPIO5 mode on all Teensy 4.x B0/B1 pads we map.
    // Use Arduino's ::pinMode/::digitalWriteFast (the fl:: overloads
    // are not interchangeable with the Arduino enum constants).
    // #3416 FX-MED-5: confirmed working on pin 8 (GPIO_B1_00 ->
    // GPIO7_IO16 fast alias) via bench loopback. Other supported
    // FlexIO2 pins (pins 6, 7, 9-13, 32) all live on B0_xx / B1_xx
    // pads that use the same GPIO7 fast-alias path on Teensy 4.x,
    // so this should work uniformly. Scope verification on each
    // unique pad bank is still recommended before declaring portable.
    *(pin_info.mux_reg) = 5;
    ::pinMode(pin_info.teensy_pin, OUTPUT);
    ::digitalWriteFast(pin_info.teensy_pin, LOW);
    delayMicroseconds(60);
    // Restore the FlexIO mux on the way back to flexio_show().
    *(pin_info.mux_reg) = 4 | 0x10;
}

// ============================================================================
// FlexIO2 Timer/Shifter Configuration
// ============================================================================

static void flexio_configure_hw(u8 flexio_pin, u32 baud_div) {
    // #3410 Round 5: complete architectural rewrite. The previous 4-timer
    // + 1-shifter design with OR-gated outputs never produced any TX
    // signal because the per-bit gating mechanism (Timer 2 conditional on
    // shifter data) is not actually expressible in FlexIO's TRGSEL
    // encoding -- TRGSEL only exposes pin-input / shifter-status-flag /
    // timer-trigger-output, NOT shifter-data-output.
    //
    // The whatnick FRDM-K82F WS2812 driver and Finomnis Rust ws2812-flexio
    // both sidestep this by encoding each WS2812 bit as MULTIPLE FlexIO
    // bits in the DMA buffer (e.g. `1000` for 0-bit, `1110` for 1-bit at
    // ~3.2 MHz baud) and routing the shifter output DIRECTLY to the pin
    // with PINCFG=3. This eliminates the timer-gating problem entirely:
    // the FlexIO hardware just clocks bits out one at a time, and the
    // pre-encoded data stream IS the WS2812 waveform.
    //
    // Architecture this function programs:
    //   - Shifter 0: TX mode, PINCFG=3, PINSEL=flexio_pin, TIMSEL=Timer 0
    //   - Timer 0: dual 8-bit baud, drives shift clock, TRGSEL=shifter 0
    //              status flag (TIMENA=trigger-high, TIMDIS=compare)
    //   - Timer 1, 2, 3: NOT USED (dropped from this design)
    //
    // Per-bit pulse shape comes from the pre-encoded DMA buffer; reset
    // latch is handled by gap between consecutive flexio_show() calls
    // plus the LOW tail of the last encoded bit.

    // Software reset.
    FLEXIO2_CTRL = (1 << 1);
    {
        const u32 swrst_start = micros();
        while (FLEXIO2_CTRL & (1 << 1)) {
            if ((u32)(micros() - swrst_start) >= 1000) {
                break;
            }
        }
    }
    FLEXIO2_CTRL = 0;

    // Shifter 0: Transmit mode, output bit drives flexio_pin directly.
    // #3416 FX-HIGH-2: mask PINSEL to 5 bits (PINSEL field per imxrt.h
    // FLEXIO_SHIFTCTL_PINSEL(n) = ((n) & 0x1F) << 8). Any FlexIO pin
    // index above 31 (a future copy-paste from a FlexIO3 table, which
    // does have higher pin indices) would otherwise silently overflow
    // into PINPOL (bit 7) and the high bit of the pin number would be
    // lost. Today this is a no-op since our table maxes at pin 17.
    FLEXIO2_SHIFTCTL[0] =
        (2u << 0) |                              // SMOD = Transmit
        ((u32)(flexio_pin & 0x1F) << 8) |        // PINSEL = output pin
        (3u << 16) |                             // PINCFG = output enabled
        (0u << 24);                              // TIMSEL = Timer 0

    FLEXIO2_SHIFTCFG[0] = 0;           // 1-bit serial, no start/stop bits

    // Timer 0: shift clock for shifter 0. Cross-referenced against the
    // working Teensyduino FlexSerial driver (UART TX uses the same
    // 1-bit-shifter + dual-8bit-baud-timer topology) -- four real bugs
    // present prior to this revision:
    //   1. TIMCMP bit-count field: per RM 47.4.18, "the number of bits in
    //      each word equals (CMP[15:8]+1)/2", so 32 bits => CMP[15:8]=63
    //      not 31. The previous value emitted only 16 of the 32
    //      pre-encoded FlexIO bits per word, so even the bits we did emit
    //      were the wrong pattern.
    //   2. TRGPOL (bit 23) missing -- trigger needs to be active-low so it
    //      asserts when SSF is LOW (= shifter just loaded with data) and
    //      deasserts when SSF is HIGH (= shifter consumed all bits). With
    //      TRGPOL=0 the polarity was inverted: timer wanted to run while
    //      shifter was empty.
    //   3. TIMENA=6 (rising-edge enable) replaced with TIMENA=2 (level:
    //      enable while trigger high). FlexSerial uses level; our use
    //      case is the same.
    //   4. (See flexio_show below) FASTACC bit must NOT be set on CTRL --
    //      FASTACC requires FlexIO clock >= 2x bus, but here FlexIO is
    //      120 MHz and bus is 600 MHz; FASTACC=1 makes register reads
    //      and writes unreliable.
    //
    // With these four fixes the loop works: shifter empties -> SSF high
    // -> trigger low -> timer disables (and DMA request fires on SSF
    // high) -> DMA writes next word to SHIFTBUF -> SSF low -> trigger
    // high -> TIMENA=2 re-enables timer -> 32 shifts -> repeat.
    const u32 bit_count = (32u * 2u) - 1u;  // = 63 for 32 bits per word

    FLEXIO2_TIMCTL[0] =
        (1u << 0) |                    // TIMOD = Dual 8-bit baud
        (1u << 22) |                   // TRGSRC = internal
        (1u << 23) |                   // TRGPOL = active low
        (1u << 24);                    // TRGSEL = 4*0+1 = shifter 0 status

    FLEXIO2_TIMCFG[0] =
        (2u << 8) |                    // TIMENA = enable while trigger high
        (2u << 12);                    // TIMDIS = on compare
                                       // TIMOUT = 0 (logic HIGH on enable):
                                       // With TIMPOL=0 (shifter shifts on
                                       // RISING edge), TIMOUT=1 starts the
                                       // timer output LOW and the first
                                       // rising edge happens at baud/2 ->
                                       // the FIRST shift fires after only
                                       // half a baud period, eating bit 0's
                                       // pin time. TIMOUT=0 keeps the timer
                                       // output HIGH on enable so the first
                                       // edge to shift on is a full baud
                                       // later. Empirically this is what
                                       // restores the leading-bit alignment.

    FLEXIO2_TIMCMP[0] = (bit_count << 8) | (baud_div & 0xFFu);

    // Clear status flags
    FLEXIO2_SHIFTSTAT = 0xFFu;
    FLEXIO2_SHIFTERR = 0xFFu;
    FLEXIO2_TIMSTAT = 0xFFu;
}

// ============================================================================
// DMA Configuration
// ============================================================================

static bool flexio_dma_init() {
    if (sDmaChannel) {
        return true;
    }
    sDmaChannel = new DMAChannel();  // ok bare allocation
    if (!sDmaChannel) {
        FL_LOG_FLEXIO_F("FlexIO: Failed to allocate DMA channel");
        return false;
    }
    // #3416 FX-HIGH-3: DMAMUX source 1 (REQUEST0) is shared between
    // Shifter 0 SSF and Shifter 1 SSF on the i.MX RT1062. We use only
    // Shifter 0, but if a future change adds a second shifter for
    // parallel-LED support, the DMA would fire on both shifters'
    // empty events and either over-feed or stall the second one.
    // Document the assumption so it's visible.
    // #3416 FX-LOW-5: DMAMUX is wired here once. ChannelEngineFlexIO's
    // pin/timing reinit path re-enters flexio_init() which calls
    // flexio_dma_init() again -- the early-return at the top means
    // the DMAMUX routing is unchanged across reinits (correct, since
    // we still target the same FlexIO2 shifter 0).
    sDmaChannel->triggerAtHardwareEvent(DMAMUX_SOURCE_FLEXIO2_REQUEST0);
    sDmaChannel->attachInterrupt(flexio_dma_isr);
    // #3416 FX-HIGH-5: DO NOT install IRQ_DMA_ERROR handler globally --
    // that vector is shared by EVERY eDMA channel on the system and
    // replacing it would break Audio I2S, FlexSerial, ObjectFLED, etc.
    // Instead, expose DMA_ES + per-channel error flags via
    // flexio_read_diagnostics() (FlexIODiagnostics now includes
    // dma_es) so user code can poll the error state on its own
    // schedule without owning the interrupt.
    return true;
}

// ============================================================================
// Free-function API (used by flexio_driver.h)
// ============================================================================

// Encode a single WS2812 byte into 32 FlexIO bits.
//
// The FlexIO shifter in TX mode is LSB-first: bit 0 of the internal
// shifter is driven to the pin first, then bits 1, 2, ..., 31. WS2812
// protocol is MSB-first (each byte's bit 7 sent first).
//
// To match those two, the FIRST WS2812 bit of the byte (= bit 7 of `b`)
// must occupy the LOW nibble of the u32, and its 4-bit pulse encoding
// must be ordered so that LSB-first emission produces the right wire
// pattern (HIGH FIRST, then trailing LOW):
//   '1' bit on the wire = HIGH for 3 ticks then LOW for 1 = stream
//     (1,1,1,0); LSB-first storage = nibble 0b0111 = 0x7
//   '0' bit on the wire = HIGH for 1 tick then LOW for 3 = stream
//     (1,0,0,0); LSB-first storage = nibble 0b0001 = 0x1
//
// The DMA target is SHIFTBUF (no swap) because the encoder already
// produces the LSB-first-friendly layout. SHIFTBUFBIS would over-correct
// and re-invert.
static inline u32 flexio_encode_ws2812_byte(u8 b) {
    u32 result = 0;
    for (int i = 7; i >= 0; --i) {
        const u32 nib = (b & (1u << i)) ? 0x7u : 0x1u;
        result |= nib << ((7 - i) * 4);
    }
    return result;
}

// FlexIO clock divider sized so each FlexIO shift bit is ~316 ns
// (4 bits per WS2812 bit, ~1.27 us total per WS2812 bit). FlexIO2 base
// clock is 120 MHz; with baud_div = 18 the shift period is
// (18+1)*2 = 38 FlexIO ticks = 38/120MHz = ~317 ns per bit.
//   0-bit: 1 HIGH + 3 LOW   = ~317 ns HIGH, ~950 ns LOW (within T0H/T0L spec)
//   1-bit: 3 HIGH + 1 LOW   = ~950 ns HIGH, ~317 ns LOW (within T1H/T1L spec)
// Total period per WS2812 bit ~= 1267 ns.
//
// #3416 FX-MED-6 / FX-LOW-7: the 1267 ns total is at the upper edge of
// the WS2812B reset-detection window. Per WS2812B datasheet rev 2017,
// reset detection requires >=50 us LOW; nominal bit period is 1250 ns
// +/- 600 ns. 1267 ns is safely within spec, but some WS2812B clones
// (notably some early SK6812-RGB-W batches) have tighter trailing-low
// detectors that may misinterpret. Scope verification across vendors
// is recommended before changing baud_div.
static constexpr u32 kFlexIOBaudDiv = 18;

bool flexio_init(const FlexIOPinInfo& pin_info, u32 t0h_ns, u32 t1h_ns,
                 u32 period_ns, u32 reset_us) {
    // #3416 FX-MED-1: t0h_ns/t1h_ns/period_ns are currently IGNORED.
    // The encoder hard-codes WS2812B nominal timing via kFlexIOBaudDiv
    // and the 0x1/0x7 nibble layout. Honouring the params requires
    // (a) computing baud_div from period_ns, (b) selecting between
    // 4-bit-per-WS-bit and 5-bit-per-WS-bit encoding for chipsets with
    // tighter timing ratios. Defer until a non-WS2812B chipset is
    // explicitly added to the supported list (ChannelEngineFlexIO's
    // canHandle() filter currently lets SK6812/WS2811/APA106 pass at
    // the period range, so they silently get WS2812B waveforms).
    (void)t0h_ns;
    (void)t1h_ns;
    (void)period_ns;
    if (sInitialized) {
        flexio_deinit();
    }

    FL_LOG_FLEXIO_F("FlexIO: init pin %s (FlexIO2:%s)", (int)pin_info.teensy_pin, (int)pin_info.flexio_pin);

    flexio_clock_init();
    flexio_pin_init(pin_info);

    sFlexIOPin = pin_info.flexio_pin;
    sLatchCycles = reset_us;
    sCurrentPinInfo = pin_info;

    flexio_configure_hw(pin_info.flexio_pin, kFlexIOBaudDiv);

    if (!flexio_dma_init()) {
        return false;
    }

    sInitialized = true;
    sDmaComplete = true;
    return true;
}

bool flexio_show(const u8* pixel_data, u32 num_bytes) {
    if (!sInitialized || !sDmaChannel || !pixel_data || num_bytes == 0) {
        return false;
    }

    flexio_wait();

    // Quiesce: hard-disable DMA + clear flags before reprogramming the TCD.
    // flexio_wait can return early via its 50 ms timeout; if it does, DMA
    // is still live and modifying the TCD mid-transfer is undefined
    // behaviour on i.MX RT eDMA (IMPRECISERR observed pre-#3411).
    sDmaChannel->disable();
    sDmaChannel->clearComplete();
    sDmaChannel->clearInterrupt();
    sDmaChannel->clearError();
    sDmaComplete = true;
    // #3416 FX-CRIT-3: ensure DMA disable + flag clears commit to the
    // eDMA engine before we touch FlexIO registers below. Without the
    // barrier, an in-flight minor-loop write to SHIFTBUF could overlap
    // the FLEXIO2_CTRL &= ~1 disable, leaving the shifter in an
    // ambiguous state right when park_low switches the mux to ALT5.
    asm volatile("dsb" ::: "memory");

    // Each WS2812 byte expands to 4 FlexIO bytes (32 FlexIO bits, 4 per
    // WS2812 bit). Cap to buffer.
    const u32 kMaxInputBytes = kMaxPixelBytes / 4u;
    if (num_bytes > kMaxInputBytes) {
        // #3416 FX-MED-2: warn loudly so the user sees that their strip
        // is being silently truncated rather than discovering tail LEDs
        // are dark. kMaxInputBytes = 1024 bytes = 341 RGB LEDs.
        FL_LOG_FLEXIO_F("FlexIO: strip truncated -- requested %d bytes exceeds buffer cap %d (~341 RGB LEDs max). Tail LEDs will not update.",
                        (int)num_bytes, (int)kMaxInputBytes);
        num_bytes = kMaxInputBytes;
    }

    // Pre-encode each pixel byte into a 32-bit FlexIO bit stream.
    for (u32 i = 0; i < num_bytes; ++i) {
        sPixelBuffer[i] = flexio_encode_ws2812_byte(pixel_data[i]);
    }

    const u32 num_words = num_bytes;  // one u32 per input byte
    arm_dcache_flush_delete(sPixelBuffer, num_words * 4u);

    FLEXIO2_CTRL &= ~1u;
    FLEXIO2_SHIFTSTAT = 0xFFu;
    FLEXIO2_SHIFTERR = 0xFFu;
    FLEXIO2_TIMSTAT = 0xFFu;
    // #3416 FX-CRIT-2 / FX-HIGH-4: SHIFTSDEN write deferred to AFTER
    // TCD is fully programmed and DMA channel is enabled. The previous
    // ordering (SHIFTSDEN here -> TCD writes -> FLEXEN -> enable())
    // theoretically allowed a half-programmed TCD to service a pending
    // DMA request if the channel was still ERQ-armed from a previous
    // frame. Even though sDmaChannel->disable() above clears ERQ, on a
    // re-init path (different pin/timing) a stale armed flag could
    // sneak through. Move SHIFTSDEN to after enable() to close the
    // window unambiguously.

    // Park the pin LOW via GPIO before re-asserting the FlexIO mux. This
    // forces a clean LOW idle the receiver can decode the first '1' bit
    // against; without it the previous-frame pin state leaks into the
    // first edge-pair capture and the WS2812 decoder eats the leading
    // data bit (off-by-one across the whole frame, observed pre-Round 7
    // residual fix).
    flexio_pin_park_low(sCurrentPinInfo);

    sDmaChannel->TCD->SADDR = sPixelBuffer;
    sDmaChannel->TCD->SOFF = 4;
    sDmaChannel->TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
    sDmaChannel->TCD->NBYTES_MLNO = 4;
    sDmaChannel->TCD->SLAST = -(i32)(num_words * 4u);
    // Write to SHIFTBUF (not SHIFTBUFBIS): SHIFTBUFBIS would bit-swap the
    // word and break our MSB-first pre-encoding. The shifter naturally
    // shifts MSB-first from SHIFTBUF, which is what we want.
    sDmaChannel->TCD->DADDR = &FLEXIO2_SHIFTBUF[0];
    sDmaChannel->TCD->DOFF = 0;
    sDmaChannel->TCD->CITER_ELINKNO = num_words;
    sDmaChannel->TCD->BITER_ELINKNO = num_words;
    sDmaChannel->TCD->DLASTSGA = 0;
    sDmaChannel->TCD->CSR = DMA_TCD_CSR_INTMAJOR | DMA_TCD_CSR_DREQ;
    // #3416 FX-LOW-1: duplicate DADDR write removed (was set above
    // already). Editing artifact from the bring-up rounds.

    sDmaComplete = false;
    // CTRL = FLEXEN only. FASTACC (bit 2) was set previously, but per RM
    // 47.4.2 "Fast access requires the FlexIO clock to be at least twice
    // the bus interface clock frequency". On Teensy 4.x the bus runs at
    // 600 MHz while FlexIO is 120 MHz (PLL3/4), so FASTACC=1 corrupts
    // register reads/writes. Working Teensyduino FlexSerial uses
    // FLEXEN alone -- match that.
    FLEXIO2_CTRL = (1u << 0);

    // Do NOT prime SHIFTBUF: with the correct trigger polarity
    // (TRGPOL=1) and TIMENA=2 (level), the first DMA request fires
    // automatically on the initial SSF=HIGH (shifter empty) state right
    // after FlexIO enable. Priming would write SHIFTBUF first, leaving
    // CITER mis-matched with actual words queued (an off-by-one
    // over-shift). The hardware self-starts cleanly.

    sDmaChannel->enable();
    // #3416 FX-CRIT-2: enable shifter-empty -> DMA request now that
    // both the TCD and the channel are fully armed. The first DMA
    // request fires on the next FLEXEN-induced SSF=HIGH transition.
    FLEXIO2_SHIFTSDEN = (1u << 0);

    return true;
}

bool flexio_is_done() {
    return sDmaComplete;
}

void flexio_wait() {
    // Bounded wait so a stuck FlexIO/DMA can't take down the whole RPC test
    // session. WS2812 frame for 100 LEDs at 800 kHz takes ~3 ms. 50 ms is
    // plenty of headroom for any reasonable strip length while staying
    // well below the autoresearch 120 s RPC deadline.
    const u32 start = millis();
    const u32 timeout_ms = 50;
    while (!sDmaComplete) {
        if ((u32)(millis() - start) >= timeout_ms) {
            // #3416 FX-MED-4: on timeout, force-recover instead of
            // leaving sDmaComplete=false. The previous code returned with
            // the flag still false, so the very next flexio_show() would
            // call flexio_wait() again, time out a second time, and the
            // stuck channel was never disposed. Force-disable + mark
            // complete so the next show() can reprogram a fresh TCD.
            if (sDmaChannel) {
                sDmaChannel->disable();
                sDmaChannel->clearComplete();
                sDmaChannel->clearError();
            }
            sDmaComplete = true;
            FL_LOG_FLEXIO_F("FlexIO: flexio_wait() timed out after %s ms -- recovering",
                            (int)timeout_ms);
            return;
        }
    }
}

void flexio_read_diagnostics(FlexIODiagnostics* out) {
    if (!out) return;

    // #3416 FX-LOW-4: snapshot atomically so the ISR can't run mid-read
    // and produce an inconsistent (e.g. dmaComplete=false but
    // tcd_citer=0) view. Interrupts are off only for the brief register
    // reads themselves; this is a diagnostic-only path so the latency
    // cost is acceptable.
    noInterrupts();

    // Always zero-fill first. The CCM clock-gate / divider registers and
    // the driver's own bookkeeping fields are always safe to read, so
    // populate them unconditionally. The FLEXIO2_* peripheral block is
    // ONLY safe to read once the CCM clock gate is on and FLEXEN has
    // been set at least once by flexio_init() -- accessing it any
    // earlier hits the same IMPRECISERR class fault we chased during
    // bring-up (#3411, #3412). Bail out of the FlexIO-side reads when
    // !sInitialized so the autoresearch RPC path stays safe on every
    // Teensy 4.x test, including those that never bring FlexIO up.
    *out = FlexIODiagnostics{};
    out->ccm_ccgr3 = CCM_CCGR3;
    out->ccm_cscmr2 = CCM_CSCMR2;
    out->ccm_cs1cdr = CCM_CS1CDR;
    out->initialized = sInitialized;
    out->dmaComplete = sDmaComplete;
    // FX-HIGH-5: eDMA error status snapshot. DMA_ES is a global register
    // shared across all eDMA channels; non-zero indicates SOMETHING
    // erred (not necessarily our channel), so consumers must cross-
    // reference the ERR field of DMA_ES against our channel index.
    out->dma_es = DMA_ES;

    if (!sInitialized) {
        interrupts();  // FX-LOW-4 atomic window close on early return
        return;
    }

    out->ctrl = FLEXIO2_CTRL;
    out->shiftstat = FLEXIO2_SHIFTSTAT;
    out->shifterr = FLEXIO2_SHIFTERR;
    out->timstat = FLEXIO2_TIMSTAT;
    out->shiftsden = FLEXIO2_SHIFTSDEN;
    out->shiftctl0 = FLEXIO2_SHIFTCTL[0];
    out->shiftcfg0 = FLEXIO2_SHIFTCFG[0];
    out->timctl0 = FLEXIO2_TIMCTL[0];
    out->timcfg0 = FLEXIO2_TIMCFG[0];
    out->timcmp0 = FLEXIO2_TIMCMP[0];
    for (int i = 0; i < kNumFlexIOPins; i++) {
        if (kFlexIOPins[i].flexio_pin == sFlexIOPin) {
            out->muxRegValue = *(volatile u32*)(kIOMUXC_BASE + kFlexIOPins[i].mux_reg_offset);
            out->padRegValue = *(volatile u32*)(kIOMUXC_BASE + kFlexIOPins[i].pad_reg_offset);
            break;
        }
    }
    if (sDmaChannel && sDmaChannel->TCD) {
        out->tcd_saddr = (u32)sDmaChannel->TCD->SADDR;
        out->tcd_daddr = (u32)sDmaChannel->TCD->DADDR;
        out->tcd_citer = sDmaChannel->TCD->CITER_ELINKNO;
        out->tcd_biter = sDmaChannel->TCD->BITER_ELINKNO;
        out->tcd_csr = sDmaChannel->TCD->CSR;
    }
    // TCD fields already zero from the FlexIODiagnostics{} value init above.
    interrupts();  // close the FX-LOW-4 atomic-snapshot window
}

void flexio_deinit() {
    if (!sInitialized) return;
    flexio_wait();
    // Shut down FlexIO BEFORE touching DMA so no more shifter-empty DMA
    // requests can fire while we're tearing the channel down.
    FLEXIO2_CTRL = 0;
    FLEXIO2_SHIFTSDEN = 0;
    if (sDmaChannel) {
        // Order: disable ERQ -> detach interrupt -> clear pending NVIC IRQ
        // -> then mark pointer null -> only THEN delete. Without the
        // detach + IRQ-clear, a pending interrupt vector dereferences a
        // deleted channel (#3410 IMPRECISERR fault).
        sDmaChannel->disable();
        sDmaChannel->detachInterrupt();
        sDmaChannel->clearInterrupt();
        DMAChannel* to_delete = sDmaChannel;
        sDmaChannel = nullptr;
        delete to_delete;  // ok bare allocation
    }
    sInitialized = false;
    sDmaComplete = true;
}

// ============================================================================
// Real IFlexIOPeripheral Implementation (Teensy hardware)
// ============================================================================

class FlexIOPeripheralReal : public IFlexIOPeripheral {
public:
    FlexIOPeripheralReal() = default;
    ~FlexIOPeripheralReal() override { deinit(); }

    bool canHandlePin(u8 teensy_pin) const override {
        FlexIOPinInfo info;
        return flexio_lookup_pin(teensy_pin, &info);
    }

    bool init(u8 teensy_pin, u32 t0h_ns, u32 t1h_ns,
              u32 period_ns, u32 reset_us) override {
        FlexIOPinInfo info;
        if (!flexio_lookup_pin(teensy_pin, &info)) {
            return false;
        }
        return flexio_init(info, t0h_ns, t1h_ns, period_ns, reset_us);
    }

    bool show(const u8* pixel_data, u32 num_bytes) override {
        return flexio_show(pixel_data, num_bytes);
    }

    bool isDone() const override {
        return flexio_is_done();
    }

    void wait() override {
        flexio_wait();
    }

    void deinit() override {
        flexio_deinit();
    }
};

// Factory: create real peripheral on Teensy hardware
fl::shared_ptr<IFlexIOPeripheral> IFlexIOPeripheral::create() {
    return fl::make_shared<FlexIOPeripheralReal>();
}

} // namespace fl

#endif // FL_IS_TEENSY_4X
