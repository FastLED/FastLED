// Streaming colour-managed pixel source for color pipeline P6 (#4040).

#include "fl/channels/color_managed_source.h"
#include "FastLED.h"
#include "fl/channels/all_drivers.h"
#include "fl/channels/channel.h"
#include "fl/channels/data.h"
#include "fl/channels/dither_frame.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/channels/pipeline_binding.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/gfx/colorimetric_response.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/span.h"
#include "fl/stl/int.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

#if FL_COLOR_PROFILE_RUNTIME
// The pipeline is reached through a pointer, not carried inside every channel.
//
// `StreamingPipelineQ16` is 88 bytes. Held inline as an `fl::optional`, every
// channel on the sketch paid that whether or not it bound a profile, and an
// unbound channel is the ordinary case -- `sizeof(fl::Channel)` measured 576
// against 488 without this feature, so 1.4 KB across a sixteen-channel
// parallel output for something none of those channels asked for. Behind a
// pointer it measures 504.
//
// Asserted on the storage type rather than on `sizeof(fl::Channel)`, which is
// a different number on every target and would pin the whole class's layout
// to whatever this host compiles.
FL_STATIC_ASSERT(sizeof(fl::Channel::ColorPipelineStorage) <= 2 * sizeof(void*),
                 "Channel must reach its colour pipeline through a pointer: "
                 "storing StreamingPipelineQ16 inline charges every channel "
                 "for a pipeline most of them never bind.");
#endif

using namespace fl;

namespace {

EmitterProfile rgbDevice() {
    EmitterProfile p = {};
    p.xy_r[0] = 0.6400f; p.xy_r[1] = 0.3300f;
    p.xy_g[0] = 0.3000f; p.xy_g[1] = 0.6000f;
    p.xy_b[0] = 0.1500f; p.xy_b[1] = 0.0600f;
    p.lum_r = 1.0f; p.lum_g = 1.0f; p.lum_b = 1.0f;
    p.native_code_depth = 8;
    return p;
}

// At namespace scope, not inside the test: a function-local static has no
// linkage, and C++11 will not take one as a template reference argument.
const EmitterProfile kStaticProfile = rgbDevice();

StreamingPipelineQ16 makePipeline() {
    StreamingPipelineQ16 pipeline;
    const bool ok = buildStreamingPipelineQ16(SourceProfile::srgbBt709(),
                                              rgbDevice(),
                                              GamutPolicy::ChromaCompress,
                                              &pipeline);
    FL_REQUIRE(ok);
    return pipeline;
}

/// Captures the bytes a channel hands to its driver.
class ByteCapturingMockEngine : public IChannelDriver {
  public:
    explicit ByteCapturingMockEngine(const char* name = "PIPELINE_CAPTURE")
        : mName(name) {}

    fl::vector<ChannelDataPtr> mCapturedChannels;

    bool canHandle(const ChannelDataPtr& data) const override {
        (void)data;
        return true;
    }
    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
            mCapturedChannels.push_back(channelData);
        }
    }
    void show() override {}
    DriverState poll() override { return DriverState::READY; }
    fl::string getName() const override { return mName; }
    Capabilities getCapabilities() const override {
        return Capabilities(true, true);
    }

  private:
    fl::string mName;
};

/// The smallest concrete `CLEDController`, so `bindStaticEmitterProfile`
/// can be called on a real one.
class StubController : public CLEDController {
  public:
    StubController(CRGB* leds, int count) { setLeds(leds, count); }
    void showColor(const CRGB&, int, fl::u8) FL_NO_EXCEPT override {}
    void show(const CRGB*, int, fl::u8) FL_NO_EXCEPT override {}
    void init() FL_NO_EXCEPT override {}
};

}  // namespace

