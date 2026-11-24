// Regression evidence for #4499: modeled demand must bound the latched frame.

#include "FastLED.h"
#include "fl/channels/all_drivers.h"
#include "fl/channels/channel.h"
#include "fl/channels/color_managed_source.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/channels/options.h"
#include "fl/channels/pipeline_binding.h"
#include "fl/channels/power_prepass.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/gfx/colorimetric_response.h"
#include "fl/stl/scope_exit.h"
#include "power_mgt.h"
#include "test.h"

using namespace fl;

FL_TEST_FILE(FL_FILEPATH) {

#if FL_COLOR_PIPELINE_SHARED
namespace {

class PowerCaptureDriver : public IChannelDriver {
  public:
    fl::vector<ChannelDataPtr> frames;

    bool canHandle(const ChannelDataPtr&) const override { return true; }
    void enqueue(ChannelDataPtr data) override { frames.push_back(data); }
    void show() override {}
    DriverState poll() override { return DriverState::READY; }
    fl::string getName() const override { return "POWER_PREPASS_CAPTURE"; }
    Capabilities getCapabilities() const override {
        return Capabilities(true, true);
    }
};

EmitterProfile dimWideProfile(colorimetric_response::EmitterTopology topology) {
    static const u16 slow_response[] = {0, 4096, 65535};
    EmitterProfile profile = {};
    profile.xy_r[0] = 0.6400f; profile.xy_r[1] = 0.3300f;
    profile.xy_g[0] = 0.3000f; profile.xy_g[1] = 0.6000f;
    profile.xy_b[0] = 0.1500f; profile.xy_b[1] = 0.0600f;
    profile.lum_r = 0.8f;
    profile.lum_g = 0.8f;
    profile.lum_b = 0.8f;
    profile.topology = topology;
    profile.xy_white1[0] = 0.39f;
    profile.xy_white1[1] = 0.38f;
    profile.lum_white1 = 0.8f;
    profile.xy_white2[0] = 0.28f;
    profile.xy_white2[1] = 0.31f;
    profile.lum_white2 = 0.8f;
    profile.native_code_depth = 8;
    profile.response_lut_r = slow_response;
    profile.response_lut_g = slow_response;
    profile.response_lut_b = slow_response;
    profile.response_lut_white1 = slow_response;
    profile.response_lut_white2 = slow_response;
    profile.response_lut_size = 3;
    return profile;
}

u8 identityCode(u8 code) { return code; }

}  // namespace

FL_TEST_CASE("[#4499] mixed RGB and managed wide frame stays within shared power budget") {
    constexpr int kCount = 32;
    static CRGB legacy_leds[kCount];
    static CRGB rgbw_leds[kCount];
    static CRGB rgbww_leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        legacy_leds[i] = CRGB(100, 100, 100);
        rgbw_leds[i] = CRGB(100, 100, 100);
        rgbww_leds[i] = CRGB(100, 100, 100);
    }

    const PowerModelRGB prior = get_power_model();
    auto reset_model = fl::make_scope_exit([&]() { set_power_model(prior); });
    set_power_model(PowerModelRGBW(90, 90, 90, 90, 5));
    set_power_model(PowerModelRGBWW(90, 90, 90, 90, 90, 5));

    auto capture = fl::make_shared<PowerCaptureDriver>();
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2049, capture);
    auto remove_driver = fl::make_scope_exit([&]() { manager.removeDriver(capture); });
    const auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();

    ChannelOptions legacy_options;
    legacy_options.mDitherMode = DISABLE_DITHER;
    ChannelOptions rgbw_options;
    rgbw_options.mDitherMode = DISABLE_DITHER;
    rgbw_options.mWhiteCfg = Rgbw(6000, RGBW_MODE::kRGBWNullWhitePixel,
                                   EOrderW::W0);
    EmitterProfile rgbw_profile = dimWideProfile(
        colorimetric_response::EmitterTopology::RGBW);
    FL_REQUIRE(rgbw_options.setColorProfile(rgbw_profile,
                                            SourceProfile::linearSrgb()));

    ChannelOptions rgbww_options;
    rgbww_options.mDitherMode = DISABLE_DITHER;
    rgbww_options.mWhiteCfg = Rgbww(2700, 6500,
                                     RGBWW_MODE::kRGBWWColorimetric,
                                     EOrderWW::WwWcStart);
    EmitterProfile rgbww_profile = dimWideProfile(
        colorimetric_response::EmitterTopology::RGBWW);
    FL_REQUIRE(rgbww_options.setColorProfile(rgbww_profile,
                                             SourceProfile::linearSrgb()));

    auto legacy = Channel::create(ChannelConfig(
        3051, timing, fl::span<CRGB>(legacy_leds, kCount), RGB, legacy_options));
    auto rgbw = Channel::create(ChannelConfig(
        3052, timing, fl::span<CRGB>(rgbw_leds, kCount), RGB, rgbw_options));
    auto rgbww = Channel::create(ChannelConfig(
        3053, timing, fl::span<CRGB>(rgbww_leds, kCount), RGB, rgbww_options));
    FL_REQUIRE(legacy && rgbw && rgbww);
    FL_REQUIRE(rgbw->isColorManaged());
    FL_REQUIRE(rgbww->isColorManaged());
    auto remove_channels = fl::make_scope_exit([&]() {
        FastLED.remove(legacy);
        FastLED.remove(rgbw);
        FastLED.remove(rgbww);
    });
    FastLED.add(legacy);
    FastLED.add(rgbw);
    FastLED.add(rgbww);

    const u32 budget_mW = 2500;
    FastLED.setBrightness(255);
    FastLED.setMaxPowerInMilliWatts(budget_mW);
    capture->frames.clear();
    FastLED.show();
    FL_REQUIRE_EQ(capture->frames.size(), fl::size(3));
    FL_REQUIRE_LT(int(FastLED.getLastShowBrightness()), 255);
    const FramePowerPlan reported_plan = calculateFramePowerPlan(255, budget_mW);
    FL_CHECK_EQ(FastLED.getEstimatedPowerInMilliWatts(true) +
                    framePowerMCUBaselineMilliwatts(),
                reported_plan.modeled_mW);
    FL_CHECK(FastLED.isPowerLimited());
    FL_CHECK_FALSE(fl::powerFrameHooks().frameFluxActive);

    // showColor() encodes a constant, not the controller source pixels used
    // by the managed prepass. Keep its existing byte-limiter semantics and
    // never leave a source-frame Q16 flux active after presentation.
    const u8 color_brightness =
        calculate_max_brightness_for_power_mW(255, budget_mW);
    FastLED.showColor(CRGB::Black, 255);
    FL_CHECK_EQ(FastLED.getLastShowBrightness(), color_brightness);
    FL_CHECK_FALSE(fl::powerFrameHooks().frameFluxActive);

    // An unlimited Q16 plan must preserve the legacy channel's requested
    // byte exactly, even at brightness 1. A true limit rounds it down.
    FastLED.setBrightness(1);
    FastLED.setMaxPowerInMilliWatts(0xFFFFFFFFu);
    FastLED.show();
    FL_CHECK_EQ(FastLED.getLastShowBrightness(), 1);
    FL_CHECK_FALSE(FastLED.isPowerLimited());
    const FramePowerPlan low_request = calculateFramePowerPlan(1, 0xFFFFFFFFu);
    const FramePowerPlan zero_request = calculateFramePowerPlan(0, 0xFFFFFFFFu);
    const u32 tight_budget = zero_request.modeled_mW;
    FL_REQUIRE_GT(low_request.modeled_mW, tight_budget);
    FastLED.setMaxPowerInMilliWatts(tight_budget);
    FastLED.show();
    FL_CHECK_EQ(FastLED.getLastShowBrightness(), 0);
    FL_CHECK(FastLED.isPowerLimited());

    // The requested Q16 endpoint must charge the exact legacy byte it
    // emits. Brightness 100 projects back to 99 if it is floored twice.
    FastLED.setBrightness(100);
    FastLED.setMaxPowerInMilliWatts(0xFFFFFFFFu);
    capture->frames.clear();
    FastLED.show();
    FL_REQUIRE_EQ(capture->frames.size(), fl::size(3));
    const auto& lit_legacy_bytes = capture->frames[0]->getData();
    const u32 lit_legacy_mW = calculate_unscaled_emitter_power_mW(
        fl::span<const u8>(lit_legacy_bytes), 3);
    const FramePowerPlan lit_plan = calculateFramePowerPlan(100, 0xFFFFFFFFu);
    FL_CHECK_EQ(lit_plan.legacy_brightness, 100);
    for (int i = 0; i < kCount; ++i) legacy_leds[i] = CRGB::Black;
    capture->frames.clear();
    FastLED.show();
    FL_REQUIRE_EQ(capture->frames.size(), fl::size(3));
    const auto& dark_legacy_bytes = capture->frames[0]->getData();
    const u32 dark_legacy_mW = calculate_unscaled_emitter_power_mW(
        fl::span<const u8>(dark_legacy_bytes), 3);
    const FramePowerPlan dark_plan = calculateFramePowerPlan(100, 0xFFFFFFFFu);
    FL_REQUIRE_GE(lit_plan.modeled_mW, dark_plan.modeled_mW);
    FL_CHECK_GE(lit_plan.modeled_mW - dark_plan.modeled_mW,
                lit_legacy_mW - dark_legacy_mW);
    for (int i = 0; i < kCount; ++i) legacy_leds[i] = CRGB(100, 100, 100);
    FastLED.setBrightness(255);
    FastLED.setMaxPowerInMilliWatts(budget_mW);
    capture->frames.clear();
    FastLED.show();
    FL_REQUIRE_EQ(capture->frames.size(), fl::size(3));

    const u8 emitters[] = {3, 4, 5};
    u32 modeled_frame_mW = 125;  // MCU baseline charged by the limiter.
    for (int c = 0; c < 3; ++c) {
        const auto& bytes = capture->frames[c]->getData();
        FL_REQUIRE_EQ(bytes.size(), fl::size(kCount * emitters[c]));
        modeled_frame_mW += calculate_unscaled_emitter_power_mW(
            fl::span<const u8>(bytes), emitters[c]);
    }
    FL_CHECK_LE(modeled_frame_mW, budget_mW);

    // The prepass may inspect a frame repeatedly, but must never consume a
    // temporal-dither phase. A fractional scalar remains available after the
    // legacy byte brightness has quantized to zero.
    rgbw->setDither(BINARY_DITHER);
    rgbww->setDither(BINARY_DITHER);
    const u8 rgbw_phase = rgbw->ditherPhase();
    const u8 rgbww_phase = rgbww->ditherPhase();
    const FramePowerPlan sub_byte = calculateFramePowerPlan(255, 705);
    FL_CHECK_EQ(rgbw->ditherPhase(), rgbw_phase);
    FL_CHECK_EQ(rgbww->ditherPhase(), rgbww_phase);
    FL_CHECK_GT(sub_byte.flux_q16, 0u);
    FL_CHECK_EQ(sub_byte.legacy_brightness, 0);
    FL_CHECK_LE(sub_byte.modeled_mW, 705u);

    const FramePowerPlan below_idle = calculateFramePowerPlan(255, 604);
    FL_CHECK(below_idle.infeasible);
    FL_CHECK_EQ(below_idle.flux_q16, 0u);
    FL_CHECK_GE(below_idle.modeled_mW, 605u);

    const FramePowerPlan first_order = calculateFramePowerPlan(255, budget_mW);
    FastLED.remove(legacy);
    FastLED.remove(rgbw);
    FastLED.remove(rgbww);
    FastLED.add(rgbww);
    FastLED.add(legacy);
    FastLED.add(rgbw);
    const FramePowerPlan reversed_order = calculateFramePowerPlan(255, budget_mW);
    FL_CHECK_EQ(reversed_order.flux_q16, first_order.flux_q16);
    FL_CHECK_EQ(reversed_order.modeled_mW, first_order.modeled_mW);

    // A later frame sees source changes; no prepass state can cache the old
    // pixels or the previous channel list across frame boundaries.
    for (int i = 0; i < kCount; ++i) {
        legacy_leds[i] = CRGB::Black;
    }
    const FramePowerPlan later_frame = calculateFramePowerPlan(255, budget_mW);
    FL_CHECK_GT(later_frame.flux_q16, first_order.flux_q16);

    EmitterProfile brighter_rgbw = rgbw_profile;
    brighter_rgbw.lum_r = brighter_rgbw.lum_g = brighter_rgbw.lum_b = 1.6f;
    brighter_rgbw.lum_white1 = 1.6f;
    ChannelOptions rebound = rgbw_options;
    FL_REQUIRE(rebound.setColorProfile(brighter_rgbw,
                                       SourceProfile::linearSrgb()));
    rgbw->applyConfig(ChannelConfig(
        3052, timing, fl::span<CRGB>(rgbw_leds, kCount), RGB, rebound));
    FL_REQUIRE(rgbw->isColorManaged());
    const FramePowerPlan rebound_frame = calculateFramePowerPlan(255, budget_mW);
    FL_CHECK_GE(rebound_frame.flux_q16, later_frame.flux_q16);
}

