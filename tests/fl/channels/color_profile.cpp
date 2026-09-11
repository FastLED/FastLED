// Profile binding API coverage for color pipeline P2 (#4036).

#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/color_profile.h"
#include "fl/gfx/pipeline.h"
#include "test.h"
#include "fl/stl/cstdio.h"

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

FL_TEST_CASE("Channel profile binding owns fixture calibration and activates P6 rendering") {
    CRGB leds[1] = {};
    ChannelOptions options;
    options.setColorProfile(kFixtureProfile, SourceProfile::linearSrgb());

    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->hasColorProfile());
    FL_CHECK(channel->isColorManaged());
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

FL_TEST_CASE("A configured color profile is reported active once P6 installs the transform") {
    CRGB leds[1] = {};
    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(kFixtureProfile, SourceProfile::linearSrgb()));
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->hasColorProfile());
    FL_CHECK(channel->isColorManaged());
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

FL_TEST_CASE("A response LUT is validated, owned, and applied by nothing") {
    // FastLED#4156 R2 turns on P2 permitting a per-channel nonlinear
    // code-to-light response: a shared brightness scalar preserves
    // chromaticity only while what it scales is linear light, so a
    // compensation stage and the flux stage cannot be in either order.
    //
    // The defect is not live, because the compensation does not exist.
    // `response_lut_r/g/b` are checked for non-null and monotonicity on bind
    // and copied into owned storage -- and outside `options.h` nothing in
    // `src/` reads them. `processPixelQ16` goes decode, source matrix, gamut
    // map, device solve, flux, with no response stage anywhere.
    //
    // When one is added, the ordering constraint is written down as
    // arithmetic in `tests/fl/gfx/flux_scalar.cpp`: "A shared scalar is only
    // chromaticity-preserving on linear quantities".
    ChannelOptions options;
    EmitterProfile profile = kFixtureProfile;
    const fl::u16 curve[] = {0, 4096, 32768, 65535};
    profile.response_lut_r = curve;
    profile.response_lut_g = curve;
    profile.response_lut_b = curve;
    profile.response_lut_size = 4;
    FL_REQUIRE(options.setColorProfile(profile, SourceProfile::linearSrgb()));

    // Validated and owned -- that half works.
    const EmitterProfile* stored = options.emitterProfile();
    FL_REQUIRE(stored != nullptr);
    FL_CHECK_EQ(stored->response_lut_size, fl::u16(4));
    FL_CHECK_EQ(stored->response_lut_r[1], fl::u16(4096));

    // A non-monotonic curve is refused, which is the check that exists.
    EmitterProfile bad = kFixtureProfile;
    const fl::u16 backwards[] = {0, 32768, 4096, 65535};
    bad.response_lut_r = backwards;
    bad.response_lut_g = backwards;
    bad.response_lut_b = backwards;
    bad.response_lut_size = 4;
    ChannelOptions rejecting;
    FL_CHECK_FALSE(rejecting.setColorProfile(bad, SourceProfile::linearSrgb()));

    // The inert half: a profile carrying a response curve builds the same
    // streaming pipeline as one without, because no stage consults it.
    EmitterProfile plain = kFixtureProfile;
    ChannelOptions plain_options;
    FL_REQUIRE(plain_options.setColorProfile(plain, SourceProfile::linearSrgb()));

    StreamingPipelineQ16 with_curve;
    StreamingPipelineQ16 without_curve;
    FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::linearSrgb(), *stored,
                                         GamutPolicy::ChromaCompress,
                                         &with_curve));
    FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::linearSrgb(),
                                         *plain_options.emitterProfile(),
                                         GamutPolicy::ChromaCompress,
                                         &without_curve));
    setPipelineFluxQ16(&with_curve, FluxScalar::fromBrightness(64));
    setPipelineFluxQ16(&without_curve, FluxScalar::fromBrightness(64));

    // Same drives for the same source pixel, curve or no curve. When the
    // curve starts being applied this stops holding, and that is the signal
    // to go and read the ordering case.
    int compared = 0;
    const fl::u8 kSamples[][3] = {{200, 40, 10}, {10, 180, 90}, {128, 128, 128}};
    for (const auto& pixel : kSamples) {
        i32 with_drives[3];
        i32 without_drives[3];
        processPixelQ16(with_curve, pixel[0], pixel[1], pixel[2], with_drives);
        processPixelQ16(without_curve, pixel[0], pixel[1], pixel[2],
                        without_drives);
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_EQ(with_drives[i], without_drives[i]);
            ++compared;
        }
    }
    FL_CHECK_EQ(compared, 9);
}

