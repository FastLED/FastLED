/// @file    PhasorField.ino
/// @brief   Multi-wavelength wave interference pattern with temporal evolution
/// @details Physical simulation of coherent wave interference from 12 emitters:
///
///          I(𝐫,t) ∝ |Σⁱ⁼¹² A · e^(i(𝐤·𝐫 + φᵢ(t)))|²
///
///          Where:
///          - 12 emitters in uniformly distributed directions
///          - 9 distinct spatial frequencies k ∈ [24,32] cycles across panel
///          - Temporal phase evolution φᵢ(t) at ~1Hz with slight detuning
///          - Interference produces standing/traveling wave patterns
///
///          Inspired by "Interferences study [7f44bc49]" - custom JS/WebGL/WebAudio
///          synthesis exploring beat frequencies and spatial patterns.

#include <FastLED.h>
#include "fl/xymap.h"

using namespace fl;

// ========== MATH CONSTANTS ==========
#ifndef PI
#define PI 3.14159265358979323846f
#endif

// ========== LED MATRIX CONFIGURATION ==========
#define WIDTH        64
#define HEIGHT       64
#define NUM_LEDS     (WIDTH * HEIGHT)

// Hardware configuration - adjust for your setup
#define DATA_PIN     2
#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB
#define BRIGHTNESS   160

CRGB leds[NUM_LEDS];

// LED matrix coordinate mapping (serpentine layout)
XYMap xymap(WIDTH, HEIGHT, true);  // true = serpentine wiring

// ========== PHYSICS PARAMETERS ==========
static const uint8_t N_EMITTERS = 12;  ///< Number of coherent wave sources
static const uint8_t N_K = 9;          ///< Number of distinct wavelengths

// Spatial frequency range (cycles across panel width)
static const uint8_t K_MIN = 24;
static const uint8_t K_MAX = 32;

// Temporal beat frequency range (Hz)
static const float FREQ_MIN = 0.7f;    ///< Minimum oscillation rate
static const float FREQ_STEP = 0.08f;  ///< Frequency increment between emitters

// ========== VISUAL CONFIGURATION ==========
// Choose your color palette (uncomment one):
CRGBPalette16 gPalette = LavaColors_p;
// CRGBPalette16 gPalette = OceanColors_p;
// CRGBPalette16 gPalette = RainbowColors_p;
// CRGBPalette16 gPalette = PartyColors_p;
// CRGBPalette16 gPalette = CloudColors_p;

#define ENABLE_PALETTE_DRIFT 1  ///< Slowly blend between color palettes
#define SHOW_FRAMERATE 0        ///< Print FPS to serial (requires Serial.begin)

// ========== FIXED-POINT MATH HELPERS ==========
// FastLED's sin16/cos16 use 0..65535 to represent a full circle (τ = 2π)
static const uint32_t TAU_U16 = 65536UL;
static const int16_t  Q15_MAX = 32767;  ///< Maximum value for Q1.15 format

// Magnitude compression: 38-bit squared sum → 16-bit intensity
// Max sum ≈ 12 emitters × 32767 ≈ 393k → square ≈ 1.5×10¹¹ (38 bits)
// Shift 38 bits down to 16 bits = 22 bit shift
static const uint8_t MAGNITUDE_SHIFT = 22;

/// @brief Wave emitter definition
/// @details Each emitter propagates waves in a fixed direction with specific
///          spatial frequency and temporally evolving phase
struct Emitter {
  int16_t  ux_q15;       ///< Direction unit vector X (Q1.15 fixed-point)
  int16_t  uy_q15;       ///< Direction unit vector Y (Q1.15 fixed-point)
  uint8_t  k;            ///< Spatial frequency (wavelength cycles per panel)
  uint16_t phi0;         ///< Initial phase offset [0, 65535) turns
  uint16_t dphi_per_ms;  ///< Phase increment per millisecond (Q0.16 turns/ms)
};

Emitter emitters[N_EMITTERS];