FL_TEST_CASE("[#4499] unbound output keeps its legacy bytes under the limiter") {
    constexpr int kCount = 12;
    static CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) {
        leds[i] = CRGB(static_cast<u8>(40 + i * 9),
                       static_cast<u8>(90 + i * 5),
                       static_cast<u8>(180 - i * 7));
    }
    auto capture = fl::make_shared<PowerCaptureDriver>();
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2051, capture);
    auto remove_driver = fl::make_scope_exit([&]() { manager.removeDriver(capture); });
    ChannelOptions options;
    options.mDitherMode = DISABLE_DITHER;
    const auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    auto channel = Channel::create(ChannelConfig(
        3054, timing, fl::span<CRGB>(leds, kCount), GRB, options));
    FL_REQUIRE(channel != nullptr);
    FastLED.add(channel);
    auto remove_channel = fl::make_scope_exit([&]() { FastLED.remove(channel); });
    FL_REQUIRE_FALSE(channel->isColorManaged());
    FastLED.setBrightness(200);
    FastLED.setMaxPowerInMilliWatts(500);
    const u8 expected_brightness = calculate_max_brightness_for_power_mW(200, 500);
    capture->frames.clear();
    FastLED.show();
    FL_REQUIRE_EQ(capture->frames.size(), fl::size(1));
    FL_CHECK_EQ(FastLED.getLastShowBrightness(), expected_brightness);
    fl::vector<u8> limited_bytes;
    const auto& emitted = capture->frames[0]->getData();
    for (fl::size i = 0; i < emitted.size(); ++i) {
        limited_bytes.push_back(emitted[i]);
    }
    capture->frames.clear();
    channel->showLeds(expected_brightness);
    FL_REQUIRE_EQ(capture->frames.size(), fl::size(1));
    const auto& direct = capture->frames[0]->getData();
    FL_REQUIRE_EQ(direct.size(), limited_bytes.size());
    for (fl::size i = 0; i < direct.size(); ++i) {
        FL_CHECK_EQ(direct[i], limited_bytes[i]);
    }
}

