// FL_AGENT_ALLOW_NEW_EXAMPLE
/// @file    hydropack.ino
/// @brief   HydroPack: dual EL-panel beat visualizer. The inner panel fires
///          on sensitive (quiet) beats, the outer panel only on loud beats.
///          INMP441 mic -> normalized bass detection -> 50 Hz PWM gates.
/// @example hydropack.ino

// @filter: (mem is large)

// This sketch targets the ESP32-C3 and is also a valid FastLED WASM sketch:
//   pip install fastled
//   fastled examples/hydropack
// On WASM the audio comes from the browser (microphone or audio file) and the
// two EL panels are rendered as an inner disc + outer ring preview. On the
// ESP32-C3 the audio comes from a real INMP441 and the panels are driven by
// low-frequency 50 Hz PWM suitable for gating an EL inverter.
//
// For a continuous-envelope EL panel example (browser audio only), see
// examples/ElPanelReactive. HydroPack differs in that it is spike-gated:
// panels flash on discrete beats instead of tracking the audio level, and it
// wires a real INMP441 for standalone wearable use.
//
// Audio pipeline ("normalize, then select-filter"):
//   1. Signal conditioning (DC removal, spike filter, noise gate) plus the
//      INMP441 frequency-response profile normalize the raw mic signal.
//   2. The vibe detector tracks the bass band of the FFT and self-normalizes
//      it against the running song average (~1.0 = average loudness), so the
//      same thresholds work at whisper and club volume.
//   3. Rising bass energy (a spike) is split by loudness: beats just above
//      the average fire the sensitive inner panel; only strong hits fire the
//      loud outer panel.

#include <Arduino.h>
#include <FastLED.h>

#include "fl/math/math.h"
#include "fl/system/pin.h"
#include "fl/ui/ui.h"

// ---------------------------------------------------------------------------
// Configuration (ESP32-C3 pin map)
// ---------------------------------------------------------------------------

// INMP441 wiring (matches examples/AudioInput):
//   SCK (BCLK) -> GPIO 4, WS (LRCLK) -> GPIO 7, SD (DATA) -> GPIO 8,
//   L/R -> 3.3V (right channel), VDD -> 3.3V, GND -> GND
#define I2S_WS 7
#define I2S_SD 8
#define I2S_CLK 4

// EL panel gate pins. Each drives the enable line / transistor gate of an EL
// inverter, active high.
#define PIN_EL_INNER 5
#define PIN_EL_OUTER 6

// EL inverters must be gated slowly - low-frequency PWM at 50 Hz.
#define EL_PWM_FREQ_HZ 50

// On-screen preview (WASM) and optional debug strip (hardware): a single
// WS2812 run laid out as an inner disc plus an outer ring.
#define PIN_PREVIEW 3
#define INNER_PREVIEW_LEDS 12
#define OUTER_PREVIEW_LEDS 24
#define NUM_PREVIEW_LEDS (INNER_PREVIEW_LEDS + OUTER_PREVIEW_LEDS)

// ---------------------------------------------------------------------------
// EL panel drive: 50 Hz low-frequency PWM through the fl pin API
// ---------------------------------------------------------------------------
// fl::setPwmFrequency() + fl::analogWrite() pick the right PWM backend per
// platform (LEDC on ESP32, including the Arduino core 2.x/3.x differences).
// The browser preview has no PWM hardware, so the calls are skipped there;
// the preview LEDs are the panels on WASM.

void initPanels() {
#ifndef __EMSCRIPTEN__
    fl::pinMode(PIN_EL_INNER, fl::PinMode::Output);
    fl::pinMode(PIN_EL_OUTER, fl::PinMode::Output);
    fl::setPwmFrequency(PIN_EL_INNER, EL_PWM_FREQ_HZ);
    fl::setPwmFrequency(PIN_EL_OUTER, EL_PWM_FREQ_HZ);
    fl::analogWrite(PIN_EL_INNER, 0);
    fl::analogWrite(PIN_EL_OUTER, 0);
#endif
}

void writePanels(uint8_t innerDuty, uint8_t outerDuty) {
#ifndef __EMSCRIPTEN__
    // Skip rewrites of an unchanged duty: the 50 Hz PWM only latches a new
    // value once per 20 ms period anyway.
    static int lastInner = -1;
    static int lastOuter = -1;
    if (innerDuty != lastInner) {
        fl::analogWrite(PIN_EL_INNER, innerDuty);
        lastInner = innerDuty;
    }
    if (outerDuty != lastOuter) {
        fl::analogWrite(PIN_EL_OUTER, outerDuty);
        lastOuter = outerDuty;
    }
#else
    FASTLED_UNUSED(innerDuty);
    FASTLED_UNUSED(outerDuty);
#endif
}

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

CRGB previewLeds[NUM_PREVIEW_LEDS];

fl::audio::Config audioConfig = fl::audio::Config::CreateInmp441(
    I2S_WS, I2S_SD, I2S_CLK, fl::audio::AudioChannel::Right);
fl::UIAudio audioUi("Audio Input", audioConfig);

fl::UITitle title("HydroPack");
fl::UIDescription description(
    "Dual EL-panel beat visualizer. The INMP441 signal is normalized, then "
    "bass beats are split by loudness: the inner panel fires on sensitive "
    "beats, the outer panel only on loud ones. Panels are gated with 50 Hz "
    "low-frequency PWM.");

