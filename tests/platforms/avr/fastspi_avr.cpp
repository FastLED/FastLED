/// @file fastspi_avr.cpp
/// @brief Runs the real AVR native SPI writeBit methods against fake registers.
///
/// fastspi_avr.h is included once per register guard, so all five independent
/// implementations are the production code rather than copies of it.

#include "test.h"

#include "fastspi_types.h"
#include "pixel_controller.h"
#include "platforms/shared/spi_pixel_writer.h"
#include "fl/stl/int.h"

namespace fl {

constexpr u8 kAvrTestDataPin = 1;
constexpr u8 kAvrTestClockPin = 2;

namespace avr_regs {

// One peripheral's in-flight byte; it completes only after being polled.
struct Shifter {
    bool busy = false;
    int polls = 0;
    int bytes = 0;
};

// A control register whose mask bits give the pins to the peripheral.
struct ControlReg {
    u8 value = 0;
    u8 enableMask = 0;
    const volatile u16* rate = nullptr;
    Shifter* shifter = nullptr; // Disabling this peripheral aborts its byte.
    static int badEnables;
    static int abortedBytes;

    ControlReg() = default;
    explicit ControlReg(const volatile u16* rateRegister) : rate(rateRegister) {}

    operator u8() const { return value; }
    ControlReg& operator=(u8 next) {
        if (shifter != nullptr && shifter->busy && (value & enableMask) && !(next & enableMask)) {
            ++abortedBytes;
            shifter->busy = false; // The byte is lost, and no completion flag follows.
        }
        const bool enabling = !(value & enableMask) && (next & enableMask);
        if (enabling && rate != nullptr && *rate != 0) {
            ++badEnables; // MSPIM must see UBRR == 0 when enabled.
        }
        value = next;
        return *this;
    }
    ControlReg& operator|=(u8 bits) { return *this = value | bits; }
    ControlReg& operator&=(u8 bits) { return *this = value & bits; }
};
int ControlReg::badEnables = 0;
int ControlReg::abortedBytes = 0;

// A status register: `always` bits read set, `done` is set when the byte ends.
struct StatusReg {
    Shifter* shifter = nullptr;
    u8 doneMask = 0;
    u8 always = 0;
    bool done = false;
    int idlePolls = 0;
    static int hangs;

    operator u8() {
        if (shifter != nullptr && shifter->busy && ++shifter->polls >= 2) {
            shifter->busy = false;
            done = true;
        }
        // Spinning on a flag that will never set is a hang; break it and report.
        if (!done && (shifter == nullptr || !shifter->busy) && ++idlePolls > 64) {
            ++hangs;
            done = true;
        }
        return static_cast<u8>(always | (done ? doneMask : 0));
    }
    StatusReg& operator=(u8 bits) {
        if (bits & doneMask) {
            done = false; // Write-one-to-clear.
            idlePolls = 0;
        }
        return *this;
    }
    StatusReg& operator|=(u8 bits) { return *this = bits; }
    StatusReg& operator&=(u8) { return *this; }
};

int StatusReg::hangs = 0;

struct DataReg {
    Shifter* shifter = nullptr;
    StatusReg* status = nullptr;
    bool clearsDone = false;

    operator u8() const { return 0; }
    DataReg& operator=(u8) {
        shifter->busy = true;
        shifter->polls = 0;
        ++shifter->bytes;
        status->idlePolls = 0;
        if (clearsDone) {
            status->done = false;
        }
        return *this;
    }
};

Shifter usart0, usart1, modern, classic;
volatile u16 ubrr0 = 0, ubrr1 = 0;
ControlReg ucsr0b(&ubrr0), ucsr1b(&ubrr1);
ControlReg spi0Ctrla, spcr;
volatile u8 sreg = 0x80;
volatile u8 ucsr0c = 0, ucsr1c = 0, spi0Ctrlb = 0;
StatusReg ucsr0a, ucsr1a, spi0Intflags, spsr;
DataReg udr0, udr1, spi0Data, spdr;

} // namespace avr_regs

// Observes pins as a device would: a write only reaches the pin when GPIO owns it.
struct AvrPinTrace {
    static const avr_regs::ControlReg* owner;
    static const avr_regs::Shifter* shifter;
    static const volatile u8* modeReg;
    static u8 modeMask;
    static bool clockLatch;
    static bool data;
    static bool clock;
    static bool sampled;
    static int risingEdges;
    static int fallingEdges;
    static int unrelatedWrites;
    static int ignoredWrites;
    static int writesWhileBusy;

