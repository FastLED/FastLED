/// @file fastspi_arm_mxrt1062.cpp
/// @brief Runs the real Teensy 4 Teensy4HardwareSPIOutput, its LpspiBus, and the
/// real SM16716Controller on three simulated LPSPI peripherals.
///
/// Each TDR word is clocked out at once as one frame of TCR.FRAMESZ + 1 bits, MSB
/// first, and leaves one word in the receive FIFO. A frame only reaches the wire
/// while the module is enabled and owns both pads. LPSPI frames are at least 8
/// bits, so a shorter one is counted as an error. Pins 11 and 13 also have GPIO
/// latches: a clock rise while both pads are muxed to GPIO puts the data latch
/// on the same wire. The test records each frame's length and whether chip
/// select was held, so a bit that is never sent, sent late, sent in an illegal
/// frame, or merged into a later transaction shows up as a wrong wire or count.

#include "test.h"

#include "FastLED.h"
#include "fastspi_types.h"
#include "fl/stl/int.h"
#include "fl/stl/vector.h"

constexpr fl::u32 kMen = 1U << 0;
constexpr fl::u32 kRst = 1U << 1;
constexpr fl::u32 kRtf = 1U << 8;
constexpr fl::u32 kRrf = 1U << 9;
constexpr fl::u32 kRxEmpty = 1U << 1;

bool gSelected = false;  // Chip select, driven by the test's Selectable.

constexpr fl::u32 kSpiMux = 3U | 0x10U;   // LPSPI4 SDO/SCK, as LpspiBus::begin sets them.
constexpr fl::u32 kGpioMux = 5U | 0x10U;  // What Teensy's pinMode writes.
constexpr fl::u32 kSpiPad = 0xB8U;        // DSE(7) | SPEED(2), as LpspiBus::begin sets it.
constexpr fl::u32 kGpioPad = 0x10B0U;

struct LpspiState {
    fl::u32 tcr = 0;
    fl::u32 cr = 0;
    fl::u32 cfgr1 = 0;
    fl::u32 ccr = 0;
    fl::u32 fcr = 0;
    int rx = 0;              // Words waiting in the receive FIFO.
    int rxUnderflows = 0;    // RDR reads with nothing to read.
    int framesUnselected = 0;
    int shortFrames = 0;     // Frames under the LPSPI minimum of 8 bits.
    int framesWithoutPads = 0;
    fl::vector<fl::u8> wire;
    fl::vector<fl::u32> frames;  // Bit length of each frame, in order.

    void reset() { *this = LpspiState(); }
};

bool spiOwnsPads(const LpspiState* s);

struct TcrReg {
    LpspiState* s;
    TcrReg& operator=(fl::u32 v) {
        s->tcr = v;
        return *this;
    }
    operator fl::u32() const { return s->tcr; }
};
struct TdrReg {
    LpspiState* s;
    TdrReg& operator=(fl::u32 v) {
        if ((s->cr & kMen) == 0) {
            return *this;
        }
        const fl::u32 bits = (s->tcr & 0xFFFU) + 1U;
        if (bits < 8) {
            ++s->shortFrames;
        }
        if (!spiOwnsPads(s)) {
            ++s->framesWithoutPads;
            ++s->rx;
            return *this;
        }
        for (fl::u32 i = bits; i-- > 0;) {
            s->wire.push_back(static_cast<fl::u8>((v >> i) & 1U));
        }
        s->frames.push_back(bits);
        ++s->rx;
        if (!gSelected) {
            ++s->framesUnselected;
        }
        return *this;
    }
};
struct RsrReg {
    LpspiState* s;
    operator fl::u32() const { return s->rx == 0 ? kRxEmpty : 0U; }
};
struct FsrReg {
    LpspiState* s;
    operator fl::u32() const { return static_cast<fl::u32>(s->rx) << 16; }
};
struct RdrReg {
    LpspiState* s;
    operator fl::u32() const {
        if (s->rx == 0) {
            ++s->rxUnderflows;
        } else {
            --s->rx;
        }
        return 0;
    }
};
struct CrReg {
    LpspiState* s;
    CrReg& operator=(fl::u32 v) {
        if (v & kRrf) {
            s->rx = 0;
        }
        s->cr = v & ~(kRrf | kRtf | kRst);
        return *this;
    }
    operator fl::u32() const { return s->cr; }
};
struct WordReg {
    fl::u32* p;
    WordReg& operator=(fl::u32 v) {
        *p = v;
        return *this;
    }
    operator fl::u32() const { return *p; }
};

