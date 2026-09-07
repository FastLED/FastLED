// Profile binding API coverage for color pipeline P2 (#4036).

#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/color_profile.h"
#include "test.h"

using namespace fl;

FL_TEST_FILE(FL_FILEPATH) {

namespace {

constexpr EmitterProfile kFixtureProfile = EmitterProfile::rgb(
    "fixture/test-r1",
    Chromaticity(0.640f, 0.330f), Chromaticity(0.300f, 0.600f),
    Chromaticity(0.150f, 0.060f), 1.0f, 1.0f, 1.0f);

ChannelPtr makeChannelWithLocalProfile(CRGB* leds) {
    const fl::u16 response[] = {0, 257, 65535};
    EmitterProfile profile = kFixtureProfile;
    profile.response_lut_r = response;
    profile.response_lut_g = response;
    profile.response_lut_b = response;
    profile.response_lut_size = 3;
    ChannelOptions options;
    if (!options.setColorProfile(profile, SourceProfile::linearSrgb())) {
        return ChannelPtr();
    }
    ChannelConfig config(ClocklessChipset(), fl::span<CRGB>(leds, 1), RGB, options);
    return Channel::create(config);
}

}  // namespace

FL_TEST_CASE("SourceProfile provides independent named and custom source spaces") {
    const SourceProfile srgb = SourceProfile::srgbBt709();
    FL_CHECK_EQ(srgb.transfer, TransferFunction::Srgb);
    FL_CHECK_CLOSE(srgb.primaries.white.x, 0.3127f, 0.0001f);

    const SourceProfile custom = SourceProfile::custom(
        RgbPrimaries(Chromaticity(0.640f, 0.330f),
                     Chromaticity(0.300f, 0.600f),
                     Chromaticity(0.150f, 0.060f),
                     Chromaticity(0.3127f, 0.3290f)),
        TransferFunction::Linear);
    FL_CHECK_EQ(custom.transfer, TransferFunction::Linear);
    FL_CHECK_CLOSE(custom.primaries.green.y, 0.600f, 0.0001f);
}

FL_TEST_CASE("Channel profile binding owns fixture calibration without activating P6 rendering") {
    CRGB leds[1] = {};
    ChannelOptions options;
    options.setColorProfile(kFixtureProfile, SourceProfile::linearSrgb());

    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->hasColorProfile());
    FL_CHECK_FALSE(channel->isColorManaged());
    FL_REQUIRE(channel->emitterProfile() != nullptr);
    FL_CHECK_EQ(fl::string(channel->emitterProfile()->id), fl::string("fixture/test-r1"));
}

FL_TEST_CASE("Profile binding and legacy correction are mutually exclusive") {
    ChannelOptions options;
    options.setColorProfile(kFixtureProfile, SourceProfile::linearSrgb());
    FL_CHECK(options.hasColorProfile());

    options.setLegacyCorrection(CRGB(255, 128, 64));
    FL_CHECK_FALSE(options.hasColorProfile());
    FL_CHECK_EQ(options.mCorrection, CRGB(255, 128, 64));
}

FL_TEST_CASE("SourceProfile default is the ordinary-buffer linear BT.709 space") {
    const SourceProfile profile;
    FL_CHECK_EQ(profile.transfer, TransferFunction::Linear);
    FL_CHECK_CLOSE(profile.primaries.red.x, 0.640f, 0.0001f);
    FL_CHECK_CLOSE(profile.primaries.white.y, 0.3290f, 0.0001f);
}

FL_TEST_CASE("Configured color profiles are not reported active before P6 rendering") {
    CRGB leds[1] = {};
    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(kFixtureProfile, SourceProfile::linearSrgb()));
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->hasColorProfile());
    FL_CHECK_FALSE(channel->isColorManaged());
}

FL_TEST_CASE("Profile binding validates and owns response tables") {
    ChannelOptions options;
    EmitterProfile invalid = kFixtureProfile;
    invalid.native_code_depth = 0;
    FL_CHECK_FALSE(options.setColorProfile(invalid, SourceProfile::linearSrgb()));
    FL_CHECK_FALSE(options.hasColorProfile());

    EmitterProfile profile = kFixtureProfile;
    const fl::u16 response[] = {0, 257, 65535};
    profile.response_lut_r = response;
    profile.response_lut_g = response;
    profile.response_lut_b = response;
    profile.response_lut_size = 3;
    FL_REQUIRE(options.setColorProfile(profile, SourceProfile::linearSrgb()));
    profile.response_lut_r = nullptr;
    profile.response_lut_size = 0;
    FL_REQUIRE(options.emitterProfile() != nullptr);
    FL_CHECK_EQ(options.emitterProfile()->response_lut_size, fl::u16(3));
    FL_CHECK_EQ(options.emitterProfile()->response_lut_r[1], fl::u16(257));
}

