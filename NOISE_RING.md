# NoiseRing Enhanced Design Document

## Overview
Enhanced version of the FxNoiseRing example that automatically cycles through different noise effects and color palettes, providing dynamic visual variety with user controls for manual selection.

**Featured Implementation**: **Plasma Waves** - Advanced graphics technique showcasing sine wave interference with noise modulation, demonstrating sophisticated mathematical visualization on circular LED arrays.

## Core Features

### Automatic Cycling
- **Palette Rotation**: Every 5 seconds using `EVERY_N_MILLISECONDS(5000)`
- **Noise Effect Rotation**: Every 10 seconds using `EVERY_N_MILLISECONDS(10000)`
- **User Override**: Dropdown controls allow manual selection to override automatic cycling

### User Interface Controls
- **Variants Dropdown**: "Noise Variants" - Manual selection of noise effects (0-9)
- **Palettes Dropdown**: "Color Palettes" - Manual selection of color schemes (0-4)
- **Auto Cycle Checkbox**: "Auto Cycle" - Enable/disable automatic rotation
- **Existing Controls**: Retain all current sliders (Brightness, Scale, Time Bitshift, Time Scale, PIR, Dither)

## 10 Noise Variations - Detailed Algorithmic Implementation

### 1. "Cosmic Swirl" - Enhanced Perlin Flow
- **Description**: Classic perlin noise with slow, flowing movements using multi-octave complexity
- **Parameters**: Base noise with moderate scale, gentle time progression
- **Characteristics**: Smooth gradients, organic flow patterns

**Algorithm**:
```cpp
CRGB drawCosmicSwirl(const RingCoord& coord, uint32_t time_ms) {
    float time_factor = time_ms * 0.0008f;
    
    // Multi-octave noise for organic complexity
    float noise1 = inoise16(coord.x * 2000, coord.y * 2000, time_factor * 1000) / 65536.0f;
    float noise2 = inoise16(coord.x * 1000, coord.y * 1000, time_factor * 2000) / 65536.0f * 0.5f;
    float noise3 = inoise16(coord.x * 4000, coord.y * 4000, time_factor * 500) / 65536.0f * 0.25f;
    
    float combined_noise = noise1 + noise2 + noise3;
    
    // Flowing hue with gentle progression
    uint8_t hue = (uint8_t)((combined_noise + coord.angle / (2*M_PI)) * 255) % 256;
    uint8_t sat = 220 + (uint8_t)(abs(noise2) * 35);
    uint8_t val = 180 + (uint8_t)(combined_noise * 75);
    
    return CHSV(hue, sat, val);
}
```

### 2. "Electric Storm" - High-Frequency Chaos
- **Description**: High-frequency noise with rapid temporal changes creating lightning effects
- **Parameters**: 8x time acceleration, high spatial frequency, quantized thresholds
- **Characteristics**: Crackling, energetic, lightning-like patterns

**Algorithm**:
```cpp
CRGB drawElectricStorm(const RingCoord& coord, uint32_t time_ms) {
    // Rapid temporal changes with quantized effects
    uint32_t fast_time = time_ms << 3; // 8x time acceleration
    
    // High-frequency spatial noise
    float x_noise = coord.x * 8000;
    float y_noise = coord.y * 8000;
    
    uint16_t noise1 = inoise16(x_noise, y_noise, fast_time);
    uint16_t noise2 = inoise16(x_noise + 10000, y_noise + 10000, fast_time + 5000);
    
    // Create lightning-like quantization
    uint8_t threshold = 200;
    bool lightning = (noise1 >> 8) > threshold || (noise2 >> 8) > threshold;
    
    if (lightning) {
        // Bright electric flash
        uint8_t intensity = max((noise1 >> 8) - threshold, (noise2 >> 8) - threshold) * 4;
        return CRGB(intensity, intensity, 255); // Electric blue-white
    } else {
        // Dark storm background
        uint8_t hue = 160 + ((noise1 >> 10) % 32); // Blue-purple range
        return CHSV(hue, 255, (noise1 >> 8) / 4); // Low brightness
    }
}
```