    static void reset() {
        data = false;
        clock = false;
        sampled = false;
        risingEdges = 0;
        fallingEdges = 0;
        unrelatedWrites = 0;
        ignoredWrites = 0;
        writesWhileBusy = 0;
        clockLatch = false;
    }
    static bool gpioOwnsPins() {
        if (shifter != nullptr && shifter->busy) {
            ++writesWhileBusy;
        }
        const bool enabled = owner != nullptr && (owner->value & owner->enableMask);
        // MSPIM keeps XCK until UMSEL clears, even with RXEN and TXEN off.
        const bool mspim = modeReg != nullptr && (*modeReg & modeMask);
        if (enabled || mspim) {
            return false;
        }
        // Released with a high clock latch: the pin rises now, sampling old data.
        if (clockLatch && !clock) {
            clock = true;
            sampled = data;
            ++risingEdges;
        }
        return true;
    }
};
const avr_regs::ControlReg* AvrPinTrace::owner = nullptr;
const avr_regs::Shifter* AvrPinTrace::shifter = nullptr;
const volatile u8* AvrPinTrace::modeReg = nullptr;
u8 AvrPinTrace::modeMask = 0;
bool AvrPinTrace::clockLatch = false;
bool AvrPinTrace::data = false;
bool AvrPinTrace::clock = false;
bool AvrPinTrace::sampled = false;
int AvrPinTrace::risingEdges = 0;
int AvrPinTrace::fallingEdges = 0;
int AvrPinTrace::unrelatedWrites = 0;
int AvrPinTrace::ignoredWrites = 0;
int AvrPinTrace::writesWhileBusy = 0;

template <u8 PIN> struct AvrRecordingPin {
    static void setOutput() {}
    static void hi() {
        if (PIN != kAvrTestDataPin && PIN != kAvrTestClockPin) {
            ++AvrPinTrace::unrelatedWrites;
        } else if (!AvrPinTrace::gpioOwnsPins()) {
            ++AvrPinTrace::ignoredWrites;
            AvrPinTrace::clockLatch = PIN == kAvrTestClockPin ? true : AvrPinTrace::clockLatch;
        } else if (PIN == kAvrTestDataPin) {
            AvrPinTrace::data = true;
        } else if (!AvrPinTrace::clock) {
            AvrPinTrace::clock = true;
            AvrPinTrace::clockLatch = true;
            AvrPinTrace::sampled = AvrPinTrace::data;
            ++AvrPinTrace::risingEdges;
        }
    }
    static void lo() {
        if (PIN != kAvrTestDataPin && PIN != kAvrTestClockPin) {
            ++AvrPinTrace::unrelatedWrites;
        } else if (PIN == kAvrTestClockPin) {
            // A low latch is always safe to preload, whoever owns the pin.
            AvrPinTrace::clockLatch = false;
            if (AvrPinTrace::gpioOwnsPins() && AvrPinTrace::clock) {
                AvrPinTrace::clock = false;
                ++AvrPinTrace::fallingEdges;
            }
        } else if (!AvrPinTrace::gpioOwnsPins()) {
            ++AvrPinTrace::ignoredWrites;
        } else if (PIN == kAvrTestDataPin) {
            AvrPinTrace::data = false;
        }
    }
};

} // namespace fl

#define AVR_HARDWARE_SPI
#define SREG avr_regs::sreg
#define cli() (avr_regs::sreg = 0)
#define FastPin AvrRecordingPin

