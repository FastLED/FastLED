// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/flexio/flexio_driver.h"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/iflexio_peripheral.h"

#include "fl/system/log.h"
#include "fl/system/log.h"
#include "fl/stl/cstring.h"

// IWYU pragma: begin_keep
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

static volatile u32& FLEXIO2_CTRL     = *(volatile u32*)(kFLEXIO2_BASE + 0x000);
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
    sDmaChannel->clearInterrupt();
    sDmaComplete = true;
}

// ============================================================================
// Clock Configuration
// ============================================================================

static void flexio_clock_init() {
    CCM_CCGR3 |= CCM_CCGR3_FLEXIO2(CCM_CCGR_ON);

    u32 cs1cdr = CCM_CS1CDR;
    cs1cdr &= ~(CCM_CS1CDR_FLEXIO2_CLK_PODF(7) |
                CCM_CS1CDR_FLEXIO2_CLK_PRED(7));
    cs1cdr |= CCM_CS1CDR_FLEXIO2_CLK_PRED(1) |
              CCM_CS1CDR_FLEXIO2_CLK_PODF(1);
    CCM_CS1CDR = cs1cdr;

    u32 cscmr2 = CCM_CSCMR2;
    cscmr2 &= ~CCM_CSCMR2_FLEXIO2_CLK_SEL(3);
    cscmr2 |= CCM_CSCMR2_FLEXIO2_CLK_SEL(3);
    CCM_CSCMR2 = cscmr2;
}

// ============================================================================
// Pin Muxing
// ============================================================================

static void flexio_pin_init(const FlexIOPinInfo& pin_info) {
    *(pin_info.mux_reg) = 4;
    *(pin_info.pad_reg) = (3 << 3) | (0 << 6);
}

// ============================================================================
// FlexIO2 Timer/Shifter Configuration
// ============================================================================

static void flexio_configure_hw(u8 flexio_pin, u32 t0h_clocks, u32 t1h_clocks,
                                 u32 period_clocks, u32 latch_clocks) {
    // Software reset
    FLEXIO2_CTRL = (1 << 1);
    while (FLEXIO2_CTRL & (1 << 1)) {}
    FLEXIO2_CTRL = 0;

    // Shifter 0: Transmit mode, 1-bit serial, output on flexio_pin
    FLEXIO2_SHIFTCTL[0] =
        (2 << 0) |                    // SMOD = Transmit
        ((u32)flexio_pin << 8) |      // PINSEL
        (3 << 16) |                   // PINCFG = output
        (0 << 24);                    // TIMSEL = Timer 0

    FLEXIO2_SHIFTCFG[0] = 0;         // 1-bit serial, no start/stop bits

    // Timer 0: Shift clock (Dual 8-bit baud)
    u32 baud_div = (period_clocks / 2) - 1;
    u32 bit_count = (32 - 1);

    FLEXIO2_TIMCTL[0] =
        (1 << 0) |                    // TIMOD = Dual 8-bit baud
        (1 << 22) |                   // TRGSRC = internal
        (1 << 24);                    // TRGSEL = shifter 0 status

    FLEXIO2_TIMCFG[0] =
        (2 << 8) |                    // TIMENA = trigger high
        (2 << 12) |                   // TIMDIS = on compare
        (1 << 24);                    // TIMOUT = logic 0 on enable

    FLEXIO2_TIMCMP[0] = ((bit_count) << 8) | (baud_div & 0xFF);

    // Timer 1: Low-bit PWM (always fires, short HIGH for '0' bit)
    u32 t0h_high = t0h_clocks - 1;
    u32 t0h_low = period_clocks - t0h_clocks - 1;

    FLEXIO2_TIMCTL[1] =
        (2 << 0) |                    // TIMOD = PWM
        ((u32)flexio_pin << 8) |      // PINSEL = output pin
        (3 << 16) |                   // PINCFG = output
        (1 << 22) |                   // TRGSRC = internal
        (0 << 24);                    // TRGSEL = Timer 0 output

    FLEXIO2_TIMCFG[1] =
        (6 << 8) |                    // TIMENA = trigger rising
        (2 << 12) |                   // TIMDIS = on compare
        (1 << 24);                    // TIMOUT = logic 0 on enable

    FLEXIO2_TIMCMP[1] = (t0h_low << 8) | (t0h_high & 0xFF);

    // Timer 2: High-bit PWM (fires only when data=1, extends HIGH)
    u32 t1h_high = t1h_clocks - t0h_clocks - 1;
    u32 t1h_low = period_clocks - t1h_clocks - 1;

    FLEXIO2_TIMCTL[2] =
        (2 << 0) |                    // TIMOD = PWM
        ((u32)flexio_pin << 8) |      // PINSEL = output pin
        (3 << 16) |                   // PINCFG = output
        (1 << 22) |                   // TRGSRC = internal
        (1 << 24);                    // TRGSEL = shifter 0 output

    FLEXIO2_TIMCFG[2] =
        (6 << 8) |                    // TIMENA = trigger rising
        (6 << 12) |                   // TIMDIS = trigger falling
        (1 << 24);                    // TIMOUT = logic 0 on enable

    FLEXIO2_TIMCMP[2] = (t1h_low << 8) | (t1h_high & 0xFF);

    // Timer 3: Latch (single 16-bit counter for reset period)
    FLEXIO2_TIMCTL[3] =
        (3 << 0) |                    // TIMOD = 16-bit counter
        ((u32)flexio_pin << 8) |      // PINSEL = output pin
        (3 << 16) |                   // PINCFG = output
        (1 << 22) |                   // TRGSRC = internal
        (0 << 24);                    // TRGSEL = Timer 0

    FLEXIO2_TIMCFG[3] =
        (6 << 8) |                    // TIMENA = trigger rising
        (2 << 12) |                   // TIMDIS = on compare
        (6 << 16) |                   // TIMRST = trigger rising
        (1 << 24);                    // TIMOUT = logic 0 on enable

    FLEXIO2_TIMCMP[3] = latch_clocks & 0xFFFF;

    // Clear status flags
    FLEXIO2_SHIFTSTAT = 0xFF;
    FLEXIO2_SHIFTERR = 0xFF;
    FLEXIO2_TIMSTAT = 0xFF;
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
        FL_LOG_FLEXIO("FlexIO: Failed to allocate DMA channel");
        return false;
    }
    sDmaChannel->triggerAtHardwareEvent(DMAMUX_SOURCE_FLEXIO2_REQUEST0);
    sDmaChannel->attachInterrupt(flexio_dma_isr);
    return true;
}

