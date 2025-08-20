/// @file    FxNoiseRing.ino
/// @brief   Noise effect on circular ring with ScreenMap
/// @example FxNoiseRing.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

#include <Arduino.h>
#include <FastLED.h>


#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/warn.h"
#include "noisegen.h"
#include "fl/screenmap.h"
#include "fl/slice.h"
#include "fl/ui.h"

#include "sensors/pir.h"
#include "./simple_timer.h"
#include "fl/sstream.h"
#include "fl/assert.h"





#define LED_PIN 2
#define COLOR_ORDER GRB  // Color order matters for a real device, web-compiler will ignore this.
#define NUM_LEDS 250
#define PIN_PIR 0

#define PIR_LATCH_MS 60000  // how long to keep the PIR sensor active after a trigger
#define PIR_RISING_TIME 1000  // how long to fade in the PIR sensor
#define PIR_FALLING_TIME 1000  // how long to fade out the PIR sensor

using namespace fl;

CRGB leds[NUM_LEDS];

// Enhanced coordinate system for ring-based effects
struct RingCoord {
    float angle;      // Position on ring (0 to 2π)
    float radius;     // Distance from center (normalized 0-1)
    float x, y;       // Cartesian coordinates
    int led_index;    // LED position on strip
};

// Convert LED index to ring coordinates
RingCoord calculateRingCoord(int led_index, int num_leds, float time_offset = 0.0f) {
    RingCoord coord;
    coord.led_index = led_index;
    coord.angle = (led_index * 2.0f * M_PI / num_leds) + time_offset;
    coord.radius = 1.0f; // Fixed radius for ring
    coord.x = cos(coord.angle);
    coord.y = sin(coord.angle);
    return coord;
}

// Performance optimization with lookup tables
class RingLUT {
private:
    float cos_table[NUM_LEDS];
    float sin_table[NUM_LEDS];
    
public:
    void initialize() {
        for(int i = 0; i < NUM_LEDS; i++) {
            float angle = i * 2.0f * M_PI / NUM_LEDS;
            cos_table[i] = cos(angle);
            sin_table[i] = sin(angle);
        }
    }
    
    RingCoord fastRingCoord(int led_index, float time_offset = 0.0f) {
        RingCoord coord;
        coord.led_index = led_index;
        coord.angle = (led_index * 2.0f * M_PI / NUM_LEDS) + time_offset;
        coord.x = cos_table[led_index];
        coord.y = sin_table[led_index];
        coord.radius = 1.0f;
        return coord;
    }
};

// Plasma wave parameters
struct PlasmaParams {
    float time_scale = 1.0f;
    float noise_intensity = 0.5f;
    float noise_amplitude = 0.8f;
    uint8_t time_bitshift = 5;
    uint8_t hue_offset = 0;
    float brightness = 1.0f;
};

// Plasma wave generator - Featured Implementation
class PlasmaWaveGenerator {
private:
    struct WaveSource {
        float x, y;          // Source position
        float frequency;     // Wave frequency
        float amplitude;     // Wave strength
        float phase_speed;   // Phase evolution rate
    };
    
