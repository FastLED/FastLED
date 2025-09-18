
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

#include <Arduino.h>      // Core Arduino functionality
#include <FastLED.h>      // Main FastLED library for controlling LEDs

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fl/math_macros.h"  // Math helper functions and macros
#include "fl/time_alpha.h"   // Time-based alpha/transition effects
#include "fl/ui.h"           // UI components for the FastLED web compiler
#include "fx/2d/blend.h"     // 2D blending effects between layers
#include "fx/2d/wave.h"      // 2D wave simulation

#include "wavefx.h"  // Header file for this sketch

using namespace fl;        // Use the FastLED namespace for convenience



// Array to hold all LED color values - one CRGB struct per LED
CRGB leds[NUM_LEDS];

// UI elements that appear in the FastLED web compiler interface:
UITitle title("FxWave2D Demo");
UIDescription description("Advanced layered and blended wave effects.");

UICheckbox xCyclical("X Is Cyclical", false);  // If true, waves wrap around the x-axis (like a loop)
// Main control UI elements:
UIButton button("Trigger");                                  // Button to trigger a single ripple
UIButton buttonFancy("Trigger Fancy");                       // Button to trigger a fancy cross-shaped effect
UICheckbox autoTrigger("Auto Trigger", true);                // Enable/disable automatic ripple triggering
UISlider triggerSpeed("Trigger Speed", .5f, 0.0f, 1.0f, 0.01f); // Controls how frequently auto-triggers happen (lower = faster)
UICheckbox easeModeSqrt("Ease Mode Sqrt", false);            // Changes how wave heights are calculated (sqrt gives more natural waves)
UICheckbox useChangeGrid("Use Change Grid", false);           // Enable performance optimization (reduces visual oscillation)
UISlider blurAmount("Global Blur Amount", 0, 0, 172, 1);     // Controls overall blur amount for all layers
UISlider blurPasses("Global Blur Passes", 1, 1, 10, 1);      // Controls how many times blur is applied (more = smoother but slower)
UISlider superSample("SuperSampleExponent", 1.f, 0.f, 3.f, 1.f); // Controls anti-aliasing quality (higher = better quality but more CPU)



// Upper wave layer controls:
UISlider speedUpper("Wave Upper: Speed", 0.12f, 0.0f, 1.0f);           // How fast the upper wave propagates
UISlider dampeningUpper("Wave Upper: Dampening", 8.9f, 0.0f, 20.0f, 0.1f); // How quickly the upper wave loses energy
UICheckbox halfDuplexUpper("Wave Upper: Half Duplex", true);           // If true, waves only go positive (not negative)
UISlider blurAmountUpper("Wave Upper: Blur Amount", 95, 0, 172, 1);    // Blur amount for upper wave layer
UISlider blurPassesUpper("Wave Upper: Blur Passes", 1, 1, 10, 1);      // Blur passes for upper wave layer

// Lower wave layer controls:
UISlider speedLower("Wave Lower: Speed", 0.26f, 0.0f, 1.0f);           // How fast the lower wave propagates
UISlider dampeningLower("Wave Lower: Dampening", 9.0f, 0.0f, 20.0f, 0.1f); // How quickly the lower wave loses energy
UICheckbox halfDuplexLower("Wave Lower: Half Duplex", true);           // If true, waves only go positive (not negative)
UISlider blurAmountLower("Wave Lower: Blur Amount", 0, 0, 172, 1);     // Blur amount for lower wave layer
UISlider blurPassesLower("Wave Lower: Blur Passes", 1, 1, 10, 1);      // Blur passes for lower wave layer

// Fancy effect controls (for the cross-shaped effect):
UISlider fancySpeed("Fancy Speed", 796, 0, 1000, 1);                   // Speed of the fancy effect animation
UISlider fancyIntensity("Fancy Intensity", 32, 1, 255, 1);             // Intensity/height of the fancy effect waves
UISlider fancyParticleSpan("Fancy Particle Span", 0.06f, 0.01f, 0.2f, 0.01f); // Width of the fancy effect lines

