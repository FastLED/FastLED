

/*
This demo is best viewed using the FastLED compiler.

Windows/MacOS binaries: https://github.com/FastLED/FastLED/releases

Python

Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.

OVERVIEW:
This sketch creates a fire effect using Perlin noise on a matrix of LEDs.
The fire appears to move upward with colors transitioning from black at the bottom
to white at the top, with red and yellow in between (for the default palette).
*/

// Perlin noise fire procedure
// 16x16 rgb led matrix demo
// Yaroslaw Turbin, 22.06.2020
// https://vk.com/ldirko
// https://www.reddit.com/user/ldirko/
// https://www.reddit.com/r/FastLED/comments/hgu16i/my_fire_effect_implementation_based_on_perlin/
// Based on the code found at: https://editor.soulmatelights.com/gallery/1229-

// HOW THE FIRE EFFECT WORKS:
// 1. We use Perlin noise with time offset for X and Z coordinates
//    to create a naturally scrolling fire pattern that changes over time
// 2. We distort the fire noise to make it look more realistic
// 3. We subtract a value based on Y coordinate to shift the fire color in the palette
//    (not just brightness). This creates a fade-out effect from the bottom of the matrix to the top
// 4. The palette is carefully designed to give realistic fire colors

#if defined(__AVR__)
// Platform does not have enough memory
void setup() {}
void loop() {}
#else

#include "FastLED.h"     // Main FastLED library for controlling LEDs
#include "fl/ui.h"       // UI components for the FastLED web compiler (sliders, etc.)
#include "fl/xymap.h"    // Mapping between 1D LED array and 2D coordinates
#include "fx/time.h"     // Time manipulation utilities

using namespace fl;      // Use the FastLED namespace for convenience

// Matrix dimensions - this defines the size of our virtual LED grid
#define HEIGHT 100       // Number of rows in the matrix
#define WIDTH 100        // Number of columns in the matrix
#define SERPENTINE true  // Whether the LED strip zigzags back and forth (common in matrix layouts)
#define BRIGHTNESS 255   // Maximum brightness level (0-255)

// TimeWarp helps control animation speed - it tracks time and allows speed adjustments
TimeWarp timeScale(0, 1.0f);  // Initialize with 0 starting time and 1.0 speed multiplier

// UI Controls that appear in the FastLED web compiler interface:
UISlider scaleXY("Scale", 20, 1, 100, 1);             // Controls the size of the fire pattern
UISlider speedY("SpeedY", 1, 1, 6, .1);               // Controls how fast the fire moves upward
UISlider invSpeedZ("Inverse SpeedZ", 20, 1, 100, 1);  // Controls how fast the fire pattern changes over time (higher = slower)
UISlider brightness("Brightness", 255, 0, 255, 1);     // Controls overall brightness
UINumberField palette("Palette", 0, 0, 2);             // Selects which color palette to use (0=fire, 1=green, 2=blue)

// Array to hold all LED color values - one CRGB struct per LED
CRGB leds[HEIGHT * WIDTH];

// Color palettes define the gradient of colors used for the fire effect
// Each entry has the format: position (0-255), R, G, B

DEFINE_GRADIENT_PALETTE(firepal){
    // Traditional fire palette - transitions from black to red to yellow to white
    0,   0,   0,   0,  // black (bottom of fire)
    32,  255, 0,   0,  // red (base of flames)
    190, 255, 255, 0,  // yellow (middle of flames)
    255, 255, 255, 255 // white (hottest part/tips of flames)
};

DEFINE_GRADIENT_PALETTE(electricGreenFirePal){
    // Green fire palette - for a toxic/alien look
    0,   0,   0,   0,  // black (bottom)
    32,  0,   70,  0,  // dark green (base)
    190, 57,  255, 20, // electric neon green (middle)
    255, 255, 255, 255 // white (hottest part)
};

DEFINE_GRADIENT_PALETTE(electricBlueFirePal) {
    // Blue fire palette - for a cold/ice fire look
    0,   0,   0,   0,    // Black (bottom)
    32,  0,   0,  70,    // Dark blue (base)
    128, 20, 57, 255,    // Electric blue (middle)
    255, 255, 255, 255   // White (hottest part)
};