    WaveSource sources[4] = {
        {0.5f, 0.5f, 1.0f, 1.0f, 0.8f},   // Center source
        {0.0f, 0.0f, 1.5f, 0.8f, 1.2f},   // Corner source
        {1.0f, 1.0f, 0.8f, 1.2f, 0.6f},   // Opposite corner
        {0.5f, 0.0f, 1.2f, 0.9f, 1.0f}    // Edge source
    };

public:
    CRGB calculatePlasmaPixel(const RingCoord& coord, uint32_t time_ms, const PlasmaParams& params) {
        float time_scaled = time_ms * params.time_scale * 0.001f;
        
        // Calculate wave interference
        float wave_sum = 0.0f;
        for (int i = 0; i < 4; i++) {
            float dx = coord.x - sources[i].x;
            float dy = coord.y - sources[i].y;
            float distance = sqrt(dx*dx + dy*dy);
            
            float wave_phase = distance * sources[i].frequency + time_scaled * sources[i].phase_speed;
            wave_sum += sin(wave_phase) * sources[i].amplitude;
        }
        
        // Add noise modulation for organic feel
        float noise_scale = params.noise_intensity;
        float noise_x = coord.x * 0xffff * noise_scale;
        float noise_y = coord.y * 0xffff * noise_scale;
        uint32_t noise_time = time_ms << params.time_bitshift;
        
        float noise_mod = (inoise16(noise_x, noise_y, noise_time) - 32768) / 65536.0f;
        wave_sum += noise_mod * params.noise_amplitude;
        
        // Map to color space
        return mapWaveToColor(wave_sum, params);
    }

private:
    CRGB mapWaveToColor(float wave_value, const PlasmaParams& params) {
        // Normalize wave to 0-1 range
        float normalized = (wave_value + 4.0f) / 8.0f; // Assuming max amplitude ~4
        normalized = constrain(normalized, 0.0f, 1.0f);
        
        // Create flowing hue based on wave phase
        uint8_t hue = (uint8_t)(normalized * 255.0f + params.hue_offset) % 256;
        
        // Dynamic saturation based on wave intensity
        float intensity = abs(wave_value);
        uint8_t sat = (uint8_t)(192 + intensity * 63); // High saturation with variation
        
        // Brightness modulation
        uint8_t val = (uint8_t)(normalized * 255.0f * params.brightness);
        
        return CHSV(hue, sat, val);
    }
};

// Advanced Color Palette System
class ColorPaletteManager {
private:
    uint8_t current_palette = 0;
    uint32_t last_palette_change = 0;
    static const uint32_t PALETTE_CHANGE_INTERVAL = 5000; // 5 seconds

public:
    void update(uint32_t now, bool auto_cycle_enabled, uint8_t manual_palette) {
        if (auto_cycle_enabled) {
            if (now - last_palette_change > PALETTE_CHANGE_INTERVAL) {
                current_palette = (current_palette + 1) % 5;
                last_palette_change = now;
            }
        } else {
            current_palette = manual_palette;
        }
    }
    
    CRGB mapColor(float hue_norm, float intensity, float special_param = 0.0f) {
        switch(current_palette) {
            case 0: return mapSunsetBoulevard(hue_norm, intensity, special_param);
            case 1: return mapOceanBreeze(hue_norm, intensity, special_param);
            case 2: return mapNeonNights(hue_norm, intensity, special_param);
            case 3: return mapForestWhisper(hue_norm, intensity, special_param);
            case 4: return mapGalaxyExpress(hue_norm, intensity, special_param);
            default: return mapSunsetBoulevard(hue_norm, intensity, special_param);
        }
    }

private:
    CRGB mapSunsetBoulevard(float hue_norm, float intensity, float special_param) {
        // Warm oranges, deep reds, golden yellows (Hue 0-45)
        uint8_t hue = (uint8_t)(hue_norm * 45);
        uint8_t sat = 200 + (uint8_t)(intensity * 55);
        uint8_t val = 150 + (uint8_t)(intensity * 105);
        return CHSV(hue, sat, val);
    }
    
    CRGB mapOceanBreeze(float hue_norm, float intensity, float special_param) {
        // Deep blues, aqua, seafoam green (Hue 120-210)
        uint8_t hue = 120 + (uint8_t)(hue_norm * 90);
        uint8_t sat = 180 + (uint8_t)(intensity * 75);
        uint8_t val = 120 + (uint8_t)(intensity * 135);
        return CHSV(hue, sat, val);
    }
    
    CRGB mapNeonNights(float hue_norm, float intensity, float special_param) {
        // Electric pink, cyan, purple, lime green - high contrast
        uint8_t base_hues[] = {0, 85, 128, 192}; // Red, Cyan, Pink, Purple
        uint8_t selected_hue = base_hues[(int)(hue_norm * 4) % 4];
        uint8_t sat = 255; // Maximum saturation for neon effect
        uint8_t val = 100 + (uint8_t)(intensity * 155);
        return CHSV(selected_hue, sat, val);
    }
    
    CRGB mapForestWhisper(float hue_norm, float intensity, float special_param) {
        // Deep greens, earth browns, golden highlights (Hue 60-150)
        uint8_t hue = 60 + (uint8_t)(hue_norm * 90);
        uint8_t sat = 150 + (uint8_t)(intensity * 105);
        uint8_t val = 100 + (uint8_t)(intensity * 155);
        return CHSV(hue, sat, val);
    }
    
