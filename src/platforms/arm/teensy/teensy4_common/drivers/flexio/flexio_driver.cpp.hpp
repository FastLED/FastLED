// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/flexio/flexio_driver.h"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/iflexio_peripheral.h"

#include "fl/log/log.h"
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

// FlexIO2 pin mapping entries
static const FlexIOPinEntry kFlexIOPins[] = {
    {10, 0,  0x014C, 0x033C},  // GPIO_B0_00
    {12, 1,  0x0150, 0x0340},  // GPIO_B0_01
    {11, 2,  0x0154, 0x0344},  // GPIO_B0_02
    {13, 3,  0x0158, 0x0348},  // GPIO_B0_03
    { 6, 10, 0x0174, 0x0364},  // GPIO_B0_10
    { 9, 11, 0x0178, 0x0368},  // GPIO_B0_11
    {32, 12, 0x017C, 0x036C},  // GPIO_B0_12
    { 8, 16, 0x018C, 0x037C},  // GPIO_B1_00
    { 7, 17, 0x0190, 0x0380},  // GPIO_B1_01
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

static constexpr u32 kMaxPixelBytes = 4096;
DMAMEM static u32 sPixelBuffer[kMaxPixelBytes / 4] __attribute__((aligned(32)));

static void flexio_dma_isr() {
    // Guard: deinit() can delete sDmaChannel while a pending IRQ is still
    // queued at the NVIC. Without this check the next IRQ vector services
    // a deleted channel and dereferences a freed pointer (IMPRECISERR
    // data bus fault observed during FlexIO bring-up, #3410).
    if (sDmaChannel != nullptr) {
        sDmaChannel->clearInterrupt();
    }
    sDmaComplete = true;
}

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

    CCM_CCGR3 |= CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);
}

// ============================================================================
// Pin Muxing
// ============================================================================

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
    *(pin_info.pad_reg) = (3 << 3) | (0 << 6);
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
    FLEXIO2_SHIFTCTL[0] =
        (2u << 0) |                    // SMOD = Transmit
        ((u32)flexio_pin << 8) |       // PINSEL = output pin
        (3u << 16) |                   // PINCFG = output enabled
        (0u << 24);                    // TIMSEL = Timer 0

    FLEXIO2_SHIFTCFG[0] = 0;           // 1-bit serial, no start/stop bits

    // Timer 0: shift clock for shifter 0. Dual 8-bit baud generates
    // `bit_count + 1 = 32` shift edges per timer enable, with each edge
    // separated by `(baud_div + 1) * 2` FlexIO clock cycles. Triggered
    // by shifter status flag (asserts when shifter is empty in TX mode);
    // disabled when compare reaches 0 (i.e. all 32 bits shifted out).
    const u32 bit_count = 32u - 1u;

    FLEXIO2_TIMCTL[0] =
        (1u << 0) |                    // TIMOD = Dual 8-bit baud
        (1u << 22) |                   // TRGSRC = internal
        (1u << 24);                    // TRGSEL = 4*0+1 = shifter 0 status

    FLEXIO2_TIMCFG[0] =
        (6u << 8) |                    // TIMENA = trigger rising edge
        (2u << 12) |                   // TIMDIS = on compare
        (1u << 24);                    // TIMOUT = logic 0 on enable

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
    sDmaChannel->triggerAtHardwareEvent(DMAMUX_SOURCE_FLEXIO2_REQUEST0);
    sDmaChannel->attachInterrupt(flexio_dma_isr);
    return true;
}

// ============================================================================
// Free-function API (used by flexio_driver.h)
// ============================================================================

// Encode a single WS2812 byte into 32 FlexIO bits.
// MSB of the input byte becomes the first bit shifted out.
// Each WS2812 bit is expanded to a 4-bit pulse pattern:
//   0-bit -> 0b1000 = pin HIGH for 1 FlexIO clock, LOW for 3
//   1-bit -> 0b1110 = pin HIGH for 3 FlexIO clocks, LOW for 1
// The shifter outputs MSB-first, so we lay down the WS2812 MSB's
// 4-bit encoding in the high nibble of the resulting u32.
static inline u32 flexio_encode_ws2812_byte(u8 b) {
    u32 result = 0;
    for (int i = 7; i >= 0; --i) {
        result <<= 4;
        result |= (b & (1u << i)) ? 0xEu : 0x8u;
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
static constexpr u32 kFlexIOBaudDiv = 18;

bool flexio_init(const FlexIOPinInfo& pin_info, u32 t0h_ns, u32 t1h_ns,
                 u32 period_ns, u32 reset_us) {
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

    // Each WS2812 byte expands to 4 FlexIO bytes (32 FlexIO bits, 4 per
    // WS2812 bit). Cap to buffer.
    const u32 kMaxInputBytes = kMaxPixelBytes / 4u;
    if (num_bytes > kMaxInputBytes) {
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
    FLEXIO2_SHIFTSDEN = (1u << 0);

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

    sDmaComplete = false;
    // Enable FlexIO BEFORE DMA so the shifter is live when the first DMA
    // request fires (per #3412 fix; writing SHIFTBUF while FLEXEN=0
    // raises IMPRECISERR).
    FLEXIO2_CTRL = (1u << 0) | (1u << 2);

    // #3410 Round 5: Prime the shifter with the first word directly so the
    // FIRST shifter status flag transition happens. SSF in TX mode toggles
    // when SHIFTBUF transfers to internal shifter; without the prime,
    // SHIFTSDEN's DMA request might never assert and Timer 0 never sees
    // the rising edge that enables shifting.
    FLEXIO2_SHIFTBUF[0] = sPixelBuffer[0];

    sDmaChannel->enable();

    return true;
}

bool flexio_is_done() {
    return sDmaComplete;
}

void flexio_wait() {
    // Bounded wait so a stuck FlexIO/DMA can't take down the whole RPC test
    // session. WS2812 frame for 100 LEDs at 800 kHz takes ~3 ms. 50 ms is
    // plenty of headroom for any reasonable strip length while staying
    // well below the autoresearch 120 s RPC deadline. If the timeout
    // elapses, leave sDmaComplete=false so callers can detect the failure
    // via flexio_is_done() and the driver state remains observable.
    const u32 start = millis();
    const u32 timeout_ms = 50;
    while (!sDmaComplete) {
        if ((u32)(millis() - start) >= timeout_ms) {
            return;
        }
    }
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
