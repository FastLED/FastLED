// Streaming colour-managed pixel source for color pipeline P6 (#4040).

#include "fl/channels/color_managed_source.h"
#include "FastLED.h"
#include "fl/channels/all_drivers.h"
#include "fl/channels/channel.h"
#include "fl/channels/data.h"
#include "fl/channels/dither_frame.h"
#include "fl/channels/five_bit_semantics.h"
#include "fl/chipsets/spi.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/channels/pipeline_binding.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/gfx/colorimetric_response.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/math/math.h"
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
    /// When set, `poll()` reports BUSY: a driver still transmitting.
    bool mBusy = false;

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
    DriverState poll() override {
        return mBusy ? DriverState::BUSY : DriverState::READY;
    }
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

#if FASTLED_HD_COLOR_MIXING
FL_TEST_CASE("[#4042] B1: a managed HD strip does not dim twice through the 5-bit field") {
    // APA102-HD and SK9822-HD take a colour triple from the RGB range and a
    // per-strip brightness from `loadRGBScaleAndBrightness`, mapped to the
    // 5-bit field. On a managed channel the triple is the pipeline's drives,
    // which already carry brightness as C4's flux scalar -- so reporting the
    // controller's brightness here as well dimmed the strip twice: a quarter
    // of the light at half brightness, a sixteenth at a quarter.
    //
    // B1's conservative treatment holds the field fixed (SK9822, and unknown
    // chips); the pipeline is the only amplitude stage. So the managed source
    // reports full scale whatever the controller was told.
    CRGB leds[1] = {CRGB(200, 120, 60)};
    const StreamingPipelineQ16 pipeline = makePipeline();

    ColorAdjustment dim;
    dim.premixed = CRGB(64, 64, 64);
    dim.color = CRGB(0xff, 0xc0, 0x80);
    dim.brightness = 64;

    PixelController<RGB> controller(leds, 1, dim, DISABLE_DITHER);
    ColorManagedPixelSource source(controller, RGB, pipeline);
    u8 c0 = 0, c1 = 0, c2 = 0, brightness = 0;
    source.loadRGBScaleAndBrightness(&c0, &c1, &c2, &brightness);
    FL_CHECK_EQ(brightness, 255);
    // And no colour scale either: legacy correction is cleared on binding,
    // and a scale here would be a second, per-channel amplitude stage.
    FL_CHECK_EQ(c0, 255);
    FL_CHECK_EQ(c1, 255);
    FL_CHECK_EQ(c2, 255);

    // Control: the legacy controller does report the dimming, so the checks
    // above are about the managed source, not a harness that never dims.
    u8 l0 = 0, l1 = 0, l2 = 0, legacy_brightness = 0;
    controller.loadRGBScaleAndBrightness(&l0, &l1, &l2, &legacy_brightness);
    FL_CHECK_EQ(legacy_brightness, 64);
}
#endif

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
    const ColorPipelineHooks kCleared = {};
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

namespace {

/// The exact 8-bit code a drive asks for, as a real number.
double exactCode(i32 drive) {
    if (drive <= 0) { return 0.0; }
    if (drive >= 65536) { return 255.0; }
    return static_cast<double>(drive) * 255.0 / 65536.0;
}

/// One frame of a managed source over `leds`, at the current dither phase.
void managedFrame(CRGB* leds, int count, const StreamingPipelineQ16& pipeline,
                  EDitherMode mode, u8 premixed, fl::vector<CRGB>* out) {
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(premixed, premixed, premixed);
    PixelController<RGB> controller(leds, count, adjustment, mode);
    ColorManagedPixelSource source(controller, RGB, pipeline);
    out->clear();
    while (source.has(1)) {
        CRGB c;
        source.loadAndScaleRGB(&c.r, &c.g, &c.b);
        out->push_back(c);
        source.advanceData();
    }
}

}  // namespace

