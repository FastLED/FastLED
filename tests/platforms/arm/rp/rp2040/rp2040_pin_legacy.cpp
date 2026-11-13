/// @file rp2040_pin_legacy.cpp
/// @brief With FL_RP_ENABLE_FAST_SOFTWARE_SPI at its default (0), real RP2040
/// pins keep the default bit-bang policy and the generic software SPI writer
/// keeps the legacy three-store-per-bit whole-port path.

#include "test.h"

#include "fl/stl/int.h"

typedef unsigned int uint;
struct FakeSio { volatile fl::u32 gpio_out, gpio_set, gpio_clr, gpio_togl, gpio_oe_set, gpio_oe_clr; };
static FakeSio gFakeSio;
#define sio_hw (&gFakeSio)
#define GPIO_FUNC_SIO 5
inline void gpio_set_function(uint, int) {}

#include "platforms/arm/rp/rp2040/rp2040_pin.h"

#include "fastspi_types.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

FL_STATIC_ASSERT(FL_RP_ENABLE_FAST_SOFTWARE_SPI == 0,
                 "the untested RP fast software SPI path must default off");

namespace fl {
namespace rp2040_pin_legacy_test {

constexpr u32 kData = 1U << 3;
constexpr u32 kClock = 1U << 17;
constexpr u8 kDataPin = 240;
constexpr u8 kClockPin = 241;

fl::vector<u32> gStores;

} // namespace rp2040_pin_legacy_test

// Real _RP2040PIN pins; only fastset is shadowed to record each port store.
template <> class FastPin<rp2040_pin_legacy_test::kDataPin>
    : public _RP2040PIN<rp2040_pin_legacy_test::kDataPin, rp2040_pin_legacy_test::kData> {
    using B = _RP2040PIN<rp2040_pin_legacy_test::kDataPin, rp2040_pin_legacy_test::kData>;
  public:
    static void fastset(B::port_ptr_t port, B::port_t value) {
        B::fastset(port, value);
        rp2040_pin_legacy_test::gStores.push_back(value);
    }
};
template <> class FastPin<rp2040_pin_legacy_test::kClockPin>
    : public _RP2040PIN<rp2040_pin_legacy_test::kClockPin, rp2040_pin_legacy_test::kClock> {};

FL_STATIC_ASSERT(!BitbangPin<FastPin<rp2040_pin_legacy_test::kDataPin>>::kAtomicWrite &&
                     !BitbangPin<FastPin<rp2040_pin_legacy_test::kDataPin>>::kTwoWriteStream &&
                     BitbangPin<FastPin<rp2040_pin_legacy_test::kDataPin>>::kMinWriteCycles == 0,
                 "default-off RP2040 pins must keep the default bit-bang policy");

FL_TEST_FILE(FL_FILEPATH) {
using namespace rp2040_pin_legacy_test;

FL_TEST_CASE("RP2040 default software SPI keeps the legacy three stores per bit") {
    gFakeSio.gpio_out = 0;
    gStores.clear();
    GenericSoftwareSPIOutput<kDataPin, kClockPin, 4> spi;
    u8 input[] = {0xA5};
    spi.writeBytes<DATA_NOP>(input, 1);
    FL_REQUIRE_EQ(gStores.size(), size_t(24));
    u8 seen = 0;
    bool clock = false;
    for (u32 v : gStores) {
        const bool next = (v & kClock) != 0;
        if (next && !clock) seen = static_cast<u8>((seen << 1) | ((v & kData) ? 1 : 0));
        clock = next;
    }
    FL_CHECK_EQ(seen, u8(0xA5));
    FL_CHECK_EQ(gFakeSio.gpio_out & kClock, u32(0));
}

} // FL_TEST_FILE

} // namespace fl
