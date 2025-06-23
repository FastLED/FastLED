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
#include "fl/array.h"

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

UITitle festivalStickTitle("Festival Stick");
UIDescription festivalStickDescription(
    "# Festival Stick Demo\n\n"
    "This example demonstrates **proper corkscrew LED mapping** for a festival stick using FastLED's advanced mapping capabilities.\n\n"
    "## Key Features\n"
    "- **19+ turns** with 288 LEDs total\n"
    "- Uses `Corkscrew.toScreenMap()` for accurate web interface visualization\n"
    "- Multiple render modes: **Noise**, **Position**, and **Fire** effects\n"
    "- Real-time cylindrical surface mapping\n\n"
    "## How It Works\n"
    "1. Draws patterns into a rectangular grid (`frameBuffer`)\n"
    "2. Maps the grid to corkscrew LED positions using `readFrom()`\n"
    "3. Web interface shows the actual spiral shape via ScreenMap\n\n"
    "*Select different render modes and adjust parameters to see various effects!*");



UISlider speed("Speed", 0.1f, 0.01f, 1.0f, 0.01f);
UISlider positionCoarse("Position Coarse (10x)", 0.0f, 0.0f, 1.0f, 0.01f);
UISlider positionFine("Position Fine (1x)", 0.0f, 0.0f, 0.1f, 0.001f);
UISlider positionExtraFine("Position Extra Fine (0.1x)", 0.0f, 0.0f, 0.01f, 0.0001f);
UISlider brightness("Brightness", 255, 0, 255, 1);

UICheckbox autoAdvance("Auto Advance", true);
UICheckbox allWhite("All White", false);
UICheckbox splatRendering("Splat Rendering", true);

// Noise controls (grouped under noiseGroup)

UISlider noiseScale("Noise Scale", 100, 10, 200, 5);
UISlider noiseSpeed("Noise Speed", 4, 1, 100, 1);

// UIDropdown examples - noise-related color palette
string paletteOptions[] = {"Party", "Heat", "Ocean", "Forest", "Rainbow"};
string renderModeOptions[] = {"Noise", "Position", "Fire"};




UIDropdown paletteDropdown("Color Palette", paletteOptions);
UIDropdown renderModeDropdown("Render Mode", renderModeOptions);


// fl::array<fl::pair<int, fl::string>> easeInfo = {
//     pair(EASE_IN_QUAD, "EASE_IN_QUAD"),
//     pair(EASE_OUT_QUAD, "EASE_OUT_QUAD"),
//     pair(EASE_IN_OUT_QUAD, "EASE_IN_OUT_QUAD"),
//     pair(EASE_IN_CUBIC, "EASE_IN_CUBIC"),
//     pair(EASE_OUT_CUBIC, "EASE_OUT_CUBIC"),
//     pair(EASE_IN_OUT_CUBIC, "EASE_IN_OUT_CUBIC"),
//     pair(EASE_IN_SINE, "EASE_IN_SINE"),
//     pair(EASE_OUT_SINE, "EASE_OUT_SINE"),
//     pair(EASE_IN_OUT_SINE, "EASE_IN_OUT_SINE")
// };


fl::vector<fl::string> easeInfo = {
    "EASE_NONE",
    "EASE_IN_QUAD",
    "EASE_OUT_QUAD",
    "EASE_IN_OUT_QUAD",
    "EASE_IN_CUBIC",
    "EASE_OUT_CUBIC",
    "EASE_IN_OUT_CUBIC",
    "EASE_IN_SINE",
    "EASE_OUT_SINE",
    "EASE_IN_OUT_SINE"
};

EaseType getEaseType(fl::string value) {
    if (value == "EASE_NONE") {
        return EASE_NONE;
    } else if (value == "EASE_IN_QUAD") {
        return EASE_IN_QUAD;
    } else if (value == "EASE_OUT_QUAD") {
        return EASE_OUT_QUAD;
    } else if (value == "EASE_IN_OUT_QUAD") {
        return EASE_IN_OUT_QUAD;
    } else if (value == "EASE_IN_CUBIC") {
        return EASE_IN_CUBIC;
    } else if (value == "EASE_OUT_CUBIC") {
        return EASE_OUT_CUBIC;
    } else if (value == "EASE_IN_OUT_CUBIC") {
        return EASE_IN_OUT_CUBIC;
    } else if (value == "EASE_IN_SINE") {
        return EASE_IN_SINE;
    } else if (value == "EASE_OUT_SINE") {
        return EASE_OUT_SINE;
    } else if (value == "EASE_IN_OUT_SINE") {
        return EASE_IN_OUT_SINE;
    } else {
        return EASE_NONE;
    }
}

// Color boost controls
UIDropdown saturationFunction("Saturation Function", easeInfo.begin(), easeInfo.end());
UIDropdown luminanceFunction("Luminance Function", easeInfo.begin(), easeInfo.end());

