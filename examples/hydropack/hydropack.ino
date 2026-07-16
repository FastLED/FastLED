// FL_AGENT_ALLOW_NEW_EXAMPLE
/// @file hydropack.ino
/// @brief HydroPack: an LED approximation of the < | > EL layout with two
///          adaptive microphone indicators: Sensitive and Loud.
/// @example hydropack.ino

// @filter: (mem is large) and (platform is esp32*)

// This is deliberately an LED prototype, not an EL renderer. It draws two
// triangular clusters and a center bar using ordinary addressable LEDs, then
// adds two status LEDs below them:
//   - cyan: Sensitive - fires on a bass transient just above the song average
//   - magenta: Loud - fires only on a substantially stronger transient
//
// The detector is FastLED's built-in Vibe detector. Like Songstone's adaptive
// window, it tracks a slow running song average; relative bass energy is about
// 1.0 at the current average. FastLED's adaptive noise-floor tracker gates
// microphone hiss before Vibe sees it. A loud hit therefore fires both LEDs,
// while a moderate hit fires Sensitive only.

#include <Arduino.h>
#include <FastLED.h>

#include "fl/math/math.h"
#include "fl/ui/ui.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// INMP441 wiring (matches examples/AudioInput):
//   SCK (BCLK) -> GPIO 4, WS (LRCLK) -> GPIO 7, SD (DATA) -> GPIO 8,
//   L/R -> 3.3V (right channel), VDD -> 3.3V, GND -> GND
#define I2S_WS 7
#define I2S_SD 8
#define I2S_CLK 4

#define PIN_PREVIEW 3

constexpr uint8_t kGeometryLedCount = 18;
constexpr uint8_t kSensitiveIndicator = 18;
constexpr uint8_t kLoudIndicator = 19;
constexpr uint8_t kPreviewLedCount = 20;

CRGB previewLeds[kPreviewLedCount];

// Six LEDs form each triangle, six form the center bar, and the last two are
// the explicitly labeled audio indicators. The layout reads < | > in the
// WASM screenmap renderer without relying on EL panel/wire primitives.
const fl::vec2f kHydroPackLedLayout[] = {
    // Left triangle (<)
    {12.0f, 50.0f}, {22.0f, 42.0f}, {22.0f, 58.0f},
    {32.0f, 34.0f}, {32.0f, 50.0f}, {32.0f, 66.0f},
    // Center bar (|)
    {50.0f, 30.0f}, {50.0f, 38.0f}, {50.0f, 46.0f},
    {50.0f, 54.0f}, {50.0f, 62.0f}, {50.0f, 70.0f},
    // Right triangle (>)
    {88.0f, 50.0f}, {78.0f, 42.0f}, {78.0f, 58.0f},
    {68.0f, 34.0f}, {68.0f, 50.0f}, {68.0f, 66.0f},
    // Sensitive, Loud
    {44.0f, 88.0f}, {56.0f, 88.0f},
};

static_assert(sizeof(kHydroPackLedLayout) / sizeof(kHydroPackLedLayout[0]) ==
                  kPreviewLedCount,
              "HydroPack screenmap must contain every preview LED");

fl::ScreenMap makePreviewMap() {
    return fl::ScreenMap(kHydroPackLedLayout, 1.4f);
}

fl::ScreenMap previewMap = makePreviewMap();

fl::audio::Config audioConfig = fl::audio::Config::CreateInmp441(
    I2S_WS, I2S_SD, I2S_CLK, fl::audio::AudioChannel::Right);
fl::UIAudio audioUi("Audio Input", audioConfig);

fl::UITitle title("HydroPack LED Audio Prototype");
fl::UIDescription description(
    "LED approximation of the < | > layout. Cyan is Sensitive: it fires on "
    "moderate bass hits. Magenta is Loud: it fires only on strong hits. "
    "Both thresholds adapt to the current song and microphone noise floor.");

fl::UISlider sensitiveThreshold("Sensitive Threshold", 1.05f, 1.0f, 3.0f,
                                0.05f);
fl::UISlider loudThreshold("Loud Threshold", 1.65f, 1.0f, 6.0f, 0.05f);
fl::UISlider fadeSeconds("Indicator Fade Seconds", 0.25f, 0.05f, 2.0f,
                          0.05f);

// 0.0-1.0 envelopes. The audio callback raises them and the render loop
// decays them. Keeping two independent envelopes makes the behavior visible:
// Sensitive can remain lit while Loud stays dark.
float gSensitiveLevel = 0.0f;
float gLoudLevel = 0.0f;

void fireIndicator(float bass, float threshold, float minIntensity,
                   float &level) {
    if (bass <= threshold) {
        return;
    }
    const float intensity = fl::clamp((bass - threshold) / threshold,
                                      minIntensity, 1.0f);
    level = fl::max(level, intensity);
}

uint8_t toByte(float value) {
    return static_cast<uint8_t>(fl::clamp(value, 0.0f, 255.0f));
}

void renderPreview() {
    // The physical-layout LEDs are electric blue only while an indicator is
    // active. Quiet input is fully dark.
    const float combined = fl::max(gSensitiveLevel, gLoudLevel);
    const uint8_t geometryBrightness = toByte(combined * 160.0f);
    for (uint8_t i = 0; i < kGeometryLedCount; ++i) {
        previewLeds[i] = CRGB(0, geometryBrightness / 3, geometryBrightness);
    }

    const uint8_t sensitive = toByte(gSensitiveLevel * 255.0f);
    const uint8_t loud = toByte(gLoudLevel * 255.0f);
    previewLeds[kSensitiveIndicator] = CRGB(0, sensitive, sensitive);
    previewLeds[kLoudIndicator] = CRGB(loud, 0, loud);
}

void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2812B, PIN_PREVIEW, GRB>(previewLeds, kPreviewLedCount)
        .setScreenMap(previewMap);
    FastLED.setBrightness(255);

    sensitiveThreshold.setGroup("Adaptive Audio Indicators");
    loudThreshold.setGroup("Adaptive Audio Indicators");
    fadeSeconds.setGroup("Adaptive Audio Indicators");

    // The unified input is browser audio on WASM and INMP441 I2S on ESP32.
    auto audio = FastLED.add(audioUi);
    if (!audio) {
        FL_WARN("HydroPack: no audio processor available");
        return;
    }

    audio->setNoiseFloorTrackingEnabled(true);
    audio->onVibeLevels([](const fl::audio::detector::VibeLevels &levels) {
        if (!levels.bassSpike) {
            return;
        }
        fireIndicator(levels.bass, sensitiveThreshold.value(), 0.35f,
                      gSensitiveLevel);
        fireIndicator(levels.bass, loudThreshold.value(), 0.50f, gLoudLevel);
    });
}

void loop() {
    EVERY_N_MILLIS(10) {
        static uint32_t lastMs = 0;
        static bool firstFrame = true;
        const uint32_t now = millis();
        const uint32_t deltaMs = firstFrame ? 0 : (now - lastMs);
        firstFrame = false;
        lastMs = now;

        const float fade = fadeSeconds.value();
        if (fade > 0.0f && deltaMs > 0) {
            const float decay = float(deltaMs) / (1000.0f * fade);
            gSensitiveLevel = fl::clamp(gSensitiveLevel - decay, 0.0f, 1.0f);
            gLoudLevel = fl::clamp(gLoudLevel - decay, 0.0f, 1.0f);
        }

        renderPreview();
        FastLED.show();  // Auto-pumps browser or I2S microphone audio.
    }
    delay(1);
}