FL_TEST_CASE("[#4042] C5: BINARY_DITHER on a managed channel is a temporal dither on the drive") {
    // Effective step: over the eight-frame cycle, the mean code tracks the
    // exact code the drive asks for within 1/16 of a code, where rounding is
    // off by up to half a code. And each frame emits floor or floor + 1 of
    // the exact code -- one code of amplitude, never the legacy offsets.
    const StreamingPipelineQ16 pipeline = makePipeline();
    double worst_dithered = 0.0;
    double worst_rounded = 0.0;
    for (int src = 1; src < 256; src += 3) {
        CRGB led[1] = {CRGB(static_cast<u8>(src), static_cast<u8>(src / 2),
                            static_cast<u8>(255 - src))};
        i32 drives[3];
        processPixelQ16(pipeline, led[0].r, led[0].g, led[0].b, drives);

        double sum[3] = {0.0, 0.0, 0.0};
        fl::vector<CRGB> frame;
        for (int f = 0; f < 8; ++f) {
            fl::detail::advanceDitherFrame();
            managedFrame(led, 1, pipeline, BINARY_DITHER, 255, &frame);
            for (int i = 0; i < 3; ++i) {
                const double exact = exactCode(drives[i]);
                const int code = frame[0].raw[i];
                FL_CHECK_GE(code, static_cast<int>(exact));
                FL_CHECK_LE(code, static_cast<int>(exact) + 1);
                sum[i] += code;
            }
        }
        fl::vector<CRGB> plain;
        managedFrame(led, 1, pipeline, DISABLE_DITHER, 255, &plain);
        for (int i = 0; i < 3; ++i) {
            const double exact = exactCode(drives[i]);
            const double dithered_err = fl::fabs(sum[i] / 8.0 - exact);
            const double rounded_err = fl::fabs(plain[0].raw[i] - exact);
            if (dithered_err > worst_dithered) { worst_dithered = dithered_err; }
            if (rounded_err > worst_rounded) { worst_rounded = rounded_err; }
        }
    }
    FL_CHECK_LE(worst_dithered, 1.0 / 16.0 + 1e-9);
    // The control: without dither the same drives miss by up to half a code,
    // so the bound above is the dither's doing.
    FL_CHECK_GT(worst_rounded, 0.4);
}

FL_TEST_CASE("[#4042] C5: the temporal dither does not pulse a uniform strip") {
    // Flicker. Eight identical pixels take eight different phases in every
    // frame, so the strip's total light is the same in each frame of the
    // cycle; only the position of the extra codes moves.
    const StreamingPipelineQ16 pipeline = makePipeline();
    CRGB leds[8];
    for (int i = 0; i < 8; ++i) { leds[i] = CRGB(37, 90, 5); }
    int first_total = -1;
    bool any_pixel_toggled = false;
    fl::vector<CRGB> frame;
    fl::vector<CRGB> previous;
    for (int f = 0; f < 8; ++f) {
        fl::detail::advanceDitherFrame();
        managedFrame(leds, 8, pipeline, BINARY_DITHER, 255, &frame);
        int total = 0;
        for (int p = 0; p < 8; ++p) {
            total += frame[p].r + frame[p].g + frame[p].b;
            if (!previous.empty() && !(frame[p] == previous[p])) {
                any_pixel_toggled = true;
            }
        }
        if (first_total < 0) { first_total = total; }
        FL_CHECK_EQ(total, first_total);
        previous = frame;
    }
    // Not vacuous: individual pixels do change across the cycle.
    FL_CHECK(any_pixel_toggled);
}

FL_TEST_CASE("[#4042] C5: a managed channel without BINARY_DITHER rounds, every frame") {
    const StreamingPipelineQ16 pipeline = makePipeline();
    CRGB led[1] = {CRGB(37, 90, 5)};
    fl::vector<CRGB> first;
    managedFrame(led, 1, pipeline, DISABLE_DITHER, 255, &first);
    fl::vector<CRGB> frame;
    for (int f = 0; f < 8; ++f) {
        fl::detail::advanceDitherFrame();
        managedFrame(led, 1, pipeline, DISABLE_DITHER, 255, &frame);
        FL_CHECK(frame[0] == first[0]);
    }
}