FL_TEST_CASE("Strict fallback turns an unavailable profile into an observable bind error") {
    CRGB leds[1] = {};
    FastLED.setColorManagementStrict(true);
    ChannelOptions options;
    options.requestColorManagement(SourceProfile::linearSrgb());
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->hasColorProfileFallback());
    FL_CHECK_FALSE(channel->profileBindingAccepted());
    FastLED.setColorManagementStrict(false);
}

FL_TEST_CASE("Channel binding owns response data after source options and config expire") {
    CRGB leds[1] = {};
    ChannelPtr channel = makeChannelWithLocalProfile(leds);
    FL_REQUIRE(channel != nullptr);
    FL_REQUIRE(channel->emitterProfile() != nullptr);
    FL_CHECK_EQ(channel->emitterProfile()->response_lut_size, fl::u16(3));
    FL_CHECK_EQ(channel->emitterProfile()->response_lut_r[1], fl::u16(257));
    FL_CHECK_EQ(channel->emitterProfile()->response_lut_g[2], fl::u16(65535));
}

FL_TEST_CASE("Profile admission rejects invalid source geometry and response curves") {
    ChannelOptions options;
    SourceProfile invalidSource = SourceProfile::linearSrgb();
    invalidSource.primaries.red.x = 1.1f;
    FL_CHECK_FALSE(options.setColorProfile(kFixtureProfile, invalidSource));

    EmitterProfile profile = kFixtureProfile;
    const fl::u16 nonMonotonic[] = {0, 1024, 1000};
    profile.response_lut_r = nonMonotonic;
    profile.response_lut_g = nonMonotonic;
    profile.response_lut_b = nonMonotonic;
    profile.response_lut_size = 3;
    FL_CHECK_FALSE(options.setColorProfile(profile, SourceProfile::linearSrgb()));
    FL_CHECK_FALSE(options.hasColorProfile());
}

FL_TEST_CASE("Profile admission accepts legal spectral-boundary primaries and rejects infinity") {
    ChannelOptions options;
    FL_CHECK(options.setColorProfile(kFixtureProfile, SourceProfile::displayP3()));
    FL_CHECK(options.setColorProfile(kFixtureProfile, SourceProfile::bt2020()));

    EmitterProfile infinite = kFixtureProfile;
    infinite.lum_r = 1.0f / 0.0f;
    FL_CHECK_FALSE(options.setColorProfile(infinite, SourceProfile::linearSrgb()));
}

FL_TEST_CASE("Profile storage is immutable and safely shared by copied channel options") {
    ChannelOptions source;
    FL_REQUIRE(source.setColorProfile(kFixtureProfile, SourceProfile::linearSrgb()));
    ChannelOptions copy = source;
    FL_REQUIRE(copy.emitterProfile() != nullptr);
    FL_CHECK_EQ(copy.emitterProfile(), source.emitterProfile());
}

FL_TEST_CASE("Binding carries target white and CFastLED exposes only global source defaults") {
    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(kFixtureProfile, SourceProfile::linearSrgb()));
    FL_REQUIRE(options.setTargetWhite(Chromaticity(0.3457f, 0.3585f)));
    FL_CHECK_CLOSE(options.targetWhite().x, 0.3457f, 0.0001f);

    FastLED.setDefaultSourceProfile(SourceProfile::displayP3());
    FL_CHECK_EQ(FastLED.defaultSourceProfile().transfer, TransferFunction::Srgb);
    FastLED.setDefaultSourceProfile(SourceProfile::linearSrgb());
}

FL_TEST_CASE("Static profile sugar and fallback status are observable without enabling rendering") {
    CRGB leds[1] = {};
    ChannelOptions options = ChannelOptions::withColorProfile<kFixtureProfile>();
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->hasColorProfile());
    FL_CHECK_FALSE(channel->isColorManaged());
    FL_CHECK_EQ(channel->colorProfileStatus(), ColorProfileStatus::Configured);
}

FL_TEST_CASE("An explicit source after a bound profile is not replaced by the global default") {
    CRGB leds[1] = {};
    // The global default differs from the source requested below, so if the
    // global-source opt-in survives clearColorProfile() the channel silently
    // ends up on linearSrgb instead of the Display P3 that was asked for.
    FastLED.setDefaultSourceProfile(SourceProfile::linearSrgb());
    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(kFixtureProfile));
    options.requestColorManagement(SourceProfile::displayP3());
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK_EQ(channel->sourceProfile().primaries.red.x,
                SourceProfile::displayP3().primaries.red.x);
}

FL_TEST_CASE("Global source defaults are applied when a channel omits a source declaration") {
    CRGB leds[1] = {};
    FastLED.setDefaultSourceProfile(SourceProfile::displayP3());
    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(kFixtureProfile));
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK_EQ(channel->sourceProfile().primaries.red.x, SourceProfile::displayP3().primaries.red.x);
    FastLED.setDefaultSourceProfile(SourceProfile::linearSrgb());
}