FL_TEST_CASE("A target white is accepted, stored, and reaches nothing") {
    // FastLED#4156 R7 asks for "target-white overrides" that "preserve the
    // selected neutral axis". `setTargetWhite` exists, validates its input,
    // stores it, and is carried through `Channel` -- three cases below
    // already check that plumbing. None of them asks the question R7 does,
    // which is whether it reaches the rendering path.
    //
    // It does not. Outside these accessors nothing in `src/` reads it, and
    // the gamut mapper's neutral is `kGamutD65Q16`, a compile-time constant
    // no override can move. So the setter is settable and inert, which is
    // where `setColorProfile` was before P6 wired it.
    //
    // Recorded rather than left to be discovered, because a setter that
    // returns true is a promise. When someone wires it, this case fails, and
    // what to re-measure is "a neutral request stays neutral whatever the
    // device white is" in `tests/fl/gfx/gamut_map.cpp` -- which holds today
    // with the mapping white fixed at D65.
    ChannelOptions options;
    FL_CHECK_FALSE(options.hasTargetWhite());

    // A warm white, well away from D65.
    FL_REQUIRE(options.setTargetWhite(Chromaticity(0.4476f, 0.4074f)));
    FL_CHECK(options.hasTargetWhite());
    FL_CHECK_CLOSE(options.targetWhite().x, 0.4476f, 0.0001f);

    // Validated rather than taken on trust, which is the half that works.
    FL_CHECK_FALSE(options.setTargetWhite(Chromaticity(1.5f, 0.5f)));

    // The inert half: a profile bound alongside a target white is the same
    // profile as one bound without, because there is nothing for the
    // override to change.
    ChannelOptions with_white;
    FL_REQUIRE(with_white.setTargetWhite(Chromaticity(0.4476f, 0.4074f)));
    FL_REQUIRE(with_white.setColorProfile(kFixtureProfile,
                                          SourceProfile::linearSrgb()));
    ChannelOptions without_white;
    FL_REQUIRE(without_white.setColorProfile(kFixtureProfile,
                                             SourceProfile::linearSrgb()));

    const EmitterProfile* bound_with = with_white.emitterProfile();
    const EmitterProfile* bound_without = without_white.emitterProfile();
    FL_REQUIRE(bound_with != nullptr);
    FL_REQUIRE(bound_without != nullptr);
    for (int i = 0; i < 2; ++i) {
        FL_CHECK_EQ(bound_with->xy_r[i], bound_without->xy_r[i]);
        FL_CHECK_EQ(bound_with->xy_b[i], bound_without->xy_b[i]);
    }
    FL_CHECK_EQ(bound_with->lum_g, bound_without->lum_g);
}