FL_TEST_CASE("[#4042] B1: the per-chip 5-bit semantics table, and the profile override") {
    // APA102's field is a slow PWM; SK9822's is a current gain; HD107's is
    // undocumented, so it gets the SK9822-conservative treatment. Non-HD
    // chips have no field solve at all.
    FL_CHECK(fiveBitSemanticsFor(SpiChipset::APA102HD, FiveBitSemantics::NotApplicable) ==
             FiveBitSemantics::SecondarySlowPwm);
    FL_CHECK(fiveBitSemanticsFor(SpiChipset::DOTSTARHD, FiveBitSemantics::NotApplicable) ==
             FiveBitSemantics::SecondarySlowPwm);
    FL_CHECK(fiveBitSemanticsFor(SpiChipset::SK9822HD, FiveBitSemantics::NotApplicable) ==
             FiveBitSemantics::CurrentGain);
    FL_CHECK(fiveBitSemanticsFor(SpiChipset::HD107HD, FiveBitSemantics::NotApplicable) ==
             FiveBitSemantics::Unknown);
    FL_CHECK(fiveBitSemanticsFor(SpiChipset::APA102, FiveBitSemantics::SecondarySlowPwm) ==
             FiveBitSemantics::NotApplicable);
    // A profile that characterises the field overrides the chip default.
    FL_CHECK(fiveBitSemanticsFor(SpiChipset::SK9822HD, FiveBitSemantics::SecondarySlowPwm) ==
             FiveBitSemantics::SecondarySlowPwm);

    // Only a slow-PWM field may go below 31, and only as far as the floor.
    FL_CHECK_EQ(hdMinimumField(FiveBitSemantics::SecondarySlowPwm, 4), 4);
    FL_CHECK_EQ(hdMinimumField(FiveBitSemantics::SecondarySlowPwm, 0), 1);
    FL_CHECK_EQ(hdMinimumField(FiveBitSemantics::CurrentGain, 4), 31);
    FL_CHECK_EQ(hdMinimumField(FiveBitSemantics::Unknown, 4), 31);
}

FL_TEST_CASE("[#4042] B1: setHdFieldFloor defaults to a fixed field and clamps") {
    FL_CHECK_EQ(FastLED.getHdFieldFloor(), 31);
    FastLED.setHdFieldFloor(0);
    FL_CHECK_EQ(FastLED.getHdFieldFloor(), 1);
    FastLED.setHdFieldFloor(200);
    FL_CHECK_EQ(FastLED.getHdFieldFloor(), 31);
}

namespace {

/// Encodes one frame of a managed HD SPI channel and returns the bytes.
fl::vector<u8> managedHdFrame(SpiEncoder encoder, CRGB pixel, u8 floor) {
    FastLED.setHdFieldFloor(floor);
    auto engine = fl::make_shared<ByteCapturingMockEngine>("HD_CAPTURE");
    ChannelManager::instance().addDriver(2030, engine);
    CRGB leds[1] = {pixel};
    ChannelOptions options;
    FL_REQUIRE(options.setColorProfile(rgbDevice()));
    auto channel = Channel::create(ChannelConfig(
        SpiChipsetConfig{5, 6, encoder}, fl::span<CRGB>(leds, 1), RGB, options));
    FL_REQUIRE(channel != nullptr);
    FL_REQUIRE(channel->isColorManaged());
    channel->showLeds(255);
    fl::vector<u8> bytes;
    if (!engine->mCapturedChannels.empty()) {
        const auto& data = engine->mCapturedChannels.back()->getData();
        bytes.assign(data.begin(), data.end());
    }
    ChannelManager::instance().removeDriver(engine);
    FastLED.setHdFieldFloor(31);
    return bytes;
}

long hdLight(const fl::vector<u8>& frame, int channel) {
    // Start frame is 4 bytes; then [0xE0|field][c0][c1][c2].
    return static_cast<long>(frame[4] & 0x1F) * frame[5 + channel];
}

}  // namespace

FL_TEST_CASE("[#4042] B1: a managed APA102-HD strip uses its field below the floor") {
    // A dim pixel. Field pinned (floor 31, the default) spends the whole
    // range on a handful of codes; with the floor at 1 the joint solve picks
    // a small field and keeps the code's resolution -- the same light, more
    // precisely.
    const CRGB dim(6, 3, 2);
    const fl::vector<u8> pinned = managedHdFrame(SpiEncoder::apa102HD(), dim, 31);
    const fl::vector<u8> free_field = managedHdFrame(SpiEncoder::apa102HD(), dim, 1);
    FL_REQUIRE_GE(pinned.size(), 8u);
    FL_REQUIRE_GE(free_field.size(), 8u);
    FL_CHECK_EQ(pinned[4], 0xFF);                 // field held at 31
    FL_CHECK_LT(int(free_field[4] & 0x1F), 31);   // field lowered
    for (int c = 0; c < 3; ++c) {
        FL_CHECK_GE(int(free_field[5 + c]), int(pinned[5 + c]));
        // Same light to within half a code at the pinned field.
        const long diff = hdLight(free_field, c) - hdLight(pinned, c);
        FL_CHECK_LE(diff < 0 ? -diff : diff, 16);
    }
}

