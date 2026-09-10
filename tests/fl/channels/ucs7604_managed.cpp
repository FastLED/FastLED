/// @file tests/fl/channels/ucs7604_managed.cpp
/// @brief The Channels UCS7604 encode path, which had no coverage at all.
///
/// `tests/fl/chipsets/ucs7604.cpp` drives `testUCS7604Controller` -- the
/// legacy `CLEDController` path. `fl::Channel`'s own `writeUCS7604` sits in an
/// anonymous namespace in `channel.cpp.hpp` and is reachable only through
/// `showPixels()`, so nothing observed what it emits (#4326).
///
/// These drive a real channel through a capturing driver and read the bytes
/// back off `ChannelData`.

#include "fl/channels/channel.h"
#include "fl/channels/manager.h"
#include "fl/channels/data.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "FastLED.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/channels/options.h"
#include "fl/gfx/crgb.h"
#include "fl/stl/int.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

/// Records the encoded bytes of the last frame enqueued.
class CapturingDriver : public IChannelDriver {
public:
    fl::vector<u8> last;
    int frames = 0;

    bool canHandle(const ChannelDataPtr& data) const override {
        (void)data;
        return true;
    }
    void enqueue(ChannelDataPtr channelData) override {
        ++frames;
        last.clear();
        if (channelData) {
            const auto& d = channelData->getData();
            for (fl::size i = 0; i < d.size(); ++i) {
                last.push_back(d[i]);
            }
        }
    }
    void show() override {}
    DriverState poll() override { return DriverState::READY; }
    fl::string getName() const override {
        return fl::string::from_literal("UCS_CAPTURE");
    }
    Capabilities getCapabilities() const override { return Capabilities(true, true); }
};

constexpr EmitterProfile kProfile = EmitterProfile::rgb(
    "fixture/ucs-4326",
    Chromaticity(0.640f, 0.330f), Chromaticity(0.300f, 0.600f),
    Chromaticity(0.150f, 0.060f), 1.0f, 1.0f, 1.0f);

ClocklessChipset ucs16(int pin) {
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "UCS7604");
    return ClocklessChipset(pin, timing,
                            ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT);
}

/// Encode one frame and hand back the bytes the driver saw.
fl::vector<u8> encodeOnce(CRGB colour, bool bindProfile, float gamma, int pin) {
    // Reset first: registrations leak across calls otherwise, and with several
    // same-priority drivers alive the frame can land on one this call does not
    // hold, which silently makes every comparison below meaningless.
    auto& mgr = ChannelManager::instance();
    mgr.clearAllDrivers();
    auto driver = fl::make_shared<CapturingDriver>();
    mgr.addDriver(9100, driver);

    CRGB leds[1] = {colour};
    ChannelOptions options;
    options.mGamma = gamma;
    if (bindProfile) {
        options.setColorProfile(kProfile, SourceProfile::linearSrgb());
    }
    ChannelConfig config(ucs16(pin), fl::span<CRGB>(leds, 1), RGB, options);
    ChannelPtr channel = Channel::create(config);
    if (channel) {
        channel->showLeds(255);
    }
    return driver->last;
}

}  // namespace

FL_TEST_CASE("[#4326] the Channels UCS7604 path encodes a frame at all") {
    // The floor this file exists to lay down. `tests/fl/chipsets/ucs7604.cpp`
    // drives the legacy CLEDController; `fl::Channel`'s writeUCS7604 lives in
    // an anonymous namespace and is reachable only through showPixels(), so
    // nothing observed it. If this stops producing bytes the cases below stop
    // meaning anything.
    fl::vector<u8> out = encodeOnce(CRGB(127, 0, 0), false, 2.8f, 11);
    FL_REQUIRE_GT((int)out.size(), 0);

    // 15-byte preamble, then RGB16 big-endian. Gamma 2.8 of 127 is the value
    // the legacy path produces too, so this pins the whole chain rather than
    // just "some bytes came out".
    FL_REQUIRE_GT((int)out.size(), 16);
    const int r16 = (out[15] << 8) | out[16];
    FL_CHECK_EQ(r16, (int)fl::gamma_2_8(127));
}

FL_TEST_CASE("[#4326] gamma reaches the encoder when no profile is bound") {
    // Legacy behaviour, and correct: an unmanaged channel is meant to be
    // shaped by mGamma. This is also the positive control for the case below
    // -- without it, "gamma does not vary the output once a profile binds"
    // could be satisfied by an encoder that ignores gamma entirely.
    fl::vector<u8> a = encodeOnce(CRGB(127, 0, 0), false, 2.8f, 12);
    fl::vector<u8> b = encodeOnce(CRGB(127, 0, 0), false, 1.6f, 13);

    FL_REQUIRE_GT((int)a.size(), 16);
    FL_REQUIRE_EQ((int)a.size(), (int)b.size());
    FL_CHECK_NE((int)((a[15] << 8) | a[16]), (int)((b[15] << 8) | b[16]));
}

FL_TEST_CASE("[#4326] binding a profile clears the caller's gamma") {
    // ChannelOptions::setColorProfile() resets mGamma alongside mCorrection,
    // mTemperature and mDitherMode (options.h). Pinned here because it is the
    // half of the managed-mode exclusion policy that #4156 R9 asked for and
    // it had no test: two channels that differ only in the gamma they asked
    // for produce identical bytes once a profile is bound.
    fl::vector<u8> a = encodeOnce(CRGB(127, 0, 0), true, 2.8f, 14);
    fl::vector<u8> b = encodeOnce(CRGB(127, 0, 0), true, 1.6f, 15);

    FL_REQUIRE_GT((int)a.size(), 0);
    FL_REQUIRE_EQ((int)a.size(), (int)b.size());
    for (fl::size i = 0; i < a.size(); ++i) {
        FL_CHECK_EQ((int)a[i], (int)b[i]);
    }
}

FL_TEST_CASE("[#4326] a bound profile does not yet make the channel colour-managed") {
    // Recorded because it bounds what the cases above can be read to mean.
    // The profile binds and is owned, but P6 rendering is not active, so
    // there is no device solve on this path today and therefore no "second
    // gamma after the solve" to observe. Whether the 2.8 that
    // `mGamma.value_or(2.8f)` falls back to should also go away is a question
    // for when this flips to true.
    auto& mgr = ChannelManager::instance();
    mgr.clearAllDrivers();
    mgr.addDriver(9100, fl::make_shared<CapturingDriver>());

    CRGB leds[1] = {CRGB(127, 0, 0)};
    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(kProfile, SourceProfile::linearSrgb()));
    ChannelConfig config(ucs16(16), fl::span<CRGB>(leds, 1), RGB, options);
    ChannelPtr channel = Channel::create(config);

    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->hasColorProfile());
    FL_CHECK_FALSE(channel->isColorManaged());
}

}
