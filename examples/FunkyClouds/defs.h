#pragma once

// define your LED hardware setup here
#define CHIPSET     WS2812B
#define LED_PIN     13
#define COLOR_ORDER GRB

// set master brightness 0-255 here to adjust power consumption
// and light intensity
#define BRIGHTNESS  60

// enter your custom matrix size if it is NOT a 16*16 and
// check in that case the setup part and
// RenderCustomMatrix() and
// ShowFrame() for more comments
#define CUSTOM_WIDTH 8
#define CUSTOM_HEIGHT 8

// MSGEQ7 wiring on spectrum analyser shield
#define MSGEQ7_STROBE_PIN 4
#define MSGEQ7_RESET_PIN  5
#define AUDIO_LEFT_PIN    0
#define AUDIO_RIGHT_PIN   1


// all 2D effects will be calculated in this matrix size
// do not touch
#define WIDTH 16
#define HEIGHT 16

// number of LEDs based on fixed calculation matrix size
// do not touch
#define NUM_LEDS (WIDTH * HEIGHT)