FL_TEST_CASE("The managed source emits one byte triple per pixel, in order") {
    // Colour order is applied to the outputs rather than by instantiating a
    // controller per order, so it needs checking directly: the same pixel
    // through GRB must be the RGB answer with the first two swapped.
    CRGB leds[2] = {CRGB(200, 120, 60), CRGB(10, 240, 90)};
    const StreamingPipelineQ16 pipeline = makePipeline();

    u8 rgb[2][3];
    u8 grb[2][3];
    {
        PixelController<RGB> controller(leds, 2, ColorAdjustment(), DISABLE_DITHER);
        ColorManagedPixelSource source(controller, RGB, pipeline);
        for (int i = 0; i < 2; ++i) {
            FL_REQUIRE(source.has(1));
            source.loadAndScaleRGB(&rgb[i][0], &rgb[i][1], &rgb[i][2]);
            source.advanceData();
        }
    }
    {
        PixelController<RGB> controller(leds, 2, ColorAdjustment(), DISABLE_DITHER);
        ColorManagedPixelSource source(controller, GRB, pipeline);
        for (int i = 0; i < 2; ++i) {
            source.loadAndScaleRGB(&grb[i][0], &grb[i][1], &grb[i][2]);
            source.advanceData();
        }
    }
    for (int i = 0; i < 2; ++i) {
        FL_CHECK_EQ(grb[i][0], rgb[i][1]);
        FL_CHECK_EQ(grb[i][1], rgb[i][0]);
        FL_CHECK_EQ(grb[i][2], rgb[i][2]);
    }
}

FL_TEST_CASE("Brightness is applied once, by the pipeline") {
    // The trap this class exists to avoid. `loadAndScale0/1/2` fold in
    // `mColorAdjustment`, whose premixed value carries brightness, and the
    // pipeline carries brightness too as C4's flux scalar. Reading through
    // the scaled accessors would apply it twice, which would show up as a
    // frame roughly a quarter as bright at half brightness.
    //
    // So: a controller told brightness 128 must produce the *same* bytes as
    // one told 255, because the source reads raw and the pipeline here is at
    // unity. Halving is then done by the flux scalar, and only there.
    CRGB leds[1] = {CRGB(200, 120, 60)};
    const StreamingPipelineQ16 unity = makePipeline();

    // `color` and `brightness` only exist under FASTLED_HD_COLOR_MIXING;
    // `premixed` is the one field that is always there, and it is the one
    // this test is about -- it is what the legacy scale would have folded in.
    ColorAdjustment dim;
    dim.premixed = CRGB(128, 128, 128);
#if FASTLED_HD_COLOR_MIXING
    dim.color = CRGB(0xff, 0xff, 0xff);
    dim.brightness = 128;
#endif

    u8 bright_bytes[3];
    u8 dim_bytes[3];
    {
        PixelController<RGB> controller(leds, 1, ColorAdjustment(), DISABLE_DITHER);
        ColorManagedPixelSource source(controller, RGB, unity);
        source.loadAndScaleRGB(&bright_bytes[0], &bright_bytes[1], &bright_bytes[2]);
    }
    {
        PixelController<RGB> controller(leds, 1, dim, DISABLE_DITHER);
        ColorManagedPixelSource source(controller, RGB, unity);
        source.loadAndScaleRGB(&dim_bytes[0], &dim_bytes[1], &dim_bytes[2]);
    }
    for (int i = 0; i < 3; ++i) {
        FL_CHECK_EQ(bright_bytes[i], dim_bytes[i]);
    }

    // And the flux scalar is what actually dims. Half brightness must land
    // near half the drive -- not a quarter, which is what double application
    // would give.
    StreamingPipelineQ16 halved = makePipeline();
    setPipelineFluxQ16(&halved, FluxScalar::fromBrightness(128));
    u8 halved_bytes[3];
    {
        PixelController<RGB> controller(leds, 1, ColorAdjustment(), DISABLE_DITHER);
        ColorManagedPixelSource source(controller, RGB, halved);
        source.loadAndScaleRGB(&halved_bytes[0], &halved_bytes[1], &halved_bytes[2]);
    }
    for (int i = 0; i < 3; ++i) {
        if (bright_bytes[i] < 8) {
            continue;  // too small for the ratio to mean anything
        }
        const float ratio = static_cast<float>(halved_bytes[i]) /
                            static_cast<float>(bright_bytes[i]);
        FL_CHECK_GT(ratio, 0.44f);
        FL_CHECK_LT(ratio, 0.56f);
    }
}