// USART bit positions (values only need to be valid shift counts here).
#define UMSEL00 7
#define UMSEL01 6
#define UMSEL10 7
#define UMSEL11 6
#define UCPHA0 1
#define UCPHA1 1
#define UCPOL0 0
#define UCPOL1 0
#define RXEN0 4
#define RXEN1 4
#define TXEN0 3
#define TXEN1 3
#define UDRE0 5
#define UDRE1 5
#define TXC0 6
#define TXC1 6
#define U2X0 1
#define U2X1 1
#define MPCM0 0
#define MPCM1 0

// SPI0_CTRLA masks.
#define SPI_ENABLE_bm 0x01
#define SPI_MASTER_bm 0x20
#define SPI_CLK2X_bm 0x10
#define SPI_CLK2X_bp 4
#define SPI_PRESC0_bm 0x02
#define SPI_PRESC0_bp 1
#define SPI_PRESC1_bm 0x04
#define SPI_PRESC1_bp 2
#define SPI_SSD_bm 0x04
#define SPI_IF_bm 0x80

// Classic SPCR/SPSR bit positions.
#define SPE 6
#define MSTR 4
#define SPIF 7
#define SPI2X 0
#define SPR0 0
#define SPR1 1
#define SPR10 1

// First pass: both USART classes and the modern SPI0_CTRLA class.
#define UBRR0 avr_regs::ubrr0
#define UCSR0A avr_regs::ucsr0a
#define UCSR0B avr_regs::ucsr0b
#define UCSR0C avr_regs::ucsr0c
#define UDR0 avr_regs::udr0
#define UBRR1 avr_regs::ubrr1
#define UCSR1A avr_regs::ucsr1a
#define UCSR1B avr_regs::ucsr1b
#define UCSR1C avr_regs::ucsr1c
#define UDR1 avr_regs::udr1
#define SPI0_CTRLA avr_regs::spi0Ctrla
#define SPI0_CTRLB avr_regs::spi0Ctrlb
#define SPI0_DATA avr_regs::spi0Data
#define SPI0_INTFLAGS avr_regs::spi0Intflags
#define AVRHardwareSPIOutput AvrModernSpiOutput
#include "platforms/avr/fastspi_avr.h" // allow-include-after-namespace
#undef AVRHardwareSPIOutput
#undef UBRR0
#undef UBRR1
#undef SPI0_CTRLA
#undef __INC_FASTSPI_AVR_H

// Second pass: classic SPSR class.
#define SPCR avr_regs::spcr
#define SPSR avr_regs::spsr
#define SPDR avr_regs::spdr
#define AVRHardwareSPIOutput AvrClassicSpiOutput
#include "platforms/avr/fastspi_avr.h" // allow-include-after-namespace
#undef AVRHardwareSPIOutput
#undef SPCR
#undef SPSR
#undef SPDR
#undef __INC_FASTSPI_AVR_H

// Third pass: classic SPSR0 class (same register shape, different guard).
#define SPCR0 avr_regs::spcr
#define SPSR0 avr_regs::spsr
#define SPDR0 avr_regs::spdr
#define AVRHardwareSPIOutput AvrClassicSpi0Output
#include "platforms/avr/fastspi_avr.h" // allow-include-after-namespace
#undef AVRHardwareSPIOutput
#undef FastPin

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

template <typename Output, u8 BIT> void checkBit() {
    for (int value = 0; value < 256; ++value) {
        const u8 byte = static_cast<u8>(value);
        AvrPinTrace::reset();
        Output::template writeBit<BIT>(byte);

        const bool expected = (byte & (1U << BIT)) != 0;
        FL_REQUIRE_EQ(AvrPinTrace::sampled, expected);
        FL_REQUIRE_EQ(AvrPinTrace::risingEdges, 1);
        FL_REQUIRE_EQ(AvrPinTrace::fallingEdges, 1);
        FL_REQUIRE_FALSE(AvrPinTrace::clock);
        FL_REQUIRE_EQ(AvrPinTrace::unrelatedWrites, 0);
        FL_REQUIRE_EQ(AvrPinTrace::ignoredWrites, 0);
    }
}

