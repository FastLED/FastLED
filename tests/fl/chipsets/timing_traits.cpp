#include "FastLED.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/chipsets/led_timing.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/gfx/rgbw.h"
#include "fl/stl/int.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/vector.h"
#include "pixel_controller.h"
#include "platforms/shared/clockless_blocking.h"
#include "test.h"

#if defined(FASTLED_TESTING)
namespace fl {

template<>
class FastPin<77> {
  public:
    static u8 transitions[128];
    static fl::size transitionCount;

    static void reset() FL_NO_EXCEPT { transitionCount = 0; }
    static void setOutput() FL_NO_EXCEPT {}
    static void hi() FL_NO_EXCEPT {
        transitions[transitionCount++] = 1;
    }
    static void lo() FL_NO_EXCEPT {
        transitions[transitionCount++] = 0;
    }
};

u8 FastPin<77>::transitions[128];
fl::size FastPin<77>::transitionCount = 0;

} // namespace fl

#endif

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// Test the single function in convert.h
FL_TEST_CASE("convert_fastled_timings_to_timedeltas") {
    FL_SUBCASE("basic conversion") {
        u16 T1 = 100;
        u16 T2 = 200;
        u16 T3 = 300;
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        // T0H = T1
        FL_CHECK_EQ(T0H, 100);
        // T0L = T2 + T3
        FL_CHECK_EQ(T0L, 500);
        // T1H = T1 + T2
        FL_CHECK_EQ(T1H, 300);
        // T1L = T3
        FL_CHECK_EQ(T1L, 300);
    }

    FL_SUBCASE("zero values") {
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(0, 0, 0, &T0H, &T0L, &T1H, &T1L);

        FL_CHECK_EQ(T0H, 0);
        FL_CHECK_EQ(T0L, 0);
        FL_CHECK_EQ(T1H, 0);
        FL_CHECK_EQ(T1L, 0);
    }

    FL_SUBCASE("maximum values") {
        u16 T1 = 0xFFFF;
        u16 T2 = 0;
        u16 T3 = 0;
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        FL_CHECK_EQ(T0H, 0xFFFF);
        FL_CHECK_EQ(T0L, 0);
        FL_CHECK_EQ(T1H, 0xFFFF);
        FL_CHECK_EQ(T1L, 0);
    }

    FL_SUBCASE("typical LED timing values (WS2812B-like)") {
        // Typical FastLED timings for WS2812B:
        // T1 = 350ns (high time for both 0 and 1)
        // T2 = 350ns (additional high time for 1-bit)
        // T3 = 550ns (low time)
        u16 T1 = 350;
        u16 T2 = 350;
        u16 T3 = 550;
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        // For 0-bit:
        FL_CHECK_EQ(T0H, 350);  // T0H = 350ns
        FL_CHECK_EQ(T0L, 900);  // T0L = 350 + 550 = 900ns

        // For 1-bit:
        FL_CHECK_EQ(T1H, 700);  // T1H = 350 + 350 = 700ns
        FL_CHECK_EQ(T1L, 550);  // T1L = 550ns

        // Verify total period is the same for both bits
        u16 period_0 = T0H + T0L;
        u16 period_1 = T1H + T1L;
        FL_CHECK_EQ(period_0, period_1);
        FL_CHECK_EQ(period_0, 1250);  // Total period = 1250ns
    }

    FL_SUBCASE("edge case - T2 and T3 sum") {
        // Test that T0L correctly sums T2 + T3
        u16 T1 = 10;
        u16 T2 = 20000;
        u16 T3 = 30000;
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        FL_CHECK_EQ(T0H, 10);
        FL_CHECK_EQ(T0L, 50000);  // 20000 + 30000
        FL_CHECK_EQ(T1H, 20010);  // 10 + 20000
        FL_CHECK_EQ(T1L, 30000);
    }

    FL_SUBCASE("overflow handling with wraparound") {
        // Test behavior when sum would exceed u16 range
        // The function doesn't check for overflow, so wraparound occurs
        u16 T1 = 0x8000;  // 32768
        u16 T2 = 0x8000;  // 32768
        u16 T3 = 0x1000;  // 4096
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        FL_CHECK_EQ(T0H, 0x8000);
        // T0L = 0x8000 + 0x1000 = 0x9000 (no overflow)
        FL_CHECK_EQ(T0L, 0x9000);
        // T1H = 0x8000 + 0x8000 = 0x10000, wraps to 0x0000
        FL_CHECK_EQ(T1H, 0);
        FL_CHECK_EQ(T1L, 0x1000);
    }

    FL_SUBCASE("realistic APA102 timing") {
        // APA102 uses different timing structure
        u16 T1 = 250;
        u16 T2 = 250;
        u16 T3 = 500;
        u16 T0H, T0L, T1H, T1L;

        convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

        FL_CHECK_EQ(T0H, 250);
        FL_CHECK_EQ(T0L, 750);  // 250 + 500
        FL_CHECK_EQ(T1H, 500);  // 250 + 250
        FL_CHECK_EQ(T1L, 500);
    }
}