FL_TEST_CASE("[#4499] APA102 and SK9822 current-field choices stay within the frame budget") {
    constexpr int kCount = 16;
    static CRGB leds[kCount];
    for (int i = 0; i < kCount; ++i) leds[i] = CRGB(100, 100, 100);
    const PowerModelRGB prior = get_power_model();
    const u8 prior_floor = FastLED.getHdFieldFloor();
    auto restore = fl::make_scope_exit([&]() {
        set_power_model(prior);
        FastLED.setHdFieldFloor(prior_floor);
    });
    set_power_model(PowerModelRGB(90, 90, 90, 5));
    auto capture = fl::make_shared<PowerCaptureDriver>();
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2050, capture);
    auto remove_driver = fl::make_scope_exit([&]() { manager.removeDriver(capture); });
    EmitterProfile profile = dimWideProfile(
        colorimetric_response::EmitterTopology::RGB);
    const SpiEncoder encoders[] = {SpiEncoder::apa102HD(),
                                   SpiEncoder::sk9822HD()};
    const u8 floors[] = {1, 7, 31};
    for (int chip = 0; chip < 2; ++chip) {
        ChannelOptions options;
        options.mDitherMode = DISABLE_DITHER;
        FL_REQUIRE(options.setColorProfile(profile,
                                            SourceProfile::linearSrgb()));
        SpiChipsetConfig spi_config{5, 6, encoders[chip]};
        auto channel = Channel::create(ChannelConfig(
            spi_config, fl::span<CRGB>(leds, kCount), RGB, options));
        FL_REQUIRE(channel != nullptr);
        FastLED.add(channel);
        for (u8 floor : floors) {
            FastLED.setHdFieldFloor(floor);
            const u32 budget_mW = 1000;
            FastLED.setMaxPowerInMilliWatts(budget_mW);
            FastLED.setBrightness(255);
            capture->frames.clear();
            FastLED.show();
            FL_REQUIRE_EQ(capture->frames.size(), fl::size(1));
            const auto& bytes = capture->frames[0]->getData();
            FL_REQUIRE_GE(bytes.size(), fl::size(4 + kCount * 4));
            u64 field_weighted_codes = 0;
            for (int i = 0; i < kCount; ++i) {
                const u8 field = bytes[4 + i * 4] & 31;
                FL_CHECK_GE(field, chip == 0 ? floor : 31);
                for (int c = 0; c < 3; ++c) {
                    field_weighted_codes += static_cast<u64>(field) *
                        bytes[5 + i * 4 + c];
                }
            }
            const u32 modeled_frame_mW = 125u + kCount * 5u +
                static_cast<u32>((field_weighted_codes * 90u + 31u * 256u - 1u) /
                                 (31u * 256u));
            FL_CHECK_LE(modeled_frame_mW, budget_mW);
        }
        FastLED.remove(channel);
    }
}

