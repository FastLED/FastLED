/// @file fastspi_avr.cpp
/// @brief Runs the real AVR native SPI writeBit methods against fake registers.
///
/// fastspi_avr.h is included once per register guard, so all five independent
/// implementations are the production code rather than copies of it.

#include "test.h"

#include "fastspi_types.h"
#include "fl/stl/int.h"

namespace fl {

constexpr u8 kAvrTestDataPin = 1;
constexpr u8 kAvrTestClockPin = 2;

namespace avr_regs {

// A control register whose mask bits give the pins to the peripheral.
struct ControlReg {
    u8 value = 0;
    u8 enableMask = 0;
    const volatile u16* rate = nullptr;
    static int badEnables;

    ControlReg() = default;
    explicit ControlReg(const volatile u16* rateRegister) : rate(rateRegister) {}

    operator u8() const { return value; }
    ControlReg& operator=(u8 next) {
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

// One peripheral's in-flight byte; it completes only after being polled.
struct Shifter {
    bool busy = false;
    int polls = 0;
    int bytes = 0;
};

// A status register: `always` bits read set, `done` is set when the byte ends.
struct StatusReg {
    Shifter* shifter = nullptr;
    u8 doneMask = 0;
    u8 always = 0;
    bool done = false;

    operator u8() {
        if (shifter != nullptr && shifter->busy && ++shifter->polls >= 2) {
            shifter->busy = false;
            done = true;
        }
        return static_cast<u8>(always | (done ? doneMask : 0));
    }
    StatusReg& operator=(u8 bits) {
        if (bits & doneMask) {
            done = false; // Write-one-to-clear.
        }
        return *this;
    }
    StatusReg& operator|=(u8 bits) { return *this = bits; }
    StatusReg& operator&=(u8) { return *this; }
};

struct DataReg {
    Shifter* shifter = nullptr;
    StatusReg* status = nullptr;
    bool clearsDone = false;

    operator u8() const { return 0; }
    DataReg& operator=(u8) {
        shifter->busy = true;
        shifter->polls = 0;
        ++shifter->bytes;
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
    }
    static bool gpioOwnsPins() {
        if (shifter != nullptr && shifter->busy) {
            ++writesWhileBusy;
        }
        if (owner != nullptr && (owner->value & owner->enableMask)) {
            ++ignoredWrites;
            return false;
        }
        return true;
    }
};
const avr_regs::ControlReg* AvrPinTrace::owner = nullptr;
const avr_regs::Shifter* AvrPinTrace::shifter = nullptr;
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
            return;
        } else if (PIN == kAvrTestDataPin) {
            AvrPinTrace::data = true;
        } else if (!AvrPinTrace::clock) {
            AvrPinTrace::clock = true;
            AvrPinTrace::sampled = AvrPinTrace::data;
            ++AvrPinTrace::risingEdges;
        }
    }
    static void lo() {
        if (PIN != kAvrTestDataPin && PIN != kAvrTestClockPin) {
            ++AvrPinTrace::unrelatedWrites;
        } else if (!AvrPinTrace::gpioOwnsPins()) {
            return;
        } else if (PIN == kAvrTestDataPin) {
            AvrPinTrace::data = false;
        } else if (AvrPinTrace::clock) {
            AvrPinTrace::clock = false;
            ++AvrPinTrace::fallingEdges;
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

// Byte, bit, byte: the shape of each SM16716 pixel, repeated across pixels.
template <typename Output> void checkHandoff(avr_regs::Shifter& shifter, avr_regs::ControlReg& control,
                                             u8 initialControl) {
    control.value = initialControl;
    avr_regs::ControlReg::badEnables = 0;
    shifter = avr_regs::Shifter();
    AvrPinTrace::owner = &control;
    AvrPinTrace::shifter = &shifter;

    // Idle: no byte was ever written, so a completion flag must not be awaited.
    AvrPinTrace::reset();
    Output::template writeBit<0>(1);
    FL_REQUIRE_EQ(AvrPinTrace::risingEdges, 1);
    FL_REQUIRE_EQ(control.value, initialControl);

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
    AvrPinTrace::owner = &ucsr1b;
    checkAllBits<Output>();
    checkHandoff<Output>(usart1, ucsr1b, ucsr1b.enableMask);
    checkHandoff<Output>(usart1, ucsr1b, 0);
    FL_CHECK_EQ(ubrr1, 7);
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
    AvrPinTrace::owner = &ucsr0b;
    checkAllBits<Output>();
    checkHandoff<Output>(usart0, ucsr0b, ucsr0b.enableMask);
    checkHandoff<Output>(usart0, ucsr0b, 0);
    FL_CHECK_EQ(ubrr0, 7);
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
