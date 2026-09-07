// Streaming colour-managed pixel source for color pipeline P6 (#4040).

#include "fl/channels/color_managed_source.h"
#include "FastLED.h"
#include "fl/channels/all_drivers.h"
#include "fl/channels/channel.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/gfx/colorimetric_response.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/span.h"
#include "fl/stl/int.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

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

    ColorAdjustment dim;
    dim.premixed = CRGB(128, 128, 128);
    dim.color = CRGB(0xff, 0xff, 0xff);
    dim.brightness = 128;

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
#endif

}  // FL_TEST_FILE