FL_TEST_CASE("[#4499] histogram bounds every wide temporal-dither phase") {
    installColorPipelineHooks();
    const PowerFrameHooks& hooks = powerFrameHooks();
    FL_REQUIRE(hooks.buildPowerHistogram != nullptr);
    FL_REQUIRE(hooks.histogramPowerNumerator != nullptr);
    CRGB leds[8] = {
        CRGB(1, 2, 3), CRGB(10, 20, 30), CRGB(40, 50, 60),
        CRGB(80, 70, 60), CRGB(100, 100, 100), CRGB(160, 80, 40),
        CRGB(200, 180, 140), CRGB(255, 220, 190),
    };
    const u32 fluxes[] = {64, 512, 8192, 32768, 65536};
    for (u8 emitter_count = 4; emitter_count <= 5; ++emitter_count) {
        EmitterProfile profile = dimWideProfile(
            emitter_count == 4 ? colorimetric_response::EmitterTopology::RGBW
                               : colorimetric_response::EmitterTopology::RGBWW);
        StreamingPipelineQ16 pipeline;
        FL_REQUIRE(buildStreamingPipelineQ16(
            SourceProfile::linearSrgb(), profile,
            GamutPolicy::ChromaCompress, &pipeline));
        ManagedPowerHistogram histogram;
        hooks.buildPowerHistogram(pipeline, fl::span<const CRGB>(leds),
                                  emitter_count, &histogram);
        const u8 weights[5] = {90, 90, 90, 90, 90};
        PowerCodecPolicy byte_codec;
        byte_codec.kind = PowerCodecKind::Byte;
        for (u32 flux_q16 : fluxes) {
            const FluxScalar flux = FluxScalar::fromRawQ16(flux_q16);
            const u64 charged = hooks.histogramPowerNumerator(
                pipeline, histogram, flux, byte_codec, weights,
                &identityCode);
            for (u8 phase = 0; phase < 8; ++phase) {
                PixelController<RGB> controller(
                    leds, 8, ColorAdjustment(), BINARY_DITHER);
                ColorManagedPixelSource source(controller, RGB, pipeline, phase);
                source.setFlux(flux);
                u64 actual = 0;
                for (int i = 0; i < 8; ++i) {
                    u8 bytes[5] = {};
                    if (emitter_count == 4) {
                        source.loadAndScaleRGBW(
                            Rgbw(6000, RGBW_MODE::kRGBWNullWhitePixel,
                                 EOrderW::W0),
                            &bytes[0], &bytes[1], &bytes[2], &bytes[3]);
                    } else {
                        source.loadAndScaleRGBWW(
                            Rgbww(2700, 6500, RGBWW_MODE::kRGBWWColorimetric,
                                  EOrderWW::WwWcStart),
                            &bytes[0], &bytes[1], &bytes[2], &bytes[3],
                            &bytes[4]);
                    }
                    for (u8 c = 0; c < emitter_count; ++c) {
                        actual += static_cast<u64>(bytes[c]) * 90u;
                    }
                    source.advanceData();
                }
                FL_CHECK_GE(charged, actual);
            }
        }
    }
}
#endif  // FL_COLOR_PIPELINE_SHARED

}  // FL_TEST_FILE