FL_TEST_CASE("Black and white land on the ends of the byte range") {
    CRGB leds[2] = {CRGB(0, 0, 0), CRGB(255, 255, 255)};
    const StreamingPipelineQ16 pipeline = makePipeline();
    PixelController<RGB> controller(leds, 2, ColorAdjustment(), DISABLE_DITHER);
    ColorManagedPixelSource source(controller, RGB, pipeline);

    u8 black[3];
    source.loadAndScaleRGB(&black[0], &black[1], &black[2]);
    for (int i = 0; i < 3; ++i) {
        FL_CHECK_EQ(black[i], 0);
    }
    source.advanceData();

    // Full white on this device is the D65 neutral, which needs unequal
    // drives -- the emitters are normalized to unit luminance each. So this
    // is not three 255s, and asserting that it were would have been wrong
    // about the profile rather than about the code.
    u8 white[3];
    source.loadAndScaleRGB(&white[0], &white[1], &white[2]);
    int lit = 0;
    for (int i = 0; i < 3; ++i) {
        if (white[i] > 0) {
            ++lit;
        }
    }
    FL_CHECK_EQ(lit, 3);
    FL_CHECK_GT(white[1], white[0]);  // green carries most of D65's luminance
    FL_CHECK_GT(white[0], white[2]);
}

FL_TEST_CASE("The source reports the controller's extent") {
    CRGB leds[4] = {};
    const StreamingPipelineQ16 pipeline = makePipeline();
    PixelController<RGB> controller(leds, 4, ColorAdjustment(), DISABLE_DITHER);
    ColorManagedPixelSource source(controller, RGB, pipeline);
    FL_CHECK_EQ(source.size(), 4);
    FL_CHECK(source.has(4));
    FL_CHECK_FALSE(source.has(5));
}

#if FL_COLOR_PROFILE_RUNTIME
FL_TEST_CASE("A bound colour profile reaches the encoded bytes") {
    // The wiring. `setColorProfile` has been settable and inert: the binding
    // was validated and its fallback tracked, and no pixel ever went through
    // it. This says it now does.
    //
    // The assertion is on the *exact* bytes the pipeline produces, not on
    // the two channels merely differing. A first version compared them and
    // passed with the pipeline forced off -- binding a profile also disables
    // dithering and clears legacy correction, so the bytes move for reasons
    // that have nothing to do with this change.
    const int NUM_LEDS = 2;
    CRGB plain_leds[NUM_LEDS] = {CRGB(200, 40, 10), CRGB(10, 180, 90)};
    CRGB managed_leds[NUM_LEDS] = {CRGB(200, 40, 10), CRGB(10, 180, 90)};
    const EmitterProfile device = rgbDevice();

    auto mockEngine = fl::make_shared<ByteCapturingMockEngine>();
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2010, mockEngine);
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();

    ChannelOptions plain_options;
    ChannelConfig plain_config(1, timing, fl::span<CRGB>(plain_leds, NUM_LEDS),
                               RGB, plain_options);
    auto plain = Channel::create(plain_config);
    FL_REQUIRE(plain != nullptr);

    ChannelOptions managed_options;
    // A source wider than the device, so the mapper has real work to do.
    FL_REQUIRE(managed_options.setColorProfile(device, SourceProfile::bt2020(),
                                               GamutPolicy::ChromaCompress));
    ChannelConfig managed_config(2, timing,
                                 fl::span<CRGB>(managed_leds, NUM_LEDS), RGB,
                                 managed_options);
    auto managed = Channel::create(managed_config);
    FL_REQUIRE(managed != nullptr);

    auto cleanup = fl::make_scope_exit([&]() {
        plain->removeFromDrawList();
        managed->removeFromDrawList();
        manager.removeDriver(mockEngine);
    });

    FastLED.add(plain);
    FastLED.add(managed);
    mockEngine->mCapturedChannels.clear();
    FastLED.show();

    FL_REQUIRE(mockEngine->mCapturedChannels.size() >= 2);
    const auto& first = mockEngine->mCapturedChannels[0]->getData();
    const auto& second = mockEngine->mCapturedChannels[1]->getData();
    FL_REQUIRE(first.size() >= static_cast<fl::size>(NUM_LEDS * 3));
    FL_REQUIRE(second.size() >= static_cast<fl::size>(NUM_LEDS * 3));

    // The unbound channel is the pass-through, byte for byte.
    FL_CHECK_EQ(first[0], 200);
    FL_CHECK_EQ(first[1], 40);
    FL_CHECK_EQ(first[2], 10);

    // The bound one carries exactly what the pipeline produces.
    StreamingPipelineQ16 expected;
    FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::bt2020(), device,
                                         GamutPolicy::ChromaCompress, &expected));
    setPipelineFluxQ16(&expected, FluxScalar::fromBrightness(255));
    for (int led = 0; led < NUM_LEDS; ++led) {
        const CRGB& source_pixel = managed_leds[led];
        i32 drives[3];
        processPixelQ16(expected, source_pixel.r, source_pixel.g,
                        source_pixel.b, drives);
        for (int channel_index = 0; channel_index < 3; ++channel_index) {
            const i32 drive = drives[channel_index];
            const int want =
                drive <= 0 ? 0
                           : (drive >= 65536
                                  ? 255
                                  : static_cast<int>((drive * 255 + 32768) >> 16));
            FL_CHECK_EQ(static_cast<int>(second[led * 3 + channel_index]), want);
        }
    }

    // And it really transformed, so this cannot be passing on an identity.
    int difference = 0;
    for (int i = 0; i < NUM_LEDS * 3; ++i) {
        const int delta = static_cast<int>(first[i]) - static_cast<int>(second[i]);
        difference += delta < 0 ? -delta : delta;
    }
    FL_CHECK_GT(difference, 16);
}