template <typename Output> void checkAllBits() {
    checkBit<Output, 0>();
    checkBit<Output, 1>();
    checkBit<Output, 2>();
    checkBit<Output, 3>();
    checkBit<Output, 4>();
    checkBit<Output, 5>();
    checkBit<Output, 6>();
    checkBit<Output, 7>();
}

// A chip-select that records whether a byte was still shifting when the
// output deselected it, or when select() was about to reprogram the rate (#5133).
struct BusyProbeSelect : Selectable {
    const avr_regs::Shifter* shifter = nullptr;
    int busyAtSelect = 0;
    int busyAtRelease = 0;
    void select() FL_NO_EXCEPT override { busyAtSelect += shifter->busy ? 1 : 0; }
    void release() FL_NO_EXCEPT override { busyAtRelease += shifter->busy ? 1 : 0; }
    bool isSelected() FL_NO_EXCEPT override { return false; }
};

// Byte, bit, byte: the shape of each SM16716 pixel, repeated across pixels.
template <typename Output> void checkHandoff(avr_regs::Shifter& shifter, avr_regs::ControlReg& control,
                                             u8 initialControl) {
    control.value = initialControl;
    avr_regs::ControlReg::badEnables = 0;
    avr_regs::ControlReg::abortedBytes = 0;
    avr_regs::StatusReg::hangs = 0;
    control.shifter = &shifter;
    shifter = avr_regs::Shifter();
    AvrPinTrace::owner = &control;
    AvrPinTrace::shifter = &shifter;

    // Idle: no byte was ever written, so a completion flag must not be awaited.
    AvrPinTrace::reset();
    Output::template writeBit<0>(1);
    FL_REQUIRE_EQ(AvrPinTrace::risingEdges, 1);
    FL_REQUIRE_EQ(control.value, initialControl);
    if (!(initialControl & control.enableMask)) {
        // A disabled peripheral cannot transmit, so only the idle path applies.
        AvrPinTrace::owner = nullptr;
        AvrPinTrace::shifter = nullptr;
        return;
    }

    for (int pixel = 0; pixel < 3; ++pixel) {
        AvrPinTrace::reset();
        const int bytesBefore = shifter.bytes;
        Output::writeByte(0xA5);
        Output::writeByte(0x5A);
        Output::writeByte(0xFF);
        Output::template writeBit<0>(1);
        FL_REQUIRE_EQ(AvrPinTrace::writesWhileBusy, 0);
        FL_REQUIRE_EQ(AvrPinTrace::ignoredWrites, 0);
        FL_REQUIRE(AvrPinTrace::sampled);
        FL_REQUIRE_EQ(AvrPinTrace::risingEdges, 1);
        FL_REQUIRE_EQ(AvrPinTrace::fallingEdges, 1);
        FL_REQUIRE_EQ(control.value, initialControl);
        FL_REQUIRE_EQ(shifter.bytes, bytesBefore + 3);
    }
    FL_REQUIRE_EQ(avr_regs::ControlReg::badEnables, 0);
    FL_REQUIRE_EQ(avr_regs::sreg, 0x80); // Interrupt state restored.

    // A post-wait byte is still shifting; waiting on it must not lose the drain.
    AvrPinTrace::reset();
    Output::writeBytePostWait(0x11);
    Output::template writeBit<0>(0);
    FL_REQUIRE_EQ(AvrPinTrace::writesWhileBusy, 0);
    FL_REQUIRE_FALSE(AvrPinTrace::sampled);

    // writeBit restored the rate; select() below legitimately reprograms it.
    if (control.rate != nullptr) {
        FL_REQUIRE_EQ(*control.rate, 7);
    }

    // Pixel then header: release() between them must not cut off the last byte.
    Output output;
    AvrPinTrace::reset();
    output.select();
    Output::writeByte(0x22);
    output.release();
    FL_REQUIRE_EQ(avr_regs::ControlReg::abortedBytes, 0);
    output.select();
    Output::template writeBit<0>(0);
    FL_REQUIRE_EQ(AvrPinTrace::risingEdges, 1);
    FL_REQUIRE_EQ(avr_regs::StatusReg::hangs, 0);
    control.value = initialControl;

    // A device behind a Selectable must see its last byte finish before it is
    // deselected, and select() must not change the rate under a shifting byte.
    {
        BusyProbeSelect cs;
        cs.shifter = &shifter;
        Output selected(&cs);
        selected.select();
        Output::writeByte(0x33);
        selected.release();
        FL_REQUIRE_EQ(cs.busyAtRelease, 0);
        Output::writeByte(0x44);
        selected.select();
        FL_REQUIRE_EQ(cs.busyAtSelect, 0);
        selected.release();
        FL_REQUIRE_EQ(avr_regs::StatusReg::hangs, 0);
        control.value = initialControl;
    }

    // The real SM16716 pixel path, then its header with no drain in between, as in
    // showPixels. The header sequence is replayed: SM16716Controller needs fl::SPIOutput.
    {
        CRGB leds[3] = {CRGB(1, 2, 3), CRGB(4, 5, 6), CRGB(7, 8, 9)};
        PixelController<RGB> pixels(leds, 3, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        AvrPinTrace::reset();
        const int bytesBefore = shifter.bytes;
        writePixelsToSPI<FLAG_START_BIT, DATA_NOP, RGB>(pixels, output, nullptr);
        FL_REQUIRE_EQ(AvrPinTrace::risingEdges, 3);
        FL_REQUIRE(AvrPinTrace::sampled); // The start bit is a 1.

        output.select();
        Output::template writeBit<0>(0);
        FL_REQUIRE_FALSE(AvrPinTrace::sampled);
        Output::writeByte(0);
        Output::writeByte(0);
        Output::writeByte(0);
        Output::template writeBit<0>(0);
        Output::writeByte(0);
        Output::writeByte(0);
        Output::writeByte(0);
        output.endTransaction();
        Output::waitFully();

        FL_REQUIRE_EQ(AvrPinTrace::risingEdges, 5);
        FL_REQUIRE_EQ(AvrPinTrace::fallingEdges, 5);
        FL_REQUIRE_EQ(AvrPinTrace::writesWhileBusy, 0);
        FL_REQUIRE_EQ(AvrPinTrace::ignoredWrites, 0);
        FL_REQUIRE_EQ(shifter.bytes, bytesBefore + 15);
        FL_REQUIRE_EQ(avr_regs::ControlReg::abortedBytes, 0);
        FL_REQUIRE_EQ(avr_regs::ControlReg::badEnables, 0);
        FL_REQUIRE_EQ(avr_regs::StatusReg::hangs, 0);
    }
    control.value = initialControl;

    // A high clock latch must not rise at release and sample the old data.
    AvrPinTrace::reset();
    AvrPinTrace::clockLatch = true;
    Output::template writeBit<0>(1);
    FL_REQUIRE_EQ(AvrPinTrace::risingEdges, 1);
    FL_REQUIRE(AvrPinTrace::sampled);
    AvrPinTrace::owner = nullptr;
    AvrPinTrace::shifter = nullptr;
}

constexpr u8 kUnrelatedControlBit = 0x08;

} // namespace

