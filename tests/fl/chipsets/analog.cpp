/// @file tests/fl/chipsets/analog.cpp
/// @brief Tests for three-pin PWM analog RGB output.

#include "FastLED.h"
#include "fl/stl/scope_exit.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

namespace {

class RecordingAnalogController : public ANALOG_RGB<3, 5, 6> {
  public:
    fl::u8 red = 0;
    fl::u8 green = 0;
    fl::u8 blue = 0;

  protected:
    void writeRgb(fl::u8 red_value, fl::u8 green_value,
                  fl::u8 blue_value) FL_NO_EXCEPT override {
        red = red_value;
        green = green_value;
        blue = blue_value;
    }
};

}  // namespace

FL_TEST_CASE("analog_rgb_applies_global_output_adjustments") {
    constexpr fl::u8 RED_PIN = 3;
    constexpr fl::u8 GREEN_PIN = 5;
    constexpr fl::u8 BLUE_PIN = 6;
    CRGB led = CRGB::White;
    RecordingAnalogController controller;

    FastLED.addLeds(&controller, &led, 1);
    auto cleanup = fl::make_scope_exit(
        [&controller]() { controller.removeFromDrawList(); });
    controller.setDither(DISABLE_DITHER);
    FL_CHECK_EQ(fl::getPwmFrequency(RED_PIN), 500u);
    FL_CHECK_EQ(fl::getPwmFrequency(GREEN_PIN), 500u);
    FL_CHECK_EQ(fl::getPwmFrequency(BLUE_PIN), 500u);

    FastLED.setBrightness(128);
    FastLED.show();
    FL_CHECK_EQ(controller.red, 128);
    FL_CHECK_EQ(controller.green, 128);
    FL_CHECK_EQ(controller.blue, 128);

    FastLED.setBrightness(255);
    FastLED.setCorrection(CRGB(255, 128, 64));
    FastLED.show();
    FL_CHECK_EQ(controller.red, 255);
    FL_CHECK_EQ(controller.green, 128);
    FL_CHECK_EQ(controller.blue, 64);

    FastLED.setCorrection(UncorrectedColor);
    FastLED.setTemperature(CRGB(128, 255, 64));
    FastLED.show();
    FL_CHECK_EQ(controller.red, 128);
    FL_CHECK_EQ(controller.green, 255);
    FL_CHECK_EQ(controller.blue, 64);
}

}  // FL_TEST_FILE