FL_TEST_CASE("Static profile create sugar binds the constexpr profile without runtime ownership") {
    CRGB leds[1] = {};
    ChannelConfig config(ClocklessChipset(), leds, RGB);
    ChannelPtr channel = Channel::create<kFixtureProfile>(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK_EQ(channel->emitterProfile(), &kFixtureProfile);
    FL_CHECK(channel->hasColorProfile());
}

FL_TEST_CASE("Fallback is propagated through the channel event surface") {
    CRGB leds[1] = {};
    ColorProfileEvent captured{};
    bool received = false;
    const int listener = FastLED.channelEvents().onColorProfileFallback.add(
        [&](const ColorProfileEvent& event) { captured = event; received = true; });
    ChannelOptions options;
    options.requestColorManagement(SourceProfile::linearSrgb());
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FastLED.channelEvents().onColorProfileFallback.remove(listener);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(received);
    FL_CHECK_EQ(captured.channelId, channel->id());
    FL_CHECK_EQ(captured.status, ColorProfileStatus::Fallback);
}

FL_TEST_CASE("Fixed-package placeholder profiles state their uncalibrated provenance") {
    const EmitterProfile& placeholder = profiles::WS2812B;
    FL_CHECK_EQ(fl::string(placeholder.provenance_kind), fl::string("placeholder"));
    FL_CHECK_EQ(fl::string(placeholder.report_id), fl::string("uncalibrated"));
}

FL_TEST_CASE("Static create preserves every pre-existing channel option") {
    CRGB leds[1] = {};
    ChannelOptions options;
    options.mBus = Bus::SPI;
    options.mDitherMode = BINARY_DITHER;
    options.setTargetWhite(Chromaticity(0.3457f, 0.3585f));
    ChannelConfig config("preserved", ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create<kFixtureProfile>(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK_EQ(channel->name(), fl::string("preserved"));
    FL_CHECK_CLOSE(channel->targetWhite().x, 0.3457f, 0.0001f);
    FL_CHECK_EQ(channel->getDither(), BINARY_DITHER);
}

FL_TEST_CASE("Strict fallback rejects and disables a channel before it can emit") {
    CRGB leds[1] = {};
    FastLED.setColorManagementStrict(true);
    ChannelOptions options;
    options.requestColorManagement();
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FastLED.setColorManagementStrict(false);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK_EQ(channel->colorProfileStatus(), ColorProfileStatus::Rejected);
    FL_CHECK_FALSE(channel->isEnabled());
}

FL_TEST_CASE("Enum selector resolves to the static profile for actual channel creation") {
    CRGB leds[1] = {};
    ChannelConfig config(ClocklessChipset(), leds, RGB);
    ChannelPtr channel = Channel::create<ProfileId::WS2812B>(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK_EQ(channel->emitterProfile(), &profiles::WS2812B);
}

FL_TEST_CASE("Legacy and profile transitions emit one warning event per direction") {
    ChannelOptions options;
    fl::vector<ColorProfileEvent> events;
    const int listener = FastLED.channelEvents().onColorProfileWarning.add(
        [&](const ColorProfileEvent& event) { events.push_back(event); });

    FL_REQUIRE(options.setColorProfile(kFixtureProfile));
    options.setLegacyCorrection(CRGB(255, 128, 64));
    options.setLegacyCorrection(CRGB(255, 128, 64));

    FL_REQUIRE(options.setColorProfile(kFixtureProfile));
    FL_REQUIRE(options.setColorProfile(kFixtureProfile));
    FastLED.channelEvents().onColorProfileWarning.remove(listener);

    FL_REQUIRE_EQ(events.size(), size_t(2));
    FL_CHECK_EQ(events[0].warning, ColorProfileWarning::ProfileClearedByLegacy);
    FL_CHECK_EQ(events[1].warning, ColorProfileWarning::LegacyClearedByProfile);
}

FL_TEST_CASE("FastLED add enum sugar binds static profile without controller-owned allocation") {
    CRGB leds[1] = {};
    CLEDController& controller = FastLED.addLeds<ProfileId::WS2812B, WS2812, 1, GRB>(leds, 1);
    FL_CHECK_EQ(controller.emitterProfile(), &profiles::WS2812B);
    FastLED.clear(ClearFlags::CHANNELS);
}

FL_TEST_CASE("FastLED add enum sugar preserves ChannelConfig and binds its static profile") {
    CRGB leds[1] = {};
    ChannelOptions options;
    options.setTargetWhite(Chromaticity(0.3457f, 0.3585f));
    ChannelConfig config("add-profile", ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = FastLED.add<ProfileId::WS2812B>(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK_EQ(channel->emitterProfile(), &profiles::WS2812B);
    FL_CHECK_EQ(channel->name(), fl::string("add-profile"));
    FL_CHECK_CLOSE(channel->targetWhite().y, 0.3585f, 0.0001f);
    FastLED.remove(channel);
}

FL_TEST_CASE("Static profile binding clears on legacy transition and yields to runtime binding") {
    ChannelOptions options = ChannelOptions::withColorProfile<kFixtureProfile>();
    FL_CHECK_EQ(options.emitterProfile(), &kFixtureProfile);
    options.setLegacyCorrection(CRGB(255, 128, 64));
    FL_CHECK_FALSE(options.hasColorProfile());
    FL_CHECK_EQ(options.emitterProfile(), nullptr);

    options = ChannelOptions::withColorProfile<kFixtureProfile>();
    EmitterProfile runtime = kFixtureProfile;
    runtime.id = "fixture/runtime-r2";
    FL_REQUIRE(options.setColorProfile(runtime, SourceProfile::linearSrgb()));
    FL_REQUIRE(options.emitterProfile() != nullptr);
    FL_CHECK_EQ(fl::string(options.emitterProfile()->id), fl::string("fixture/runtime-r2"));
}

FL_TEST_CASE("Static Channel factory identity is visible through ChannelPtr and base controller") {
    CRGB leds[1] = {};
    ChannelConfig config(ClocklessChipset(), leds, RGB);
    ChannelPtr channel = Channel::create<kFixtureProfile>(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK_EQ(channel->emitterProfile(), &kFixtureProfile);
    CLEDController& base = *channel;
    FL_CHECK_EQ(base.emitterProfile(), &kFixtureProfile);
}

// applyConfig() replaces mSettings wholesale. Without reconciliation it keeps
// the verdict of the configuration it replaced, in both directions.
FL_TEST_CASE("applyConfig rejects a newly requested profile under strict mode") {
    CRGB leds[1] = {};
    ChannelOptions plain;
    ChannelConfig config(ClocklessChipset(), leds, RGB, plain);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_REQUIRE(channel->isEnabled());
    FL_REQUIRE_FALSE(channel->hasColorProfileFallback());

    // Now reconfigure asking for management with nothing to bind to.
    FastLED.setColorManagementStrict(true);
    ChannelOptions requested;
    requested.requestColorManagement();
    ChannelConfig rebound(ClocklessChipset(), leds, RGB, requested);
    channel->applyConfig(rebound);
    FastLED.setColorManagementStrict(false);

    FL_CHECK_EQ(channel->colorProfileStatus(), ColorProfileStatus::Rejected);
    FL_CHECK_FALSE(channel->isEnabled());
}

FL_TEST_CASE("applyConfig clears a stale rejection when a real profile arrives") {
    CRGB leds[1] = {};
    FastLED.setColorManagementStrict(true);
    ChannelOptions requested;
    requested.requestColorManagement();
    ChannelConfig config(ClocklessChipset(), leds, RGB, requested);
    ChannelPtr channel = Channel::create(config);
    FastLED.setColorManagementStrict(false);
    FL_REQUIRE(channel != nullptr);
    FL_REQUIRE_EQ(channel->colorProfileStatus(), ColorProfileStatus::Rejected);
    FL_REQUIRE_FALSE(channel->isEnabled());

    // A valid profile withdraws the rejection: the channel must stop
    // reporting fallback and regain the enable the rejection took away.
    ChannelOptions bound;
    FL_REQUIRE(bound.setColorProfile(kFixtureProfile));
    ChannelConfig rebound(ClocklessChipset(), leds, RGB, bound);
    channel->applyConfig(rebound);

    FL_CHECK_FALSE(channel->hasColorProfileFallback());
    FL_CHECK_EQ(channel->colorProfileStatus(), ColorProfileStatus::Configured);
    FL_CHECK(channel->isEnabled());
}

FL_TEST_CASE("Runtime static Channel factory yields to legacy clear and runtime rebind") {
    CRGB leds[1] = {};
    ChannelConfig config(ClocklessChipset(), leds, RGB);
    ChannelPtr channel = Channel::create<kFixtureProfile>(config);
    FL_REQUIRE(channel != nullptr);
    channel->setCorrection(CRGB(255, 128, 64));
    FL_CHECK_EQ(channel->emitterProfile(), nullptr);

    EmitterProfile runtime = kFixtureProfile;
    runtime.id = "fixture/channel-runtime-r2";
    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(runtime, SourceProfile::linearSrgb()));
    ChannelConfig rebound(ClocklessChipset(), leds, RGB, options);
    channel->applyConfig(rebound);
    FL_REQUIRE(channel->emitterProfile() != nullptr);
    FL_CHECK_EQ(fl::string(channel->emitterProfile()->id), fl::string("fixture/channel-runtime-r2"));
}


}  // FL_TEST_FILE