### 3. "Lava Lamp" - Slow Blobby Movement
- **Description**: Slow, blobby movements with high contrast using low-frequency modulation
- **Parameters**: Ultra-low frequency, high amplitude, threshold-based blob creation
- **Characteristics**: Large, slow-moving color blobs with organic boundaries

**Algorithm**:
```cpp
CRGB drawLavaLamp(const RingCoord& coord, uint32_t time_ms) {
    float slow_time = time_ms * 0.0002f; // Very slow movement
    
    // Large-scale blob generation
    float blob_scale = 800; // Large spatial scale for big blobs
    uint16_t primary_noise = inoise16(coord.x * blob_scale, coord.y * blob_scale, slow_time * 1000);
    uint16_t secondary_noise = inoise16(coord.x * blob_scale * 0.5f, coord.y * blob_scale * 0.5f, slow_time * 1500);
    
    // Create blob boundaries with thresholding
    float blob_value = (primary_noise + secondary_noise * 0.3f) / 65536.0f;
    
    // High contrast blob regions
    if (blob_value > 0.6f) {
        // Hot blob center
        uint8_t hue = 0 + (uint8_t)((blob_value - 0.6f) * 400); // Red to orange
        return CHSV(hue, 255, 255);
    } else if (blob_value > 0.3f) {
        // Blob edge gradient
        float edge_factor = (blob_value - 0.3f) / 0.3f;
        uint8_t brightness = (uint8_t)(edge_factor * 255);
        return CHSV(20, 200, brightness); // Orange edge
    } else {
        // Background
        return CHSV(240, 100, 30); // Dark blue background
    }
}
```

### 4. "Digital Rain" - Matrix Cascade
- **Description**: Matrix-style cascading effect using vertical noise mapping
- **Parameters**: Angle-to-vertical conversion, time-based cascade, stream segregation
- **Characteristics**: Vertical streams, binary-like transitions, matrix green

**Algorithm**:
```cpp
CRGB drawDigitalRain(const RingCoord& coord, uint32_t time_ms) {
    // Convert angle to vertical position for cascade effect
    float vertical_pos = sin(coord.angle) * 0.5f + 0.5f; // 0-1 range
    
    // Time-based cascade with varying speeds
    float cascade_speed = 0.002f;
    float time_offset = time_ms * cascade_speed;
    
    // Create vertical streams
    int stream_id = (int)(coord.angle * 10) % 8; // 8 distinct streams
    float stream_phase = fmod(vertical_pos + time_offset + stream_id * 0.125f, 1.0f);
    
    // Binary-like transitions
    uint16_t noise = inoise16(stream_id * 1000, stream_phase * 10000, time_ms / 4);
    uint8_t digital_value = (noise >> 8) > 128 ? 255 : 0;
    
    // Matrix green with digital artifacts
    uint8_t green_intensity = digital_value;
    uint8_t trailing = max(0, green_intensity - (int)(stream_phase * 200));
    
    return CRGB(0, green_intensity, trailing / 2);
}
```

### 5. "Plasma Waves" - **FEATURED IMPLEMENTATION** 
- **Description**: Multiple overlapping sine waves with noise modulation creating electromagnetic plasma effects
- **Parameters**: 4-source wave interference, noise modulation, dynamic color mapping
- **Characteristics**: Smooth wave interference patterns, flowing electromagnetic appearance

**Algorithm**:
```cpp
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

struct PlasmaParams {
    float time_scale = 1.0f;
    float noise_intensity = 0.5f;
    float noise_amplitude = 0.8f;
    uint8_t time_bitshift = 5;
    uint8_t hue_offset = 0;
    float brightness = 1.0f;
};
```

### 6. "Glitch City" - Chaotic Digital Artifacts
- **Description**: Chaotic, stuttering effects with quantized noise and bit manipulation
- **Parameters**: Time quantization, XOR operations, random bit shifts
- **Characteristics**: Harsh transitions, digital artifacts, strobe-like effects