struct FakeLpspi {
    explicit FakeLpspi(LpspiState* s)
        : TCR{s}, TDR{s}, RSR{s}, FSR{s}, RDR{s}, CR{s}, CFGR1{&s->cfgr1}, CCR{&s->ccr},
          FCR{&s->fcr} {}
    TcrReg TCR;
    TdrReg TDR;
    RsrReg RSR;
    FsrReg FSR;
    RdrReg RDR;
    CrReg CR;
    WordReg CFGR1;
    WordReg CCR;
    WordReg FCR;
};

// Index 0 is LPSPI4, 1 is LPSPI3, 2 is LPSPI1.
LpspiState gLpspi4;
LpspiState gLpspi3;
LpspiState gLpspi1;
FakeLpspi gPort4(&gLpspi4);
FakeLpspi gPort3(&gLpspi3);
FakeLpspi gPort1(&gLpspi1);

volatile fl::u32 gCcgr1 = 0;
fl::u32 gCbcmr = 0;
volatile fl::u32 gSelectInput[12] = {};
volatile fl::u32 gPads[64] = {};
volatile fl::u32 gPadMux[64] = {};

// Only LPSPI4's pads (11 and 13) are modelled; the other buses always own theirs.
bool spiOwnsPads(const LpspiState* s) {
    return s != &gLpspi4 || (gPadMux[11] == kSpiMux && gPadMux[13] == kSpiMux);
}

// GPIO latches for pins 11 (data) and 13 (clock).
LpspiState* gGpioWire = &gLpspi4;  // Where GPIO-clocked bits land.
bool gDataLatch = false;
bool gClockLatch = false;
int gInvisiblePulses = 0;    // Clock rises while the pads were not GPIO.

