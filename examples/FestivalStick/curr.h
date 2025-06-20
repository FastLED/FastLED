/*
Festival Stick - Corkscrew LED Mapping Demo

This example demonstrates proper corkscrew LED mapping for a festival stick
(19+ turns, 288 LEDs) using the new Corkscrew ScreenMap functionality.

Key Features:
- Uses Corkscrew.toScreenMap() for accurate web interface visualization
- Draws patterns into a rectangular grid (frameBuffer)
- Maps the rectangular grid to the corkscrew LED positions using readFrom()
- Supports both noise patterns and manual LED positioning
- Proper color boost and brightness controls

Workflow:
1. Draw patterns into frameBuffer (rectangular grid for easy 2D drawing)
2. Use corkscrew.readFrom(frameBuffer) to map grid to corkscrew LED positions
3. Display the corkscrew buffer directly via FastLED
4. Web interface shows actual corkscrew spiral shape via ScreenMap

*/

#include "FastLED.h"

#include "fl/assert.h"
#include "fl/corkscrew.h"
#include "fl/grid.h"
#include "fl/leds.h"
#include "fl/screenmap.h"
#include "fl/sstream.h"
#include "fl/warn.h"
#include "noise.h"

// #include "vec3.h"

using namespace fl;



#ifndef PIN_DATA
#define PIN_DATA 1  // Universally available pin
#endif

#ifndef PIN_CLOCK
#define PIN_CLOCK 2  // Universally available pin
#endif


#ifdef TEST
#define NUM_LEDS  4
#define CORKSCREW_TURNS 2 // Default to 19 turns
#else
#define NUM_LEDS  288
#define CORKSCREW_TURNS 19.25 // Default to 19 turns
#endif

// #define CM_BETWEEN_LEDS 1.0 // 1cm between LEDs
// #define CM_LED_DIAMETER 0.5 // 0.5cm LED diameter

UITitle festivalStickTitle("Corkscrew");
UIDescription festivalStickDescription(
    "Tests the ability to map a cork screw onto a 2D cylindrical surface. ");

UISlider speed("Speed", 0.1f, 0.01f, 1.0f, 0.01f);
UISlider positionCoarse("Position Coarse (10x)", 0.0f, 0.0f, 1.0f, 0.01f);
UISlider positionFine("Position Fine (1x)", 0.0f, 0.0f, 0.1f, 0.001f);
UISlider positionExtraFine("Position Extra Fine (0.1x)", 0.0f, 0.0f, 0.01f, 0.0001f);
UISlider brightness("Brightness", 255, 0, 255, 1);

UICheckbox autoAdvance("Auto Advance", true);
UICheckbox allWhite("All White", false);
UICheckbox splatRendering("Splat Rendering", true);
UICheckbox useNoise("Use Noise Pattern", true);
UISlider noiseScale("Noise Scale", 30, 10, 200, 5);
UISlider noiseSpeed("Noise Speed", 4, 1, 100, 1);

// Color boost controls
UINumberField saturationFunction("Saturation Function", 1, 0, 9);
UINumberField luminanceFunction("Luminance Function", 0, 0, 9);

// Color palette for noise
CRGBPalette16 noisePalette = PartyColors_p;
uint8_t colorLoop = 1;

// Option 1: Runtime Corkscrew (flexible, configurable at runtime)
Corkscrew::Input corkscrewInput(CORKSCREW_TURNS, NUM_LEDS, 0);
Corkscrew corkscrew(corkscrewInput);

// Simple position tracking - one variable for both modes
static float currentPosition = 0.0f;
static uint32_t lastUpdateTime = 0;

EaseType getEaseType(int value) {
    switch (value) {
        case 0: return EASE_NONE;
        case 1: return EASE_IN_QUAD;
        case 2: return EASE_OUT_QUAD;
        case 3: return EASE_IN_OUT_QUAD;
        case 4: return EASE_IN_CUBIC;
        case 5: return EASE_OUT_CUBIC;
        case 6: return EASE_IN_OUT_CUBIC;
        case 7: return EASE_IN_SINE;
        case 8: return EASE_OUT_SINE;
        case 9: return EASE_IN_OUT_SINE;
    }
    FL_ASSERT(false, "Invalid ease type");
    return EASE_NONE;
}

// Option 2: Constexpr dimensions for compile-time array sizing
constexpr uint16_t CORKSCREW_WIDTH =
    fl::calculateCorkscrewWidth(CORKSCREW_TURNS, NUM_LEDS);
constexpr uint16_t CORKSCREW_HEIGHT =
    fl::calculateCorkscrewHeight(CORKSCREW_TURNS, NUM_LEDS);

// Now you can use these for array initialization:
// CRGB frameBuffer[CORKSCREW_WIDTH * CORKSCREW_HEIGHT];  // Compile-time sized
// array

