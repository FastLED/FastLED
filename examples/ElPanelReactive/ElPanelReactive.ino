/// @file    ElPanelReactive.ino
/// @brief   Audio-reactive EL panel driver with auto-gain via Vibe detector.
/// @example ElPanelReactive.ino

// Two EL panels on D2 and D1 driven at 50 Hz PWM.
// Auto-gain via Vibe detector's self-normalizing bass analysis:
//   - Bass level centers around 1.0 (long-term EMA normalization)
//   - Above 1.0 = louder than average → panels respond
//   - Intense parts naturally attenuate, quiet parts are amplified
//   - Fast attack from asymmetric smoothing preserves transients
// Per-panel thresholds keep the two panels at staggered levels:
//   - Panel 1 (D2): higher threshold + squared signal → only strong hits
//   - Panel 2 (D1): lower threshold + linear signal → more responsive

// @filter: (mem is large)

#include <FastLED.h>
#include "el_panel.h"
#include "fl/math/filter/filter.h"
#include "fl/math/math.h"
#include "fl/ui/ui.h"

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------
fl::UITitle title("ElPanel");
fl::UIDescription description("A visualizer for ElPanel, in wasm mode it shows as 2 sets of 4 dots representing the panels. In real device mode it drives EL panel PWM at 50Hz");
fl::UIAudio audio_ui("Audio Input");
fl::UISlider sensitivity("Sensitivity", 1.5f, 0.3f, 4.0f, 0.1f);

// Panel 1 controls
fl::UISlider threshold1("Threshold", 0.54f, 0.0f, 1.0f, 0.01f);
fl::UISlider attack1("Attack", 0.081f, 0.001f, 0.5f, 0.005f);
fl::UISlider decay1("Decay", 0.3f, 0.01f, 1.0f, 0.01f);
fl::UIGroup group1("Panel 1", threshold1, attack1, decay1);

// Panel 2 controls
fl::UISlider threshold2("Threshold", 0.94f, 0.0f, 1.0f, 0.01f);
fl::UISlider attack2("Attack", 0.081f, 0.001f, 0.5f, 0.005f);
fl::UISlider decay2("Decay", 0.3f, 0.01f, 1.0f, 0.01f);
fl::UIGroup group2("Panel 2", threshold2, attack2, decay2);

// ---------------------------------------------------------------------------
// Attack/Decay filters
// ---------------------------------------------------------------------------
static fl::AttackDecayFilter<float> filterHigh(0.081f, 0.3f);
static fl::AttackDecayFilter<float> filterLow(0.081f, 0.3f);

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static bool     isSilent       = false;
static uint32_t lastMillis     = 0;
static uint32_t lastAudioMs    = 0;  // last time onVibeLevels fired
static const uint32_t kAudioTimeoutMs = 150; // decay if no audio for this long

void setup() {
    Serial.begin(115200);
    initPanels();

    auto audio = FastLED.add(audio_ui);
    if (audio) {
        audio->onSilence([&](u8 silent) {
            isSilent = (silent != 0);
            if (isSilent) {
                filterHigh.reset();
                filterLow.reset();
            }
        });

        audio->onVibeLevels(
            [&](const fl::audio::detector::VibeLevels &levels) {
                if (isSilent) return;
                lastAudioMs = millis();

                // Vibe's self-normalizing bass: ~1.0 = average level.
                // Subtract 1.0 so silence/average → 0, beats → positive.
                float signal = (levels.bass - 1.0f) * sensitivity.value();
                signal = fl::clamp(signal, 0.0f, 1.0f);

                uint32_t now = millis();
                float dt = (now - lastMillis) / 1000.0f;
                lastMillis = now;

                // Apply UI-driven attack/decay settings
                filterHigh.setAttackTau(attack1.value());
                filterHigh.setDecayTau(decay1.value());
                filterLow.setAttackTau(attack2.value());
                filterLow.setDecayTau(decay2.value());

                // Per-panel thresholds keep staggered levels.
                // Remap [threshold..1] → [0..1], clamped.
                float t1 = threshold1.value();
                float sig1 = fl::map_range_clamped(signal, t1, 1.0f, 0.0f, 1.0f);

                float t2 = threshold2.value();
                float sig2 = fl::map_range_clamped(signal, t2, 1.0f, 0.0f, 1.0f);

                // Panel 1: stronger signal (squared for contrast)
                filterHigh.update(sig1 * sig1, dt);
                // Panel 2: more responsive (linear)
                filterLow.update(sig2, dt);
            });
    }
}

void loop() {
    // Decay filters when audio is silent OR paused (no callbacks arriving).
    uint32_t now = millis();
    bool audioTimedOut = (now - lastAudioMs) > kAudioTimeoutMs;
    if (isSilent || audioTimedOut) {
        float dt = (now - lastMillis) / 1000.0f;
        lastMillis = now;
        filterHigh.update(0.0f, dt);
        filterLow.update(0.0f, dt);
    }
    setPanelHigh(filterHigh.value());
    setPanelLow(filterLow.value());
    showPanels();
}