**Algorithm**:
```cpp
CRGB drawGlitchCity(const RingCoord& coord, uint32_t time_ms) {
    // Stuttering time progression
    uint32_t glitch_time = (time_ms / 100) * 100; // Quantize time to create stutters
    
    // Bit manipulation for digital artifacts
    uint16_t noise1 = inoise16(coord.x * 3000, coord.y * 3000, glitch_time);
    uint16_t noise2 = inoise16(coord.x * 5000, coord.y * 5000, glitch_time + 1000);
    
    // XOR operation for harsh digital effects
    uint16_t glitch_value = noise1 ^ noise2;
    
    // Random bit shifts for channel corruption
    uint8_t r = (glitch_value >> (time_ms % 8)) & 0xFF;
    uint8_t g = (glitch_value << (time_ms % 5)) & 0xFF;
    uint8_t b = ((noise1 | noise2) >> 4) & 0xFF;
    
    // Occasional full-bright flashes
    if ((glitch_value & 0xF000) == 0xF000) {
        return CRGB(255, 255, 255);
    }
    
    return CRGB(r, g, b);
}
```

### 7. "Ocean Depths" - Underwater Currents
- **Description**: Slow, deep undulations mimicking underwater currents with blue-green bias
- **Parameters**: Ultra-low frequency, blue-green color bias, depth-based brightness
- **Characteristics**: Calm, flowing, deep water feel with gentle undulations

**Algorithm**:
```cpp
CRGB drawOceanDepths(const RingCoord& coord, uint32_t time_ms) {
    float ocean_time = time_ms * 0.0005f; // Very slow like deep water
    
    // Multi-layer current simulation
    float current1 = inoise16(coord.x * 1200, coord.y * 1200, ocean_time * 800) / 65536.0f;
    float current2 = inoise16(coord.x * 2400, coord.y * 2400, ocean_time * 600) / 65536.0f * 0.5f;
    float current3 = inoise16(coord.x * 600, coord.y * 600, ocean_time * 1000) / 65536.0f * 0.3f;
    
    float depth_factor = current1 + current2 + current3;
    
    // Ocean color palette (blue-green spectrum)
    uint8_t base_hue = 140; // Cyan-blue
    uint8_t hue_variation = (uint8_t)(abs(depth_factor) * 40); // Vary within blue-green
    uint8_t final_hue = (base_hue + hue_variation) % 256;
    
    // Depth-based brightness (deeper = darker)
    uint8_t depth_brightness = 120 + (uint8_t)(depth_factor * 135);
    uint8_t saturation = 200 + (uint8_t)(abs(current2) * 55);
    
    return CHSV(final_hue, saturation, depth_brightness);
}
```

### 8. "Fire Dance" - Upward Flame Simulation
- **Description**: Flickering, flame-like patterns with upward bias and turbulent noise
- **Parameters**: Vertical gradient bias, turbulent noise, fire color palette
- **Characteristics**: Orange/red dominated, upward movement, flickering

**Algorithm**:
```cpp
CRGB drawFireDance(const RingCoord& coord, uint32_t time_ms) {
    // Vertical bias for upward flame movement
    float vertical_component = sin(coord.angle) * 0.5f + 0.5f; // 0 at bottom, 1 at top
    
    // Turbulent noise with upward bias
    float flame_x = coord.x * 1500;
    float flame_y = coord.y * 1500 + time_ms * 0.003f; // Upward drift
    
    uint16_t turbulence = inoise16(flame_x, flame_y, time_ms);
    float flame_intensity = (turbulence / 65536.0f) * (1.0f - vertical_component * 0.3f);
    
    // Fire color palette (red->orange->yellow)
    uint8_t base_hue = 0; // Red
    uint8_t hue_variation = (uint8_t)(flame_intensity * 45); // Up to orange/yellow
    uint8_t final_hue = (base_hue + hue_variation) % 256;
    
    uint8_t saturation = 255 - (uint8_t)(vertical_component * 100); // Less saturated at top
    uint8_t brightness = (uint8_t)(flame_intensity * 255);
    
    return CHSV(final_hue, saturation, brightness);
}
```