// Vibe bass is self-normalizing: ~1.0 = the song's running average, so the
// thresholds read as "x times louder than average".
fl::UISlider innerThreshold("Inner (Sensitive) Threshold", 1.05f, 1.0f, 3.0f, 0.05f);
fl::UISlider outerThreshold("Outer (Loud) Threshold", 1.6f, 1.0f, 6.0f, 0.05f);
fl::UISlider fadeSeconds("Panel Fade Seconds", 0.25f, 0.05f, 2.0f, 0.05f);

// Beat envelopes, 0.0-1.0. Raised by the audio callback, decayed in loop().
float gInnerLevel = 0.0f;
float gOuterLevel = 0.0f;

// Preview layout: inner disc at radius 14, outer ring at radius 40.
fl::ScreenMap previewMap =
    fl::ScreenMap(NUM_PREVIEW_LEDS, 0.5f, [](int index, fl::vec2f &pt_out) {
        const bool isInner = index < INNER_PREVIEW_LEDS;
        const int count = isInner ? INNER_PREVIEW_LEDS : OUTER_PREVIEW_LEDS;
        const int i = isInner ? index : index - INNER_PREVIEW_LEDS;
        const float radius = isInner ? 14.0f : 40.0f;
        const float angle = (float(i) / float(count)) * 2.0f * FL_M_PI;
        pt_out.x = 50.0f + radius * fl::cos(angle);
        pt_out.y = 50.0f + radius * fl::sin(angle);
    });

// Raise a panel envelope when a bass spike crosses its threshold. Intensity
// scales with how far the beat lands above the threshold, with a floor so
// barely-over beats still visibly flash.
void firePanel(float bass, float threshold, float minIntensity, float &level) {
    if (bass <= threshold) {
        return;
    }
    const float intensity =
        fl::clamp((bass - threshold) / threshold, minIntensity, 1.0f);
    level = fl::max(level, intensity);
}

// ---------------------------------------------------------------------------
// Arduino setup / loop
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);

    FastLED.addLeds<WS2812B, PIN_PREVIEW, GRB>(previewLeds, NUM_PREVIEW_LEDS)
        .setScreenMap(previewMap);
    FastLED.setBrightness(255);

    initPanels();

    innerThreshold.setGroup("Beat Detection");
    outerThreshold.setGroup("Beat Detection");
    fadeSeconds.setGroup("Beat Detection");

    // Unified audio path: browser audio on WASM, INMP441 over I2S on the
    // ESP32-C3. Samples are auto-pumped into the processor during show().
    auto audio = FastLED.add(audioUi);
    if (!audio) {
        FL_WARN("HydroPack: no audio processor available");
        return;
    }

    // Normalization: the conditioning pipeline (DC removal, spike filter,
    // noise gate) is on by default; adaptive noise floor tracking keeps
    // quiet-room hiss from triggering the panels.
    audio->setNoiseFloorTrackingEnabled(true);

    // Select-filter: only rising bass energy counts as a beat, then the
    // self-normalized level picks which panel(s) fire.
    audio->onVibeLevels([](const fl::audio::detector::VibeLevels &levels) {
        if (!levels.bassSpike) {
            return;
        }
        // Inner panel: sensitive - beats just above the running average.
        firePanel(levels.bass, innerThreshold.value(), 0.35f, gInnerLevel);
        // Outer panel: loud beats only.
        firePanel(levels.bass, outerThreshold.value(), 0.5f, gOuterLevel);
    });
}

void loop() {
    // 100 fps is plenty for a 50 Hz output device; pacing the frame keeps a
    // battery-powered wearable from busy-looping at full speed.
    EVERY_N_MILLIS(10) {
        // Frame-time based decay so the fade speed is FPS independent.
        static uint32_t lastMs = 0;
        static bool firstFrame = true;
        const uint32_t now = millis();
        const uint32_t deltaMs = firstFrame ? 0 : (now - lastMs);
        firstFrame = false;
        lastMs = now;

        const float fade = fadeSeconds.value();
        if (fade > 0.0f && deltaMs > 0) {
            const float decay = float(deltaMs) / (1000.0f * fade);
            gInnerLevel = fl::clamp(gInnerLevel - decay, 0.0f, 1.0f);
            gOuterLevel = fl::clamp(gOuterLevel - decay, 0.0f, 1.0f);
        }

        // Drive the EL panels: envelope -> 50 Hz PWM duty.
        const uint8_t innerDuty = uint8_t(gInnerLevel * 255.0f);
        const uint8_t outerDuty = uint8_t(gOuterLevel * 255.0f);
        writePanels(innerDuty, outerDuty);

        // Preview: inner disc = aqua (sensitive), outer ring = ice blue (loud).
        fill_solid(previewLeds, INNER_PREVIEW_LEDS, CHSV(128, 200, innerDuty));
        fill_solid(previewLeds + INNER_PREVIEW_LEDS, OUTER_PREVIEW_LEDS,
                   CHSV(160, 80, outerDuty));

        FastLED.show(); // audio is auto-pumped here
    }
    delay(1);
}