FL_TEST_CASE("AVR USART1 writeBit") {
    using Output = AVRUSART1SPIOutput<kAvrTestDataPin, kAvrTestClockPin, 2>;
    using namespace avr_regs;
    ucsr1a.shifter = &usart1;
    ucsr1a.doneMask = 1 << TXC1;
    ucsr1a.always = 1 << UDRE1; // UDRE alone must never pass the drain.
    udr1.shifter = &usart1;
    udr1.status = &ucsr1a;
    ucsr1b.enableMask = (1 << RXEN1) | (1 << TXEN1);
    ubrr1 = 7;
    ucsr1b.value = ucsr1b.enableMask;
    const u8 mspim = (1 << UMSEL11) | (1 << UMSEL10);
    ucsr1c = mspim;
    AvrPinTrace::modeReg = &ucsr1c;
    AvrPinTrace::modeMask = mspim;
    AvrPinTrace::owner = &ucsr1b;
    checkAllBits<Output>();
    checkHandoff<Output>(usart1, ucsr1b, ucsr1b.enableMask);
    checkHandoff<Output>(usart1, ucsr1b, 0);
    FL_CHECK_EQ(ucsr1c, mspim);
    AvrPinTrace::modeReg = nullptr;
}

FL_TEST_CASE("AVR USART0 writeBit") {
    using Output = AVRUSART0SPIOutput<kAvrTestDataPin, kAvrTestClockPin, 2>;
    using namespace avr_regs;
    ucsr0a.shifter = &usart0;
    ucsr0a.doneMask = 1 << TXC0;
    ucsr0a.always = 1 << UDRE0;
    udr0.shifter = &usart0;
    udr0.status = &ucsr0a;
    ucsr0b.enableMask = (1 << RXEN0) | (1 << TXEN0);
    ubrr0 = 7;
    ucsr0b.value = ucsr0b.enableMask;
    const u8 mspim = (1 << UMSEL01) | (1 << UMSEL00);
    ucsr0c = mspim;
    AvrPinTrace::modeReg = &ucsr0c;
    AvrPinTrace::modeMask = mspim;
    AvrPinTrace::owner = &ucsr0b;
    checkAllBits<Output>();
    checkHandoff<Output>(usart0, ucsr0b, ucsr0b.enableMask);
    checkHandoff<Output>(usart0, ucsr0b, 0);
    FL_CHECK_EQ(ucsr0c, mspim);
    AvrPinTrace::modeReg = nullptr;
}