### 9. "Nebula Drift" - Cosmic Cloud Simulation
- **Description**: Slow cosmic clouds with starfield sparkles using multi-octave noise
- **Parameters**: Multiple noise octaves, sparse bright spots, cosmic color palette
- **Characteristics**: Misty backgrounds with occasional bright stars

**Algorithm**:
```cpp
CRGB drawNebulaDrift(const RingCoord& coord, uint32_t time_ms) {
    float nebula_time = time_ms * 0.0003f; // Cosmic slow drift
    
    // Multi-octave nebula clouds
    float cloud1 = inoise16(coord.x * 800, coord.y * 800, nebula_time * 1000) / 65536.0f;
    float cloud2 = inoise16(coord.x * 1600, coord.y * 1600, nebula_time * 700) / 65536.0f * 0.5f;
    float cloud3 = inoise16(coord.x * 400, coord.y * 400, nebula_time * 1200) / 65536.0f * 0.25f;
    
    float nebula_density = cloud1 + cloud2 + cloud3;
    
    // Sparse starfield generation
    uint16_t star_noise = inoise16(coord.x * 4000, coord.y * 4000, nebula_time * 200);
    bool is_star = (star_noise > 60000); // Very sparse stars
    
    if (is_star) {
        // Bright white/blue stars
        uint8_t star_brightness = 200 + ((star_noise - 60000) / 256);
        return CRGB(star_brightness, star_brightness, 255);
    } else {
        // Nebula background
        uint8_t nebula_hue = 200 + (uint8_t)(nebula_density * 80); // Purple-pink spectrum
        uint8_t nebula_sat = 150 + (uint8_t)(abs(cloud2) * 105);
        uint8_t nebula_bright = 40 + (uint8_t)(nebula_density * 120);
        
        return CHSV(nebula_hue, nebula_sat, nebula_bright);
    }
}
```

### 10. "Binary Pulse" - Digital Heartbeat
- **Description**: Digital heartbeat with expanding/contracting rings using threshold-based noise
- **Parameters**: Concentric pattern generation, rhythmic pulsing, geometric thresholds
- **Characteristics**: Rhythmic, geometric, tech-inspired

**Algorithm**:
```cpp
CRGB drawBinaryPulse(const RingCoord& coord, uint32_t time_ms) {
    // Create rhythmic heartbeat timing
    float pulse_period = 2000.0f; // 2-second pulse cycle
    float pulse_phase = fmod(time_ms, pulse_period) / pulse_period; // 0-1 cycle
    
    // Generate expanding rings from center
    float distance_from_center = sqrt(coord.x * coord.x + coord.y * coord.y);
    
    // Pulse wave propagation
    float ring_frequency = 5.0f; // Number of rings
    float pulse_offset = pulse_phase * 2.0f; // Expanding wave
    float ring_value = sin((distance_from_center * ring_frequency - pulse_offset) * 2 * M_PI);
    
    // Digital quantization
    uint16_t noise = inoise16(coord.x * 2000, coord.y * 2000, time_ms / 8);
    float digital_mod = ((noise >> 8) > 128) ? 1.0f : -0.5f;
    
    float final_value = ring_value * digital_mod;
    
    // Binary color mapping
    if (final_value > 0.3f) {
        // Active pulse regions
        uint8_t intensity = (uint8_t)(final_value * 255);
        return CRGB(intensity, 0, intensity); // Magenta pulse
    } else if (final_value > -0.2f) {
        // Transition zones
        uint8_t dim_intensity = (uint8_t)((final_value + 0.2f) * 500);
        return CRGB(0, dim_intensity, 0); // Green transitions
    } else {
        // Background
        return CRGB(10, 0, 20); // Dark purple background
    }
}
```

## 5 Color Palettes