// Fire-related UI controls (added for cylindrical fire effect)
UISlider fireScaleXY("Fire Scale", 8, 1, 100, 1);              
UISlider fireSpeedY("Fire SpeedY", 1.3, 1, 6, .1);             
UISlider fireScaleX("Fire ScaleX", .3, 0.1, 3, .01);           
UISlider fireInvSpeedZ("Fire Inverse SpeedZ", 20, 1, 100, 1);  
UINumberField firePalette("Fire Palette", 0, 0, 2);             

// Fire color palettes (from FireCylinder)
DEFINE_GRADIENT_PALETTE(firepal){
    0,   0,   0,   0,  
    32,  255, 0,   0,  
    190, 255, 255, 0,  
    255, 255, 255, 255 
};

DEFINE_GRADIENT_PALETTE(electricGreenFirePal){
    0,   0,   0,   0,  
    32,  0,   70,  0,  
    190, 57,  255, 20, 
    255, 255, 255, 255 
};

DEFINE_GRADIENT_PALETTE(electricBlueFirePal){
    0,   0,   0,   0,   
    32,  0,   0,   70,  
    128, 20,  57,  255, 
    255, 255, 255, 255  
};

// Create UIGroup for noise controls using variadic constructor
// This automatically assigns all specified controls to the "Noise Controls" group
UIGroup noiseGroup("Noise Controls", noiseScale, noiseSpeed, paletteDropdown);
UIGroup fireGroup("Fire Controls", fireScaleXY, fireSpeedY, fireScaleX, fireInvSpeedZ, firePalette);
UIGroup renderGroup("Render Options", renderModeDropdown, splatRendering, allWhite, brightness);
UIGroup colorBoostGroup("Color Boost", saturationFunction, luminanceFunction);
UIGroup pointGraphicsGroup("Point Graphics Mode", speed, positionCoarse, positionFine, positionExtraFine, autoAdvance);




// Color palette for noise
CRGBPalette16 noisePalette = PartyColors_p;
uint8_t colorLoop = 1;

// Option 1: Runtime Corkscrew (flexible, configurable at runtime)
Corkscrew::Input corkscrewInput(CORKSCREW_TURNS, NUM_LEDS, 0);
Corkscrew corkscrew(corkscrewInput);

// Simple position tracking - one variable for both modes
static float currentPosition = 0.0f;
static uint32_t lastUpdateTime = 0;




// Option 2: Constexpr dimensions for compile-time array sizing
constexpr uint16_t CORKSCREW_WIDTH =
    calculateCorkscrewWidth(CORKSCREW_TURNS, NUM_LEDS);
constexpr uint16_t CORKSCREW_HEIGHT =
    calculateCorkscrewHeight(CORKSCREW_TURNS, NUM_LEDS);

// Now you can use these for array initialization:
// CRGB frameBuffer[CORKSCREW_WIDTH * CORKSCREW_HEIGHT];  // Compile-time sized
// array

// Create a corkscrew with:
// - 30cm total length (300mm)
// - 5cm width (50mm)
// - 2mm LED inner diameter
// - 24 LEDs per turn
// ScreenMap screenMap = makeCorkScrew(NUM_LEDS,
// 300.0f, 50.0f, 2.0f, 24.0f);

// vector<vec3f> mapCorkScrew = makeCorkScrew(args);
ScreenMap screenMap;
Grid<CRGB> frameBuffer;

void setup() {
    // Use constexpr dimensions (computed at compile time)
    constexpr int width = CORKSCREW_WIDTH;   // = 16
    constexpr int height = CORKSCREW_HEIGHT; // = 18


    // Noise controls are now automatically grouped by the UIGroup constructor
    // The noiseGroup variadic constructor automatically called setGroup() on all controls


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
    ScreenMap corkscrewScreenMap = corkscrew.toScreenMap(0.2f);
    
    // OLD WAY (rectangular grid - not accurate for corkscrew visualization):
    // ScreenMap screenMap = xyMap.toScreenMap();
    // screenMap.setDiameter(.2f);

    // Set the corkscrew screen map for the controller
    // This allows the web interface to display the actual corkscrew spiral shape
    controller->setScreenMap(corkscrewScreenMap);
    
    // Demonstrate UIGroup functionality for noise controls
    FL_WARN("Noise UI Group initialized: " << noiseGroup.name());
    FL_WARN("  This group contains noise pattern controls:");
    FL_WARN("  - Use Noise Pattern toggle");
    FL_WARN("  - Noise Scale and Speed sliders");
    FL_WARN("  - Color Palette selection for noise");
    FL_WARN("  UIGroup automatically applied group membership via variadic constructor");
    
    // Set initial dropdown selections
    paletteDropdown.setSelectedIndex(0);    // Party
    renderModeDropdown.setSelectedIndex(0); // Fire (new default)
    
    // Add onChange callbacks for dropdowns
    paletteDropdown.onChanged([](UIDropdown &dropdown) {
        string selectedPalette = dropdown.value();
        FL_WARN("Noise palette changed to: " << selectedPalette);
        if (selectedPalette == "Party") {
            noisePalette = PartyColors_p;
        } else if (selectedPalette == "Heat") {
            noisePalette = HeatColors_p;
        } else if (selectedPalette == "Ocean") {
            noisePalette = OceanColors_p;
        } else if (selectedPalette == "Forest") {
            noisePalette = ForestColors_p;
        } else if (selectedPalette == "Rainbow") {
            noisePalette = RainbowColors_p;
        }
    });
    
    renderModeDropdown.onChanged([](UIDropdown &dropdown) {
        string mode = dropdown.value();
        // Simple example of using getOption()
        for(size_t i = 0; i < dropdown.getOptionCount(); i++) {
            if(dropdown.getOption(i) == mode) {
                FL_WARN("Render mode changed to: " << mode);
            }
        }
    });
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
    FL_UNUSED(now);
    fillFrameBufferNoise();
}