    CRGB mapGalaxyExpress(float hue_norm, float intensity, float special_param) {
        // Deep purples, cosmic blues, silver stars (Hue 200-300)
        if (special_param > 0.8f) {
            // Silver/white stars
            uint8_t brightness = 200 + (uint8_t)(intensity * 55);
            return CRGB(brightness, brightness, brightness);
        } else {
            uint8_t hue = 200 + (uint8_t)(hue_norm * 100);
            uint8_t sat = 180 + (uint8_t)(intensity * 75);
            uint8_t val = 80 + (uint8_t)(intensity * 175);
            return CHSV(hue, sat, val);
        }
    }
};

// ALL 10 ALGORITHM IMPLEMENTATIONS
CRGB drawCosmicSwirl(const RingCoord& coord, uint32_t time_ms, ColorPaletteManager& palette) {
    float time_factor = time_ms * 0.0008f;
    
    // Multi-octave noise for organic complexity
    float noise1 = inoise16(coord.x * 2000, coord.y * 2000, time_factor * 1000) / 65536.0f;
    float noise2 = inoise16(coord.x * 1000, coord.y * 1000, time_factor * 2000) / 65536.0f * 0.5f;
    float noise3 = inoise16(coord.x * 4000, coord.y * 4000, time_factor * 500) / 65536.0f * 0.25f;
    
    float combined_noise = noise1 + noise2 + noise3;
    float hue_norm = (combined_noise + coord.angle / (2*M_PI) + 1.0f) * 0.5f;
    float intensity = (combined_noise + 1.0f) * 0.5f;
    
    return palette.mapColor(hue_norm, intensity);
}

CRGB drawElectricStorm(const RingCoord& coord, uint32_t time_ms, ColorPaletteManager& palette) {
    uint32_t fast_time = time_ms << 3; // 8x time acceleration
    
    float x_noise = coord.x * 8000;
    float y_noise = coord.y * 8000;
    
    uint16_t noise1 = inoise16(x_noise, y_noise, fast_time);
    uint16_t noise2 = inoise16(x_noise + 10000, y_noise + 10000, fast_time + 5000);
    
    uint8_t threshold = 200;
    bool lightning = (noise1 >> 8) > threshold || (noise2 >> 8) > threshold;
    
    if (lightning) {
        float lightning_intensity = max((noise1 >> 8) - threshold, (noise2 >> 8) - threshold) / 55.0f;
        return palette.mapColor(0.7f, lightning_intensity, 1.0f); // Special lightning effect
    } else {
        float storm_intensity = (noise1 >> 8) / 1020.0f; // Very low intensity for background
        return palette.mapColor(0.6f, storm_intensity);
    }
}

CRGB drawLavaLamp(const RingCoord& coord, uint32_t time_ms, ColorPaletteManager& palette) {
    float slow_time = time_ms * 0.0002f;
    
    float blob_scale = 800;
    uint16_t primary_noise = inoise16(coord.x * blob_scale, coord.y * blob_scale, slow_time * 1000);
    uint16_t secondary_noise = inoise16(coord.x * blob_scale * 0.5f, coord.y * blob_scale * 0.5f, slow_time * 1500);
    
    float blob_value = (primary_noise + secondary_noise * 0.3f) / 65536.0f;
    
    if (blob_value > 0.6f) {
        // Hot blob center
        float intensity = (blob_value - 0.6f) / 0.4f;
        return palette.mapColor(0.1f, intensity); // Warm colors
    } else if (blob_value > 0.3f) {
        // Blob edge gradient
        float edge_factor = (blob_value - 0.3f) / 0.3f;
        return palette.mapColor(0.2f, edge_factor);
    } else {
        // Background
        return palette.mapColor(0.8f, 0.2f); // Cool background
    }
}

