/// @file    Sailboat.ino
/// @brief   Audio-reactive sailboat lighting with perlin noise, ambient
///          particles, and beat meteors for EDM music.
/// @example Sailboat.ino

// Ported from github.com/zackees/sailboat-lights (FastLED 3.6.0 era).
// The effect logic lives in PerlinParticlePunch (Fx1d); this sketch wires up
// hardware, audio detector callbacks, and color palettes.

// @filter: (mem is large)

#include <FastLED.h>

#include "fl/channels/config.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/fx/1d/perlin_particle_punch.h"
#include "fl/fx/fx_engine.h"
#include "fl/ui/ui.h"


// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define NUM_LEDS 200
#define DATA_PIN 3

#define BRIGHTNESS 255
#define COLOR_ORDER RGB

// I2S pins for INMP441 microphone (adjust for your board)
#define I2S_WS  7
#define I2S_SD  8
#define I2S_CLK 4

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
CRGB leds[NUM_LEDS];
fl::audio::Config audioConfig = fl::audio::Config::CreateInmp441(
    I2S_WS, I2S_SD, I2S_CLK, fl::audio::AudioChannel::Right);
fl::UIAudio audio_ui("Audio Input", audioConfig);
fl::UICheckbox enableAmbient("Ambient Particles", true);
fl::UICheckbox enableMeteors("Beat Meteors", true);
fl::UICheckbox enableTimeWarp("Noise Time-Warp", false);
// Vibe bass is self-normalizing: ~1.0 = song average.
// meteorThreshold: bass must exceed this multiple of average to spawn meteor.
fl::UISlider meteorThreshold("Meteor Threshold", 1.5f, 1.5f, 6.0f, 0.1f);
// ambientThreshold: bass above this spawns ambient particles.
fl::UISlider ambientThreshold("Ambient Threshold", 1.0f, 0.5f, 3.0f, 0.1f);
fl::UISlider dragSlider("Particle Drag", 0.06f, 0.0f, 1.0f, 0.01f);
fl::UISlider speedSlider("Particle Speed", 2.6f, 0.1f, 3.0f, 0.1f);
fl::UISlider ambientTrailSlider("Ambient Trail", 217.0f, 0.0f, 255.0f, 1.0f);
fl::UISlider meteorTrailSlider("Meteor Trail", 171.0f, 0.0f, 255.0f, 1.0f);
fl::UISlider ambientDecaySlider("Ambient Fade", 0.955f, 0.90f, 1.0f, 0.005f);
fl::UISlider minVelocitySlider("Min Velocity", 0.01f, 0.01f, 0.5f, 0.01f);
fl::UISlider debrisDecaySlider("Debris Fade", 0.96f, 0.80f, 1.0f, 0.01f);
fl::UISlider debrisVelDecaySlider("Debris Drag", 0.95f, 0.85f, 1.0f, 0.01f);
fl::PerlinParticlePunch sailboatFx(NUM_LEDS);

fl::FxEngine fxEngine(NUM_LEDS);

// Perlin time-warp state (decays each frame in loop())
float noiseTimeMultiplier = 1.0f;

// Screen map: LEDs follow the main sail hypotenuse with natural sag (catenary)
fl::ScreenMap screenMap =
    fl::ScreenMap(NUM_LEDS, 0.15f, [](int index, fl::vec2f &pt_out) {
        float t = float(index) / float(NUM_LEDS - 1);
        // Straight line: lower-left (15, 5) → upper-right (95, 95)
        // Y-up coordinate system: high y = top of screen
        pt_out.x = 15.0f + t * 80.0f;
        float straight_y = 5.0f + t * 90.0f;
        // Sag: catenary droop, subtract to bulge downward visually
        float sag = 15.0f * fl::sin(t * FL_M_PI);
        pt_out.y = straight_y - sag;
    });