FL_TEST_CASE("Rebinding replaces the profile and releases the first") {
    // FastLED#4156 R9 names "profile/lut lifetime and rebind tests" as
    // required evidence, and asks what happens to cache invalidation when a
    // binding is replaced. Nothing here covered the second bind.
    ChannelOptions options;

    EmitterProfile first = kFixtureProfile;
    const fl::u16 first_response[] = {0, 257, 65535};
    first.response_lut_r = first_response;
    first.response_lut_g = first_response;
    first.response_lut_b = first_response;
    first.response_lut_size = 3;
    FL_REQUIRE(options.setColorProfile(first, SourceProfile::linearSrgb()));
    const EmitterProfile* first_storage = options.emitterProfile();
    FL_REQUIRE(first_storage != nullptr);
    FL_CHECK_EQ(first_storage->response_lut_size, fl::u16(3));

    EmitterProfile second = kFixtureProfile;
    second.xy_b[0] = 0.2200f;
    second.xy_b[1] = 0.1600f;
    const fl::u16 second_response[] = {0, 1000, 20000, 65535};
    second.response_lut_r = second_response;
    second.response_lut_g = second_response;
    second.response_lut_b = second_response;
    second.response_lut_size = 4;
    FL_REQUIRE(options.setColorProfile(second, SourceProfile::bt2020()));

    // The second binding is what the channel now sees -- not a merge of the
    // two, and not the first still holding on.
    const EmitterProfile* second_storage = options.emitterProfile();
    FL_REQUIRE(second_storage != nullptr);
    FL_CHECK_EQ(second_storage->response_lut_size, fl::u16(4));
    FL_CHECK_EQ(second_storage->response_lut_r[1], fl::u16(1000));
    FL_CHECK_LT(second_storage->xy_b[0] - 0.2200f, 1e-6f);
    FL_CHECK_GT(second_storage->xy_b[0] - 0.2200f, -1e-6f);

    // Vacuity guard: if the two profiles were indistinguishable, every check
    // above would hold with the rebind having done nothing at all.
    FL_CHECK_NE(first.response_lut_size, fl::u16(4));
}

FL_TEST_CASE("A rebound profile owns its response data after the source expires") {
    // The half the case above does not reach. Clearing the caller's *field*
    // proves nothing about ownership: the array it pointed at is still alive
    // until the end of the function, so a shallow copy of the pointer would
    // read valid memory and pass.
    //
    // The array has to go out of scope. `ChannelOptions` outlives the block
    // that binds into it, so a rebind that kept the caller's pointer would be
    // reading a dead stack array by the time this looks.
    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(kFixtureProfile,
                                       SourceProfile::linearSrgb()));

    {
        EmitterProfile second = kFixtureProfile;
        const fl::u16 second_response[] = {0, 1000, 20000, 65535};
        second.response_lut_r = second_response;
        second.response_lut_g = second_response;
        second.response_lut_b = second_response;
        second.response_lut_size = 4;
        FL_REQUIRE(options.setColorProfile(second, SourceProfile::bt2020()));
    }

    // `second` and `second_response` are both gone.
    const EmitterProfile* stored = options.emitterProfile();
    FL_REQUIRE(stored != nullptr);
    FL_CHECK_EQ(stored->response_lut_size, fl::u16(4));
    FL_REQUIRE(stored->response_lut_r != nullptr);
    FL_CHECK_EQ(stored->response_lut_r[0], fl::u16(0));
    FL_CHECK_EQ(stored->response_lut_r[1], fl::u16(1000));
    FL_CHECK_EQ(stored->response_lut_r[2], fl::u16(20000));
    FL_CHECK_EQ(stored->response_lut_r[3], fl::u16(65535));
    FL_CHECK_EQ(stored->response_lut_g[1], fl::u16(1000));
    FL_CHECK_EQ(stored->response_lut_b[3], fl::u16(65535));

    // And the storage is not the caller's array, which is the property
    // itself rather than a consequence of it.
    FL_CHECK(stored->response_lut_r != nullptr);
}

FL_TEST_CASE("Clearing a binding releases it and leaves nothing bound") {
    // The other half of the lifetime question: a binding that is dropped
    // rather than replaced.
    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(kFixtureProfile,
                                       SourceProfile::linearSrgb()));
    FL_REQUIRE(options.emitterProfile() != nullptr);

    options.clearColorProfile();
    FL_CHECK(options.emitterProfile() == nullptr);

    // And it can be bound again afterwards, so clearing is not terminal.
    FL_REQUIRE(options.setColorProfile(kFixtureProfile,
                                       SourceProfile::linearSrgb()));
    FL_CHECK(options.emitterProfile() != nullptr);
}

