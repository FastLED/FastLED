/// @file avr_pin_legacy.cpp
/// @brief With FL_AVR_ENABLE_FAST_SOFTWARE_SPI at its default (0), the generic
/// software SPI writer keeps the legacy three-store-per-bit whole-port path.

#include "test.h"

#include "platforms/avr/atmega/common/avr_pin.h"

#include "fastspi_types.h"
#include "fl/stl/int.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

FL_STATIC_ASSERT(FL_AVR_ENABLE_FAST_SOFTWARE_SPI == 0,
                 "the untested AVR fast software SPI path must default off");

namespace fl {
namespace avr_pin_legacy_test {

constexpr u8 kData = 0x08;
constexpr u8 kClock = 0x20;

fl::vector<u8> gStores;

struct Latch {
    u8 value = 0;
    operator u8() const { return value; }
    Latch& operator=(u8 next) {
        value = next;
        gStores.push_back(next);
        return *this;
    }
    Latch& operator|=(u8 bits) { return *this = value | bits; }
    Latch& operator&=(u8 bits) { return *this = value & bits; }
    volatile u8* operator&() { return reinterpret_cast<volatile u8*>(&value); } // ok reinterpret cast
};

struct Port {
    static Latch& r() {
        static Latch latch;
        return latch;
    }
};
struct Unused {
    static u8& r() {
        static u8 value = 0;
        return value;
    }
};

constexpr u8 kDataPin = 240;
constexpr u8 kClockPin = 241;

} // namespace avr_pin_legacy_test

template <> class FastPin<avr_pin_legacy_test::kDataPin>
    : public _AVRPIN<avr_pin_legacy_test::kDataPin, avr_pin_legacy_test::kData,
                     avr_pin_legacy_test::Port, avr_pin_legacy_test::Unused,
                     avr_pin_legacy_test::Unused> {};
template <> class FastPin<avr_pin_legacy_test::kClockPin>
    : public _AVRPIN<avr_pin_legacy_test::kClockPin, avr_pin_legacy_test::kClock,
                     avr_pin_legacy_test::Port, avr_pin_legacy_test::Unused,
                     avr_pin_legacy_test::Unused> {};

FL_STATIC_ASSERT(!BitbangPin<FastPin<avr_pin_legacy_test::kDataPin>>::kTwoWriteStream,
                 "default-off AVR pins must not stream");

FL_TEST_FILE(FL_FILEPATH) {
using namespace avr_pin_legacy_test;

FL_TEST_CASE("AVR default software SPI keeps the legacy three stores per bit") {
    Port::r().value = 0;
    gStores.clear();
    GenericSoftwareSPIOutput<kDataPin, kClockPin, 4> spi;
    u8 input[] = {0xA5};
    spi.writeBytes<DATA_NOP>(input, 1);
    FL_REQUIRE_EQ(gStores.size(), size_t(24));
    u8 seen = 0;
    bool clock = false;
    for (u8 v : gStores) {
        const bool next = (v & kClock) != 0;
        if (next && !clock) seen = static_cast<u8>((seen << 1) | ((v & kData) ? 1 : 0));
        clock = next;
    }
    FL_CHECK_EQ(seen, u8(0xA5));
    FL_CHECK_EQ(u8(Port::r().value & kClock), u8(0));
}

} // FL_TEST_FILE

} // namespace fl