// ---------------------------------------------------------------------------
// Arduino setup / loop
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    //FastLED.setExclusiveDriver(fl::Bus::SPI);
    fl::ChannelOptions opts;
    opts.mCorrection = TypicalLEDStrip;
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ClocklessChipset chipset(DATA_PIN, timing);
    fl::ChannelConfig config(chipset, fl::span<CRGB>(leds, NUM_LEDS), COLOR_ORDER, opts);
    config.setScreenMap(screenMap);
    FastLED.add(config);
    FastLED.setBrightness(BRIGHTNESS);

    // Group UI elements
    enableAmbient.setGroup("Features");
    enableMeteors.setGroup("Features");
    enableTimeWarp.setGroup("Features");

    meteorThreshold.setGroup("Audio Detection");
    ambientThreshold.setGroup("Audio Detection");

    dragSlider.setGroup("Physics");
    speedSlider.setGroup("Physics");
    minVelocitySlider.setGroup("Physics");

    ambientTrailSlider.setGroup("Trails");
    meteorTrailSlider.setGroup("Trails");
    ambientDecaySlider.setGroup("Trails");
    debrisDecaySlider.setGroup("Trails");
    debrisVelDecaySlider.setGroup("Trails");

    // Blue-white palettes
    CRGBPalette16 bluePalette(CRGB(0, 0, 40), CRGB(0, 40, 120),
                              CRGB(100, 160, 255), CRGB(255, 255, 255));
    sailboatFx.setNoisePalette(bluePalette);
    sailboatFx.setAmbientPalette(bluePalette);
    sailboatFx.setMeteorGradient(CRGB(255, 255, 255),  // white-hot head
                                 CRGB(140, 180, 255),   // light blue mid
                                 CRGB(0, 40, 120));     // deep blue tail

    // Register effect with engine
    fxEngine.addFx(sailboatFx);

    // --- Wire up audio detector callbacks ---
    // Uses the Vibe detector which self-normalizes bass levels:
    //   getVibeBass() ~1.0 = average for current song
    //   >1.0 = louder than average, <1.0 = quieter
    // This approach works across all music volumes and styles.
    auto audio = FastLED.add(audio_ui);
    fxEngine.setAudio(audio); // delivers AudioBatch to effects via DrawContext
    if (audio) {
        // Called every frame with full vibe data — we do threshold
        // checks here for both ambient and meteor spawning.
        audio->onVibeLevels(
            [&](const fl::audio::detector::VibeLevels &levels) {
                float bass = levels.bass; // self-normalizing, ~1.0 avg

                // Ambient: spawn on bass spikes above ambient threshold
                if (enableAmbient && levels.bassSpike &&
                    bass > ambientThreshold.value()) {
                    // Scale intensity: map bass from threshold..threshold*2
                    // to 0.3..1.0
                    float thresh = ambientThreshold.value();
                    float intensity =
                        fl::clamp((bass - thresh) / thresh, 0.3f, 1.0f);
                    int count = 1 + int(intensity * 2.0f); // 1-3 particles
                    for (int i = 0; i < count; i++) {
                        sailboatFx.spawnAmbient(intensity);
                    }
                }

                // Meteor: spawn only on very strong bass hits
                if (enableMeteors && levels.bassSpike &&
                    bass > meteorThreshold.value()) {
                    float thresh = meteorThreshold.value();
                    // Map bass from threshold..threshold*2 to 0.4..1.0
                    float intensity =
                        fl::clamp((bass - thresh) / thresh, 0.4f, 1.0f);
                    sailboatFx.spawnMeteor(intensity);
                    if (enableTimeWarp)
                        noiseTimeMultiplier = 1.0f + intensity * 2.0f;
                }
            });

        // Drop detector: biggest moments (has built-in 2s cooldown)
        audio->onDropImpact([&](float impact) {
            if (enableMeteors)
                sailboatFx.spawnMeteor(impact);
            if (enableTimeWarp)
                noiseTimeMultiplier = 2.0f + impact;
        });
    }
}

void loop() {
    //FL_WARN("Loop");
    //delay(10);
    // Decay time-warp back to normal
    noiseTimeMultiplier *= 0.95f;
    if (noiseTimeMultiplier < 1.01f)
        noiseTimeMultiplier = 1.0f;
    sailboatFx.setTimeMultiplier(noiseTimeMultiplier);

    // Update physics params from sliders
    float drag = 0.99f - dragSlider.value() * 0.19f;
    sailboatFx.setDrag(drag);
    sailboatFx.setSpeed(speedSlider.value());
    sailboatFx.setAmbientTrailIntensity(u8(ambientTrailSlider));
    sailboatFx.setMeteorTrailIntensity(u8(meteorTrailSlider));
    sailboatFx.setAmbientBrightnessDecay(ambientDecaySlider.value());
    sailboatFx.setMinVelocity(minVelocitySlider.value());
    sailboatFx.setDebrisBrightnessDecay(debrisDecaySlider.value());
    sailboatFx.setDebrisVelocityDecay(debrisVelDecaySlider.value());

    // Draw current effect and show
    fxEngine.draw(millis(), leds);
    FastLED.show();
    //delay(10);
}
