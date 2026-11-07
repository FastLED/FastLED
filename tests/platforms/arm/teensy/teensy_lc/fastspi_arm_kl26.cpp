/// @file fastspi_arm_kl26.cpp
/// @brief Runs the real Teensy LC ARMHardwareSPIOutput and SM16716Controller on a
/// simulated SPI0: pad mux ownership, transmit buffer, and a shifter that takes time.
///
/// Time advances only when firmware reads S, one bus clock per read. That is the
/// fastest a peripheral read can be, so it is the worst case for a drain that counts
/// reads. The registers are static members because the header casts its u32 base
/// address to a pointer: no memory at that address is ever loaded or stored.

#include "test.h"

#include "FastLED.h"
#include "fastspi_types.h"
#include "fl/stl/int.h"
#include "fl/stl/vector.h"

constexpr fl::u32 kMuxSpi = 2U << 8;
constexpr fl::u32 kMuxGpio = 1U << 8;
constexpr fl::u32 kSre = 1U << 2;
constexpr fl::u32 kIsf = 1U << 24; // Interrupt flag: write 1 to clear, write 0 to keep.

fl::vector<fl::u8> gWire;     // Every bit a device on the bus would clock in.
int gLostBits = 0;            // Shifted while SPI did not own both pads.
int gInvisiblePulses = 0;     // GPIO clock pulses while SPI owned the clock pad.
int gMuxExitsWithClockHigh = 0;
int gMuxChangesWhileShifting = 0;
int gDataWrittenWhileBufferFull = 0;
bool gClockLatch = false;
bool gDataLatch = false;

struct PinCfg {
    fl::u32 value = kMuxSpi;
    bool isClock = false;
    PinCfg& operator=(fl::u32 next);
    operator fl::u32() const { return value; }
};

PinCfg gOtherPins[8]; // Must never be touched: wrong pins for this bus.
PinCfg gDataCfg;
PinCfg gClockCfg;

bool spiOwnsPads() {
    return (gDataCfg.value & ~kIsf) == kMuxSpi && (gClockCfg.value & ~kIsf) == kMuxSpi;
}

struct Shifter {
    fl::u8 buffer = 0;
    bool bufferFull = false;
    fl::u8 shift = 0;
    int bitsLeft = 0;
    fl::u32 clocks = 0;
    fl::u32 divisor = 4;

    void tick() {
        if (bitsLeft == 0) {
            return;
        }
        if (++clocks < divisor) {
            return;
        }
        clocks = 0;
        if (spiOwnsPads()) {
            gWire.push_back((shift & 0x80) ? 1 : 0);
        } else {
            ++gLostBits;
        }
        shift = static_cast<fl::u8>(shift << 1);
        if (--bitsLeft == 0 && bufferFull) {
            load();
        }
    }
    void load() {
        shift = buffer;
        bitsLeft = 8;
        clocks = 0;
        bufferFull = false;
    }
};

Shifter gShifter;

PinCfg& PinCfg::operator=(fl::u32 next) {
    if (next != value) {
        if (gShifter.bitsLeft != 0 || gShifter.bufferFull) {
            ++gMuxChangesWhileShifting;
        }
        if (isClock && next != kMuxSpi && gClockLatch) {
            ++gMuxExitsWithClockHigh;
        }
    }
    value = (next & ~kIsf) | (value & kIsf & ~next);
    return *this;
}

constexpr fl::u8 kSptef = 0x20;

struct StatusReg {
    operator fl::u8() const {
        gShifter.tick();
        return gShifter.bufferFull ? 0 : kSptef;
    }
};
struct DataReg {
    DataReg& operator=(fl::u8 value) {
        if (gShifter.bufferFull) {
            ++gDataWrittenWhileBufferFull;
        }
        gShifter.buffer = value;
        gShifter.bufferFull = true;
        if (gShifter.bitsLeft == 0) {
            gShifter.load();
        }
        return *this;
    }
};
struct RateReg {
    fl::u8 value = 0;
    RateReg& operator=(fl::u8 next) {
        value = next;
        gShifter.divisor = static_cast<fl::u32>((next >> 4) & 7U) + 1U;
        gShifter.divisor <<= (next & 0xFU) + 1U;
        return *this;
    }
    operator fl::u8() const { return value; }
};
struct PlainReg {
    fl::u8 value = 0;
    PlainReg& operator=(fl::u8 next) {
        value = next;
        return *this;
    }
    PlainReg& operator|=(fl::u8 next) {
        value = static_cast<fl::u8>(value | next);
        return *this;
    }
};