#define IMXRT_LPSPI_t FakeLpspi
#define IMXRT_LPSPI4_S gPort4
#define IMXRT_LPSPI3_S gPort3
#define IMXRT_LPSPI1_S gPort1
#define LPSPI_TCR_FRAMESZ(n) (static_cast<fl::u32>(n) & 0xFFFU)
#define LPSPI_TCR_LSBF (1U << 23)
#define LPSPI_TCR_CPHA (1U << 30)
#define LPSPI_TCR_CPOL (1U << 31)
#define LPSPI_RSR_RXEMPTY kRxEmpty
#define LPSPI_CR_MEN kMen
#define LPSPI_CR_RST kRst
#define LPSPI_CR_RTF kRtf
#define LPSPI_CR_RRF kRrf
#define LPSPI_CFGR1_MASTER (1U << 0)
#define LPSPI_CFGR1_SAMPLE (1U << 3)
#define LPSPI_CCR_SCKDIV(n) (static_cast<fl::u32>(n) & 0xFFU)
#define LPSPI_CCR_DBT(n) ((static_cast<fl::u32>(n) & 0xFFU) << 8)
#define LPSPI_CCR_PCSSCK(n) ((static_cast<fl::u32>(n) & 0xFFU) << 16)
#define LPSPI_FCR_TXWATER(n) (static_cast<fl::u32>(n) & 0xFU)
#define CCM_CBCMR gCbcmr
#define CCM_CBCMR_LPSPI_PODF_MASK (7U << 26)
#define CCM_CBCMR_LPSPI_CLK_SEL_MASK (3U << 4)
#define CCM_CBCMR_LPSPI_PODF(n) ((static_cast<fl::u32>(n) & 7U) << 26)
#define CCM_CBCMR_LPSPI_CLK_SEL(n) ((static_cast<fl::u32>(n) & 3U) << 4)
#define CCM_CCGR1 gCcgr1
#define CCM_CCGR_ON 3U
#define CCM_CCGR1_LPSPI1(n) (static_cast<fl::u32>(n) << 0)
#define CCM_CCGR1_LPSPI3(n) (static_cast<fl::u32>(n) << 4)
#define CCM_CCGR1_LPSPI4(n) (static_cast<fl::u32>(n) << 6)
#define IOMUXC_LPSPI4_SDI_SELECT_INPUT gSelectInput[0]
#define IOMUXC_LPSPI4_SDO_SELECT_INPUT gSelectInput[1]
#define IOMUXC_LPSPI4_SCK_SELECT_INPUT gSelectInput[2]
#define IOMUXC_LPSPI4_PCS0_SELECT_INPUT gSelectInput[3]
#define IOMUXC_LPSPI3_SDI_SELECT_INPUT gSelectInput[4]
#define IOMUXC_LPSPI3_SDO_SELECT_INPUT gSelectInput[5]
#define IOMUXC_LPSPI3_SCK_SELECT_INPUT gSelectInput[6]
#define IOMUXC_LPSPI3_PCS0_SELECT_INPUT gSelectInput[7]
#define IOMUXC_LPSPI1_SDI_SELECT_INPUT gSelectInput[8]
#define IOMUXC_LPSPI1_SDO_SELECT_INPUT gSelectInput[9]
#define IOMUXC_LPSPI1_SCK_SELECT_INPUT gSelectInput[10]
#define IOMUXC_LPSPI1_PCS0_SELECT_INPUT gSelectInput[11]
#define IOMUXC_PAD_DSE(n) ((static_cast<fl::u32>(n) & 7U) << 3)
#define IOMUXC_PAD_SPEED(n) ((static_cast<fl::u32>(n) & 3U) << 6)
#define portControlRegister(pin) (&gPads[(pin) & 63])
#define portConfigRegister(pin) (&gPadMux[(pin) & 63])
#ifndef INPUT_DISABLE
#define INPUT_DISABLE static_cast<fl::u8>(0)
#endif
#ifndef MSBFIRST
#define MSBFIRST 1
#endif
#ifndef LSBFIRST
#define LSBFIRST 0
#endif

// The header body is gated on the Teensy 4 family; enable it for this file only.
#define FL_IS_TEENSY_4X
#include "platforms/arm/teensy/teensy4_common/fastspi_arm_mxrt1062.h"
#undef FL_IS_TEENSY_4X