FL_TEST_CASE("AVR SPI0_CTRLA writeBit") {
    using Output = AvrModernSpiOutput<kAvrTestDataPin, kAvrTestClockPin, 2>;
    using namespace avr_regs;
    spi0Intflags.shifter = &modern;
    spi0Intflags.doneMask = SPI_IF_bm;
    spi0Data.shifter = &modern;
    spi0Data.status = &spi0Intflags;
    spi0Data.clearsDone = true;
    spi0Ctrla.enableMask = SPI_ENABLE_bm;
    spi0Ctrla.value = SPI_ENABLE_bm;
    AvrPinTrace::owner = &spi0Ctrla;
    checkAllBits<Output>();
    checkHandoff<Output>(modern, spi0Ctrla, SPI_ENABLE_bm | kUnrelatedControlBit);
    checkHandoff<Output>(modern, spi0Ctrla, kUnrelatedControlBit);
}

template <typename Output> void checkClassic() {
    using namespace avr_regs;
    spsr.shifter = &classic;
    spsr.doneMask = 1 << SPIF;
    spdr.shifter = &classic;
    spdr.status = &spsr;
    spdr.clearsDone = true;
    spcr.enableMask = 1 << SPE;
    spcr.value = 1 << SPE;
    AvrPinTrace::owner = &spcr;
    checkAllBits<Output>();
    checkHandoff<Output>(classic, spcr, (1 << SPE) | kUnrelatedControlBit);
    checkHandoff<Output>(classic, spcr, kUnrelatedControlBit);
}

FL_TEST_CASE("AVR classic SPSR writeBit") {
    checkClassic<AvrClassicSpiOutput<kAvrTestDataPin, kAvrTestClockPin, 2>>();
}

FL_TEST_CASE("AVR classic SPSR0 writeBit") {
    checkClassic<AvrClassicSpi0Output<kAvrTestDataPin, kAvrTestClockPin, 2>>();
}

} // FL_TEST_FILE