struct FakeRegs {
    static StatusReg S;
    static DataReg DL;
    static RateReg BR;
    static PlainReg C1;
    static PlainReg C2;
};
StatusReg FakeRegs::S;
DataReg FakeRegs::DL;
RateReg FakeRegs::BR;
PlainReg FakeRegs::C1;
PlainReg FakeRegs::C2;

fl::u32 gScgc4 = 0;

void resetBus(fl::u32 padConfig) {
    gWire.clear();
    gLostBits = gInvisiblePulses = gMuxExitsWithClockHigh = 0;
    gMuxChangesWhileShifting = gDataWrittenWhileBufferFull = 0;
    gClockLatch = gDataLatch = false;
    gShifter = Shifter();
    gDataCfg.value = padConfig;
    gClockCfg.value = padConfig;
    gClockCfg.isClock = true;
    FakeRegs::BR = 0x10; // SPPR(1) SPR(0): the init rate.
}

#define KINETISL_SPI_t FakeRegs
#define SPI_S_SPTEF kSptef
#define SPI_C1_MSTR 0x10
#define SPI_C1_SPE 0x40
#define SPI_BR_SPPR(n) static_cast<fl::u8>(((n) & 7) << 4)
#define SPI_BR_SPR(n) static_cast<fl::u8>((n) & 15)
#define SIM_SCGC4 gScgc4
#define SIM_SCGC4_SPI0 (1U << 22)
#define SIM_SCGC4_SPI1 (1U << 23)
#define PORT_PCR_MUX(n) (static_cast<fl::u32>(n) << 8)
#define PORT_PCR_SRE kSre
#define PORT_PCR_ISF kIsf
#define CORE_PIN0_CONFIG gOtherPins[0]
#define CORE_PIN1_CONFIG gOtherPins[1]
#define CORE_PIN7_CONFIG gOtherPins[2]
#define CORE_PIN8_CONFIG gOtherPins[3]
#define CORE_PIN11_CONFIG gDataCfg
#define CORE_PIN12_CONFIG gOtherPins[4]
#define CORE_PIN13_CONFIG gClockCfg
#define CORE_PIN14_CONFIG gOtherPins[5]
#define CORE_PIN20_CONFIG gOtherPins[6]
#define CORE_PIN21_CONFIG gOtherPins[7]

#include "platforms/arm/teensy/teensy_lc/fastspi_arm_kl26.h"