CRGB drawDigitalRain(const RingCoord& coord, uint32_t time_ms, ColorPaletteManager& palette) {
    float vertical_pos = sin(coord.angle) * 0.5f + 0.5f;
    float cascade_speed = 0.002f;
    float time_offset = time_ms * cascade_speed;
    
    int stream_id = (int)(coord.angle * 10) % 8;
    float stream_phase = fmod(vertical_pos + time_offset + stream_id * 0.125f, 1.0f);
    
    uint16_t noise = inoise16(stream_id * 1000, stream_phase * 10000, time_ms / 4);
    uint8_t digital_value = (noise >> 8) > 128 ? 255 : 0;
    
    if (digital_value > 0) {
        float intensity = 1.0f - stream_phase * 0.8f; // Fade trailing
        return palette.mapColor(0.4f, intensity); // Matrix green area
    } else {
        return CRGB::Black;
    }
}

CRGB drawGlitchCity(const RingCoord& coord, uint32_t time_ms, ColorPaletteManager& palette) {
    uint32_t glitch_time = (time_ms / 100) * 100; // Quantize time
    
    uint16_t noise1 = inoise16(coord.x * 3000, coord.y * 3000, glitch_time);
    uint16_t noise2 = inoise16(coord.x * 5000, coord.y * 5000, glitch_time + 1000);
    
    uint16_t glitch_value = noise1 ^ noise2; // XOR for harsh digital effects
    
    if ((glitch_value & 0xF000) == 0xF000) {
        return CRGB(255, 255, 255); // Full-bright glitch flash
    }
    
    float intensity = (glitch_value & 0xFF) / 255.0f;
    float hue_chaos = ((glitch_value >> 8) & 0xFF) / 255.0f;
    
    return palette.mapColor(hue_chaos, intensity, 0.5f);
}

CRGB drawOceanDepths(const RingCoord& coord, uint32_t time_ms, ColorPaletteManager& palette) {
    float ocean_time = time_ms * 0.0005f;
    
    float current1 = inoise16(coord.x * 1200, coord.y * 1200, ocean_time * 800) / 65536.0f;
    float current2 = inoise16(coord.x * 2400, coord.y * 2400, ocean_time * 600) / 65536.0f * 0.5f;
    float current3 = inoise16(coord.x * 600, coord.y * 600, ocean_time * 1000) / 65536.0f * 0.3f;
    
    float depth_factor = (current1 + current2 + current3 + 1.5f) / 3.0f;
    float hue_variation = (current2 + 0.5f);
    
    return palette.mapColor(hue_variation, depth_factor);
}

CRGB drawFireDance(const RingCoord& coord, uint32_t time_ms, ColorPaletteManager& palette) {
    float vertical_component = sin(coord.angle) * 0.5f + 0.5f;
    
    float flame_x = coord.x * 1500;
    float flame_y = coord.y * 1500 + time_ms * 0.003f;
    
    uint16_t turbulence = inoise16(flame_x, flame_y, time_ms);
    float flame_intensity = (turbulence / 65536.0f) * (1.0f - vertical_component * 0.3f);
    
    float fire_hue = flame_intensity * 0.15f; // Red to orange range
    return palette.mapColor(fire_hue, flame_intensity);
}

CRGB drawNebulaDrift(const RingCoord& coord, uint32_t time_ms, ColorPaletteManager& palette) {
    float nebula_time = time_ms * 0.0003f;
    
    float cloud1 = inoise16(coord.x * 800, coord.y * 800, nebula_time * 1000) / 65536.0f;
    float cloud2 = inoise16(coord.x * 1600, coord.y * 1600, nebula_time * 700) / 65536.0f * 0.5f;
    float cloud3 = inoise16(coord.x * 400, coord.y * 400, nebula_time * 1200) / 65536.0f * 0.25f;
    
    float nebula_density = cloud1 + cloud2 + cloud3;
    
    uint16_t star_noise = inoise16(coord.x * 4000, coord.y * 4000, nebula_time * 200);
    bool is_star = (star_noise > 60000);
    
    if (is_star) {
        float star_intensity = (star_noise - 60000) / 5536.0f;
        return palette.mapColor(0.0f, star_intensity, 1.0f); // Stars
    } else {
        float hue_drift = (nebula_density + 1.0f) * 0.5f;
        float intensity = (nebula_density + 1.0f) * 0.4f;
        return palette.mapColor(hue_drift, intensity);
    }
}