// ========== WAVE PHYSICS INITIALIZATION ==========
/// @brief Initialize emitter array with evenly distributed directions and frequencies
/// @details Creates 12 emitters in uniformly spaced angular directions (30° apart),
///          assigns 9 spatial frequencies from K_MIN to K_MAX, and sets up temporal
///          phase evolution with slight beat frequency detuning (~1Hz base)
void initEmitters() {
  // Generate linearly spaced spatial frequencies
  uint8_t spatial_freqs[N_K];
  for (uint8_t i = 0; i < N_K; ++i) {
    spatial_freqs[i] = K_MIN + (i * (K_MAX - K_MIN)) / (N_K - 1);
  }

  // Convert Hz to phase increment per millisecond in 0..65535 units
  auto hz_to_dphi = [](float hz) -> uint16_t {
    // Turns per second: 65536 * hz
    // Turns per millisecond: (65536 * hz) / 1000
    float dphi = (TAU_U16 * hz) / 1000.0f;
    // Round to nearest integer and clamp to uint16_t range
    int32_t dphi_int = (int32_t)(dphi + 0.5f);
    if (dphi_int < 0) return 0;
    if (dphi_int > 65535) return 65535;
    return (uint16_t)dphi_int;
  };

  // Initialize each emitter with unique direction, wavelength, and beat frequency
  for (uint8_t i = 0; i < N_EMITTERS; ++i) {
    // Direction: uniformly distributed around circle (360° / 12 = 30° spacing)
    float angle = (2.0f * PI * i) / N_EMITTERS;
    float ux = fl::cosf(angle);
    float uy = fl::sinf(angle);

    // Convert direction to Q1.15 fixed-point format for fast integer math
    emitters[i].ux_q15 = (int16_t)(ux * Q15_MAX);
    emitters[i].uy_q15 = (int16_t)(uy * Q15_MAX);

    // Assign spatial frequency (cycle through available wavelengths)
    emitters[i].k = spatial_freqs[i % N_K];

    // Random initial phase for visual variety
    emitters[i].phi0 = random16();

    // Temporal frequency with slight detuning (creates slow beat patterns)
    float freq = FREQ_MIN + FREQ_STEP * (float)(i % N_K);
    emitters[i].dphi_per_ms = hz_to_dphi(freq);
  }
}

// ========== COLOR MAPPING ==========
/// @brief Map interference intensity to color using palette
/// @param intensity Wave interference magnitude squared (0..65535)
/// @return RGB color from current palette
inline CRGB intensityToColor(uint16_t intensity) {
  // Use upper 8 bits for palette index (0..255)
  uint8_t palette_index = intensity >> 8;
  return ColorFromPalette(gPalette, palette_index, 255, LINEARBLEND);
}