// ============================================================================
// Free-function API (used by flexio_driver.h)
// ============================================================================

bool flexio_init(const FlexIOPinInfo& pin_info, u32 t0h_ns, u32 t1h_ns,
                 u32 period_ns, u32 reset_us) {
    if (sInitialized) {
        flexio_deinit();
    }

    FL_LOG_FLEXIO("FlexIO: init pin " << (int)pin_info.teensy_pin
           << " (FlexIO2:" << (int)pin_info.flexio_pin << ")");

    flexio_clock_init();
    flexio_pin_init(pin_info);

    static constexpr u32 kFlexIOClkMHz = 120;
    u32 t0h_clocks = (t0h_ns * kFlexIOClkMHz + 500) / 1000;
    u32 t1h_clocks = (t1h_ns * kFlexIOClkMHz + 500) / 1000;
    u32 period_clocks = (period_ns * kFlexIOClkMHz + 500) / 1000;
    u32 latch_clocks = reset_us * kFlexIOClkMHz;

    if (t0h_clocks < 2) t0h_clocks = 2;
    if (t1h_clocks < t0h_clocks + 2) t1h_clocks = t0h_clocks + 2;
    if (period_clocks < t1h_clocks + 2) period_clocks = t1h_clocks + 2;
    if (latch_clocks > 0xFFFF) latch_clocks = 0xFFFF;
    if (latch_clocks < 100) latch_clocks = 6000;

    sFlexIOPin = pin_info.flexio_pin;
    sLatchCycles = latch_clocks;

    flexio_configure_hw(pin_info.flexio_pin, t0h_clocks, t1h_clocks,
                        period_clocks, latch_clocks);

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

    if (num_bytes > kMaxPixelBytes) {
        num_bytes = kMaxPixelBytes;
    }

    u32 num_words = (num_bytes + 3) / 4;
    fl::memset(sPixelBuffer, 0, num_words * 4);
    fl::memcpy(sPixelBuffer, pixel_data, num_bytes);
    arm_dcache_flush_delete(sPixelBuffer, num_words * 4);

    FLEXIO2_CTRL &= ~1;
    FLEXIO2_SHIFTSTAT = 0xFF;
    FLEXIO2_SHIFTERR = 0xFF;
    FLEXIO2_TIMSTAT = 0xFF;
    FLEXIO2_SHIFTSDEN = (1 << 0);

    sDmaChannel->TCD->SADDR = sPixelBuffer;
    sDmaChannel->TCD->SOFF = 4;
    sDmaChannel->TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
    sDmaChannel->TCD->NBYTES_MLNO = 4;
    sDmaChannel->TCD->SLAST = -(i32)(num_words * 4);
    sDmaChannel->TCD->DADDR = &FLEXIO2_SHIFTBUFBIS[0];
    sDmaChannel->TCD->DOFF = 0;
    sDmaChannel->TCD->CITER_ELINKNO = num_words;
    sDmaChannel->TCD->BITER_ELINKNO = num_words;
    sDmaChannel->TCD->DLASTSGA = 0;
    sDmaChannel->TCD->CSR = DMA_TCD_CSR_INTMAJOR | DMA_TCD_CSR_DREQ;

    sDmaComplete = false;
    sDmaChannel->enable();
    FLEXIO2_CTRL = (1 << 0) | (1 << 2);

    return true;
}

bool flexio_is_done() {
    return sDmaComplete;
}

void flexio_wait() {
    while (!sDmaComplete) {}
}

void flexio_deinit() {
    if (!sInitialized) return;
    flexio_wait();
    FLEXIO2_CTRL = 0;
    FLEXIO2_SHIFTSDEN = 0;
    if (sDmaChannel) {
        sDmaChannel->disable();
        delete sDmaChannel;  // ok bare allocation
        sDmaChannel = nullptr;
    }
    sInitialized = false;
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