FL_TEST_CASE("[#4042] B1: SK9822-HD and HD107-HD keep the field fixed whatever the floor") {
    const CRGB dim(6, 3, 2);
    const fl::vector<u8> sk = managedHdFrame(SpiEncoder::sk9822HD(), dim, 1);
    const fl::vector<u8> hd107 = managedHdFrame(SpiEncoder::hd107HD(), dim, 1);
    FL_REQUIRE_GE(sk.size(), 8u);
    FL_REQUIRE_GE(hd107.size(), 8u);
    FL_CHECK_EQ(sk[4], 0xFF);
    FL_CHECK_EQ(hd107[4], 0xFF);
}

namespace {

/// A managed channel on a capture driver whose frames can be dropped by
/// disabling that driver -- the drop path `showPixels` actually takes.
struct DroppableChannel {
    CRGB leds[1];
    fl::shared_ptr<ByteCapturingMockEngine> engine;
    ChannelPtr channel;
    const char* name;

    DroppableChannel(const char* driver_name, CRGB source, bool managed)
        : name(driver_name) {
        leds[0] = source;
        engine = fl::make_shared<ByteCapturingMockEngine>(driver_name);
        ChannelManager::instance().addDriver(2020, engine);
        ChannelOptions options;
        if (managed) {
            FL_REQUIRE(options.setColorProfile(rgbDevice()));
        }
        options.mDitherMode = BINARY_DITHER;
        auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
        channel = Channel::create(ChannelConfig(
            7, timing, fl::span<CRGB>(leds, 1), RGB, options));
        FL_REQUIRE(channel != nullptr);
    }
    ~DroppableChannel() {
        ChannelManager::instance().setDriverEnabled(name, true);
        ChannelManager::instance().removeDriver(engine);
    }
    /// One attempt at a frame. A dropped one has no enabled driver at all --
    /// every registered driver is disabled for it and restored afterwards --
    /// so nothing else in the test binary can take the frame instead.
    /// Returns true if the capture driver accepted it.
    bool attempt(bool drop) {
        ChannelManager& manager = ChannelManager::instance();
        fl::vector<fl::string> reenable;
        if (drop) {
            for (const DriverInfo& info : manager.getDriverInfos()) {
                if (info.enabled) {
                    reenable.push_back(info.name);
                }
            }
            for (const fl::string& n : reenable) {
                manager.setDriverEnabled(n.c_str(), false);
            }
        }
        const fl::size before = engine->mCapturedChannels.size();
        channel->showLeds(255);
        for (const fl::string& n : reenable) {
            manager.setDriverEnabled(n.c_str(), true);
        }
        return engine->mCapturedChannels.size() > before;
    }
    u8 lastRed() const {
        return engine->mCapturedChannels.back()->getData()[0];
    }
};

}  // namespace

FL_TEST_CASE("[#4347] reseeding at a phase matches constructing at it") {
    // The channel re-points an already-built controller at its own phase.
    // That must land exactly where the constructor would have put it for the
    // same phase, or the per-channel phase would change what legacy dither
    // emits rather than only when it advances.
    const u8 kScales[] = {0, 1, 2, 16, 51, 128, 200, 255};
    CRGB led[1] = {CRGB(10, 20, 30)};
    const u8 saved = fl::detail::gDitherFrame;
    for (int r = 0; r < 256; ++r) {
        for (u8 s0 : kScales) {
            ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
            adjustment.premixed = CRGB(s0, static_cast<u8>(255 - s0), 64);

            fl::detail::gDitherFrame = static_cast<u8>(r);
            PixelController<RGB> constructed(led, 1, adjustment, BINARY_DITHER);

            fl::detail::gDitherFrame = static_cast<u8>(r + 3);  // some other phase
            PixelController<RGB> reseeded(led, 1, adjustment, BINARY_DITHER);
            reseeded.reseed_binary_dithering(static_cast<u8>(r));

            for (int i = 0; i < 3; ++i) {
                FL_CHECK_EQ(reseeded.d[i], constructed.d[i]);
                FL_CHECK_EQ(reseeded.e[i], constructed.e[i]);
            }
        }
    }
    fl::detail::gDitherFrame = saved;
}