namespace fl {

template <> class FastPin<11> {
  public:
    static void setOutput() {
        gPadMux[11] = kGpioMux;
        gPads[11] = kGpioPad;
    }
    static void hi() { gDataLatch = true; }
    static void lo() { gDataLatch = false; }
};
template <> class FastPin<13> {
  public:
    static void setOutput() {
        gPadMux[13] = kGpioMux;
        gPads[13] = kGpioPad;
    }
    static void lo() { gClockLatch = false; }
    static void hi() {
        if (!gClockLatch) {
            if (gPadMux[11] == kGpioMux && gPadMux[13] == kGpioMux) {
                gGpioWire->wire.push_back(gDataLatch ? 1 : 0);
                if (!gSelected) {
                    ++gGpioWire->framesUnselected;
                }
            } else {
                ++gInvisiblePulses;
            }
        }
        gClockLatch = true;
    }
};

constexpr u32 kRate = 4000000;
template <int INDEX> using OutputAt = Teensy4HardwareSPIOutput<11, 13, kRate, INDEX>;
using Output = OutputAt<0>;

// Not the production dispatch: on Teensy 4, SPIOutput is SPIDeviceProxy, which
// has no writeBit, so SM16716 cannot reach this class there. This routes the real
// controller onto the real output class directly.
template <> class SPIOutput<11, 13, kRate> : public Output {};

class Controller : public SM16716Controller<11, 13, RGB, kRate> {
  public:
    using SM16716Controller<11, 13, RGB, kRate>::showPixels;
};

FL_TEST_FILE(FL_FILEPATH) {

namespace {

// Chip select. Records how much had reached the wire when it was released.
class FakeSelect : public Selectable {
  public:
    void select() FL_NO_EXCEPT override { gSelected = true; }
    void release() FL_NO_EXCEPT override {
        gSelected = false;
        wireAtRelease = gLpspi4.wire.size();
        clockLatchAtRelease = gClockLatch;
        ++releases;
    }
    bool isSelected() FL_NO_EXCEPT override { return gSelected; }
    fl::size wireAtRelease = 0;
    bool clockLatchAtRelease = false;
    int releases = 0;
};

void resetAll() {
    gLpspi4.reset();
    gLpspi3.reset();
    gLpspi1.reset();
    gSelected = false;
    gGpioWire = &gLpspi4;
    gDataLatch = gClockLatch = false;
    gInvisiblePulses = 0;
    for (int pin : {11, 13}) {
        gPadMux[pin] = kSpiMux;
        gPads[pin] = kSpiPad;
    }
}

// The TCR a transaction leaves on the bus: 8-bit frames, mode 0, MSB first.
constexpr u32 kIdleTcr = LPSPI_TCR_FRAMESZ(7);

void checkClean(const LpspiState& s) {
    FL_CHECK_EQ(s.rx, 0);
    FL_CHECK_EQ(s.rxUnderflows, 0);
    FL_CHECK_EQ(s.framesUnselected, 0);
    FL_CHECK_EQ(s.shortFrames, 0);
    FL_CHECK_EQ(s.framesWithoutPads, 0);
    FL_CHECK_EQ(s.tcr, kIdleTcr);
    FL_CHECK_EQ(gInvisiblePulses, 0);
    FL_CHECK_FALSE(gClockLatch);
    for (int pin : {11, 13}) {
        FL_CHECK_EQ(gPadMux[pin], kSpiMux);
        FL_CHECK_EQ(gPads[pin], kSpiPad);
    }
}

void expectBits(const LpspiState& s, const fl::vector<u8>& bits) {
    FL_REQUIRE_EQ(s.wire.size(), bits.size());
    for (fl::size i = 0; i < bits.size(); ++i) {
        FL_CHECK_EQ(s.wire[i], bits[i]);
    }
}

void appendByte(fl::vector<u8>& bits, u8 value) {
    for (int i = 7; i >= 0; --i) {
        bits.push_back(static_cast<u8>((value >> i) & 1));
    }
}

template <u8 BIT> void checkEveryValue() {
    for (int value = 0; value < 256; ++value) {
        resetAll();
        FakeSelect cs;
        Output spi(&cs);
        spi.select();
        spi.template writeBit<BIT>(static_cast<u8>(value));
        FL_REQUIRE(gLpspi4.wire.empty()); // Held until the frame ends.
        spi.endTransaction();
        FL_REQUIRE_EQ(gLpspi4.wire.size(), 1U);
        FL_REQUIRE_EQ(gLpspi4.wire[0], static_cast<u8>((value >> BIT) & 1));
        FL_REQUIRE_EQ(cs.wireAtRelease, 1U);
        checkClean(gLpspi4);
    }
}

// The bit a writeBit test sends at position i: an irregular, known pattern.
u8 patternBit(int i) { return static_cast<u8>(((0xB5C3U >> (i % 16)) ^ (i / 16)) & 1U); }

} // namespace

FL_TEST_CASE("writeBit: each selected bit, every value, sent by endTransaction") {
    checkEveryValue<0>();
    checkEveryValue<1>();
    checkEveryValue<2>();
    checkEveryValue<3>();
    checkEveryValue<4>();
    checkEveryValue<5>();
    checkEveryValue<6>();
    checkEveryValue<7>();
}

FL_TEST_CASE("bit counts 1-9, 15, 16 and 17 arrive in order before release") {
    // Whole bytes go as 8-bit LPSPI frames; a 1-7 bit remainder goes by GPIO.
    const int counts[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17};
    for (int n : counts) {
        resetAll();
        FakeSelect cs;
        Output spi(&cs);
        spi.select();
        fl::vector<u8> want;
        for (int i = 0; i < n; ++i) {
            spi.template writeBit<0>(patternBit(i));
            want.push_back(patternBit(i));
        }
        spi.release();
        expectBits(gLpspi4, want);
        FL_CHECK_EQ(cs.wireAtRelease, want.size());
        FL_REQUIRE_EQ(gLpspi4.frames.size(), static_cast<fl::size>(n / 8));
        for (u32 f : gLpspi4.frames) {
            FL_CHECK_EQ(f, 8U);
        }
        FL_CHECK_FALSE(cs.clockLatchAtRelease);
        checkClean(gLpspi4);
    }
}

FL_TEST_CASE("bit, byte, bit and bit, word keep their order") {
    resetAll();
    FakeSelect cs;
    Output spi(&cs);
    spi.select();
    spi.template writeBit<0>(1);
    spi.writeByte(0xA5);
    spi.template writeBit<0>(0);
    spi.template writeBit<0>(1);
    spi.writeWord(0x1234);
    spi.template writeBit<0>(1);
    spi.release();
    fl::vector<u8> want = {1};
    appendByte(want, 0xA5);
    want.push_back(0);
    want.push_back(1);
    appendByte(want, 0x12);
    appendByte(want, 0x34);
    want.push_back(1);
    expectBits(gLpspi4, want);
    FL_CHECK_EQ(cs.wireAtRelease, want.size());
    checkClean(gLpspi4);
}

FL_TEST_CASE("raw repeated bytes do not overtake pending bits") {
    resetAll();
    FakeSelect cs;
    Output spi(&cs);
    spi.select();
    spi.template writeBit<0>(1);
    spi.template writeBit<0>(0);
    spi.template writeBit<0>(1);
    spi.writeBytesValueRaw(0x0F, 2);
    spi.release();
    fl::vector<u8> want = {1, 0, 1};
    appendByte(want, 0x0F);
    appendByte(want, 0x0F);
    expectBits(gLpspi4, want);
    checkClean(gLpspi4);
}

FL_TEST_CASE("waitFully and finalizeTransmission each send pending bits") {
    resetAll();
    FakeSelect cs;
    Output spi(&cs);
    spi.select();
    spi.template writeBit<0>(1);
    spi.template writeBit<0>(1);
    spi.waitFully();
    expectBits(gLpspi4, {1, 1});
    spi.template writeBit<0>(0);
    spi.finalizeTransmission();
    expectBits(gLpspi4, {1, 1, 0});
    spi.release();
    expectBits(gLpspi4, {1, 1, 0});
    FL_CHECK_EQ(cs.releases, 1);
    checkClean(gLpspi4);
}

FL_TEST_CASE("pending bits never cross into the next transaction") {
    resetAll();
    FakeSelect cs;
    Output spi(&cs);
    spi.select();
    spi.template writeBit<0>(1);
    spi.release();
    spi.select(); // Reselect: a fresh transaction.
    spi.writeByte(0x00);
    spi.release();
    fl::vector<u8> want = {1};
    appendByte(want, 0x00);
    expectBits(gLpspi4, want);
    FL_REQUIRE_EQ(gLpspi4.frames.size(), 1U); // The lone bit went by GPIO.
    FL_CHECK_EQ(gLpspi4.frames[0], 8U);
    FL_CHECK_EQ(cs.releases, 2);
    checkClean(gLpspi4);
}

FL_TEST_CASE("byte-only transfers are unchanged: 8-bit frames, nothing extra") {
    resetAll();
    FakeSelect cs;
    Output spi(&cs);
    u8 data[] = {0x00, 0xFF, 0x5A};
    spi.writeBytes(data, 3);
    fl::vector<u8> want;
    for (u8 b : data) {
        appendByte(want, b);
    }
    expectBits(gLpspi4, want);
    FL_REQUIRE_EQ(gLpspi4.frames.size(), 3U);
    for (u32 f : gLpspi4.frames) {
        FL_CHECK_EQ(f, 8U);
    }
    spi.release(); // Nothing pending: an idle release sends nothing.
    FL_CHECK_EQ(gLpspi4.wire.size(), want.size());
    checkClean(gLpspi4);
}

FL_TEST_CASE("bit operations leave CFGR1 and CCR as select set them") {
    resetAll();
    Output spi;
    spi.select();
    const u32 cfgr1 = gLpspi4.cfgr1;
    const u32 ccr = gLpspi4.ccr;
    spi.template writeBit<0>(1);
    spi.writeByte(0x81);
    spi.template writeBit<0>(1);
    spi.release();
    FL_CHECK_EQ(gLpspi4.cfgr1, cfgr1);
    FL_CHECK_EQ(gLpspi4.ccr, ccr);
    FL_CHECK_EQ(gLpspi4.tcr, kIdleTcr);
}

FL_TEST_CASE("each LPSPI index drives only its own peripheral") {
    resetAll();
    OutputAt<0> a;
    OutputAt<1> b;
    OutputAt<2> c;
    gSelected = true;
    // A byte plus a bit: one 9-bit LPSPI frame on the bus the index names.
    a.select();
    a.template writeBit<0>(1);
    a.writeByte(0x80);
    a.release();
    b.select();
    b.template writeBit<0>(0);
    b.writeByte(0x01);
    b.release();
    c.select();
    c.template writeBit<0>(1);
    c.template writeBit<0>(1);
    c.writeByte(0xFF);
    c.release();
    fl::vector<u8> wa = {1};
    appendByte(wa, 0x80);
    fl::vector<u8> wb = {0};
    appendByte(wb, 0x01);
    fl::vector<u8> wc = {1, 1};
    appendByte(wc, 0xFF);
    expectBits(gLpspi4, wa);
    expectBits(gLpspi3, wb);
    expectBits(gLpspi1, wc);
    checkClean(gLpspi4);
    checkClean(gLpspi3);
    checkClean(gLpspi1);
}

FL_TEST_CASE("the real SM16716 controller: start bits, data, and the 50-bit header") {
    resetAll();
    gSelected = true;
    Controller controller;
    controller.init();
    gLpspi4.reset(); // Drop init's bookkeeping; keep the bus configured.
    gSelected = true;
    CRGB leds[3] = {CRGB(0x81, 0x42, 0x24), CRGB(0xFF, 0x00, 0x18), CRGB(0x00, 0xFF, 0x7E)};
    PixelController<RGB> pixels(leds, 3, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.showPixels(pixels);
    fl::vector<u8> want;
    for (const CRGB& led : leds) {
        want.push_back(1); // The start bit.
        appendByte(want, led.r);
        appendByte(want, led.g);
        appendByte(want, led.b);
    }
    for (int i = 0; i < 50; ++i) {
        want.push_back(0);
    }
    expectBits(gLpspi4, want);
    // Every start bit and both header bits ride in front of a byte: 9-bit frames.
    FL_REQUIRE_EQ(gLpspi4.frames.size(), 3U * 3U + 6U);
    for (fl::size i = 0; i < gLpspi4.frames.size(); ++i) {
        const bool leads = (i < 9) ? (i % 3 == 0) : (i == 9 || i == 12);
        FL_CHECK_EQ(gLpspi4.frames[i], leads ? 9U : 8U);
    }
    checkClean(gLpspi4);
}

} // FL_TEST_FILE

} // namespace fl
