/// @file fastspi_avr.cpp
/// @brief Runs the real AVR native SPI writeBit methods against fake registers.
///
/// fastspi_avr.h is included once per register guard, so all five independent
/// implementations are the production code rather than copies of it.

#include "test.h"

#include "fastspi_types.h"
#include "fl/stl/int.h"

namespace fl {

// Records the data level sampled on each rising clock edge.
struct AvrPinTrace {
    static bool data;
    static bool clock;
    static bool sampled;
    static int risingEdges;
    static int fallingEdges;
    static int unrelatedWrites;

    static void reset() {
        data = false;
        clock = false;
        sampled = false;
        risingEdges = 0;
        fallingEdges = 0;
        unrelatedWrites = 0;
    }
};
bool AvrPinTrace::data = false;
bool AvrPinTrace::clock = false;
bool AvrPinTrace::sampled = false;
int AvrPinTrace::risingEdges = 0;
int AvrPinTrace::fallingEdges = 0;
int AvrPinTrace::unrelatedWrites = 0;

constexpr u8 kAvrTestDataPin = 1;
constexpr u8 kAvrTestClockPin = 2;

template <u8 PIN> struct AvrRecordingPin {
    static void setOutput() {}
    static void hi() {
        if (PIN == kAvrTestDataPin) {
            AvrPinTrace::data = true;
        } else if (PIN != kAvrTestClockPin) {
            ++AvrPinTrace::unrelatedWrites;
        } else if (!AvrPinTrace::clock) {
            AvrPinTrace::clock = true;
            AvrPinTrace::sampled = AvrPinTrace::data;
            ++AvrPinTrace::risingEdges;
        }
    }
    static void lo() {
        if (PIN == kAvrTestDataPin) {
            AvrPinTrace::data = false;
        } else if (PIN != kAvrTestClockPin) {
            ++AvrPinTrace::unrelatedWrites;
        } else if (AvrPinTrace::clock) {
            AvrPinTrace::clock = false;
            ++AvrPinTrace::fallingEdges;
        }
    }
};

namespace avr_regs {
volatile u16 ubrr0, ubrr1;
volatile u8 ucsr0a, ucsr0b, ucsr0c, udr0;
volatile u8 ucsr1a, ucsr1b, ucsr1c, udr1;
volatile u8 spi0Ctrla, spi0Ctrlb, spi0Data, spi0Intflags;
volatile u8 spcr, spsr, spdr;
} // namespace avr_regs

} // namespace fl

#define AVR_HARDWARE_SPI
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

} // namespace

FL_TEST_CASE("AVR USART1 writeBit samples the selected bit") {
    checkAllBits<AVRUSART1SPIOutput<kAvrTestDataPin, kAvrTestClockPin, 2>>();
}

FL_TEST_CASE("AVR USART0 writeBit samples the selected bit") {
    checkAllBits<AVRUSART0SPIOutput<kAvrTestDataPin, kAvrTestClockPin, 2>>();
}

FL_TEST_CASE("AVR SPI0_CTRLA writeBit samples the selected bit") {
    checkAllBits<AvrModernSpiOutput<kAvrTestDataPin, kAvrTestClockPin, 2>>();
}

FL_TEST_CASE("AVR classic SPSR writeBit samples the selected bit") {
    checkAllBits<AvrClassicSpiOutput<kAvrTestDataPin, kAvrTestClockPin, 2>>();
    FL_CHECK((avr_regs::spcr & (1 << SPE)) != 0);
}

FL_TEST_CASE("AVR classic SPSR0 writeBit samples the selected bit") {
    checkAllBits<AvrClassicSpi0Output<kAvrTestDataPin, kAvrTestClockPin, 2>>();
    FL_CHECK((avr_regs::spcr & (1 << SPE)) != 0);
}

} // FL_TEST_FILE
