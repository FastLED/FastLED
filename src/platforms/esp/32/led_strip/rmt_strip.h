#pragma once

#include <stdint.h>
#include "configure_led.h"
#include "led_strip_types.h"
#include "namespace.h"

LED_STRIP_NAMESPACE_BEGIN

class RmtLedStrip {
public:
    RmtLedStrip(int pin, uint32_t max_leds, bool is_rgbw);
    ~RmtLedStrip();
    void set_pixel(uint32_t i, uint8_t r, uint8_t g, uint8_t b);
    void set_pixel_rgbw(uint32_t i, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    void draw();

private:
    void* mLedStrip;
    bool is_rgbw;
    uint32_t mMaxLeds;
    uint8_t* mBuffer = nullptr;
};

LED_STRIP_NAMESPACE_END