### 1. "Sunset Boulevard"
- **Colors**: Warm oranges, deep reds, golden yellows
- **Description**: Classic sunset gradient perfect for relaxing ambiance
- **HSV Range**: Hue 0-45, high saturation, varying brightness

### 2. "Ocean Breeze"
- **Colors**: Deep blues, aqua, seafoam green, white caps
- **Description**: Cool ocean palette for refreshing visual effects
- **HSV Range**: Hue 120-210, medium-high saturation

### 3. "Neon Nights"
- **Colors**: Electric pink, cyan, purple, lime green
- **Description**: Cyberpunk-inspired high-contrast palette
- **HSV Range**: Saturated primaries, high brightness contrasts

### 4. "Forest Whisper"
- **Colors**: Deep greens, earth browns, golden highlights
- **Description**: Natural woodland palette for organic feels
- **HSV Range**: Hue 60-150, natural saturation levels

### 5. "Galaxy Express"
- **Colors**: Deep purples, cosmic blues, silver stars, pink nebula
- **Description**: Space-themed palette for cosmic adventures
- **HSV Range**: Hue 200-300, with bright white accents

## Implementation Strategy

### Core Mathematical Framework

```cpp
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
```

### Enhanced Control System with Smooth Transitions

```cpp
class NoiseVariantManager {
private:
uint8_t current_variant = 0;
    uint8_t target_variant = 0;
    float transition_progress = 1.0f; // 0.0 = old, 1.0 = new
    uint32_t transition_start = 0;
    static const uint32_t TRANSITION_DURATION = 1000; // 1 second fade
    
    // Algorithm instances
    PlasmaWaveGenerator plasma_gen;
    PlasmaParams plasma_params;

public:
    void update(uint32_t now, bool auto_cycle_enabled, uint8_t manual_variant) {
        // Handle automatic cycling vs manual override
        if (auto_cycle_enabled) {
            EVERY_N_MILLISECONDS(10000) {
                startTransition((current_variant + 1) % 10, now);
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
            // Blend between variants
            CRGB old_color = renderVariant(current_variant, coord, time_ms);
            CRGB new_color = renderVariant(target_variant, coord, time_ms);
            return lerpCRGB(old_color, new_color, transition_progress);
        }
    }

private:
    void startTransition(uint8_t new_variant, uint32_t now) {
        target_variant = new_variant;
        transition_start = now;
        transition_progress = 0.0f;
    }
    
    CRGB renderVariant(uint8_t variant, const RingCoord& coord, uint32_t time_ms) {
        switch(variant) {
            case 0: return drawCosmicSwirl(coord, time_ms);
            case 1: return drawElectricStorm(coord, time_ms);
            case 2: return drawLavaLamp(coord, time_ms);
            case 3: return drawDigitalRain(coord, time_ms);
            case 4: return plasma_gen.calculatePlasmaPixel(coord, time_ms, plasma_params);
            case 5: return drawGlitchCity(coord, time_ms);
            case 6: return drawOceanDepths(coord, time_ms);
            case 7: return drawFireDance(coord, time_ms);
            case 8: return drawNebulaDrift(coord, time_ms);
            case 9: return drawBinaryPulse(coord, time_ms);
            default: return CRGB::Black;
        }
    }
    
    CRGB lerpCRGB(const CRGB& a, const CRGB& b, float t) {
        return CRGB(
            a.r + (int)((b.r - a.r) * t),
            a.g + (int)((b.g - a.g) * t),
            a.b + (int)((b.b - a.b) * t)
        );
    }
};
```

### Data Structures and UI Integration

```cpp
// Enhanced data structures
String variant_names[10] = {
    "Cosmic Swirl", "Electric Storm", "Lava Lamp", "Digital Rain", "Plasma Waves",
    "Glitch City", "Ocean Depths", "Fire Dance", "Nebula Drift", "Binary Pulse"
};

String palette_names[5] = {
    "Sunset Boulevard", "Ocean Breeze", "Neon Nights", "Forest Whisper", "Galaxy Express"
};

// Global instances
NoiseVariantManager variant_manager;
RingLUT ring_lut;

// Enhanced UI controls
UIDropdown variants("Noise Variants", variant_names, 10);
UIDropdown palettes("Color Palettes", palette_names, 5);
UICheckbox autoCycle("Auto Cycle", true);
```