FL_TEST_CASE("[#4347] a dropped submission does not consume the dither phase") {
    // R8: "State advances on presentation ... with explicit tests for
    // dropped submissions and irregular dwell." The shared counter moves on
    // every attempt; the channel's own phase moves only when its driver
    // accepts the frame.
    DroppableChannel strip("DITHER_DROP", CRGB(37, 90, 5), false);
    const u8 start = strip.channel->ditherPhase();

    FL_REQUIRE(strip.attempt(false));
    FL_CHECK_EQ(strip.channel->ditherPhase(), static_cast<u8>(start + 1));

    // Three drops, and irregular dwell around them -- frames that pass with
    // no attempt on this channel at all. The shared counter moves through
    // all of it; the channel's phase does not.
    const u8 shared_before = fl::detail::ditherFrame();
    for (int i = 0; i < 3; ++i) {
        FL_CHECK_FALSE(strip.attempt(true));
    }
    fl::detail::advanceDitherFrame();
    fl::detail::advanceDitherFrame();
    FL_CHECK_GE(static_cast<u8>(fl::detail::ditherFrame() - shared_before), 2);
    FL_CHECK_EQ(strip.channel->ditherPhase(), static_cast<u8>(start + 1));

    FL_REQUIRE(strip.attempt(false));
    FL_CHECK_EQ(strip.channel->ditherPhase(), static_cast<u8>(start + 2));
}

FL_TEST_CASE("[#4347] a busy buffer drops the frame without consuming the phase") {
    // The third drop path: the channel's buffer is still in use by a driver
    // that does not come READY in time. (The disabled-everything drops above
    // take the no-driver path.) Costs one waitForReady timeout, ~1 s.
    DroppableChannel strip("DITHER_BUSY", CRGB(37, 90, 5), false);
    FL_REQUIRE(strip.attempt(false));
    const u8 after_first = strip.channel->ditherPhase();

    // The channel's own buffer, as the driver holds it, marked in flight.
    FL_REQUIRE(!strip.engine->mCapturedChannels.empty());
    ChannelDataPtr in_flight = strip.engine->mCapturedChannels.back();
    in_flight->setInUse(true);
    strip.engine->mBusy = true;
    const fl::size before = strip.engine->mCapturedChannels.size();
    strip.channel->showLeds(255);
    FL_CHECK_EQ(strip.engine->mCapturedChannels.size(), before);  // dropped
    FL_CHECK_EQ(strip.channel->ditherPhase(), after_first);

    // Driver done: the next frame is presented and the phase moves on.
    strip.engine->mBusy = false;
    in_flight->setInUse(false);
    FL_REQUIRE(strip.attempt(false));
    FL_CHECK_EQ(strip.channel->ditherPhase(), static_cast<u8>(after_first + 1));
}

FL_TEST_CASE("[#4347] phase-correlated drops no longer bias the dither cycle") {
    // The failure mode #4347 describes: drops that correlate with phase. Here
    // every other attempt is dropped -- on the shared counter, the presented
    // frames would all be even phases, and the cycle's mean would land on
    // half of the pattern. On the channel's own phase, eight presentations
    // are eight consecutive phases, whatever was dropped between them.
    DroppableChannel strip("DITHER_BIAS", CRGB(37, 90, 5), true);
    FL_REQUIRE(strip.channel->isColorManaged());

    StreamingPipelineQ16 pipeline;
    FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::linearSrgb(), rgbDevice(),
                                         GamutPolicy::ChromaCompress, &pipeline));
    i32 drives[3];
    processPixelQ16(pipeline, 37, 90, 5, drives);
    const double exact_red = exactCode(drives[0]);

    bool phase_seen[8] = {false, false, false, false, false, false, false, false};
    int presented = 0;
    double sum = 0.0;
    for (int attempt = 0; presented < 8; ++attempt) {
        const bool drop = (attempt % 2) == 1;
        const u8 phase = strip.channel->ditherPhase();
        if (strip.attempt(drop)) {
            phase_seen[phase & 7] = true;
            sum += strip.lastRed();
            ++presented;
        }
        FL_REQUIRE_LT(attempt, 64);
    }
    for (int p = 0; p < 8; ++p) {
        FL_CHECK(phase_seen[p]);
    }
    FL_CHECK_LE(fl::fabs(sum / 8.0 - exact_red), 1.0 / 16.0 + 1e-9);
}