// Create a mapping between 1D array positions and 2D x,y coordinates
XYMap xyMap(WIDTH, HEIGHT, SERPENTINE);

void setup() {
    Serial.begin(115200);  // Initialize serial communication for debugging
    
    // Initialize the LED strip:
    // - NEOPIXEL is the LED type
    // - 3 is the data pin number (for real hardware)
    // - setScreenMap connects our 2D coordinate system to the 1D LED array

    fl::ScreenMap screen_map = xyMap.toScreenMap();
    screen_map.setDiameter(0.1f);  // Set the diameter for the cylinder (0.2 cm per LED)
    FastLED.addLeds<NEOPIXEL, 3>(leds, HEIGHT * WIDTH).setScreenMap(screen_map);
    
    // Apply color correction for more accurate colors on LED strips
    FastLED.setCorrection(TypicalLEDStrip);
}

uint8_t getPaletteIndex(uint32_t millis32, int i, int j, uint32_t y_speed) {
    // This function calculates which color to use from our palette for each LED
    
    // Get the scale factor from the UI slider (controls the "size" of the fire)
    uint16_t scale = scaleXY.as<uint16_t>();
    
    // Calculate 3D coordinates for the Perlin noise function:
    uint16_t x = i * scale;                           // X position (horizontal in matrix)
    uint32_t y = j * scale + y_speed;                 // Y position (vertical) + movement offset
    uint16_t z = millis32 / invSpeedZ.as<uint16_t>(); // Z position (time dimension)
    
    // Generate 16-bit Perlin noise value using these coordinates
    // The << 8 shifts values left by 8 bits (multiplies by 256) to use the full 16-bit range
    uint16_t noise16 = inoise16(x << 8, y << 8, z << 8);
    
    // Convert 16-bit noise to 8-bit by taking the high byte (>> 8 shifts right by 8 bits)
    uint8_t noise_val = noise16 >> 8;
    
    // Calculate how much to subtract based on vertical position (j)
    // This creates the fade-out effect from bottom to top
    // abs8() ensures we get a positive value
    // The formula maps j from 0 to HEIGHT-1 to a value from 255 to 0
    int8_t subtraction_factor = abs8(j - (HEIGHT - 1)) * 255 / (HEIGHT - 1);
    
    // Subtract the factor from the noise value (with underflow protection)
    // qsub8 is a "saturating subtraction" - it won't go below 0
    return qsub8(noise_val, subtraction_factor);
}

CRGBPalette16 getPalette() {
    // This function returns the appropriate color palette based on the UI selection
    switch (palette) {
    case 0:
        return firepal;               // Traditional orange/red fire
    case 1:
        return electricGreenFirePal;  // Green "toxic" fire
    case 2:
        return electricBlueFirePal;   // Blue "cold" fire
    default:
        return firepal;               // Default to traditional fire if invalid value
    }
}

void loop() {
    // The main program loop that runs continuously
    
    // Set the overall brightness from the UI slider
    FastLED.setBrightness(brightness);
    
    // Get the selected color palette
    CRGBPalette16 myPal = getPalette();
    
    // Get the current time in milliseconds
    uint32_t now = millis();
    
    // Update the animation speed from the UI slider
    timeScale.setSpeed(speedY);
    
    // Calculate the current y-offset for animation (makes the fire move)
    uint32_t y_speed = timeScale.update(now);
    
    // Loop through every LED in our matrix
    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < HEIGHT; j++) {
            // Calculate which color to use from our palette for this LED
            uint8_t palette_index = getPaletteIndex(now, i, j, y_speed);
            
            // Get the actual RGB color from the palette
            // BRIGHTNESS ensures we use the full brightness range
            CRGB c = ColorFromPalette(myPal, palette_index, BRIGHTNESS);
            
            // Convert our 2D coordinates (i,j) to the 1D array index
            // We use (WIDTH-1)-i and (HEIGHT-1)-j to flip the coordinates
            // This makes the fire appear to rise from the bottom
            int index = xyMap((WIDTH - 1) - i, (HEIGHT - 1) - j);
            
            // Set the LED color in our array
            leds[index] = c;
        }
    }
    
    // Send the color data to the actual LEDs
    FastLED.show();
}

#endif