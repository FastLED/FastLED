// FL_AGENT_ALLOW_NEW_EXAMPLE
/// @file hydropack.ino
/// @brief HydroPack: two adaptive bass analyzers animate an LED < | > layout.
/// @example hydropack.ino

// @filter: (mem is large) and (platform is esp32*)

// This is deliberately an LED prototype, not an EL renderer. It draws two
// triangular clusters and a center bar using ordinary addressable LEDs. Two
// independent threshold analyzers consume the same self-normalized bass input:
//   - center: sensitive - fires on a transient just above the song average
//   - triangles: loud - fire only on a substantially stronger transient
//
// The detector is FastLED's built-in Vibe detector. Like Songstone's adaptive
// window, it tracks a slow running song average; relative bass energy is about
// 1.0 at the current average. FastLED's adaptive noise-floor tracker gates
// microphone hiss before Vibe sees it. A loud hit lights the center, then
// launches into the triangles; a moderate hit lights the center only.

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

constexpr uint8_t kTriangleLedCount = 6;
constexpr uint8_t kCenterStart = 6;
constexpr uint8_t kRightTriangleStart = 12;
constexpr uint8_t kPreviewLedCount = 18;
constexpr uint32_t kTriangleLaunchDelayMs = 5;

CRGB previewLeds[kPreviewLedCount];

// Six LEDs form each triangle and six form the center bar. The layout reads
// < | > in the WASM screenmap renderer without EL panel/wire primitives.
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

fl::UITitle title("HydroPack Two-Level Bass Launch");
fl::UIDescription description(
    "Two analyzers read the same adaptive bass signal. The center bar fires "
    "at the sensitive level. Stronger hits launch out to both triangles. "
    "Quiet input is fully dark.");

fl::UISlider centerThreshold("Center (Sensitive) Threshold", 1.05f, 1.0f,
                             3.0f, 0.05f);
fl::UISlider triangleThreshold("Triangles (Loud) Threshold", 1.65f, 1.0f,
                               6.0f, 0.05f);
fl::UISlider fadeSeconds("Launch Fade Seconds", 0.25f, 0.05f, 2.0f, 0.05f);

// The two analyzers have independent envelopes but share one bass input.
// The loud analyzer is released 5 ms after the center, preserving an outward
// launch without adding a perceptible artificial delay.
float gCenterLevel = 0.0f;
float gTriangleLevel = 0.0f;
float gPendingTriangleLevel = 0.0f;
uint32_t gTriangleFireAtMs = 0;

void fireAnalyzer(float bass, float threshold, float minIntensity,
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
    // Quiet is fully dark. The sensitive center appears first; a loud hit
    // subsequently drives the two triangle clusters outward from it.
    const uint8_t centerBrightness = toByte(gCenterLevel * 200.0f);
    const uint8_t triangleBrightness = toByte(gTriangleLevel * 180.0f);
    const CRGB centerColor(0, centerBrightness / 3, centerBrightness);
    const CRGB triangleColor(0, triangleBrightness / 3, triangleBrightness);

    for (uint8_t i = 0; i < kTriangleLedCount; ++i) {
        previewLeds[i] = triangleColor;
        previewLeds[kRightTriangleStart + i] = triangleColor;
    }
    for (uint8_t i = 0; i < kTriangleLedCount; ++i) {
        previewLeds[kCenterStart + i] = centerColor;
    }
}

void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2812B, PIN_PREVIEW, GRB>(previewLeds, kPreviewLedCount)
        .setScreenMap(previewMap);
    FastLED.setBrightness(255);

    centerThreshold.setGroup("Two-Level Bass Analyzers");
    triangleThreshold.setGroup("Two-Level Bass Analyzers");
    fadeSeconds.setGroup("Two-Level Bass Analyzers");

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
        fireAnalyzer(levels.bass, centerThreshold.value(), 0.35f,
                     gCenterLevel);
        if (levels.bass > triangleThreshold.value()) {
            fireAnalyzer(levels.bass, triangleThreshold.value(), 0.50f,
                         gPendingTriangleLevel);
            gTriangleFireAtMs = millis() + kTriangleLaunchDelayMs;
        }
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

        if (gPendingTriangleLevel > 0.0f &&
            static_cast<int32_t>(now - gTriangleFireAtMs) >= 0) {
            gTriangleLevel = fl::max(gTriangleLevel, gPendingTriangleLevel);
            gPendingTriangleLevel = 0.0f;
        }

        const float fade = fadeSeconds.value();
        if (fade > 0.0f && deltaMs > 0) {
            const float decay = float(deltaMs) / (1000.0f * fade);
            gCenterLevel = fl::clamp(gCenterLevel - decay, 0.0f, 1.0f);
            gTriangleLevel = fl::clamp(gTriangleLevel - decay, 0.0f, 1.0f);
        }

        renderPreview();
        FastLED.show();  // Auto-pumps browser or I2S microphone audio.
    }
    delay(1);
}
