/// @file generic_software_spi_esp32_legacy.cpp
/// @brief With FL_ESP32_ENABLE_FAST_SOFTWARE_SPI at 0, ESP32 writeBit() must
/// emit the FastLED 3.10.9 pin sequence. Records each pin operation and delay.

#include "test.h"

#include "fl/stl/int.h"
#include "fl/stl/string.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace spi_esp32_legacy_test {

fl::string gOps;

template <char NAME> struct RecordingPin {
    typedef volatile u32* port_ptr_t;
    typedef u32 port_t;
    static constexpr bool validpin() { return true; }
    static void setOutput() {}
    static void hi() { gOps += NAME; gOps += '+'; }
    static void lo() { gOps += NAME; gOps += '-'; }
    static void toggle() { gOps += NAME; gOps += '~'; }
    static port_ptr_t port() { return nullptr; }
    static port_t mask() { return 0; }
};

} // namespace spi_esp32_legacy_test

template <> class FastPin<250> : public spi_esp32_legacy_test::RecordingPin<'D'> {};
template <> class FastPin<251> : public spi_esp32_legacy_test::RecordingPin<'C'> {};
template <> class FastPin<252> : public spi_esp32_legacy_test::RecordingPin<'C'> {};

namespace spi_esp32_legacy_test {
// Default policy, except delays are recorded as "[cycles]".
template <class Pin> struct RecordingDelays : BitbangPinDefaults<Pin> {
    template <fl::cycle_t CYCLES> static void delay() {
        gOps += '[';
        gOps += fl::to_string(static_cast<long>(CYCLES));
        gOps += ']';
    }
};
} // namespace spi_esp32_legacy_test

template <> struct BitbangPin<FastPin<251>> : spi_esp32_legacy_test::RecordingDelays<FastPin<251>> {};
// The policy fastpin_esp32.h gives every pin when FL_ESP32_ENABLE_FAST_SOFTWARE_SPI is 0.
template <> struct BitbangPin<FastPin<252>> : spi_esp32_legacy_test::RecordingDelays<FastPin<252>> {
    static constexpr bool kEsp32LegacyWriteBit = true;
};

FL_TEST_FILE(FL_FILEPATH) {
using namespace spi_esp32_legacy_test;

namespace {
constexpr u32 kSpeed = 40;
using Output = GenericSoftwareSPIOutput<250, 251, kSpeed>;
using Esp32DefaultOutput = GenericSoftwareSPIOutput<250, 252, kSpeed>;

fl::string delayStr(long cycles) {
    fl::string s = "[";
    s += fl::to_string(cycles);
    s += "]";
    return s;
}
} // namespace

FL_TEST_CASE("ESP32 legacy writeBit keeps the 3.10.9 pin sequence") {
    // Same formulas as CLOCK_HI_DELAY / CLOCK_LO_DELAY in the writer.
    const long minDelay = (NS(35) > 3) ? (NS(35) - 3) : 1;
    const long half = (kSpeed - 6) / 2;
    const fl::string hiDelay = delayStr(minDelay) + delayStr(half > minDelay ? half - minDelay : 0);
    const fl::string loDelay = delayStr(half);

    // 3.10.9 `#ifdef FL_IS_ESP32` branch for a one bit: data high, clock low,
    // clock high, high delay, clock toggle, low delay.
    gOps.clear();
    Output::writeBitEsp32Legacy<0>(1);
    FL_CHECK_EQ(gOps, fl::string("D+C-C+") + hiDelay + "C~" + loDelay);

    // Zero bit: data low, clock high, high delay, toggle, then a second HIGH
    // delay (3.10.9 used CLOCK_HI_DELAY on both phases of a zero bit).
    gOps.clear();
    Output::writeBitEsp32Legacy<0>(0);
    FL_CHECK_EQ(gOps, fl::string("D-C+") + hiDelay + "C~" + hiDelay);

    // writeBit() takes the policy writer unless the clock pin's policy asks
    // for the legacy ESP32 sequence.
    gOps.clear();
    Output::writeBit<0>(1);
    FL_CHECK_EQ(gOps, fl::string("D+C+") + hiDelay + "C-" + loDelay);

    gOps.clear();
    Esp32DefaultOutput::writeBit<0>(1);
    FL_CHECK_EQ(gOps, fl::string("D+C-C+") + hiDelay + "C~" + loDelay);
}

FL_TEST_CASE("ESP32 default policy sends whole bytes through the 3.10.9 writeBit") {
    const long minDelay = (NS(35) > 3) ? (NS(35) - 3) : 1;
    const long half = (kSpeed - 6) / 2;
    const fl::string hiDelay = delayStr(minDelay) + delayStr(half > minDelay ? half - minDelay : 0);
    const fl::string loDelay = delayStr(half);
    const fl::string one = fl::string("D+C-C+") + hiDelay + "C~" + loDelay;
    const fl::string zero = fl::string("D-C+") + hiDelay + "C~" + hiDelay;

    gOps.clear();
    Esp32DefaultOutput::writeByte(0xA5); // 1010 0101, MSB first
    FL_CHECK_EQ(gOps, one + zero + one + zero + zero + one + zero + one);
}

} // FL_TEST_FILE

} // namespace fl