CRGB drawBinaryPulse(const RingCoord& coord, uint32_t time_ms, ColorPaletteManager& palette) {
    float pulse_period = 2000.0f;
    float pulse_phase = fmod(time_ms, pulse_period) / pulse_period;
    
    float distance_from_center = sqrt(coord.x * coord.x + coord.y * coord.y);
    
    float ring_frequency = 5.0f;
    float pulse_offset = pulse_phase * 2.0f;
    float ring_value = sin((distance_from_center * ring_frequency - pulse_offset) * 2 * M_PI);
    
    uint16_t noise = inoise16(coord.x * 2000, coord.y * 2000, time_ms / 8);
    float digital_mod = ((noise >> 8) > 128) ? 1.0f : -0.5f;
    
    float final_value = ring_value * digital_mod;
    
    if (final_value > 0.3f) {
        return palette.mapColor(0.8f, final_value, 0.8f); // Active pulse
    } else if (final_value > -0.2f) {
        float transition_intensity = (final_value + 0.2f) * 2.0f;
        return palette.mapColor(0.3f, transition_intensity); // Transition zones
    } else {
        return palette.mapColor(0.7f, 0.1f); // Background
    }
}

// Enhanced variant manager with ALL 10 ALGORITHMS and smooth transitions
class NoiseVariantManager {
private:
    uint8_t current_variant = 0;
    uint8_t target_variant = 0;
    float transition_progress = 1.0f; // 0.0 = old, 1.0 = new
    uint32_t transition_start = 0;
    static const uint32_t TRANSITION_DURATION = 1500; // 1.5 second fade for smoother transitions
    
    // Algorithm instances
    PlasmaWaveGenerator plasma_gen;
    PlasmaParams plasma_params;
    ColorPaletteManager& palette_manager;

public:
    NoiseVariantManager(ColorPaletteManager& palette_mgr) : palette_manager(palette_mgr) {}
    
    void update(uint32_t now, bool auto_cycle_enabled, uint8_t manual_variant, const PlasmaParams& params) {
        plasma_params = params;
        
        // Handle automatic cycling vs manual override
        if (auto_cycle_enabled) {
            EVERY_N_MILLISECONDS(12000) { // Slightly longer for each variant
                startTransition((current_variant + 1) % 10, now); // ALL 10 variants
            }
        } else if (manual_variant != target_variant && transition_progress >= 1.0f) {
            // Manual override
            startTransition(manual_variant, now);
        }
        
        // Update transition progress
        if (transition_progress < 1.0f) {
            uint32_t elapsed = now - transition_start;
            transition_progress = min(1.0f, elapsed / (float)TRANSITION_DURATION);
            
            if (transition_progress >= 1.0f) {
                current_variant = target_variant;
            }
        }
    }
    
    CRGB renderPixel(const RingCoord& coord, uint32_t time_ms) {
        if (transition_progress >= 1.0f) {
            // No transition, render current variant
            return renderVariant(current_variant, coord, time_ms);
        } else {
            // Advanced cross-fade with brightness preservation
            CRGB old_color = renderVariant(current_variant, coord, time_ms);
            CRGB new_color = renderVariant(target_variant, coord, time_ms);
            return smoothLerpCRGB(old_color, new_color, transition_progress);
        }
    }
    
    uint8_t getCurrentVariant() const { return current_variant; }
    const char* getCurrentVariantName() const {
        const char* names[] = {
            "Cosmic Swirl", "Electric Storm", "Lava Lamp", "Digital Rain", "Plasma Waves",
            "Glitch City", "Ocean Depths", "Fire Dance", "Nebula Drift", "Binary Pulse"
        };
        return names[current_variant % 10];
    }

private:
    void startTransition(uint8_t new_variant, uint32_t now) {
        target_variant = new_variant % 10; // Ensure valid range
        transition_start = now;
        transition_progress = 0.0f;
    }
    