### Integration with Existing Framework

```cpp
void setup() {
    Serial.begin(115200);
    ScreenMap xyMap = ScreenMap::Circle(NUM_LEDS, 2.0, 2.0);
    controller = &FastLED.addLeds<WS2811, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
                .setCorrection(TypicalLEDStrip)
                .setDither(DISABLE_DITHER)
                .setScreenMap(xyMap);
    FastLED.setBrightness(brightness);
    pir.activate(millis());
    
    // Initialize performance optimizations
    ring_lut.initialize();
}

void draw(uint32_t now) {
    // Update variant manager
    variant_manager.update(now, autoCycle.value(), variants.value());
    
    // Render each LED with current variant
    for (int i = 0; i < NUM_LEDS; i++) {
        RingCoord coord = ring_lut.fastRingCoord(i);
        leds[i] = variant_manager.renderPixel(coord, now);
    }
}

void loop() {
    controller->setDither(useDither ? BINARY_DITHER : DISABLE_DITHER);
    uint32_t now = millis();
    uint8_t bri = pir.transition(now);
    FastLED.setBrightness(bri * brightness.as<float>());
    
    draw(now);
    FastLED.show();
}
```

## Technical Considerations

### Performance Optimization
- **Lookup Table Pre-computation**: Pre-calculate trigonometric values for ring positions
- **Fixed-Point Arithmetic**: Use integer math where possible for embedded systems
- **Noise Caching**: Cache noise parameters between frames for consistent animation
- **Memory-Efficient Algorithms**: Optimize noise calculations for real-time performance
- **Parallel Processing**: Structure algorithms for potential multi-core optimization

### Memory Management
- **PROGMEM Storage**: Store palettes and static data in program memory for Arduino compatibility
- **Dynamic Allocation Avoidance**: Minimize heap usage during effect transitions
- **Stack Optimization**: Use local variables efficiently in nested algorithm calls
- **Buffer Management**: Reuse coordinate calculation buffers where possible

### Mathematical Precision
- **16-bit Noise Space**: Maintain precision in noise calculations before 8-bit mapping
- **Floating Point Efficiency**: Balance precision vs. performance based on target platform
- **Color Space Optimization**: Use HSV for smooth transitions, RGB for final output
- **Numerical Stability**: Prevent overflow/underflow in wave interference calculations

### User Experience
- **Smooth Transitions**: 1-second cross-fade between effects using linear interpolation
- **Responsive Controls**: Immediate override of automatic cycling via manual selection
- **Visual Feedback**: Clear indication of current variant and palette selection
- **Performance Consistency**: Maintain stable frame rate across all effect variants

## First Pass Implementation: Plasma Waves

### Why Start with Plasma Waves?
1. **Visual Impact**: Most impressive demonstration of advanced graphics programming
2. **Mathematical Showcase**: Demonstrates sine wave interference and noise modulation
3. **Building Foundation**: Establishes the RingCoord system used by all other variants
4. **Performance Baseline**: Tests the most computationally intensive algorithm first