FL_TEST_CASE("[#4042] B3: managed LPD8806/LPD6803 quantize the drive once, at wire width") {
    // The 8-bit path rounds the drive to 8 bits and the encoder then shifts
    // it down to 7 (LPD8806, also forcing the low bit on) or truncates to 5
    // (LPD6803): two quantizations. The managed path quantizes the 16-bit
    // drive straight to the chip's width. Checked against that computation
    // over a spread of colours, and required to differ from the two-step
    // result somewhere, or the check says nothing.
    // The same pipeline `setColorProfile(rgbDevice())` binds: linear sRGB in.
    StreamingPipelineQ16 pipeline;
    FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::linearSrgb(), rgbDevice(),
                                         GamutPolicy::ChromaCompress, &pipeline));
    auto quantize16 = [](i32 drive) -> u32 {
        if (drive <= 0) { return 0; }
        if (drive >= 65536) { return 65535; }
        return static_cast<u32>((static_cast<u64>(drive) * 65535u + 32768u) >> 16);
    };
    int differs_8806 = 0;
    int differs_6803 = 0;
    for (int v = 1; v < 256; v += 5) {
        const CRGB pixel(static_cast<u8>(v), static_cast<u8>(255 - v),
                         static_cast<u8>((v * 7) & 0xFF));
        i32 drives[3];
        processPixelQ16(pipeline, pixel.r, pixel.g, pixel.b, drives);

        const fl::vector<u8> f8806 = managedHdFrame(SpiEncoder::lpd8806(), pixel, 31);
        const fl::vector<u8> f6803 = managedHdFrame(SpiEncoder::lpd6803(), pixel, 31);
        FL_REQUIRE_GE(f8806.size(), 3u);
        FL_REQUIRE_GE(f6803.size(), 6u);
        u16 command = 0x8000;
        for (int c = 0; c < 3; ++c) {
            const u32 w = quantize16(drives[c]);  // RGB channel order
            const u32 code7 = (w * 127u + 32767u) / 65535u;
            FL_CHECK_EQ(int(f8806[c]), int(0x80 | code7));
            const u32 code5 = (w * 31u + 32767u) / 65535u;
            command = static_cast<u16>(command | (code5 << (10 - 5 * c)));
            // The two-step result the legacy encoders would have produced.
            const u32 byte8 = (static_cast<u32>(drives[c] < 0 ? 0 : (drives[c] > 65536 ? 65536 : drives[c])) * 255u + 32768u) >> 16;
            if ((0x80 | code7) != fl::lpd8806Encode(static_cast<u8>(byte8))) { ++differs_8806; }
            if (code5 != (byte8 >> 3)) { ++differs_6803; }
        }
        FL_CHECK_EQ(int(f6803[4]), int(command >> 8));
        FL_CHECK_EQ(int(f6803[5]), int(command & 0xFF));
    }
    FL_CHECK_GT(differs_8806, 0);
    FL_CHECK_GT(differs_6803, 0);
}