// Create a corkscrew with:
// - 30cm total length (300mm)
// - 5cm width (50mm)
// - 2mm LED inner diameter
// - 24 LEDs per turn
// fl::ScreenMap screenMap = makeCorkScrew(NUM_LEDS,
// 300.0f, 50.0f, 2.0f, 24.0f);

// fl::vector<vec3f> mapCorkScrew = makeCorkScrew(args);
fl::ScreenMap screenMap;
fl::Grid<CRGB> frameBuffer;

void setup() {
    // Use constexpr dimensions (computed at compile time)
    constexpr int width = CORKSCREW_WIDTH;   // = 16
    constexpr int height = CORKSCREW_HEIGHT; // = 18

    // Or use runtime corkscrew for dynamic sizing
    // int width = corkscrew.cylinder_width();
    // int height = corkscrew.cylinder_height();

    frameBuffer.reset(width, height);
    XYMap xyMap = XYMap::constructRectangularGrid(width, height, 0);

    // Use the corkscrew's internal buffer for the LED strip
    CLEDController *controller =
        &FastLED.addLeds<APA102HD, PIN_DATA, PIN_CLOCK, BGR>(corkscrew.data(), NUM_LEDS);

    // CLEDController *controller =
    //     &FastLED.addLeds<WS2812, 3, BGR>(stripLeds, NUM_LEDS);

    // NEW: Create ScreenMap directly from Corkscrew using toScreenMap()
    // This maps each LED index to its exact position on the corkscrew spiral
    // instead of using a rectangular grid mapping
    fl::ScreenMap corkscrewScreenMap = corkscrew.toScreenMap(0.2f);
    
    // OLD WAY (rectangular grid - not accurate for corkscrew visualization):
    // fl::ScreenMap screenMap = xyMap.toScreenMap();
    // screenMap.setDiameter(.2f);

    // Set the corkscrew screen map for the controller
    // This allows the web interface to display the actual corkscrew spiral shape
    controller->setScreenMap(corkscrewScreenMap);
}

float get_position(uint32_t now) {
    if (autoAdvance.value()) {
        // Check if auto-advance was just enabled
        // Auto-advance mode: increment smoothly from current position
        float elapsedSeconds = (now - lastUpdateTime) / 1000.0f;
        float increment = elapsedSeconds * speed.value() *
                          0.3f; // Make it 1/20th the original speed
        currentPosition = fmodf(currentPosition + increment, 1.0f);
        lastUpdateTime = now;
        return currentPosition;
    } else {
        // Manual mode: use the dual slider control
        float combinedPosition = positionCoarse.value() + positionFine.value() + positionExtraFine.value();
        // Clamp to ensure we don't exceed 1.0
        if (combinedPosition > 1.0f)
            combinedPosition = 1.0f;
        return combinedPosition;
    }
}

void fillFrameBufferNoise() {
    // Get current UI values
    uint8_t noise_scale = noiseScale.value();
    uint8_t noise_speed = noiseSpeed.value();
    
    // Derive noise coordinates from current time instead of forward iteration
    uint32_t now = millis();
    uint16_t noise_z = now * noise_speed / 10;  // Primary time dimension
    uint16_t noise_x = now * noise_speed / 80;  // Slow drift in x
    uint16_t noise_y = now * noise_speed / 160; // Even slower drift in y (opposite direction)
    
    int width = frameBuffer.width();
    int height = frameBuffer.height();
    
    // Data smoothing for low speeds (from NoisePlusPalette example)
    uint8_t dataSmoothing = 0;
    if(noise_speed < 50) {
        dataSmoothing = 200 - (noise_speed * 4);
    }
    
    // Generate noise for each pixel in the frame buffer using cylindrical mapping
    for(int x = 0; x < width; x++) {
        for(int y = 0; y < height; y++) {
            // Convert rectangular coordinates to cylindrical coordinates
            // Map x to angle (0 to 2*PI), y remains as height
            float angle = (float(x) / float(width)) * 2.0f * PI;
            
            // Convert cylindrical coordinates to cartesian for noise sampling
            // Use the noise_scale to control the cylinder size in noise space
            float cylinder_radius = noise_scale; // Use the existing noise_scale parameter
            
            // Calculate cartesian coordinates on the cylinder surface
            float noise_x_cyl = cos(angle) * cylinder_radius;
            float noise_y_cyl = sin(angle) * cylinder_radius;
            float noise_z_height = float(y) * noise_scale; // Height component
            
            // Apply time-based offsets
            int xoffset = int(noise_x_cyl) + noise_x;
            int yoffset = int(noise_y_cyl) + noise_y;
            int zoffset = int(noise_z_height) + noise_z;
            
            // Generate 8-bit noise value using 3D Perlin noise with cylindrical coordinates
            uint8_t data = inoise8(xoffset, yoffset, zoffset);
            
            // Expand the range from ~16-238 to 0-255 (from NoisePlusPalette)
            data = qsub8(data, 16);
            data = qadd8(data, scale8(data, 39));
            
            // Apply data smoothing if enabled
            if(dataSmoothing) {
                CRGB oldColor = frameBuffer.at(x, y);
                uint8_t olddata = (oldColor.r + oldColor.g + oldColor.b) / 3; // Simple brightness extraction
                uint8_t newdata = scale8(olddata, dataSmoothing) + scale8(data, 256 - dataSmoothing);
                data = newdata;
            }
            
            // Map noise to color using palette (adapted from NoisePlusPalette)
            uint8_t index = data;
            uint8_t bri = data;
            
            // Add color cycling if enabled - also derive from time
            uint8_t ihue = 0;
            if(colorLoop) {
                ihue = (now / 100) % 256;  // Derive hue from time instead of incrementing
                index += ihue;
            }
            
            // Enhance brightness (from NoisePlusPalette example)
            // if(bri > 127) {
            //     //bri = 255;
            // } else {
            //     //bri = dim8_raw(bri * 2);
            // }
            
            // Get color from palette and set pixel
            CRGB color = ColorFromPalette(noisePalette, index, bri);
            
            // Apply color boost using ease functions
            EaseType sat_ease = getEaseType(saturationFunction.value());
            EaseType lum_ease = getEaseType(luminanceFunction.value());
            color = color.colorBoost(sat_ease, lum_ease);
            
            frameBuffer.at(x, y) = color;
        }
    }
}