    CRGB renderVariant(uint8_t variant, const RingCoord& coord, uint32_t time_ms) {
        switch(variant % 10) {
            case 0: return drawCosmicSwirl(coord, time_ms, palette_manager);
            case 1: return drawElectricStorm(coord, time_ms, palette_manager);
            case 2: return drawLavaLamp(coord, time_ms, palette_manager);
            case 3: return drawDigitalRain(coord, time_ms, palette_manager);
            case 4: return drawPlasmaWithPalette(coord, time_ms, palette_manager);
            case 5: return drawGlitchCity(coord, time_ms, palette_manager);
            case 6: return drawOceanDepths(coord, time_ms, palette_manager);
            case 7: return drawFireDance(coord, time_ms, palette_manager);
            case 8: return drawNebulaDrift(coord, time_ms, palette_manager);
            case 9: return drawBinaryPulse(coord, time_ms, palette_manager);
            default: return drawCosmicSwirl(coord, time_ms, palette_manager);
        }
    }
    
    // Enhanced Plasma Waves with palette integration
    CRGB drawPlasmaWithPalette(const RingCoord& coord, uint32_t time_ms, ColorPaletteManager& palette) {
        // Generate base plasma waves
        CRGB plasma_color = plasma_gen.calculatePlasmaPixel(coord, time_ms, plasma_params);
        
        // Extract intensity and hue information from plasma
        float intensity = (plasma_color.r + plasma_color.g + plasma_color.b) / 765.0f;
        
        // Calculate wave interference for hue mapping
        float time_scaled = time_ms * plasma_params.time_scale * 0.001f;
        float wave_sum = 0.0f;
        
        // Simplified wave calculation for hue determination
        float dx = coord.x - 0.5f;
        float dy = coord.y - 0.5f;
        float distance = sqrt(dx*dx + dy*dy);
        float wave_phase = distance * 2.0f + time_scaled * 1.5f;
        wave_sum = sin(wave_phase);
        
        float hue_norm = (wave_sum + 1.0f) * 0.5f; // Normalize to 0-1
        
        // Use palette system for consistent color theming
        return palette.mapColor(hue_norm, intensity, intensity > 0.8f ? 1.0f : 0.0f);
    }
    
    // Enhanced interpolation with brightness preservation and smooth curves
    CRGB smoothLerpCRGB(const CRGB& a, const CRGB& b, float t) {
        // Apply smooth curve to transition
        float smooth_t = t * t * (3.0f - 2.0f * t); // Smoothstep function
        
        // Preserve brightness during transition to avoid flickering
        float brightness_a = (a.r + a.g + a.b) / 765.0f;
        float brightness_b = (b.r + b.g + b.b) / 765.0f;
        float target_brightness = brightness_a + (brightness_b - brightness_a) * smooth_t;
        
        CRGB result = CRGB(
            a.r + (int)((b.r - a.r) * smooth_t),
            a.g + (int)((b.g - a.g) * smooth_t),
            a.b + (int)((b.b - a.b) * smooth_t)
        );
        
        // Brightness compensation
        float current_brightness = (result.r + result.g + result.b) / 765.0f;
        if (current_brightness > 0.01f) {
            float compensation = target_brightness / current_brightness;
            compensation = min(compensation, 2.0f); // Limit boost
            result.r = min(255, (int)(result.r * compensation));
            result.g = min(255, (int)(result.g * compensation));
            result.b = min(255, (int)(result.b * compensation));
        }
        
        return result;
    }
};

// ALL 10 Variant names for UI
fl::string variant_names[10] = {
    "Cosmic Swirl", "Electric Storm", "Lava Lamp", "Digital Rain", "Plasma Waves",
    "Glitch City", "Ocean Depths", "Fire Dance", "Nebula Drift", "Binary Pulse"
};

// 5 Color Palette names for UI
fl::string palette_names[5] = {
    "Sunset Boulevard", "Ocean Breeze", "Neon Nights", "Forest Whisper", "Galaxy Express"
};

// Helper functions to get indices from names
uint8_t getVariantIndex(const fl::string& name) {
    for (int i = 0; i < 10; i++) {
        if (variant_names[i] == name) {
            return i;
        }
    }
    return 0; // Default to first variant
}

uint8_t getPaletteIndex(const fl::string& name) {
    for (int i = 0; i < 5; i++) {
        if (palette_names[i] == name) {
            return i;
        }
    }
    return 0; // Default to first palette
}

// Global instances - order matters for initialization
ColorPaletteManager palette_manager;
NoiseVariantManager variant_manager(palette_manager);
RingLUT ring_lut;

