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

// #5162: every constant typed chipset resolves to a factory bound to its one
// encoder, never to the shared selector that links all of them. Chipsets that
// share an encoder share a factory; distinct encoders get distinct factories.
FL_TEST_CASE("[#5162] constant typed chipsets bind only their own encoder") {
    using Clockless = TypedChannel<Bus::BIT_BANG, ClocklessChipset>;
    using Spi = TypedChannel<Bus::BIT_BANG, SpiChipsetConfig>;
    const ChannelFactory clocklessSelector = static_cast<ChannelFactory>(&Clockless::create);
    const ChannelFactory spiSelector = static_cast<ChannelFactory>(&Spi::create);
    const ChipsetTimingConfig timing(800, 450, 450, 50, "T");
    auto clockless = [&](ClocklessEncoder encoder) {
        const ClocklessChipset chipset(14, timing, encoder);
        const ChannelFactory factory = Clockless::factoryFor(chipset);
        // The erased-config overload (add<B>(ChannelConfig)) agrees.
        FL_CHECK(factory == Clockless::factoryFor(ChipsetVariant(chipset)));
        return factory;
    };
    auto spi = [&](SpiChipset chipset) {
        const SpiChipsetConfig config(14, 15, SpiEncoder::spiEncoderForChipset(chipset));
        const ChannelFactory factory = Spi::factoryFor(config);
        FL_CHECK(factory == Spi::factoryFor(ChipsetVariant(config)));
        return factory;
    };

    // Clockless: WS2812, TM1812, TM1908 and the three UCS7604 modes (one encoder).
    const ChannelFactory ws2812 = clockless(ClocklessEncoder::CLOCKLESS_ENCODER_WS2812);
    const ChannelFactory tm1812 = clockless(ClocklessEncoder::CLOCKLESS_ENCODER_TM1812_RGBWW);
    const ChannelFactory tm1908 = clockless(ClocklessEncoder::CLOCKLESS_ENCODER_TM1908);
    const ChannelFactory ucs7604 = clockless(ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_8BIT);
    FL_CHECK(clockless(ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT) == ucs7604);
    FL_CHECK(clockless(ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT_1600) == ucs7604);
    const ChannelFactory clocklessBound[] = {ws2812, tm1812, tm1908, ucs7604};
    for (fl::size i = 0; i < 4; ++i) {
        FL_CHECK(clocklessBound[i] != clocklessSelector);
        for (fl::size j = i + 1; j < 4; ++j) {
            FL_CHECK(clocklessBound[i] != clocklessBound[j]);
        }
    }

    // SPI: the APA102 and SK9822 families each share one encoder.
    const ChannelFactory apa102 = spi(SpiChipset::APA102);
    const SpiChipset apa102Family[] = {SpiChipset::DOTSTAR, SpiChipset::APA102HD,
                                       SpiChipset::DOTSTARHD, SpiChipset::HD107,
                                       SpiChipset::HD107HD};
    for (const auto chipset : apa102Family) {
        FL_CHECK(spi(chipset) == apa102);
    }
    const ChannelFactory sk9822 = spi(SpiChipset::SK9822);
    FL_CHECK(spi(SpiChipset::SK9822HD) == sk9822);
    const ChannelFactory spiBound[] = {
        apa102, sk9822, spi(SpiChipset::WS2801), spi(SpiChipset::WS2803),
        spi(SpiChipset::P9813), spi(SpiChipset::LPD8806), spi(SpiChipset::LPD6803),
        spi(SpiChipset::SM16716), spi(SpiChipset::MY9221), spi(SpiChipset::HD108)};
    const fl::size spiCount = sizeof(spiBound) / sizeof(spiBound[0]);
    for (fl::size i = 0; i < spiCount; ++i) {
        FL_CHECK(spiBound[i] != spiSelector);
        for (fl::size j = i + 1; j < spiCount; ++j) {
            FL_CHECK(spiBound[i] != spiBound[j]);
        }
    }
}
} // FL_TEST_FILE
