

#include <Arduino.h>

#include "platforms/esp/32/led_strip/rmt_strip.h"

using namespace fastled_rmt51_strip;

// How many leds in your strip?
#define NUM_LEDS 256

// For led chips like WS2812, which have a data line, ground, and power, you
// just need to define DATA_PIN.  For led chipsets that are SPI based (four
// wires - data, clock, ground, and power), like the LPD8806 define both
// DATA_PIN and CLOCK_PIN Clock pin only needed for SPI based chipsets when not
// using hardware SPI

#define PIN1 1
#define PIN2 6
#define PIN3 7
#define PIN4 8




#define TAG "main.cpp"



class ColorCycle {
public:
    ColorCycle(uint32_t num_leds, bool rgb_active): mNumLeds(num_leds), mRgbwActive(rgb_active) {}
    void draw_loop(IRmtLedStrip* led_strip);
private:
    uint32_t mNumLeds;
    bool mRgbwActive;
};


void ColorCycle::draw_loop(IRmtLedStrip* led_strip) {
    led_strip->wait_for_draw_complete();
    const int MAX_BRIGHTNESS = 64;
    uint32_t now = millis();
    double now_f = now / 1000.0;

    bool toggle = millis() / 500 % 2;
    uint8_t r = toggle ? MAX_BRIGHTNESS : 0;
    uint8_t g = toggle ? 0 : MAX_BRIGHTNESS;
    uint8_t b = 0;
    for (int i = 0; i < mNumLeds; i++) {
        // float hue = fmodf(now_f + (float)i / num_pixels(), 1.0f);
        // float r = MAX_BRIGHTNESS * (0.5f + 0.5f * std::sin(2 * PI * (hue + 0.0f / 3.0f)));
        // float g = MAX_BRIGHTNESS * (0.5f + 0.5f * std::sin(2 * PI * (hue + 1.0f / 3.0f)));
        // float b = MAX_BRIGHTNESS * (0.5f + 0.5f * std::sin(2 * PI * (hue + 2.0f / 3.0f)));
        // set_pixel(i, r, g, b);
        // set_pixel(i, r, g, b);
        led_strip->set_pixel(i, r, g, b);
    }
    led_strip->draw();
}



void setup() {
    Serial.begin(9600);
    Serial.setDebugOutput(true);
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    delay(1000);
    ESP_LOGI(TAG, "Start blinking LED strip");

}


void demo_high_level_api(int pin1, int pin2, int pin3, int pin4, uint32_t num_leds) {

    IRmtLedStrip* strip = create_rmt_led_strip(350, 800, 700, 600, 30000, pin1, NUM_LEDS, false);
    IRmtLedStrip* strip2 = create_rmt_led_strip(350, 800, 700, 600, 30000, pin2, NUM_LEDS, false);
    IRmtLedStrip* strip3 = create_rmt_led_strip(350, 800, 700, 600, 30000, pin3, NUM_LEDS, false);
    IRmtLedStrip* strip4 = create_rmt_led_strip(350, 800, 700, 600, 30000, pin4, NUM_LEDS, false);

    const bool is_rgbw_active = false;
    ColorCycle color_cycle(num_leds, is_rgbw_active);
    while (1) {
        uint32_t start = millis();
        color_cycle.draw_loop(strip);
        color_cycle.draw_loop(strip2);
        color_cycle.draw_loop(strip3);
        color_cycle.draw_loop(strip4);
        uint32_t diff = millis() - start;
        ESP_LOGE(TAG, "Time to draw: %d", diff);
    }

    // never reached.
    delete strip;
    delete strip2;
    delete strip3;
    delete strip4;
}

void loop() {
    demo_high_level_api(PIN1, PIN2, PIN3, PIN4, NUM_LEDS);
}