void drawNoise(uint32_t now) {
    fillFrameBufferNoise();
}

void draw(float pos) {
    if (allWhite) {
        CRGB whiteColor = CRGB(8, 8, 8);
        // Apply color boost using ease functions
        EaseType sat_ease = getEaseType(saturationFunction.value());
        EaseType lum_ease = getEaseType(luminanceFunction.value());
        whiteColor = whiteColor.colorBoost(sat_ease, lum_ease);
        for (size_t i = 0; i < frameBuffer.size(); ++i) {
            frameBuffer.data()[i] = whiteColor;
        }
    }

    if (splatRendering) {
        Tile2x2_u8_wrap pos_tile = corkscrew.at_wrap(pos);
        FL_WARN("pos_tile: " << pos_tile);
        CRGB color = CRGB::Blue;
        // Apply color boost using ease functions
        EaseType sat_ease = getEaseType(saturationFunction.value());
        EaseType lum_ease = getEaseType(luminanceFunction.value());
        color = color.colorBoost(sat_ease, lum_ease);
        // Draw each pixel in the 2x2 tile using the new wrapping API
        for (int dx = 0; dx < 2; ++dx) {
            for (int dy = 0; dy < 2; ++dy) {
                Tile2x2_u8_wrap::Entry data = pos_tile.at(dx, dy);
                vec2i16 wrapped_pos = data.first; // Already wrapped position
                uint8_t alpha = data.second;      // Alpha value

                if (alpha > 0) { // Only draw if there's some alpha
                    CRGB c = color;
                    c.nscale8(alpha); // Scale the color by the alpha value
                    frameBuffer.at(wrapped_pos.x, wrapped_pos.y) = c;
                }
            }
        }
    } else {
        // None splat rendering, looks aweful.
        vec2f pos_vec2f = corkscrew.at_exact(pos);
        vec2i16 pos_i16 = vec2i16(pos_vec2f.x, pos_vec2f.y);
        
        CRGB color = CRGB::Blue;
        // Apply color boost using ease functions
        EaseType sat_ease = getEaseType(saturationFunction.value());
        EaseType lum_ease = getEaseType(luminanceFunction.value());
        color = color.colorBoost(sat_ease, lum_ease);
        
        // Now map the cork screw position to the cylindrical buffer that we
        // will draw.
        frameBuffer.at(pos_i16.x, pos_i16.y) = color; // Draw a blue pixel at (w, h)
    }
}

void loop() {
    uint32_t now = millis();
    fl::clear(frameBuffer);

    // Update the corkscrew mapping with auto-advance or manual position control
    float combinedPosition = get_position(now);
    float pos = combinedPosition * (corkscrew.size() - 1);

    if (useNoise.value()) {
        drawNoise(now);
    } else {
        draw(pos);
    }
    
    // Use the new readFrom workflow:
    // 1. Read directly from the frameBuffer Grid into the corkscrew's internal buffer
    corkscrew.readFrom(frameBuffer);
    
    // The corkscrew's buffer is now populated and FastLED will display it directly
    
    FastLED.setBrightness(brightness.value());
    FastLED.show();
}