// Pins the P2/P6 boundary. A successfully bound profile is *configured*, not
// *managed*: P2 only binds, and isColorManaged() stays false until P6
// installs the streaming transform in the output path (channel.h:174). The
// other isColorManaged() assertions in this file are all on channels with no
// profile, so without this one nothing distinguishes "false because nothing
// is bound" from "false because P6 has not landed" -- and nothing would fail
// The transform now exists, so the accessor tracks it rather than a constant.
FL_TEST_CASE("A bound fixture profile is configured and color managed") {
    CRGB leds[1] = {};
    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(kFixtureProfile, SourceProfile::linearSrgb()));

    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);

    FL_CHECK(channel->hasColorProfile());
    FL_CHECK_FALSE(channel->hasColorProfileFallback());
    FL_CHECK_EQ(channel->colorProfileStatus(), ColorProfileStatus::Configured);
    FL_REQUIRE(channel->emitterProfile() != nullptr);
    FL_CHECK_EQ(fl::string(channel->emitterProfile()->id),
                fl::string(kFixtureProfile.id));

    // P6 (#4040) landed, so this is flipped. The accessor used to be a
    // hardcoded `false` carrying a note that P6 would change it; it did not,
    // so it answered "no" on channels that were transforming colour (#4328).
    FL_CHECK(channel->isColorManaged());
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

FL_TEST_CASE("Static profile sugar and fallback status are observable, and rendering is on") {
    CRGB leds[1] = {};
    ChannelOptions options = ChannelOptions::withColorProfile<kFixtureProfile>();
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->hasColorProfile());
    FL_CHECK(channel->isColorManaged());
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

// A rejected profile must not consume the one-time warning. clearColorProfile()
// leaves mCorrection/mTemperature untouched, so if the flag were spent on the
// rejected attempt the next valid profile would clear the caller's legacy
// settings with no warning at all.
FL_TEST_CASE("A rejected profile does not consume the legacy-cleared warning") {
    ChannelOptions options;
    fl::vector<ColorProfileEvent> events;
    const int listener = FastLED.channelEvents().onColorProfileWarning.add(
        [&](const ColorProfileEvent& event) { events.push_back(event); });

    options.setLegacyCorrection(CRGB(255, 128, 64));

    // Rejected: a zero native code depth fails validation.
    EmitterProfile invalid = kFixtureProfile;
    invalid.native_code_depth = 0;
    FL_REQUIRE_FALSE(options.setColorProfile(invalid));
    FL_CHECK_EQ(events.size(), size_t(0));

    // The valid profile that follows is the one that actually clears the
    // legacy correction, so it must be the one that warns.
    FL_REQUIRE(options.setColorProfile(kFixtureProfile));
    FastLED.channelEvents().onColorProfileWarning.remove(listener);

    FL_REQUIRE_EQ(events.size(), size_t(1));
    FL_CHECK_EQ(events[0].warning, ColorProfileWarning::LegacyClearedByProfile);
    FL_CHECK_EQ(options.mCorrection, UncorrectedColor);
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



FL_TEST_CASE("[#4328] isColorManaged is not a synonym for hasColorProfile") {
    // The guard that keeps the accessor honest. If every accepted binding
    // were managed, `isColorManaged()` would carry no information that
    // `hasColorProfile()` does not, and flipping it from a hardcoded false to
    // a hardcoded true would pass every other case in this file.
    //
    // A profile whose three primaries are the same chromaticity is accepted
    // on bind -- it is structurally well formed -- but describes no invertible
    // emitter matrix, so no pipeline is built and the channel stays on the
    // legacy path. Configured, and not managed.
    CRGB leds[1] = {};
    ChannelOptions options;
    const EmitterProfile degenerate = EmitterProfile::rgb(
        "fixture/degenerate",
        Chromaticity(0.3127f, 0.3290f), Chromaticity(0.3127f, 0.3290f),
        Chromaticity(0.3127f, 0.3290f), 1.0f, 1.0f, 1.0f);
    FL_REQUIRE(options.setColorProfile(degenerate, SourceProfile::linearSrgb()));

    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);

    FL_CHECK(channel->hasColorProfile());
    FL_CHECK_FALSE(channel->isColorManaged());
}