namespace fl {

// Port latches. They reach the pad only while the mux selects GPIO.
template <> class FastPin<11> {
  public:
    static void setOutput() {}
    static void hi() { gDataLatch = true; }
    static void lo() { gDataLatch = false; }
};
template <> class FastPin<13> {
  public:
    static void setOutput() {}
    static void lo() { gClockLatch = false; }
    static void hi() {
        if (!gClockLatch) {
            if (gClockCfg.value == kMuxSpi) {
                ++gInvisiblePulses;
            } else if (gDataCfg.value != kMuxSpi) {
                gWire.push_back(gDataLatch ? 1 : 0);
            }
        }
        gClockLatch = true;
    }
};

constexpr u32 kDivider = 4;
using Output = ARMHardwareSPIOutput<11, 13, kDivider, 0x40076000>;

// The production dispatch for Teensy LC, onto the host's SPIOutput name.
template <> class SPIOutput<11, 13, kDivider> : public Output {};

class Controller : public SM16716Controller<11, 13, RGB, kDivider> {
  public:
    using SM16716Controller<11, 13, RGB, kDivider>::showPixels;
};

FL_TEST_FILE(FL_FILEPATH) {

namespace {

void checkClean() {
    FL_CHECK_EQ(gLostBits, 0);
    FL_CHECK_EQ(gInvisiblePulses, 0);
    FL_CHECK_EQ(gMuxExitsWithClockHigh, 0);
    FL_CHECK_EQ(gMuxChangesWhileShifting, 0);
    FL_CHECK_EQ(gDataWrittenWhileBufferFull, 0);
    FL_CHECK_FALSE(gClockLatch);
    for (const PinCfg& pin : gOtherPins) {
        FL_CHECK_EQ(pin.value, kMuxSpi);
    }
}

void expectByte(fl::size at, u8 value) {
    FL_REQUIRE(at + 8 <= gWire.size());
    for (int i = 0; i < 8; ++i) {
        FL_CHECK_EQ(gWire[at + i], static_cast<u8>((value >> (7 - i)) & 1));
    }
}

template <u8 BIT> void checkEveryValue() {
    for (int value = 0; value < 256; ++value) {
        resetBus(kMuxSpi);
        Output::template writeBit<BIT>(static_cast<u8>(value));
        FL_REQUIRE_EQ(gWire.size(), 1U);
        FL_REQUIRE_EQ(gWire[0], static_cast<u8>((value >> BIT) & 1));
        FL_REQUIRE_EQ(gDataCfg.value, kMuxSpi);
        FL_REQUIRE_EQ(gClockCfg.value, kMuxSpi);
        checkClean();
    }
}

} // namespace

FL_TEST_CASE("writeBit pulses once with the selected bit: every bit, every value") {
    checkEveryValue<0>();
    checkEveryValue<1>();
    checkEveryValue<2>();
    checkEveryValue<3>();
    checkEveryValue<4>();
    checkEveryValue<5>();
    checkEveryValue<6>();
    checkEveryValue<7>();
}

FL_TEST_CASE("byte, bit, byte keeps order at a range of clock rates") {
    for (u8 rate : {u8(0x00), u8(0x10), u8(0x70), u8(0x08), u8(0x78)}) {
        resetBus(kMuxSpi);
        FakeRegs::BR = rate;
        Output::writeByte(0xA5);
        Output::writeByte(0x3C); // The first is shifting, this one is buffered.
        Output::writeBit<0>(1);
        Output::writeByte(0xC3);
        Output::waitFully();
        FL_REQUIRE_EQ(gWire.size(), 25U);
        expectByte(0, 0xA5);
        expectByte(8, 0x3C);
        FL_CHECK_EQ(gWire[16], 1);
        expectByte(17, 0xC3);
        checkClean();
    }
}

FL_TEST_CASE("an idle call restores a pad configuration it did not set") {
    const u32 gpio = kSre | kMuxGpio;
    resetBus(gpio);
    Output::writeBit<7>(0x80);
    FL_REQUIRE_EQ(gWire.size(), 1U);
    FL_CHECK_EQ(gWire[0], 1);
    FL_CHECK_EQ(gDataCfg.value, gpio);
    FL_CHECK_EQ(gClockCfg.value, gpio);
    checkClean();
}

FL_TEST_CASE("a clock latch left high is parked before the pad leaves the mux") {
    resetBus(kMuxSpi);
    gClockLatch = true; // SPI owns the pad, so this level is not on the wire yet.
    Output::writeBit<0>(1);
    FL_REQUIRE_EQ(gWire.size(), 1U);
    FL_CHECK_EQ(gWire[0], 1);
    checkClean();
}

FL_TEST_CASE("restoring a pad does not clear its pending interrupt flag") {
    resetBus(kMuxSpi | PORT_PCR_ISF);
    Output::writeBit<0>(1);
    FL_CHECK_EQ(gDataCfg.value, kMuxSpi | kIsf);
    FL_CHECK_EQ(gClockCfg.value, kMuxSpi | kIsf);
}

FL_TEST_CASE("select forces mode 0 after another client left the clock polarity set") {
    resetBus(kSre | kMuxGpio);
    FakeRegs::C1 = static_cast<fl::u8>(SPI_C1_MSTR | SPI_C1_SPE | 0x08); // CPOL
    Output output;
    output.select();
    FL_CHECK_EQ(FakeRegs::C1.value, static_cast<fl::u8>(SPI_C1_MSTR | SPI_C1_SPE));
    output.release();
}

FL_TEST_CASE("release does not cut off the last byte") {
    resetBus(kSre | kMuxGpio);
    Output output;
    output.select();
    Output::writeByte(0xFF);
    Output::writeByte(0x81);
    output.release();
    FL_REQUIRE_EQ(gWire.size(), 16U);
    expectByte(0, 0xFF);
    expectByte(8, 0x81);
    checkClean();
    FL_CHECK_EQ(gDataCfg.value, kSre | kMuxGpio);
}

FL_TEST_CASE("the real SM16716 controller: start bits, data, and the 50-bit header") {
    resetBus(kSre | kMuxGpio);
    Controller controller;
    controller.init();
    CRGB leds[3] = {CRGB(0x81, 0x42, 0x24), CRGB(0xFF, 0x00, 0x18), CRGB(0x00, 0xFF, 0x7E)};
    PixelController<RGB> pixels(leds, 3, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.showPixels(pixels);

    FL_REQUIRE_EQ(gWire.size(), 3U * 25U + 50U);
    for (int p = 0; p < 3; ++p) {
        const fl::size at = static_cast<fl::size>(p) * 25;
        FL_CHECK_EQ(gWire[at], 1); // The start bit.
        expectByte(at + 1, leds[p].r);
        expectByte(at + 9, leds[p].g);
        expectByte(at + 17, leds[p].b);
    }
    for (fl::size i = 75; i < gWire.size(); ++i) {
        FL_CHECK_EQ(gWire[i], 0);
    }
    checkClean();
}

} // FL_TEST_FILE

} // namespace fl
