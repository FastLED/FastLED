
/*
This demo is best viewed using the FastLED compiler.

Windows/MacOS binaries: https://github.com/FastLED/FastLED/releases

Python

Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.

OVERVIEW:
This sketch creates a fire effect on a cylindrical LED display using Perlin noise.
Unlike a flat matrix, this cylinder connects the left and right edges (x=0 and x=width-1),
creating a seamless wrap-around effect. The fire appears to rise from the bottom,
with colors transitioning from black to red/yellow/white (or other palettes).
*/

// Perlin noise fire procedure
// 16x16 rgb led cylinder demo
// Exactly the same as the FireMatrix example, but with a cylinder, meaning that the x=0
// and x = len-1 are connected.
// This also showcases the inoise16(x,y,z,t) function which handles 3D+time noise effects.
// Keep in mind that a cylinder is embedded in a 3D space with time being used to add
// additional noise to the effect.

// HOW THE CYLINDRICAL FIRE EFFECT WORKS:
// 1. We use sine and cosine to map the x-coordinate to a circle in 3D space
// 2. This creates a cylindrical mapping where the left and right edges connect seamlessly
// 3. We use 4D Perlin noise (x,y,z,t) to generate natural-looking fire patterns
// 4. The height coordinate controls color fade-out to create the rising fire effect
// 5. The time dimension adds continuous variation to make the fire look dynamic

#include "FastLED.h"     // Main FastLED library for controlling LEDs
#include "fl/ui.h"       // UI components for the FastLED web compiler (sliders, buttons, etc.)
#include "fl/xymap.h"    // Mapping between 1D LED array and 2D coordinates
#include "fx/time.h"     // Time manipulation utilities for animations

using namespace fl;      // Use the FastLED namespace for convenience

// Cylinder dimensions - this defines the size of our virtual LED grid
#define HEIGHT 100       // Number of rows in the cylinder (vertical dimension)
#define WIDTH 100        // Number of columns in the cylinder (circumference)
#define SERPENTINE true  // Whether the LED strip zigzags back and forth (common in matrix layouts)
#define BRIGHTNESS 255   // Maximum brightness level (0-255)

// UI elements that appear in the FastLED web compiler interface:
UITitle title("FireCylinder Demo");                    // Title displayed in the UI
UIDescription description("This Fire demo wraps around the cylinder. It uses Perlin noise to create a fire effect.");

// TimeWarp helps control animation speed - it tracks time and allows speed adjustments
TimeWarp timeScale(0, 1.0f);  // Initialize with 0 starting time and 1.0 speed multiplier

// UI Controls for adjusting the fire effect:
UISlider scaleXY("Scale", 8, 1, 100, 1);              // Controls the overall size of the fire pattern
UISlider speedY("SpeedY", 1.3, 1, 6, .1);             // Controls how fast the fire moves upward
UISlider scaleX("ScaleX", .3, 0.1, 3, .01);           // Controls the horizontal scale (affects the wrap-around)
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

DEFINE_GRADIENT_PALETTE(electricBlueFirePal){
    // Blue fire palette - for a cold/ice fire look
    0,   0,   0,   0,   // Black (bottom)
    32,  0,   0,   70,  // Dark blue (base)
    128, 20,  57,  255, // Electric blue (middle)
    255, 255, 255, 255  // White (hottest part)
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

uint8_t getPaletteIndex(uint32_t millis32, int width, int max_width, int height, int max_height,
                        uint32_t y_speed) {
    // This function calculates which color to use from our palette for each LED
    
    // Get the scale factor from the UI slider
    uint16_t scale = scaleXY.as<uint16_t>();
    
    // Convert width position to an angle (0-255 represents 0-360 degrees)
    // This maps our flat coordinate to a position on a cylinder
    float xf = (float)width / (float)max_width;  // Normalized position (0.0 to 1.0)
    uint8_t x = (uint8_t)(xf * 255);            // Convert to 0-255 range for trig functions
    
    // Calculate the sine and cosine of this angle to get 3D coordinates on the cylinder
    uint32_t cosx = cos8(x);  // cos8 returns a value 0-255 representing cosine
    uint32_t sinx = sin8(x);  // sin8 returns a value 0-255 representing sine
    
    // Apply scaling to the sine/cosine values
    // This controls how "wide" the noise pattern is around the cylinder
    float trig_scale = scale * scaleX.value();
    cosx *= trig_scale;
    sinx *= trig_scale;
    
    // Calculate Y coordinate (vertical position) with speed offset for movement
    uint32_t y = height * scale + y_speed;
    
    // Calculate Z coordinate (time dimension) - controls how the pattern changes over time
    uint16_t z = millis32 / invSpeedZ.as<uint16_t>();
    FL_UNUSED(z);  // Suppress unused variable warning

    // Generate 16-bit Perlin noise using our 4D coordinates (x,y,z,t)
    // The << 8 shifts values left by 8 bits (multiplies by 256) to use the full 16-bit range
    // The last parameter (0) could be replaced with another time variable for more variation
    uint16_t noise16 = inoise16(cosx << 8, sinx << 8, y << 8, 0);
    
    // Convert 16-bit noise to 8-bit by taking the high byte
    uint8_t noise_val = noise16 >> 8;
    
    // Calculate how much to subtract based on vertical position (height)
    // This creates the fade-out effect from bottom to top
    // The formula maps height from 0 to max_height-1 to a value from 255 to 0
    int8_t subtraction_factor = abs8(height - (max_height - 1)) * 255 /
                                (max_height - 1);
    
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
    
    // Loop through every LED in our cylindrical matrix
    for (int width = 0; width < WIDTH; width++) {
        for (int height = 0; height < HEIGHT; height++) {
            // Calculate which color to use from our palette for this LED
            // This function handles the cylindrical mapping using sine/cosine
            uint8_t palette_index =
                getPaletteIndex(now, width, WIDTH, height, HEIGHT, y_speed);
            
            // Get the actual RGB color from the palette
            // BRIGHTNESS ensures we use the full brightness range
            CRGB c = ColorFromPalette(myPal, palette_index, BRIGHTNESS);
            
            // Convert our 2D coordinates to the 1D array index
            // We use (WIDTH-1)-width and (HEIGHT-1)-height to flip the coordinates
            // This makes the fire appear to rise from the bottom
            int index = xyMap((WIDTH - 1) - width, (HEIGHT - 1) - height);
            
            // Set the LED color in our array
            leds[index] = c;
        }
    }
    
    // Send the color data to the actual LEDs
    FastLED.show();
}
