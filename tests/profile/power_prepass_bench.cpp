// ok standalone
// #4499: candidate-specific shared Q16 power prepass versus the old
// full-scale-linear u8 limiter, plus complete mixed-channel show/encode.
// Host CPU and in-memory capture only; no physical LED or wire time.

#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
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

class CaptureDriver : public fl::IChannelDriver {
  public:
    fl::ChannelDataPtr frames[3];
    int count = 0;
    bool canHandle(const fl::ChannelDataPtr&) const FL_NO_EXCEPT override {
        return true;
    }
    void enqueue(fl::ChannelDataPtr data) FL_NO_EXCEPT override {
        if (count < 3) frames[count++] = data;
    }
    void show() FL_NO_EXCEPT override {}
    DriverState poll() FL_NO_EXCEPT override { return DriverState::READY; }
    fl::string getName() const FL_NO_EXCEPT override {
        return "POWER_PREPASS_PROFILE_CAPTURE";
    }
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, true);
    }
};

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

bool measure(int n, int iterations) {
    const auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions legacy_opts;
    legacy_opts.mDitherMode = DISABLE_DITHER;
    fl::ChannelOptions rgbw_opts;
    rgbw_opts.mDitherMode = DISABLE_DITHER;
    rgbw_opts.mWhiteCfg = fl::Rgbw(6000, fl::RGBW_MODE::kRGBWNullWhitePixel,
                                    fl::EOrderW::W0);
    const fl::EmitterProfile p4 = profile(
        fl::colorimetric_response::EmitterTopology::RGBW);
    if (!rgbw_opts.setColorProfile(p4, fl::SourceProfile::linearSrgb())) return false;
    fl::ChannelOptions rgbww_opts;
    rgbww_opts.mDitherMode = DISABLE_DITHER;
    rgbww_opts.mWhiteCfg = fl::Rgbww(2700, 6500,
        fl::RGBWW_MODE::kRGBWWColorimetric, fl::EOrderWW::WwWcStart);
    const fl::EmitterProfile p5 = profile(
        fl::colorimetric_response::EmitterTopology::RGBWW);
    if (!rgbww_opts.setColorProfile(p5, fl::SourceProfile::linearSrgb())) return false;
    for (int i = 0; i < n; ++i) {
        gLegacy[i] = gRgbw[i] = gRgbww[i] = CRGB(100, 100, 100);
    }
    auto legacy = fl::Channel::create(fl::ChannelConfig(
        5001, timing, fl::span<CRGB>(gLegacy, n), RGB, legacy_opts));
    auto rgbw = fl::Channel::create(fl::ChannelConfig(
        5002, timing, fl::span<CRGB>(gRgbw, n), RGB, rgbw_opts));
    auto rgbww = fl::Channel::create(fl::ChannelConfig(
        5003, timing, fl::span<CRGB>(gRgbww, n), RGB, rgbww_opts));
    if (!legacy || !rgbw || !rgbww) return false;
    auto capture = fl::make_shared<CaptureDriver>();
    if (!capture) return false;
    fl::ChannelManager::instance().addDriver(1000000, capture);
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

    // Full CPU path: the same three channels are encoded into the capture
    // driver, with and without the shared limiter. Driver/wire time is absent.
    FastLED.setBrightness(255);
    FastLED.setMaxPowerInMilliWatts(budget);
    capture->count = 0;
    FastLED.show();
    bool captured = capture->count == 3;
    begin = fl::micros();
    for (int i = 0; i < iterations; ++i) {
        capture->count = 0;
        FastLED.show();
        captured = captured && capture->count == 3;
    }
    const fl::u32 limited_show_time = fl::micros() - begin;
    if (capture->count == 3) {
        gPowerSink += capture->frames[0]->getData()[0];
    }
    FastLED.clear(ClearFlags::POWER_SETTINGS);
    capture->count = 0;
    FastLED.show();
    captured = captured && capture->count == 3;
    begin = fl::micros();
    for (int i = 0; i < iterations; ++i) {
        capture->count = 0;
        FastLED.show();
        captured = captured && capture->count == 3;
    }
    const fl::u32 unlimited_show_time = fl::micros() - begin;
    if (capture->count == 3) {
        gPowerSink += capture->frames[0]->getData()[0];
    }
    const char* on_name = n == 32 ? "show_limited_32" :
        (n == 256 ? "show_limited_256" : "show_limited_1000");
    const char* off_name = n == 32 ? "show_unlimited_32" :
        (n == 256 ? "show_unlimited_256" : "show_unlimited_1000");
    ProfileResultBuilder::print_result(on_name, "power_show_encode", iterations,
                                       limited_show_time);
    ProfileResultBuilder::print_result(off_name, "power_show_encode", iterations,
                                       unlimited_show_time);
    FastLED.remove(legacy);
    FastLED.remove(rgbw);
    FastLED.remove(rgbww);
    fl::ChannelManager::instance().removeDriver(capture);
    return captured;
}

}  // namespace

int main() {
    set_power_model(PowerModelRGBW(90, 90, 90, 90, 5));
    set_power_model(PowerModelRGBWW(90, 90, 90, 90, 90, 5));
    if (!measure(32, 12) || !measure(256, 4) || !measure(1000, 2)) return 1;
    return gPowerSink == 0 ? 1 : 0;
}
