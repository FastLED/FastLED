// ok standalone
// #4499: candidate-specific shared Q16 power prepass versus the old
// full-scale-linear u8 limiter. Host CPU only; no wire/driver time.

#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/options.h"
#include "fl/channels/power_prepass.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/chrono.h"
#include "profile_result.h"
#include "power_mgt.h"

namespace {

volatile fl::u32 gPowerSink = 0;
constexpr int kMaxPixels = 1000;
CRGB gLegacy[kMaxPixels];
CRGB gRgbw[kMaxPixels];
CRGB gRgbww[kMaxPixels];

fl::EmitterProfile profile(fl::colorimetric_response::EmitterTopology topology) {
    static const fl::u16 response[] = {0, 4096, 65535};
    fl::EmitterProfile p = {};
    p.xy_r[0] = .64f; p.xy_r[1] = .33f;
    p.xy_g[0] = .30f; p.xy_g[1] = .60f;
    p.xy_b[0] = .15f; p.xy_b[1] = .06f;
    p.lum_r = p.lum_g = p.lum_b = .8f;
    p.xy_white1[0] = .39f; p.xy_white1[1] = .38f;
    p.xy_white2[0] = .28f; p.xy_white2[1] = .31f;
    p.lum_white1 = p.lum_white2 = .8f;
    p.topology = topology;
    p.native_code_depth = 8;
    p.response_lut_r = p.response_lut_g = p.response_lut_b = response;
    p.response_lut_white1 = p.response_lut_white2 = response;
    p.response_lut_size = 3;
    return p;
}

void measure(int n, int iterations) {
    const auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions legacy_opts;
    legacy_opts.mDitherMode = DISABLE_DITHER;
    fl::ChannelOptions rgbw_opts;
    rgbw_opts.mDitherMode = DISABLE_DITHER;
    rgbw_opts.mWhiteCfg = fl::Rgbw(6000, fl::RGBW_MODE::kRGBWNullWhitePixel,
                                    fl::EOrderW::W0);
    const fl::EmitterProfile p4 = profile(
        fl::colorimetric_response::EmitterTopology::RGBW);
    if (!rgbw_opts.setColorProfile(p4, fl::SourceProfile::linearSrgb())) return;
    fl::ChannelOptions rgbww_opts;
    rgbww_opts.mDitherMode = DISABLE_DITHER;
    rgbww_opts.mWhiteCfg = fl::Rgbww(2700, 6500,
        fl::RGBWW_MODE::kRGBWWColorimetric, fl::EOrderWW::WwWcStart);
    const fl::EmitterProfile p5 = profile(
        fl::colorimetric_response::EmitterTopology::RGBWW);
    if (!rgbww_opts.setColorProfile(p5, fl::SourceProfile::linearSrgb())) return;
    for (int i = 0; i < n; ++i) {
        gLegacy[i] = gRgbw[i] = gRgbww[i] = CRGB(100, 100, 100);
    }
    auto legacy = fl::Channel::create(fl::ChannelConfig(
        5001, timing, fl::span<CRGB>(gLegacy, n), RGB, legacy_opts));
    auto rgbw = fl::Channel::create(fl::ChannelConfig(
        5002, timing, fl::span<CRGB>(gRgbw, n), RGB, rgbw_opts));
    auto rgbww = fl::Channel::create(fl::ChannelConfig(
        5003, timing, fl::span<CRGB>(gRgbww, n), RGB, rgbww_opts));
    if (!legacy || !rgbw || !rgbww) return;
    FastLED.add(legacy);
    FastLED.add(rgbw);
    FastLED.add(rgbww);
    const fl::u32 budget = 70u * static_cast<fl::u32>(n);
    const char* old_name = n == 32 ? "old_32" : (n == 256 ? "old_256" : "old_1000");
    const char* new_name = n == 32 ? "q16_32" : (n == 256 ? "q16_256" : "q16_1000");
    gPowerSink += fl::calculateFramePowerPlan(255, budget).flux_q16;
    gPowerSink += calculate_max_brightness_for_power_mW(255, budget);
    fl::u32 begin = fl::micros();
    for (int i = 0; i < iterations; ++i) {
        gPowerSink += calculate_max_brightness_for_power_mW(255, budget);
    }
    const fl::u32 old_time = fl::micros() - begin;
    begin = fl::micros();
    for (int i = 0; i < iterations; ++i) {
        gPowerSink += fl::calculateFramePowerPlan(255, budget).flux_q16;
    }
    const fl::u32 new_time = fl::micros() - begin;
    ProfileResultBuilder::print_result(old_name, "power_limit_frame", iterations,
                                       old_time);
    ProfileResultBuilder::print_result(new_name, "power_limit_frame", iterations,
                                       new_time);
    FastLED.remove(legacy);
    FastLED.remove(rgbw);
    FastLED.remove(rgbww);
}

}  // namespace

int main() {
    set_power_model(PowerModelRGBW(90, 90, 90, 90, 5));
    set_power_model(PowerModelRGBWW(90, 90, 90, 90, 90, 5));
    measure(32, 12);
    measure(256, 4);
    measure(1000, 2);
    return gPowerSink == 0 ? 1 : 0;
}
