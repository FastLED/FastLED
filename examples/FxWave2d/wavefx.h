
#pragma once



/*
This demo is best viewed using the FastLED compiler.

Windows/MacOS binaries: https://github.com/FastLED/FastLED/releases

Python

Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.

OVERVIEW:
This sketch demonstrates a 2D wave simulation with multiple layers and blending effects.
It creates ripple effects that propagate across the LED matrix, similar to water waves.
The demo includes two wave layers (upper and lower) with different colors and properties,
which are blended together to create complex visual effects.
*/


// Define the dimensions of our LED matrix
#define HEIGHT 64          // Number of rows in the matrix
#define WIDTH 64           // Number of columns in the matrix
#define NUM_LEDS ((WIDTH) * (HEIGHT))  // Total number of LEDs
#define IS_SERPINTINE true // Whether the LED strip zigzags back and forth (common in matrix layouts)

void wavefx_setup();
void wavefx_loop();