// ========== WAVE INTERFERENCE RENDERING ==========
/// @brief Compute and render interference pattern for current time
/// @details Calculates complex phasor sum at each pixel position:
///
///          For each pixel (x,y):
///            1. Normalize coordinates to [0,1]
///            2. For each emitter i:
///               - Compute spatial phase: k_i · (û_i · r̂)
///               - Add temporal phase: φ_i(t) = φ₀_i + ω_i·t
///               - Accumulate complex exponential: e^(i·phase)
///            3. Intensity = |sum|² = Re²(sum) + Im²(sum)
///            4. Map intensity to color
///
/// @param t_ms Current time in milliseconds since startup
void renderInterference(uint32_t t_ms) {
  // Precompute temporal phase for each emitter (constant across all pixels)
  uint16_t temporal_phase[N_EMITTERS];
  for (uint8_t i = 0; i < N_EMITTERS; ++i) {
    // Phase evolves linearly: φ(t) = φ₀ + (dφ/dt)·t
    uint32_t phase_offset = (uint32_t)emitters[i].dphi_per_ms * t_ms;
    temporal_phase[i] = emitters[i].phi0 + (uint16_t)phase_offset;
  }

  // Render each pixel
  for (uint8_t y = 0; y < HEIGHT; ++y) {
    // Normalize Y coordinate to Q1.15 format: yn ∈ [0, 1)
    int16_t yn_q15 = (int16_t)(((int32_t)y * Q15_MAX) / HEIGHT);

    for (uint8_t x = 0; x < WIDTH; ++x) {
      // Normalize X coordinate to Q1.15 format: xn ∈ [0, 1)
      int16_t xn_q15 = (int16_t)(((int32_t)x * Q15_MAX) / WIDTH);

      // Accumulate complex phasor sum: Σ e^(i·θ_i) = Σ(cos θ_i + i·sin θ_i)
      int32_t sum_real = 0;
      int32_t sum_imag = 0;

      for (uint8_t i = 0; i < N_EMITTERS; ++i) {
        // Compute dot product: û_i · r̂ = ux_i·xn + uy_i·yn
        // Both operands in Q1.15 → result in Q2.30 → shift to Q1.15
        int32_t dot_q30 = (int32_t)emitters[i].ux_q15 * xn_q15 +
                          (int32_t)emitters[i].uy_q15 * yn_q15;
        int16_t dot_q15 = (int16_t)(dot_q30 >> 15);

        // Spatial phase contribution: k · (û · r̂)
        int32_t spatial_q15 = (int32_t)emitters[i].k * dot_q15;

        // Convert Q1.15 to phase units (0..65535 = full circle)
        uint16_t spatial_phase = (uint16_t)((spatial_q15 * TAU_U16) >> 15);

        // Total phase: spatial + temporal
        uint16_t total_phase = spatial_phase + temporal_phase[i];

        // Evaluate complex exponential: e^(i·θ) = cos(θ) + i·sin(θ)
        int16_t cos_val = cos16(total_phase);  // Returns Q1.15
        int16_t sin_val = sin16(total_phase);  // Returns Q1.15

        sum_real += cos_val;
        sum_imag += sin_val;
      }

      // Compute intensity: I = |sum|² = Re²(sum) + Im²(sum)
      // Use 64-bit arithmetic to prevent overflow during multiplication
      uint64_t magnitude_sq = (uint64_t)sum_real * (uint64_t)sum_real +
                              (uint64_t)sum_imag * (uint64_t)sum_imag;

      // Compress to 16-bit dynamic range for color mapping
      uint16_t intensity = (uint16_t)(magnitude_sq >> MAGNITUDE_SHIFT);

      // Map to color and set LED using XYMap
      leds[xymap(x, y)] = intensityToColor(intensity);
    }
  }
}

// ========== VISUAL EFFECTS ==========
#if ENABLE_PALETTE_DRIFT
/// @brief Gradually blend between color palettes for evolving appearance
/// @details Slowly transitions current palette toward PartyColors_p to prevent
///          static visual lock-in and add temporal variation
void updatePaletteDrift() {
  EVERY_N_MILLISECONDS(40) {
    static CRGBPalette16 targetPalette = PartyColors_p;
    nblendPaletteTowardPalette(gPalette, targetPalette, 8);
  }
}
#endif

#if SHOW_FRAMERATE
/// @brief Calculate and print framerate to serial monitor
void printFramerate() {
  EVERY_N_SECONDS(2) {
    Serial.print(F("FPS: "));
    Serial.println(FastLED.getFPS());
  }
}
#endif

// ========== ARDUINO MAIN FUNCTIONS ==========
void setup() {
  delay(300);  // Power-up safety delay

  #if SHOW_FRAMERATE
  Serial.begin(115200);
  Serial.println(F("PhasorField - Wave Interference Demo"));
  #endif

  // Initialize LED strip with XYMap for 2D coordinate mapping
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setScreenMap(xymap);
  FastLED.setBrightness(BRIGHTNESS);

  // Seed random number generator from analog noise
  random16_add_entropy(analogRead(A0));

  // Initialize wave physics
  initEmitters();

  #if SHOW_FRAMERATE
  Serial.print(F("Emitters: "));
  Serial.println(N_EMITTERS);
  Serial.print(F("Wavelengths: "));
  Serial.println(N_K);
  Serial.print(F("Resolution: "));
  Serial.print(WIDTH);
  Serial.print(F("x"));
  Serial.println(HEIGHT);
  #endif
}

void loop() {
  uint32_t t = millis();

  // Compute and render wave interference
  renderInterference(t);

  // Apply visual effects
  #if ENABLE_PALETTE_DRIFT
  updatePaletteDrift();
  #endif

  // Update display
  FastLED.show();

  // Performance monitoring
  #if SHOW_FRAMERATE
  printFramerate();
  #endif
}