FL_TEST_CASE("two channels keep their own profiles") {
    // FastLED#4156 R9 asks how "distinct runtime profiles remain associated
    // with multiple channels", and names two channels as required evidence.
    // The case above compares a bound channel against an unbound one, which
    // cannot see a profile leaking from one binding into another.
    //
    // Two bindings that differ only in the device, driven in the same
    // `show()`, and each checked against the pipeline built from *its own*
    // device.
    const int NUM_LEDS = 2;
    CRGB wide_leds[NUM_LEDS] = {CRGB(200, 40, 10), CRGB(10, 180, 90)};
    CRGB narrow_leds[NUM_LEDS] = {CRGB(200, 40, 10), CRGB(10, 180, 90)};

    // Same primaries, different blue. Enough to move the solve without
    // changing anything else about the two bindings.
    EmitterProfile wide_device = rgbDevice();
    EmitterProfile narrow_device = rgbDevice();
    narrow_device.xy_b[0] = 0.2200f;
    narrow_device.xy_b[1] = 0.1600f;

    auto mockEngine = fl::make_shared<ByteCapturingMockEngine>();
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2011, mockEngine);
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();

    ChannelOptions wide_options;
    FL_REQUIRE(wide_options.setColorProfile(wide_device, SourceProfile::bt2020(),
                                            GamutPolicy::ChromaCompress));
    ChannelConfig wide_config(3, timing, fl::span<CRGB>(wide_leds, NUM_LEDS), RGB,
                              wide_options);
    auto wide = Channel::create(wide_config);
    FL_REQUIRE(wide != nullptr);

    ChannelOptions narrow_options;
    FL_REQUIRE(narrow_options.setColorProfile(narrow_device,
                                              SourceProfile::bt2020(),
                                              GamutPolicy::ChromaCompress));
    ChannelConfig narrow_config(4, timing,
                                fl::span<CRGB>(narrow_leds, NUM_LEDS), RGB,
                                narrow_options);
    auto narrow = Channel::create(narrow_config);
    FL_REQUIRE(narrow != nullptr);

    auto cleanup = fl::make_scope_exit([&]() {
        wide->removeFromDrawList();
        narrow->removeFromDrawList();
        manager.removeDriver(mockEngine);
    });

    FastLED.add(wide);
    FastLED.add(narrow);
    mockEngine->mCapturedChannels.clear();
    FastLED.show();

    FL_REQUIRE(mockEngine->mCapturedChannels.size() >= 2);
    const auto& wide_bytes = mockEngine->mCapturedChannels[0]->getData();
    const auto& narrow_bytes = mockEngine->mCapturedChannels[1]->getData();
    FL_REQUIRE(wide_bytes.size() >= static_cast<fl::size>(NUM_LEDS * 3));
    FL_REQUIRE(narrow_bytes.size() >= static_cast<fl::size>(NUM_LEDS * 3));

    // Each against a pipeline built from its own device. A profile leaking
    // between bindings shows up here as one channel matching the other's
    // expectation.
    const EmitterProfile devices[2] = {wide_device, narrow_device};
    const fl::u8* captured[2] = {wide_bytes.data(), narrow_bytes.data()};
    for (int which = 0; which < 2; ++which) {
        StreamingPipelineQ16 expected;
        FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::bt2020(),
                                             devices[which],
                                             GamutPolicy::ChromaCompress,
                                             &expected));
        setPipelineFluxQ16(&expected, FluxScalar::fromBrightness(255));
        for (int led = 0; led < NUM_LEDS; ++led) {
            const CRGB& source_pixel = wide_leds[led];
            i32 drives[3];
            processPixelQ16(expected, source_pixel.r, source_pixel.g,
                            source_pixel.b, drives);
            for (int index = 0; index < 3; ++index) {
                const i32 drive = drives[index];
                const int want =
                    drive <= 0 ? 0
                               : (drive >= 65536
                                      ? 255
                                      : static_cast<int>((drive * 255 + 32768) >> 16));
                FL_CHECK_EQ(static_cast<int>(captured[which][led * 3 + index]),
                            want);
            }
        }
    }

    // Vacuity guard, and the reason the two devices differ at all: if the
    // profiles produced the same bytes, every check above would hold with
    // both channels sharing one binding.
    int difference = 0;
    for (int i = 0; i < NUM_LEDS * 3; ++i) {
        const int delta =
            static_cast<int>(wide_bytes[i]) - static_cast<int>(narrow_bytes[i]);
        difference += delta < 0 ? -delta : delta;
    }
    FL_CHECK_GT(difference, 8);
}
#endif