void draw(float pos) {


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
        vec2f pos_vec2f = corkscrew.at_no_wrap(pos);
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

CRGBPalette16 getFirePalette() {
    int paletteIndex = (int)firePalette.value();
    switch (paletteIndex) {
    case 0:
        return firepal;               
    case 1:
        return electricGreenFirePal;  
    case 2:
        return electricBlueFirePal;   
    default:
        return firepal;               
    }
}

uint8_t getFirePaletteIndex(uint32_t millis32, int width, int max_width, int height, int max_height,
                        uint32_t y_speed) {
    uint16_t scale = fireScaleXY.as<uint16_t>();
    
    float xf = (float)width / (float)max_width;  
    uint8_t x = (uint8_t)(xf * 255);            
    
    uint32_t cosx = cos8(x);  
    uint32_t sinx = sin8(x);  
    
    float trig_scale = scale * fireScaleX.value();
    cosx *= trig_scale;
    sinx *= trig_scale;
    
    uint32_t y = height * scale + y_speed;
    
    uint16_t z = millis32 / fireInvSpeedZ.as<uint16_t>();

    uint16_t noise16 = inoise16(cosx << 8, sinx << 8, y << 8, z << 8);
    
    uint8_t noise_val = noise16 >> 8;
    
    int8_t subtraction_factor = abs8(height - (max_height - 1)) * 255 /
                                (max_height - 1);
    
    return qsub8(noise_val, subtraction_factor);
}

void fillFrameBufferFire(uint32_t now) {
    CRGBPalette16 myPal = getFirePalette();
    
    // Calculate the current y-offset for animation (makes the fire move)
    uint32_t y_speed = now * fireSpeedY.value();
    
    int width = frameBuffer.width();
    int height = frameBuffer.height();
    
    // Loop through every pixel in our cylindrical matrix
    for (int w = 0; w < width; w++) {
        for (int h = 0; h < height; h++) {
            // Calculate which color to use from our palette for this pixel
            uint8_t palette_index =
                getFirePaletteIndex(now, w, width, h, height, y_speed);
            
            // Get the actual RGB color from the palette
            CRGB color = ColorFromPalette(myPal, palette_index, 255);
            
            // Apply color boost using ease functions
            EaseType sat_ease = getEaseType(saturationFunction.value());
            EaseType lum_ease = getEaseType(luminanceFunction.value());
            color = color.colorBoost(sat_ease, lum_ease);
            
            // Set the pixel in the frame buffer
            // Flip coordinates to make fire rise from bottom
            frameBuffer.at((width - 1) - w, (height - 1) - h) = color;
        }
    }
}

void drawFire(uint32_t now) {
    fillFrameBufferFire(now);
}

void loop() {
    uint32_t now = millis();
    clear(frameBuffer);

    if (allWhite) {
        CRGB whiteColor = CRGB(8, 8, 8);
        for (size_t i = 0; i < frameBuffer.size(); ++i) {
            frameBuffer.data()[i] = whiteColor;
        }
    }

    // Update the corkscrew mapping with auto-advance or manual position control
    float combinedPosition = get_position(now);
    float pos = combinedPosition * (corkscrew.size() - 1);


    if (renderModeDropdown.value() == "Noise") {
        drawNoise(now);
    } else if (renderModeDropdown.value() == "Fire") {
        drawFire(now);
    } else {
        draw(pos);
    }
    
    // Use the new readFrom workflow:
    // 1. Read directly from the frameBuffer Grid into the corkscrew's internal buffer
    // use_multi_sampling = true will use multi-sampling to sample from the source grid,
    // this will give a little bit better accuracy and the screenmap will be more accurate.
    const bool use_multi_sampling = splatRendering;
    corkscrew.readFrom(frameBuffer, use_multi_sampling);
    
    // The corkscrew's buffer is now populated and FastLED will display it directly
    
    FastLED.setBrightness(brightness.value());
    FastLED.show();
}