// Help text explaining the Use Change Grid feature
UIHelp changeGridHelp("Use Change Grid preserves the set point over multiple iterations to ensure more stable results across simulation resolutions. However, turning it off may result in more dramatic effects and saves memory.");

// Color palettes define the gradient of colors used for the wave effects
// Each entry has the format: position (0-255), R, G, B

DEFINE_GRADIENT_PALETTE(electricBlueFirePal){
    0,   0,   0,   0,   // Black (lowest wave height)
    32,  0,   0,   70,  // Dark blue (low wave height)
    128, 20,  57,  255, // Electric blue (medium wave height)
    255, 255, 255, 255  // White (maximum wave height)
};

DEFINE_GRADIENT_PALETTE(electricGreenFirePal){
    0,   0,   0,   0,   // Black (lowest wave height)
    8,   128, 64,  64,  // Green with red tint (very low wave height)
    16,  255, 222, 222, // Pinkish red (low wave height)
    64,  255, 255, 255, // White (medium wave height)
    255, 255, 255, 255  // White (maximum wave height)
};

// Create mappings between 1D array positions and 2D x,y coordinates
XYMap xyMap(WIDTH, HEIGHT, IS_SERPINTINE);  // For the actual LED output (may be serpentine)
XYMap xyRect(WIDTH, HEIGHT, false);         // For the wave simulation (always rectangular grid)

// Create default configuration for the lower wave layer
WaveFx::Args CreateArgsLower() {
    WaveFx::Args out;
    out.factor = SuperSample::SUPER_SAMPLE_2X;  // 2x supersampling for smoother waves
    out.half_duplex = true;                     // Only positive waves (no negative values)
    out.auto_updates = true;                    // Automatically update the simulation each frame
    out.speed = 0.18f;                          // Wave propagation speed
    out.dampening = 9.0f;                       // How quickly waves lose energy
    out.crgbMap = fl::make_shared<WaveCrgbGradientMap>(electricBlueFirePal);  // Color palette for this wave
    return out;
}

// Create default configuration for the upper wave layer
WaveFx::Args CreateArgsUpper() {
    WaveFx::Args out;
    out.factor = SuperSample::SUPER_SAMPLE_2X;  // 2x supersampling for smoother waves
    out.half_duplex = true;                     // Only positive waves (no negative values)
    out.auto_updates = true;                    // Automatically update the simulation each frame
    out.speed = 0.25f;                          // Wave propagation speed (faster than lower)
    out.dampening = 3.0f;                       // How quickly waves lose energy (less than lower)
    out.crgbMap = fl::make_shared<WaveCrgbGradientMap>(electricGreenFirePal);  // Color palette for this wave
    return out;
}

// Create the two wave simulation layers with their default configurations
WaveFx waveFxLower(xyRect, CreateArgsLower());  // Lower/background wave layer (blue)
WaveFx waveFxUpper(xyRect, CreateArgsUpper());  // Upper/foreground wave layer (green/red)

// Create a blender that will combine the two wave layers
Blend2d fxBlend(xyMap);


// Convert the UI slider value to the appropriate SuperSample enum value
// SuperSample controls the quality of the wave simulation (higher = better quality but more CPU)
SuperSample getSuperSample() {
    switch (int(superSample)) {
    case 0:
        return SuperSample::SUPER_SAMPLE_NONE;  // No supersampling (fastest, lowest quality)
    case 1:
        return SuperSample::SUPER_SAMPLE_2X;    // 2x supersampling (2x2 grid = 4 samples per pixel)
    case 2:
        return SuperSample::SUPER_SAMPLE_4X;    // 4x supersampling (4x4 grid = 16 samples per pixel)
    case 3:
        return SuperSample::SUPER_SAMPLE_8X;    // 8x supersampling (8x8 grid = 64 samples per pixel, slowest)
    default:
        return SuperSample::SUPER_SAMPLE_NONE;  // Default fallback
    }
}

