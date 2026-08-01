/// @file bus_traits.cpp
/// @brief Sanity tests for fl::Bus + BusTraits foundational types.

#include "FastLED.h"
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "platforms/is_platform.h"
#include "platforms/shared/bitbang/bus_traits.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_STATIC_ASSERT(static_cast<fl::u8>(fl::Bus::AUTO) == 0,
                 "Bus::AUTO must be the zero/default sentinel");
FL_STATIC_ASSERT(static_cast<fl::u8>(fl::Bus::FLEX_IO) == 8,
                 "Bus enum must contain exactly the portable values from #3439");
FL_STATIC_ASSERT(sizeof(fl::Bus) == 1,
                 "Bus underlying type should be u8 to keep ChannelOptions cheap");

FL_STATIC_ASSERT(fl::DefaultBus<fl::ClocklessChipset>::value == fl::Bus::BIT_BANG,
                 "Host default clockless bus should be BIT_BANG");
FL_STATIC_ASSERT(fl::DefaultBus<fl::SpiChipsetConfig>::value == fl::Bus::BIT_BANG,
                 "Host default SPI bus should be BIT_BANG");

FL_STATIC_ASSERT(fl::BusSupports<fl::Bus::BIT_BANG, fl::ClocklessChipset>::value,
                 "BIT_BANG must support clockless chipsets");
FL_STATIC_ASSERT(fl::BusSupports<fl::Bus::BIT_BANG, fl::SpiChipsetConfig>::value,
                 "BIT_BANG must support SPI chipsets");
FL_STATIC_ASSERT(!fl::BusSupports<fl::Bus::RMT, fl::SpiChipsetConfig>::value,
                 "Bus values without specializations must default to false_type");

FL_TEST_CASE("BusTraits<Bus::BIT_BANG>::instance returns a stable singleton") {
    auto& a = fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    auto& b = fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    FL_CHECK(&a == &b);
}

FL_TEST_CASE("BusTraits<Bus::BIT_BANG>::Driver implements IChannelDriver contract") {
    using Driver = fl::BusTraits<fl::Bus::BIT_BANG>::Driver;
    fl::IChannelDriver* iface = &fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    FL_REQUIRE(iface != nullptr);
    FL_CHECK(iface->getName() == fl::string::from_literal("BIT_BANG"));
    FL_CHECK(iface->getCapabilities().supportsClockless);
    FL_CHECK(iface->getCapabilities().supportsSpi);
    Driver* concrete = static_cast<Driver*>(iface);
    FL_CHECK(concrete != nullptr);
}

FL_TEST_CASE("BusTraits<Bus::BIT_BANG>::instancePtr returns a non-null shared_ptr") {
    auto p = fl::BusTraits<fl::Bus::BIT_BANG>::instancePtr();
    FL_REQUIRE(p != nullptr);
    FL_CHECK(p.get() == &fl::BusTraits<fl::Bus::BIT_BANG>::instance());
}

FL_TEST_CASE("enableDrivers<Bus::BIT_BANG>() registers the bit-bang driver") {
    auto& mgr = fl::ChannelManager::instance();
    auto before = mgr.getDriverCount();
    fl::enableDrivers<fl::Bus::BIT_BANG>();
    auto after = mgr.getDriverCount();
    FL_CHECK(after >= before);
    auto driver = mgr.getDriverByName(fl::string::from_literal("BIT_BANG"));
    FL_REQUIRE(driver != nullptr);
    FL_CHECK(driver.get() == &fl::BusTraits<fl::Bus::BIT_BANG>::instance());
}

FL_TEST_CASE("busKeepAlive<Bus::AUTO>() is a no-op and compiles") {
    fl::busKeepAlive<fl::Bus::AUTO>();
    auto& a = fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    auto& b = fl::BusTraits<fl::Bus::BIT_BANG>::instance();
    FL_CHECK(&a == &b);
}

FL_TEST_CASE("busKeepAlive<Bus::BIT_BANG>() ODR-uses the singleton accessor") {
    fl::busKeepAlive<fl::Bus::BIT_BANG>();
    auto p = fl::BusTraits<fl::Bus::BIT_BANG>::instancePtr();
    FL_REQUIRE(p != nullptr);
    FL_CHECK(p.get() == &fl::BusTraits<fl::Bus::BIT_BANG>::instance());
}