### Development Strategy
```cpp
// Phase 1: Core Infrastructure
void setupPlasmaDemo() {
    // Initialize basic ring coordinate system
    ring_lut.initialize();
    
    // Configure plasma parameters
    plasma_params.time_scale = timescale.as<float>();
    plasma_params.noise_intensity = scale.as<float>();
    plasma_params.brightness = brightness.as<float>();
}

// Phase 2: Plasma-Only Implementation
void drawPlasmaOnly(uint32_t now) {
    for (int i = 0; i < NUM_LEDS; i++) {
        RingCoord coord = ring_lut.fastRingCoord(i);
        leds[i] = plasma_gen.calculatePlasmaPixel(coord, now, plasma_params);
    }
}

// Phase 3: Add Manual Variants (No Auto-Cycling Yet)
void drawWithManualSelection(uint32_t now) {
    uint8_t selected_variant = variants.value();
    
    for (int i = 0; i < NUM_LEDS; i++) {
        RingCoord coord = ring_lut.fastRingCoord(i);
        
        switch(selected_variant) {
            case 0: leds[i] = drawCosmicSwirl(coord, now); break;
            case 1: leds[i] = plasma_gen.calculatePlasmaPixel(coord, now, plasma_params); break;
            // Add variants incrementally
            default: leds[i] = plasma_gen.calculatePlasmaPixel(coord, now, plasma_params);
        }
    }
}

// Phase 4: Full System with Auto-Cycling and Transitions
void drawFullSystem(uint32_t now) {
    variant_manager.update(now, autoCycle.value(), variants.value());
    
    for (int i = 0; i < NUM_LEDS; i++) {
        RingCoord coord = ring_lut.fastRingCoord(i);
        leds[i] = variant_manager.renderPixel(coord, now);
    }
}
```

### Testing and Validation
1. **Plasma Waves Only**: Verify smooth wave interference and noise modulation
2. **Parameter Responsiveness**: Test all UI sliders affect plasma generation correctly
3. **Performance Metrics**: Measure frame rate with plasma algorithm on target hardware
4. **Visual Quality**: Confirm smooth color transitions and no artifacts
5. **Memory Usage**: Monitor RAM consumption during plasma calculations

### Incremental Development Plan
1. **Week 1**: Implement Plasma Waves algorithm and RingCoord system
2. **Week 2**: Add 2-3 simpler variants (Cosmic Swirl, Electric Storm, Fire Dance)
3. **Week 3**: Implement transition system and automatic cycling
4. **Week 4**: Add remaining variants and color palette system
5. **Week 5**: Optimization, polish, and platform-specific tuning

## Advanced Graphics Techniques Demonstrated

### Wave Interference Mathematics
The plasma algorithm showcases classical physics simulation:
- **Superposition Principle**: Multiple wave sources combine linearly
- **Phase Relationships**: Time-varying phase creates animation
- **Distance-Based Attenuation**: Realistic wave propagation modeling
- **Noise Modulation**: Organic variation through Perlin noise overlay

### Color Theory Implementation
- **HSV Color Space**: Smooth hue transitions for natural color flow
- **Saturation Modulation**: Dynamic saturation based on wave intensity
- **Brightness Mapping**: Normalized wave values to brightness curves
- **Gamma Correction**: Perceptually linear brightness progression

### Performance Optimization Strategies
- **Trigonometric Lookup**: Pre-computed sine/cosine tables
- **Fixed-Point Math**: Integer approximations for embedded platforms
- **Loop Unrolling**: Minimize function call overhead in tight loops
- **Memory Access Patterns**: Cache-friendly coordinate calculations

## Future Enhancements

### Advanced Features
- **Save/Load Configurations**: User-defined effect combinations and parameters
- **BPM Synchronization**: Music-reactive timing for effect transitions
- **Custom Palette Editor**: User-defined color schemes with preview
- **Effect Intensity Controls**: Per-variant amplitude and speed modulation
- **Multi-Ring Support**: Expand to multiple concentric LED rings

### Platform Extensions
- **Multi-Core Optimization**: Parallel processing for complex calculations
- **GPU Acceleration**: WebGL compute shaders for web platform
- **Hardware Acceleration**: Platform-specific optimizations (ESP32, Teensy)
- **Memory Mapping**: Direct hardware buffer access for maximum performance

### Algorithm Enhancements
- **Physically-Based Rendering**: More realistic light simulation
- **Particle Systems**: Dynamic particle-based effects
- **Fractal Algorithms**: Mandelbrot and Julia set visualizations
- **Audio Visualization**: Spectrum analysis and reactive algorithms