// KICKASS UI controls - comprehensive control suite
UISlider brightness("Brightness", 1, 0, 1);
UISlider scale("Scale", 4, .1, 4, .1);
UISlider timeBitshift("Time Bitshift", 5, 0, 16, 1);
UISlider timescale("Time Scale", 1, .1, 10, .1);

// Advanced variant and palette controls
UIDropdown variants("Noise Variants", variant_names);
UIDropdown palettes("Color Palettes", palette_names);
UICheckbox autoCycle("Auto Cycle Effects", true);
UICheckbox autoPalette("Auto Cycle Palettes", true);
// This PIR type is special because it will bind to a pin for a real device,
// but also provides a UIButton when run in the simulator.
Pir pir(PIN_PIR, PIR_LATCH_MS, PIR_RISING_TIME, PIR_FALLING_TIME);
UICheckbox useDither("Use Binary Dither", true);

Timer timer;
float current_brightness = 0;

// Save a pointer to the controller so that we can modify the dither in real time.
CLEDController* controller = nullptr;

void setup() {
    Serial.begin(115200);
    // ScreenMap is purely something that is needed for the sketch to correctly
    // show on the web display. For deployements to real devices, this essentially
    // becomes a no-op.
    ScreenMap xyMap = ScreenMap::Circle(NUM_LEDS, 2.0, 2.0);
    controller = &FastLED.addLeds<WS2811, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
                .setCorrection(TypicalLEDStrip)
                .setDither(DISABLE_DITHER)
                .setScreenMap(xyMap);
    FastLED.setBrightness(brightness);
    pir.activate(millis());  // Activate the PIR sensor on startup.
    
    // Initialize performance optimizations
    ring_lut.initialize();
}

void draw(uint32_t now) {
    // Configure plasma parameters from UI controls with enhanced scaling
    PlasmaParams plasma_params;
    plasma_params.time_scale = timescale.as<float>();
    plasma_params.noise_intensity = scale.as<float>() * 0.8f; // Slightly reduce for better visual balance
    plasma_params.brightness = brightness.as<float>();
    plasma_params.time_bitshift = timeBitshift.as<int>();
    plasma_params.hue_offset = (now / 100) % 256; // Slow hue rotation for extra dynamism
    plasma_params.noise_amplitude = 0.6f + 0.4f * sin(now * 0.001f); // Breathing noise effect
    
    // Update palette manager with auto-cycling and manual control
    palette_manager.update(now, autoPalette.value(), getPaletteIndex(palettes.value()));
    
    // Update variant manager with enhanced parameters
    variant_manager.update(now, autoCycle.value(), getVariantIndex(variants.value()), plasma_params);
    
    // KICKASS rendering with performance optimizations
    for (int i = 0; i < NUM_LEDS; i++) {
        RingCoord coord = ring_lut.fastRingCoord(i);
        CRGB pixel_color = variant_manager.renderPixel(coord, now);
        
        // Apply global brightness and gamma correction for better visual quality
        float global_brightness = brightness.as<float>();
        pixel_color.r = (uint8_t)(pixel_color.r * global_brightness);
        pixel_color.g = (uint8_t)(pixel_color.g * global_brightness);
        pixel_color.b = (uint8_t)(pixel_color.b * global_brightness);
        
        leds[i] = pixel_color;
    }
    
    // Optional: Add subtle sparkle overlay for extra visual interest
    EVERY_N_MILLISECONDS(50) {
        // Add random sparkles to 1% of LEDs
        int sparkle_count = NUM_LEDS / 100 + 1;
        for (int s = 0; s < sparkle_count; s++) {
            int sparkle_pos = random16() % NUM_LEDS;
            if (random8() > 250) { // Very rare sparkles
                leds[sparkle_pos] = blend(leds[sparkle_pos], CRGB::White, 128);
            }
        }
    }
}

void loop() {
    // Allow the dither to be enabled and disabled.
    controller->setDither(useDither ? BINARY_DITHER : DISABLE_DITHER);
    uint32_t now = millis();
    uint8_t bri = pir.transition(now);
    FastLED.setBrightness(bri * brightness.as<float>());
    // Apply leds generation to the leds.
    draw(now);
    
    
    FastLED.show();
}
