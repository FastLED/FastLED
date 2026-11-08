// This TU deliberately disagrees with the library's optional-encoder macros.
// Availability must be determined by the library, never by a calling header.
#include "test.h"
#include "FastLED.h"
#include "fl/channels/channel_typed.h"
#include "fl/channels/data.h"
#include "fl/channels/manager.h"
#include "fl/stl/scope_exit.h"

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;

namespace {
class AvailabilityCapture : public IChannelDriver {
  public:
    fl::vector<u8> bytes;
    int frames = 0;
    bool canHandle(const ChannelDataPtr&) const FL_NO_EXCEPT override { return true; }
    void enqueue(ChannelDataPtr data) FL_NO_EXCEPT override {
        ++frames;
        bytes.assign(data->getData().begin(), data->getData().end());
    }
    void show() FL_NO_EXCEPT override {}
    DriverState poll() FL_NO_EXCEPT override { return DriverState::READY; }
    fl::string getName() const FL_NO_EXCEPT override { return "AVAILABILITY_CAPTURE"; }
    Capabilities getCapabilities() const FL_NO_EXCEPT override { return Capabilities(true, true); }
};
}

FL_TEST_CASE("[#5157] typed and erased encoders use library availability") {
    CRGB leds[1] = {CRGB(11, 57, 201)};
    ChannelOptions options;
    options.mDitherMode = DISABLE_DITHER;
    auto& manager = ChannelManager::instance();
    auto cleanup = fl::make_scope_exit([&]() { manager.clearAllDrivers(); });
    bool spi = false;
    FL_SUBCASE("UCS7604 despite caller disable macro") { spi = false; }
    FL_SUBCASE("SPI despite caller disable macro") { spi = true; }
    ChipsetVariant chipset = spi
        ? ChipsetVariant(SpiChipsetConfig(14, 15, SpiEncoder::apa102()))
        : ChipsetVariant(ClocklessChipset(14, ChipsetTimingConfig(800, 450, 450, 50, "UCS7604"),
              ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT));
    ChannelConfig config(chipset, leds, GRB, options);
    auto erased = Channel::create(config);
    auto typed = spi ? TypedChannel<Bus::BIT_BANG, SpiChipsetConfig>::create(config)
                     : TypedChannel<Bus::BIT_BANG, ClocklessChipset>::create(config);
    auto typedConfig = spi
        ? TypedChannel<Bus::BIT_BANG, SpiChipsetConfig>::create(
              ChannelConfigOf<SpiChipsetConfig>(*chipset.ptr<SpiChipsetConfig>(), leds, GRB, options))
        : TypedChannel<Bus::BIT_BANG, ClocklessChipset>::create(
              ChannelConfigOf<ClocklessChipset>(*chipset.ptr<ClocklessChipset>(), leds, GRB, options));
    FL_REQUIRE(erased != nullptr);
    FL_REQUIRE(typed != nullptr);
    FL_REQUIRE(typedConfig != nullptr);
    manager.clearAllDrivers();
    auto capture = fl::make_shared<AvailabilityCapture>();
    manager.addDriver(9900, capture);
    erased->showLeds(255);
    const auto expected = capture->bytes;
    FL_REQUIRE_EQ(capture->frames, 1);
    FL_REQUIRE_GT(expected.size(), 0u);
    typed->showLeds(255);
    FL_REQUIRE_EQ(capture->frames, 2);
    FL_CHECK_EQ(capture->bytes, expected);
    typedConfig->showLeds(255);
    FL_REQUIRE_EQ(capture->frames, 3);
    FL_CHECK_EQ(capture->bytes, expected);
}
} // FL_TEST_FILE