// Create a ripple effect at a random position within the central area of the display
void triggerRipple() {
    // Define a margin percentage to keep ripples away from the edges
    float perc = .15f;
    
    // Calculate the boundaries for the ripple (15% from each edge)
    uint8_t min_x = perc * WIDTH;          // Left boundary
    uint8_t max_x = (1 - perc) * WIDTH;    // Right boundary
    uint8_t min_y = perc * HEIGHT;         // Top boundary
    uint8_t max_y = (1 - perc) * HEIGHT;   // Bottom boundary
    
    // Generate a random position within these boundaries
    int x = random(min_x, max_x);
    int y = random(min_y, max_y);
    
    // Set a wave peak at this position in both wave layers
    // The value 1.0 represents the maximum height of the wave
    waveFxLower.setf(x, y, 1);  // Create ripple in lower layer
    waveFxUpper.setf(x, y, 1);  // Create ripple in upper layer
}

// Create a fancy cross-shaped effect that expands from the center
void applyFancyEffect(uint32_t now, bool button_active) {
    // Calculate the total animation duration based on the speed slider
    // Higher fancySpeed value = shorter duration (faster animation)
    uint32_t total =
        map(fancySpeed.as<uint32_t>(), 0, fancySpeed.getMax(), 1000, 100);
    
    // Create a static TimeRamp to manage the animation timing
    // TimeRamp handles the transition from start to end over time
    static TimeRamp pointTransition = TimeRamp(total, 0, 0);

    // If the button is active, start/restart the animation
    if (button_active) {
        pointTransition.trigger(now, total, 0, 0);
    }

    // If the animation isn't currently active, exit early
    if (!pointTransition.isActive(now)) {
        // no need to draw
        return;
    }
    
    // Find the center of the display
    int mid_x = WIDTH / 2;
    int mid_y = HEIGHT / 2;
    
    // Calculate the maximum distance from center (half the width)
    int amount = WIDTH / 2;
    
    // Calculate the start and end coordinates for the cross
    int start_x = mid_x - amount;  // Leftmost point
    int end_x = mid_x + amount;    // Rightmost point
    int start_y = mid_y - amount;  // Topmost point
    int end_y = mid_y + amount;    // Bottommost point
    
    // Get the current animation progress (0-255)
    int curr_alpha = pointTransition.update8(now);
    
    // Map the animation progress to the four points of the expanding cross
    // As curr_alpha increases from 0 to 255, these points move from center to edges
    int left_x = map(curr_alpha, 0, 255, mid_x, start_x);  // Center to left
    int down_y = map(curr_alpha, 0, 255, mid_y, start_y);  // Center to top
    int right_x = map(curr_alpha, 0, 255, mid_x, end_x);   // Center to right
    int up_y = map(curr_alpha, 0, 255, mid_y, end_y);      // Center to bottom

    // Convert the 0-255 alpha to 0.0-1.0 range
    float curr_alpha_f = curr_alpha / 255.0f;

    // Calculate the wave height value - starts high and decreases as animation progresses
    // This makes the waves stronger at the beginning of the animation
    float valuef = (1.0f - curr_alpha_f) * fancyIntensity.value() / 255.0f;

    // Calculate the width of the cross lines
    int span = fancyParticleSpan.value() * WIDTH;
    
    // Add wave energy along the four expanding lines of the cross
    // Each line is a horizontal or vertical span of pixels
    
    // Left-moving horizontal line
    for (int x = left_x - span; x < left_x + span; x++) {
        waveFxLower.addf(x, mid_y, valuef);  // Add to lower layer
        waveFxUpper.addf(x, mid_y, valuef);  // Add to upper layer
    }

    // Right-moving horizontal line
    for (int x = right_x - span; x < right_x + span; x++) {
        waveFxLower.addf(x, mid_y, valuef);
        waveFxUpper.addf(x, mid_y, valuef);
    }

    // Downward-moving vertical line
    for (int y = down_y - span; y < down_y + span; y++) {
        waveFxLower.addf(mid_x, y, valuef);
        waveFxUpper.addf(mid_x, y, valuef);
    }

    // Upward-moving vertical line
    for (int y = up_y - span; y < up_y + span; y++) {
        waveFxLower.addf(mid_x, y, valuef);
        waveFxUpper.addf(mid_x, y, valuef);
    }
}