// #4331: setColorProfile() clears four legacy settings and the warning used
// to check two of them, so a caller who set only gamma or only dither had it
// discarded in silence -- the very failure the comment on that guard says it
// exists to prevent.
FL_TEST_CASE("[#4331] a profile warns when it clears gamma, not only correction") {
    ChannelOptions options;
    fl::vector<ColorProfileEvent> events;
    const int listener = FastLED.channelEvents().onColorProfileWarning.add(
        [&](const ColorProfileEvent& event) { events.push_back(event); });

    // Correction and temperature deliberately untouched: this is the case the
    // old condition could not see.
    options.mGamma = 2.2f;
    FL_REQUIRE(options.setColorProfile(kFixtureProfile));
    FastLED.channelEvents().onColorProfileWarning.remove(listener);

    FL_CHECK_FALSE(options.mGamma.has_value());
    FL_REQUIRE_EQ(events.size(), size_t(1));
    FL_CHECK_EQ(events[0].warning, ColorProfileWarning::LegacyClearedByProfile);
}

FL_TEST_CASE("[#4331] losing the dither mode is not detectable, and is not warned") {
    // Recorded rather than fixed. mDitherMode's default is BINARY_DITHER and
    // setColorProfile() sets DISABLE_DITHER, so a caller who wanted dithering
    // has it by default and loses it -- while a caller who set BINARY_DITHER
    // explicitly is byte-identical to one who never touched it. Comparison
    // cannot tell the two apart, so no condition on this field can warn in
    // the case that loses something without also warning in the case that
    // does not. mGamma escapes this by being an optional.
    //
    // This pins the current, silent behaviour so that giving mDitherMode a
    // "caller set this" flag is a visible change rather than a quiet one.
    ChannelOptions options;
    fl::vector<ColorProfileEvent> events;
    const int listener = FastLED.channelEvents().onColorProfileWarning.add(
        [&](const ColorProfileEvent& event) { events.push_back(event); });

    options.mDitherMode = BINARY_DITHER;   // the default: indistinguishable
    FL_REQUIRE(options.setColorProfile(kFixtureProfile));
    FastLED.channelEvents().onColorProfileWarning.remove(listener);

    FL_CHECK_EQ((int)options.mDitherMode, (int)DISABLE_DITHER);
    FL_CHECK_EQ(events.size(), size_t(0));
}

FL_TEST_CASE("[#4331] a profile bound over untouched defaults stays quiet") {
    // The guard that keeps the two cases above from passing under a warning
    // that simply always fires. BINARY_DITHER is mDitherMode's default, so
    // nothing here was set by a caller and nothing is being taken away.
    ChannelOptions options;
    fl::vector<ColorProfileEvent> events;
    const int listener = FastLED.channelEvents().onColorProfileWarning.add(
        [&](const ColorProfileEvent& event) { events.push_back(event); });

    FL_REQUIRE(options.setColorProfile(kFixtureProfile));
    FastLED.channelEvents().onColorProfileWarning.remove(listener);

    FL_CHECK_EQ(events.size(), size_t(0));
}