// Additional test to verify the function doesn't modify input parameters
FL_TEST_CASE("convert_fastled_timings_to_timedeltas input preservation") {
    u16 T1 = 123;
    u16 T2 = 456;
    u16 T3 = 789;
    u16 T0H, T0L, T1H, T1L;

    u16 T1_orig = T1;
    u16 T2_orig = T2;
    u16 T3_orig = T3;

    convert_fastled_timings_to_timedeltas(T1, T2, T3, &T0H, &T0L, &T1H, &T1L);

    // Verify inputs weren't modified
    FL_CHECK_EQ(T1, T1_orig);
    FL_CHECK_EQ(T2, T2_orig);
    FL_CHECK_EQ(T3, T3_orig);
}

namespace {

fl::vector<u8> encodeWs2814Pixels(fl::span<CRGB> leds, const Rgbw& rgbw) {
    PixelController<RGB> pixels(
        leds.data(), static_cast<int>(leds.size()),
        ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    PixelIterator iterator = pixels.as_iterator(rgbw);
    fl::vector<u8> bytes;
    iterator.writeWS2812(&bytes);
    return bytes;
}

#if defined(FASTLED_TESTING)
class Ws2814BackendProbe : public WS2814<4, RGB> {
  public:
    void encode(PixelController<RGB>& pixels) FL_NO_EXCEPT {
        this->showPixels(pixels);
    }
};

class GenericWs2814Probe
    : public ClocklessBlockingGeneric<77, TIMING_WS2814, RGB, 0, false,
                                      TIMING_WS2814::RESET> {
  public:
    void encode(PixelController<RGB>& pixels) FL_NO_EXCEPT {
        this->showPixels(pixels);
    }
};
#endif

FL_TEST_CASE("WS2814 timing matches the 800 kHz RGBW protocol") {
    constexpr ChipsetTimingConfig timing =
        makeTimingConfig<TIMING_WS2814>();

    FL_CHECK_EQ(timing.t1_ns, 320);
    FL_CHECK_EQ(timing.t2_ns, 320);
    FL_CHECK_EQ(timing.t3_ns, 640);
    FL_CHECK_EQ(timing.reset_us, 300);
    FL_CHECK_EQ(timing.t1_ns + timing.t2_ns, 640);
    FL_CHECK_EQ(timing.t2_ns + timing.t3_ns, 960);
    FL_CHECK_EQ(timing.total_period_ns(), 1280);
}

FL_TEST_CASE("WS2818 timing stays inside the datasheet pulse envelope") {
    constexpr ChipsetTimingConfig timing =
        makeTimingConfig<TIMING_WS2818>();

    FL_CHECK_EQ(timing.t1_ns, 300);
    FL_CHECK_EQ(timing.t2_ns, 300);
    FL_CHECK_EQ(timing.t3_ns, 600);
    FL_CHECK_EQ(timing.reset_us, 300);
    FL_CHECK_EQ(timing.t1_ns + timing.t2_ns, 600);
    FL_CHECK_EQ(timing.t2_ns + timing.t3_ns, 900);
    FL_CHECK_EQ(timing.total_period_ns(), 1200);

    // Compile the documented public template path, not only its timing trait.
    WS2818<5, GRB> controller;
    FL_CHECK_FALSE(controller.getRgbw().active());

    // Bind the datasheet reset to the actual controller template. The generic
    // clockless default is only 280us, which is not strictly greater than the
    // WS2818B minimum.
    using ExpectedBase = fl::ClocklessControllerImpl<
        5, fl::TIMING_WS2818, GRB, 0, false, fl::TIMING_WS2818::RESET>;
    FL_CHECK_TRUE((fl::is_base_of<ExpectedBase,
                                  WS2818Controller<5, GRB>>::value));
}

FL_TEST_CASE("WS2814 enables RGBW with white in byte four") {
    static CRGB leds[1];
    CLEDController& controller =
        FastLED.addLeds<WS2814, 1, RGB>(leds, 1);

    const Rgbw rgbw = controller.getRgbw();
    FL_CHECK_TRUE(rgbw.active());
    FL_CHECK_EQ(rgbw.rgbw_mode, RGBW_MODE::kRGBWExactColors);
    FL_CHECK_EQ(rgbw.w_placement, EOrderW::W3);

    CRGB primary_colors[] = {
        CRGB(255, 0, 0),
        CRGB(0, 255, 0),
        CRGB(0, 0, 255),
    };
    const fl::vector<u8> primary_bytes =
        encodeWs2814Pixels(primary_colors, rgbw);
    const u8 expected_primaries[] = {
        255, 0, 0, 0,
        0, 255, 0, 0,
        0, 0, 255, 0,
    };
    FL_REQUIRE_EQ(primary_bytes.size(), sizeof(expected_primaries));
    for (fl::size i = 0; i < primary_bytes.size(); ++i) {
        FL_CHECK_EQ(primary_bytes[i], expected_primaries[i]);
    }

    CRGB white[] = {CRGB::White};
    const fl::vector<u8> white_bytes = encodeWs2814Pixels(white, rgbw);
    FL_REQUIRE_EQ(white_bytes.size(), 4);
    FL_CHECK_EQ(white_bytes[0], 0);
    FL_CHECK_EQ(white_bytes[1], 0);
    FL_CHECK_EQ(white_bytes[2], 0);
    FL_CHECK_EQ(white_bytes[3], 255);
}

FL_TEST_CASE("WS2814 rejects explicit RGBW reconfiguration") {
    static CRGB leds[1];
    const Rgbw custom(
        kRGBWDefaultColorTemp,
        RGBW_MODE::kRGBWExactColors,
        EOrderW::W0);
    CLEDController& controller =
        FastLED.addLeds<WS2814, 2, RGB>(leds, 1).setRgbw(custom);

    const Rgbw configured = controller.getRgbw();
    FL_CHECK_TRUE(configured.active());
    FL_CHECK_EQ(configured.w_placement, EOrderW::W3);
    FL_CHECK_FALSE(controller.getRgbww().active());

    controller.setRgbww(RgbwwDefault::value());
    FL_CHECK_TRUE(controller.getRgbw().active());
    FL_CHECK_EQ(controller.getRgbw().w_placement, EOrderW::W3);
    FL_CHECK_FALSE(controller.getRgbww().active());

    controller.clearWhiteChannel();
    FL_CHECK_TRUE(controller.getRgbw().active());
    FL_CHECK_EQ(controller.getRgbw().w_placement, EOrderW::W3);
    FL_CHECK_FALSE(controller.getRgbww().active());

    CRGB white[] = {CRGB::White};
    const fl::vector<u8> bytes = encodeWs2814Pixels(white, configured);
    FL_REQUIRE_EQ(bytes.size(), 4);
    FL_CHECK_EQ(bytes[0], 0);
    FL_CHECK_EQ(bytes[1], 0);
    FL_CHECK_EQ(bytes[2], 0);
    FL_CHECK_EQ(bytes[3], 255);
}

FL_TEST_CASE("WS2814 emits four bytes for small strips without changing RGB") {
    WS2814<3, RGB> controller;
    controller.init();

    CRGB leds[] = {
        CRGB(0xF0, 0x0F, 0xAA),
        CRGB(0x55, 0xFF, 0x00),
        CRGB(0x0F, 0xAA, 0xF0),
        CRGB(0x12, 0x34, 0x56),
    };
    for (fl::size count = 1; count <= 4; ++count) {
        const fl::vector<u8> rgbw_bytes = encodeWs2814Pixels(
            fl::span<CRGB>(leds, count), controller.getRgbw());
        FL_CHECK_EQ(rgbw_bytes.size(), count * 4);

        const fl::vector<u8> rgb_bytes = encodeWs2814Pixels(
            fl::span<CRGB>(leds, count), RgbwInvalid::value());
        FL_CHECK_EQ(rgb_bytes.size(), count * 3);
    }

    WS2813<5, RGB> existing_rgb_controller;
    existing_rgb_controller.init();
    FL_CHECK_FALSE(existing_rgb_controller.getRgbw().active());
    const fl::vector<u8> existing_rgb_bytes = encodeWs2814Pixels(
        fl::span<CRGB>(leds, 1), existing_rgb_controller.getRgbw());
    FL_CHECK_EQ(existing_rgb_bytes.size(), 3);
}

#if defined(FASTLED_TESTING)
FL_TEST_CASE("generic WS2814 fallback emits one contiguous four-byte frame") {
    CRGB leds[] = {CRGB::White};
    GenericWs2814Probe controller;
    controller.setRgbw(RgbwDefault::value());
    PixelController<RGB> pixels(
        leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);

    fl::FastPin<77>::reset();
    controller.encode(pixels);

    FL_REQUIRE_EQ(fl::FastPin<77>::transitionCount, 65);
    for (fl::size i = 0; i < 64; i += 2) {
        FL_CHECK_EQ(fl::FastPin<77>::transitions[i], 1);
        FL_CHECK_EQ(fl::FastPin<77>::transitions[i + 1], 0);
    }
    FL_CHECK_EQ(fl::FastPin<77>::transitions[64], 0);
}

FL_TEST_CASE("WS2814 controller backend enqueues exact RGBW bytes") {
    CRGB leds[] = {CRGB::White};
    Ws2814BackendProbe controller;
    controller.init();

    PixelController<RGB> pixels(
        leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.encode(pixels);

    const fl::vector<u8>& bytes =
        controller.channelDataForTesting()->getData();
    FL_REQUIRE_EQ(bytes.size(), 4);
    FL_CHECK_EQ(bytes[0], 0);
    FL_CHECK_EQ(bytes[1], 0);
    FL_CHECK_EQ(bytes[2], 0);
    FL_CHECK_EQ(bytes[3], 255);
}
#endif

} // namespace

} // FL_TEST_FILE