#if FL_COLOR_PROFILE_RUNTIME
FL_TEST_CASE("Every static binding path installs the pipeline seam") {
    // Regression. Four call sites bind a colour profile and only one of them
    // is `setColorProfile`: `Channel::create<Profile>`,
    // `ChannelOptions::withColorProfile<Profile>` and
    // `CLEDController::bindStaticEmitterProfile` set `mStaticProfile`
    // directly. The seam that keeps the pipeline linker-elidable installs
    // its hooks from the binding call, so a path that forgets to install
    // leaves that channel silently on the legacy path -- the exact
    // "settable but inert" bug this change exists to fix, reintroduced by
    // the fix.
    //
    // The hooks are one process-wide struct, so a sibling test that binds a
    // profile would install them and make a naive assertion here vacuous.
    // Each leg therefore clears them first; that is what makes this a test
    // of the binding path rather than of the test order.
    const ColorPipelineHooks kCleared = {nullptr, nullptr, nullptr, nullptr};
    auto restore = fl::make_scope_exit([]() { installColorPipelineHooks(); });

    {
        colorPipelineHooks() = kCleared;
        ChannelOptions options =
            ChannelOptions::withColorProfile<kStaticProfile>();
        FL_REQUIRE(options.hasColorProfile());
        // `REQUIRE`, not `CHECK`: the pipeline build below dereferences it.
        FL_REQUIRE(colorPipelineHooks().build != nullptr);
        FL_CHECK(colorPipelineHooks().makeIterator != nullptr);
        FL_CHECK(colorPipelineHooks().destroyIterator != nullptr);
        FL_CHECK(colorPipelineHooks().setFlux != nullptr);

        // And the binding really yields a pipeline, so the hooks being
        // installed is not the whole of the claim.
        StreamingPipelineQ16 pipeline;
        FL_CHECK(colorPipelineHooks().build(options.mColorProfile, &pipeline));
    }

    {
        colorPipelineHooks() = kCleared;
        auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
        CRGB leds[2] = {};
        ChannelOptions options;
        ChannelConfig config(2101, timing, fl::span<CRGB>(leds, 2), RGB,
                             options);
        auto channel = Channel::create<kStaticProfile>(config);
        FL_REQUIRE(channel != nullptr);
        auto cleanup =
            fl::make_scope_exit([&]() { channel->removeFromDrawList(); });
        FL_CHECK(colorPipelineHooks().build != nullptr);
        FL_CHECK(colorPipelineHooks().makeIterator != nullptr);
    }

    {
        colorPipelineHooks() = kCleared;
        CRGB leds[2] = {};
        StubController controller(leds, 2);
        controller.bindStaticEmitterProfile(&kStaticProfile);
        FL_REQUIRE(controller.emitterProfile() == &kStaticProfile);
        FL_CHECK(colorPipelineHooks().build != nullptr);
        FL_CHECK(colorPipelineHooks().makeIterator != nullptr);
    }

    // Unbinding must not install: a null profile means the legacy path, and
    // paying for the pipeline there is the regression the seam prevents.
    {
        colorPipelineHooks() = kCleared;
        CRGB leds[2] = {};
        StubController controller(leds, 2);
        controller.bindStaticEmitterProfile(nullptr);
        FL_CHECK(colorPipelineHooks().build == nullptr);
    }
}
#endif