// Structure to hold the state of UI buttons
struct ui_state {
    bool button = false;     // Regular ripple button state
    bool bigButton = false;  // Fancy effect button state
};

// Apply all UI settings to the wave effects and return button states
ui_state ui() {
    // Set the easing function based on the checkbox
    // Easing controls how wave heights are calculated:
    // - LINEAR: Simple linear mapping (sharper waves)
    // - SQRT: Square root mapping (more natural, rounded waves)
    U8EasingFunction easeMode = easeModeSqrt
                                    ? U8EasingFunction::WAVE_U8_MODE_SQRT
                                    : U8EasingFunction::WAVE_U8_MODE_LINEAR;
    
    // Apply all settings from UI controls to the lower wave layer
    waveFxLower.setSpeed(speedLower);              // Wave propagation speed
    waveFxLower.setDampening(dampeningLower);      // How quickly waves lose energy
    waveFxLower.setHalfDuplex(halfDuplexLower);    // Whether waves can go negative
    waveFxLower.setSuperSample(getSuperSample());  // Anti-aliasing quality
    waveFxLower.setEasingMode(easeMode);           // Wave height calculation method
    waveFxLower.setUseChangeGrid(useChangeGrid);   // Performance optimization vs visual quality

    // Apply all settings from UI controls to the upper wave layer
    waveFxUpper.setSpeed(speedUpper);              // Wave propagation speed
    waveFxUpper.setDampening(dampeningUpper);      // How quickly waves lose energy
    waveFxUpper.setHalfDuplex(halfDuplexUpper);    // Whether waves can go negative
    waveFxUpper.setSuperSample(getSuperSample());  // Anti-aliasing quality
    waveFxUpper.setEasingMode(easeMode);           // Wave height calculation method
    waveFxUpper.setUseChangeGrid(useChangeGrid);   // Performance optimization vs visual quality
    
    // Apply global blur settings to the blender
    fxBlend.setGlobalBlurAmount(blurAmount);       // Overall blur strength
    fxBlend.setGlobalBlurPasses(blurPasses);       // Number of blur passes

    // Create parameter structures for each wave layer's blur settings
    fl::Blend2dParams lower_params;
    lower_params.blur_amount = blurAmountLower;    // Blur amount for lower layer
    lower_params.blur_passes = blurPassesLower;    // Blur passes for lower layer

    fl::Blend2dParams upper_params;
    upper_params.blur_amount = blurAmountUpper;    // Blur amount for upper layer
    upper_params.blur_passes = blurPassesUpper;    // Blur passes for upper layer

    // Apply the layer-specific blur parameters
    fxBlend.setParams(waveFxLower, lower_params);
    fxBlend.setParams(waveFxUpper, upper_params);
    
    // Return the current state of the UI buttons
    ui_state state;
    state.button = button;         // Regular ripple button
    state.bigButton = buttonFancy; // Fancy effect button
    return state;
}