FL_TEST_CASE("[#4457] a managed non-HD APA102/SK9822 holds its field at 31") {
    // FASTLED_USE_GLOBAL_BRIGHTNESS would derive a strip-wide field from the
    // first pixel and rescale only that pixel -- a second shaping stage on
    // solved drives. The managed hook owns these chips and always emits field
    // 31, the codes quantized once from the 16-bit drive, whatever that option
    // says; checked on the hook directly, so a hook that declined the chip --
    // leaving it to the option-dependent encoder -- fails here.
    installColorPipelineHooks();
    const ColorPipelineHooks& hooks = colorPipelineHooks();
    FL_REQUIRE(hooks.encodeManagedSpi != nullptr);
    const StreamingPipelineQ16 pipeline = makePipeline();
    CRGB leds[2] = {CRGB(200, 40, 10), CRGB(10, 180, 90)};
    StubController stub(leds, 2);

    const SpiChipset chips[] = {SpiChipset::APA102, SpiChipset::DOTSTAR,
                                SpiChipset::HD107, SpiChipset::SK9822};
    for (SpiChipset chip : chips) {
        PixelController<RGB> controller(leds, 2, ColorAdjustment::noAdjustment(),
                                        DISABLE_DITHER);
        ColorManagedPixelSource source(controller, RGB, pipeline);
        PixelIterator iterator(&source, Rgbw(), Rgbww());
        fl::vector_psram<u8> out;
        FL_REQUIRE(hooks.encodeManagedSpi(iterator, &out, chip, stub));
        FL_REQUIRE_GE(out.size(), 12u);
        for (int led = 0; led < 2; ++led) {
            i32 drives[3];
            processPixelQ16(pipeline, leds[led].r, leds[led].g, leds[led].b, drives);
            const fl::size at = 4 + 4 * static_cast<fl::size>(led);
            FL_CHECK_EQ(int(out[at]), 0xFF);  // 0xE0 | field 31
            for (int c = 0; c < 3; ++c) {
                // quantize16, then the joint solve's rounding at field 31.
                const i32 d = drives[c] < 0 ? 0 : (drives[c] > 65536 ? 65536 : drives[c]);
                const u64 w = d >= 65536 ? 65535u
                                         : (static_cast<u64>(d) * 65535u + 32768u) >> 16;
                const u64 target = (w * 7905u * 256u) / 65535u;
                const u64 code = (target + 31u * 128u) / (31u * 256u);
                FL_CHECK_EQ(int(out[at + 1 + c]), int(code > 255 ? 255 : code));
            }
        }
    }
}

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

FL_TEST_CASE("[#4497] physical response reaches 8-bit and 16-bit managed output") {
    EmitterProfile device = rgbDevice();
    const u16 linear[] = {0, 32768, 65535};
    const u16 dim_green[] = {0, 8192, 65535};
    device.response_lut_r = linear;
    device.response_lut_g = dim_green;
    device.response_lut_b = linear;
    device.response_lut_size = 3;
    StreamingPipelineQ16 curved;
    StreamingPipelineQ16 plain;
    FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::linearSrgb(), device,
                                         GamutPolicy::ChromaCompress, &curved));
    FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::linearSrgb(), rgbDevice(),
                                         GamutPolicy::ChromaCompress, &plain));
    const FluxScalar half = FluxScalar::fromBrightness(128);
    setPipelineFluxQ16(&curved, half);
    setPipelineFluxQ16(&plain, half);

    CRGB led(128, 128, 128);
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    PixelController<RGB> curved_ctrl(&led, 1, adjustment, DISABLE_DITHER);
    PixelController<RGB> plain_ctrl(&led, 1, adjustment, DISABLE_DITHER);
    ColorManagedPixelSource curved_source(curved_ctrl, RGB, curved);
    ColorManagedPixelSource plain_source(plain_ctrl, RGB, plain);
    u8 curved8[3];
    u8 plain8[3];
    curved_source.loadAndScaleRGB(&curved8[0], &curved8[1], &curved8[2]);
    plain_source.loadAndScaleRGB(&plain8[0], &plain8[1], &plain8[2]);
    FL_CHECK_GT(curved8[1], plain8[1]);
    const auto green_light = [](double code) {
        if (code <= 0.5) return code * (8192.0 / 65535.0) * 2.0;
        return (8192.0 + (code - 0.5) * 2.0 * (65535.0 - 8192.0)) / 65535.0;
    };
    FL_CHECK_LE(fl::fabs(green_light(curved8[1] / 255.0) - plain8[1] / 255.0),
                0.01);
#if !FL_PLATFORM_HAS_TINY_MEMORY
    u16 curved16[3];
    u16 plain16[3];
    curved_source.loadAndScaleRGB16(&curved16[0], &curved16[1], &curved16[2]);
    plain_source.loadAndScaleRGB16(&plain16[0], &plain16[1], &plain16[2]);
    FL_CHECK_GT(curved16[1], plain16[1]);
    FL_CHECK_LE(fl::fabs(green_light(curved16[1] / 65535.0) - plain16[1] / 65535.0),
                0.0001);
#endif
}

}  // FL_TEST_FILE
