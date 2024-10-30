#pragma once

#include <stdint.h>
#include "namespace.h"
#include "crgb.h"

FASTLED_NAMESPACE_BEGIN

// Abstract base class for effects on a strip/grid of LEDs.

struct _DrawContext {
    uint32_t now;
    CRGB* leds;
    uint16_t frame_time = 0;
    float speed = 1.0f;
    uint8_t* alpha_channel = nullptr;
    _DrawContext(
        uint32_t now,
        CRGB* leds,
        uint16_t frame_time = 0,
        float speed = 1.0f,
        uint8_t* alpha_channel = nullptr
    ): now(now),
        leds(leds),
        frame_time(frame_time),
        speed(speed),
        alpha_channel(alpha_channel) {}
};

FASTLED_NAMESPACE_END