// Handle automatic triggering of ripples at random intervals
void processAutoTrigger(uint32_t now) {
    // Static variable to remember when the next auto-trigger should happen
    static uint32_t nextTrigger = 0;
    
    // Calculate time until next trigger
    uint32_t trigger_delta = nextTrigger - now;
    
    // Handle timer overflow (happens after ~49 days of continuous running)
    if (trigger_delta > 10000) {
        // If the delta is suspiciously large, we probably rolled over
        trigger_delta = 0;
    }
    
    // Only proceed if auto-trigger is enabled
    if (autoTrigger) {
        // Check if it's time for the next trigger
        if (now >= nextTrigger) {
            // Create a ripple
            triggerRipple();
            
            // Calculate the next trigger time based on the speed slider
            // Invert the speed value so higher slider = faster triggers
            float speed = 1.0f - triggerSpeed.value();
            
            // Calculate min and max random intervals
            // Higher speed = shorter intervals between triggers
            uint32_t min_rand = 400 * speed;   // Minimum interval (milliseconds)
            uint32_t max_rand = 2000 * speed;  // Maximum interval (milliseconds)

            // Ensure min is actually less than max (handles edge cases)
            uint32_t min = MIN(min_rand, max_rand);
            uint32_t max = MAX(min_rand, max_rand);
            
            // Ensure min and max aren't equal (would cause random() to crash)
            if (min == max) {
                max += 1;
            }
            
            // Schedule the next trigger at a random time in the future
            nextTrigger = now + random(min, max);
        }
    }
}

void wavefx_setup() {
    // Create a screen map for visualization in the FastLED web compiler
    auto screenmap = xyMap.toScreenMap();
    screenmap.setDiameter(.2);  // Set the size of the LEDs in the visualization

    // Initialize the LED strip:
    // - NEOPIXEL is the LED type
    // - 2 is the data pin number (for real hardware)
    // - setScreenMap connects our 2D coordinate system to the 1D LED array
    FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS).setScreenMap(screenmap);

    // Set up UI groupings
    // Main Controls
    title.setGroup("Main Controls");
    description.setGroup("Main Controls");
    button.setGroup("Main Controls");
    buttonFancy.setGroup("Main Controls");
    autoTrigger.setGroup("Main Controls");
    triggerSpeed.setGroup("Main Controls");

    // Global Settings
    xCyclical.setGroup("Global Settings");
    easeModeSqrt.setGroup("Global Settings");
    useChangeGrid.setGroup("Global Settings");
    blurAmount.setGroup("Global Settings");
    blurPasses.setGroup("Global Settings");
    superSample.setGroup("Global Settings");

    // Upper Wave Layer
    speedUpper.setGroup("Upper Wave Layer");
    dampeningUpper.setGroup("Upper Wave Layer");
    halfDuplexUpper.setGroup("Upper Wave Layer");
    blurAmountUpper.setGroup("Upper Wave Layer");
    blurPassesUpper.setGroup("Upper Wave Layer");

    // Lower Wave Layer
    speedLower.setGroup("Lower Wave Layer");
    dampeningLower.setGroup("Lower Wave Layer");
    halfDuplexLower.setGroup("Lower Wave Layer");
    blurAmountLower.setGroup("Lower Wave Layer");
    blurPassesLower.setGroup("Lower Wave Layer");

    // Fancy Effects
    fancySpeed.setGroup("Fancy Effects");
    fancyIntensity.setGroup("Fancy Effects");
    fancyParticleSpan.setGroup("Fancy Effects");

    // Add both wave layers to the blender
    // The order matters - lower layer is added first (background)
    fxBlend.add(waveFxLower);
    fxBlend.add(waveFxUpper);
}


void wavefx_loop() {
    // The main program loop that runs continuously
    
    // Get the current time in milliseconds
    uint32_t now = millis();

    // set the x cyclical
    waveFxLower.setXCylindrical(xCyclical.value());  // Set whether lower wave wraps around x-axis
    
    // Apply all UI settings and get button states
    ui_state state = ui();
    
    // Check if the regular ripple button was pressed
    if (state.button) {
        triggerRipple();  // Create a single ripple
    }
    
    // Apply the fancy cross effect if its button is pressed
    applyFancyEffect(now, state.bigButton);
    
    // Handle automatic triggering of ripples
    processAutoTrigger(now);
    
    // Create a drawing context with the current time and LED array
    Fx::DrawContext ctx(now, leds);
    
    // Draw the blended result of both wave layers to the LED array
    fxBlend.draw(ctx);
    
    // Send the color data to the actual LEDs
    FastLED.show();
}


#endif  // SKETCH_HAS_LOTS_OF_MEMORY