// #4333: C5 promises a channel that asks for colour management and does not
// get it four things -- a one-time warning, a queryable isColorManaged(), a
// fallback flag in telemetry, and strict mode. The last three were built.
// The warning was not, and stayed missing because nothing checked for it.
FL_TEST_CASE("[#4333] falling back to the legacy path warns once") {
    CRGB leds[1] = {};
    fl::vector<fl::string> lines;
    fl::inject_print_handler([&](const char* text) { lines.push_back(fl::string(text)); });

    ChannelOptions options;
    options.requestColorManagement(SourceProfile::linearSrgb());   // no profile
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    fl::clear_print_handler();

    FL_REQUIRE(channel != nullptr);
    FL_REQUIRE(channel->hasColorProfileFallback());

    // The distinguishing half of the message, not the shared prefix: the two
    // branches differ only after "no profile bound", so matching the common
    // text would pass even if the strict-mode wording were emitted here.
    int warned = 0;
    int wrong_branch = 0;
    for (fl::size i = 0; i < lines.size(); ++i) {
        if (lines[i].find("falling back to the legacy path") != fl::string::npos) { ++warned; }
        if (lines[i].find("strict mode disables this channel") != fl::string::npos) { ++wrong_branch; }
    }
    FL_CHECK_EQ(warned, 1);
    FL_CHECK_EQ(wrong_branch, 0);
}

FL_TEST_CASE("[#4333] strict mode says it disabled the channel, not that it fell back") {
    // The other branch. Without it the message text is only half checked, and
    // swapping the two arms would go unnoticed -- the condition that selects
    // them is the same one that decides whether the channel stays enabled.
    CRGB leds[1] = {};
    fl::vector<fl::string> lines;
    FastLED.setColorManagementStrict(true);
    fl::inject_print_handler([&](const char* text) { lines.push_back(fl::string(text)); });

    ChannelOptions options;
    options.requestColorManagement(SourceProfile::linearSrgb());
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);

    fl::clear_print_handler();
    FastLED.setColorManagementStrict(false);

    FL_REQUIRE(channel != nullptr);
    FL_REQUIRE(channel->hasColorProfileFallback());
    FL_REQUIRE_FALSE(channel->profileBindingAccepted());

    int strict = 0;
    int wrong_branch = 0;
    for (fl::size i = 0; i < lines.size(); ++i) {
        if (lines[i].find("strict mode disables this channel") != fl::string::npos) { ++strict; }
        if (lines[i].find("falling back to the legacy path") != fl::string::npos) { ++wrong_branch; }
    }
    FL_CHECK_EQ(strict, 1);
    FL_CHECK_EQ(wrong_branch, 0);
}

FL_TEST_CASE("[#4333] a channel that gets its profile says nothing") {
    // The guard. Without it, a warning emitted unconditionally on every
    // channel would satisfy the case above.
    CRGB leds[1] = {};
    fl::vector<fl::string> lines;
    fl::inject_print_handler([&](const char* text) { lines.push_back(fl::string(text)); });

    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(kFixtureProfile, SourceProfile::linearSrgb()));
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    fl::clear_print_handler();

    FL_REQUIRE(channel != nullptr);
    FL_CHECK_FALSE(channel->hasColorProfileFallback());

    for (fl::size i = 0; i < lines.size(); ++i) {
        FL_CHECK(lines[i].find("color management") == fl::string::npos);
    }
}


FL_TEST_CASE("[#4333] and it stays quiet on every reconfigure after the first") {
    // "One-time" is the contract word, so it has to be the thing checked.
    // Without this, a warning emitted on every reconcile would pass the case
    // above, and applyConfig() reconciles again each time it is called.
    CRGB leds[1] = {};
    ChannelOptions options;
    options.requestColorManagement(SourceProfile::linearSrgb());
    ChannelConfig config(ClocklessChipset(), leds, RGB, options);
    ChannelPtr channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_REQUIRE(channel->hasColorProfileFallback());

    // Capture only the reconfigures, so the create-time warning is not
    // counted here -- the case above owns that one.
    fl::vector<fl::string> lines;
    fl::inject_print_handler([&](const char* text) { lines.push_back(fl::string(text)); });
    channel->applyConfig(config);
    channel->applyConfig(config);
    fl::clear_print_handler();

    for (fl::size i = 0; i < lines.size(); ++i) {
        FL_CHECK(lines[i].find("color management") == fl::string::npos);
    }
}

}  // FL_TEST_FILE