// C5: "Pipeline dithering and `BINARY_DITHER` are mutually exclusive per
// channel." `ChannelOptions::setColorProfile` enforces half of that by setting
// DISABLE_DITHER, but `mDitherMode` is a public field rather than a setter --
// unlike `setLegacyCorrection`, which clears the profile and warns -- so a
// caller can re-enable it after binding and reach the forbidden combination
// through the type system.
//
// What makes that harmless is structural, and this file's own header says why:
// the managed source reads `mController.mData` rather than `loadAndScale0/1/2`
// because those fold in `mColorAdjustment`, whose `premixed` carries
// brightness the pipeline also carries. Legacy dithering is applied inside
// those same calls, so bypassing them for the brightness reason excludes the
// dither as a consequence.
//
// Tested here rather than through `Channel::showLeds`, where the controller's
// dither state sits several layers away. An attempt at that level could not be
// made to fail under mutation: it could not distinguish "the managed path
// ignores dither" from "dither was never armed in the harness".

FL_TEST_CASE("[#4042] C5: legacy dithering cannot reach the managed source") {
    CRGB leds[1] = {CRGB(200, 40, 9)};
    const StreamingPipelineQ16 pipeline = makePipeline();

    // A controller that *is* dithering: BINARY_DITHER, with a premixed scale
    // low enough that the offsets are large -- `e[i]` is 256/s + 1, so a
    // quarter-scale strip carries five codes of offset rather than two.
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(16, 16, 16);

    fl::vector<int> managed_seen;
    fl::vector<int> legacy_seen;
    for (int frame = 0; frame < 8; ++frame) {
        fl::detail::advanceDitherFrame();

        PixelController<RGB> managed_ctrl(leds, 1, adjustment, BINARY_DITHER);
        ColorManagedPixelSource source(managed_ctrl, RGB, pipeline);
        // The 16-bit entry point, not the 8-bit one. Both read the raw pixel,
        // but the 8-bit output quantizes a dither-scale input change away --
        // measured: feeding the dithered value through it produces the same
        // byte every frame, so a test built on it cannot fail when the read
        // is wrong. The wide output has 256x the resolution and does move.
        u16 m0, m1, m2;
        source.loadAndScaleRGB16(&m0, &m1, &m2);

        PixelController<RGB> legacy_ctrl(leds, 1, adjustment, BINARY_DITHER);
        const u8 l0 = legacy_ctrl.loadAndScale0();

        bool seen_m = false;
        for (fl::size j = 0; j < managed_seen.size(); ++j) {
            if (managed_seen[j] == static_cast<int>(m0)) { seen_m = true; }
        }
        if (!seen_m) { managed_seen.push_back(static_cast<int>(m0)); }

        bool seen_l = false;
        for (fl::size j = 0; j < legacy_seen.size(); ++j) {
            if (legacy_seen[j] == static_cast<int>(l0)) { seen_l = true; }
        }
        if (!seen_l) { legacy_seen.push_back(static_cast<int>(l0)); }
    }

    // The legacy controller varies across the cycle. That is dithering
    // working, and it is what stops the assertion below being a statement
    // about a harness that never dithers anything.
    FL_CHECK_GT((int)legacy_seen.size(), 1);

    // The managed source does not. Same settings, same phases, one value: the
    // raw read has no path to the offsets.
    FL_CHECK_EQ((int)managed_seen.size(), 1);
}

}  // FL_TEST_FILE