FL_TEST_CASE("FastLED.addLeds<WS2812, ..., fl::Bus::BIT_BANG> clockless variant compiles") {
    static CRGB leds[8];
    auto& ctrl = FastLED.addLeds<WS2812, 2, GRB, fl::Bus::BIT_BANG>(leds, 8);
    (void)ctrl;
    FL_CHECK(&fl::BusTraits<fl::Bus::BIT_BANG>::instance() != nullptr);
}

FL_TEST_CASE("FastLED.addLeds<APA102, ..., fl::Bus::BIT_BANG> SPI variant compiles") {
    static CRGB leds[8];
    auto& ctrl = FastLED.addLeds<APA102, 23, 18, RGB, DATA_RATE_MHZ(12), fl::Bus::BIT_BANG>(leds, 8);
    (void)ctrl;
    FL_CHECK(&fl::BusTraits<fl::Bus::BIT_BANG>::instance() != nullptr);
}

FL_TEST_CASE("FastLED.addLeds default-AUTO call sites compile") {
    static CRGB clockless[8];
    static CRGB apa102[8];
    static CRGB sk9822[8];
    auto& a = FastLED.addLeds<WS2812, 3, GRB>(clockless, 8);
    auto& b = FastLED.addLeds<APA102, 23, 18, RGB, DATA_RATE_MHZ(12)>(apa102, 8);
    auto& c = FastLED.addLeds<SK9822, 24, 19, RGB, DATA_RATE_MHZ(12)>(sk9822, 8);
    (void)a;
    (void)b;
    (void)c;
}

FL_TEST_CASE("enableDrivers<Bus::BIT_BANG>() is idempotent") {
    auto& mgr = fl::ChannelManager::instance();
    fl::enableDrivers<fl::Bus::BIT_BANG>();
    auto registered = mgr.getDriverByName(fl::string::from_literal("BIT_BANG"));
    FL_REQUIRE(registered != nullptr);
    FL_CHECK(registered.get() == &fl::BusTraits<fl::Bus::BIT_BANG>::instance());
    fl::enableDrivers<fl::Bus::BIT_BANG>();
    auto registered2 = mgr.getDriverByName(fl::string::from_literal("BIT_BANG"));
    FL_REQUIRE(registered2 != nullptr);
    FL_CHECK(registered2.get() == &fl::BusTraits<fl::Bus::BIT_BANG>::instance());
}


// Issues #974 and #976: `template instantiation depth exceeds maximum` when a
// software-SPI chipset was given an explicit data rate. delaycycles<N> used to
// recurse linearly (N -> N-1); it now splits binary, with <0>/<1>/<2> base
// cases present in every translation unit.
//
// Scope note, so this is not mistaken for full coverage: FASTLED_STUB_IMPL is
// in the *hardware* SPI branch of fastspi.h, so DATA_RATE_KHZ here expands to
// a frequency, not to the cycles-per-bit divider that fed delaycycles. These
// cases pin the template surface -- that every rate, including the
// pathological ones from the reports, instantiates -- but the software-SPI
// path that actually broke can only be exercised by compiling for a
// software-SPI platform. Verified there by hand:
// `bash compile uno --examples Apa102` with DATA_RATE_KHZ(1).

FL_TEST_CASE("SPI chipsets instantiate across the data-rate range (#974, #976)") {
    static CRGB slowest[8];
    static CRGB slow[8];
    static CRGB mid[8];
    static CRGB fast[8];
    // DATA_RATE_KHZ(1) is the exact rate from #976.
    auto& a = FastLED.addLeds<SK9822, 23, 18, BGR, DATA_RATE_KHZ(1)>(slowest, 8);
    auto& b = FastLED.addLeds<APA102, 23, 18, RGB, DATA_RATE_KHZ(500)>(slow, 8);
    auto& c = FastLED.addLeds<APA102, 23, 18, RGB, DATA_RATE_MHZ(1)>(mid, 8);
    auto& d = FastLED.addLeds<APA102, 23, 18, RGB, DATA_RATE_MHZ(24)>(fast, 8);
    (void)a; (void)b; (void)c; (void)d;
}

}  // FL_TEST_FILE
