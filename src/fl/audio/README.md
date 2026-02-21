# FastLED Audio

## Quick Start Examples

### High-Level: Beat-Reactive LEDs (Recommended)

The `AudioProcessor` is the easiest way to build audio-reactive sketches. Register callbacks for the events you care about and feed it audio samples.

```cpp
#include "FastLED.h"
#include "fl/audio_input.h"
#include "fl/audio/audio_processor.h"

using namespace fl;

#define NUM_LEDS 60
#define DATA_PIN 5

CRGB leds[NUM_LEDS];
AudioProcessor audio;
shared_ptr<IAudioInput> mic;
uint8_t gHue = 0;
bool gBeat = false;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    // Create microphone input (ESP32 + INMP441)
    auto config = AudioConfig::CreateInmp441(
        15,  // WS pin
        32,  // SD pin
        14,  // SCK pin
        Left
    );
    string errorMsg;
    mic = IAudioInput::create(config, &errorMsg);
    if (!mic) {
        // Handle error - check errorMsg
        return;
    }
    mic->start();

    // Register event callbacks
    audio.onBeat([]() {
        gBeat = true;
    });

    audio.onBass([](float level) {
        // level is 0.0 - 1.0
        fill_solid(leds, NUM_LEDS / 3, CHSV(0, 255, level * 255));
    });

    audio.onMid([](float level) {
        fill_solid(leds + NUM_LEDS / 3, NUM_LEDS / 3, CHSV(96, 255, level * 255));
    });

    audio.onTreble([](float level) {
        fill_solid(leds + 2 * NUM_LEDS / 3, NUM_LEDS / 3, CHSV(160, 255, level * 255));
    });
}

void loop() {
    AudioSample sample = mic->read();
    if (sample.isValid()) {
        audio.update(sample);
    }

    if (gBeat) {
        gBeat = false;
        // Flash white on beat
        fill_solid(leds, NUM_LEDS, CRGB::White);
    }

    fadeToBlackBy(leds, NUM_LEDS, 20);
    FastLED.show();
}
```

### High-Level: Percussion-Reactive LEDs

Use `onKick()`, `onSnare()`, and `onHiHat()` callbacks to trigger different colors for each drum hit.

```cpp
#include "FastLED.h"
#include "fl/audio_input.h"
#include "fl/audio/audio_processor.h"

using namespace fl;

#define NUM_LEDS 60
#define DATA_PIN 5

CRGB leds[NUM_LEDS];
AudioProcessor audio;
shared_ptr<IAudioInput> mic;
CRGB gFlashColor = CRGB::Black;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    auto config = AudioConfig::CreateInmp441(15, 32, 14, Left);
    string errorMsg;
    mic = IAudioInput::create(config, &errorMsg);
    if (!mic) return;
    mic->start();

    // Each drum hit triggers a different color
    audio.onKick([]() {
        gFlashColor = CRGB::Red;
    });

    audio.onSnare([]() {
        gFlashColor = CRGB::Yellow;
    });

    audio.onHiHat([]() {
        gFlashColor = CRGB::Cyan;
    });

    audio.onTom([]() {
        gFlashColor = CRGB::Purple;
    });
}

void loop() {
    AudioSample sample = mic->read();
    if (sample.isValid()) {
        audio.update(sample);
    }

    if (gFlashColor != CRGB(CRGB::Black)) {
        fill_solid(leds, NUM_LEDS, gFlashColor);
        gFlashColor = CRGB::Black;
    }

    fadeToBlackBy(leds, NUM_LEDS, 30);
    FastLED.show();
}
```

### High-Level: Polling API (No Callbacks)

If you prefer polling over callbacks, `AudioProcessor` also provides getter methods that return `uint8_t`-scaled values (0-255), perfect for direct LED control.

```cpp
#include "FastLED.h"
#include "fl/audio_input.h"
#include "fl/audio/audio_processor.h"

using namespace fl;

#define NUM_LEDS 60
#define DATA_PIN 5

CRGB leds[NUM_LEDS];
AudioProcessor audio;
shared_ptr<IAudioInput> mic;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    auto config = AudioConfig::CreateInmp441(15, 32, 14, Left);
    string errorMsg;
    mic = IAudioInput::create(config, &errorMsg);
    if (!mic) return;
    mic->start();
}

void loop() {
    AudioSample sample = mic->read();
    if (sample.isValid()) {
        audio.update(sample);
    }

    // Poll values directly - all return 0-255
    uint8_t bass   = audio.getBassLevel();
    uint8_t mid    = audio.getMidLevel();
    uint8_t treble = audio.getTrebleLevel();
    uint8_t energy = audio.getEnergy();

    // Map bass to hue, energy to brightness
    fill_solid(leds, NUM_LEDS, CHSV(bass, 255, energy));

    // Strobe on beat
    if (audio.isBeat()) {
        fill_solid(leds, NUM_LEDS, CRGB::White);
    }

    // Percussion polling
    if (audio.isKick()) {
        fill_solid(leds, NUM_LEDS / 3, CRGB::Red);
    }
    if (audio.isSnare()) {
        fill_solid(leds + NUM_LEDS / 3, NUM_LEDS / 3, CRGB::Yellow);
    }

    // Tempo-aware effects
    float bpm = audio.getBPM();
    // Use bpm to sync animation speed

    FastLED.show();
}
```

### Mid-Level: Custom Detector

Create your own detector by subclassing `AudioDetector`. This gives you direct access to `AudioContext` for FFT data while integrating into the update loop.

```cpp
#include "FastLED.h"
#include "fl/audio_input.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_processor.h"

using namespace fl;

// Custom detector that triggers when a specific frequency band spikes
class SparkleDetector : public AudioDetector {
public:
    bool triggered = false;
    float intensity = 0.0f;

    // Tell the system we need FFT data
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "SparkleDetector"; }

    void update(shared_ptr<AudioContext> context) override {
        // Access cached FFT (computed once, shared across all detectors)
        const FFTBins& bins = context->getFFT(16);

        // Check high-frequency bins (bins 12-15) for sparkle trigger
        float highEnergy = 0.0f;
        for (int i = 12; i < 16; i++) {
            highEnergy += bins.bins_raw[i];
        }
        highEnergy /= 4.0f;

        triggered = (highEnergy > 0.6f);
        intensity = highEnergy;
    }

    void fireCallbacks() override {
        // Called after all detectors have updated - safe to trigger effects
        // (In this simple example we just set flags read in loop())
    }

    void reset() override {
        triggered = false;
        intensity = 0.0f;
    }
};
```

### Mid-Level: Multi-Detector with AudioContext

Create an `AudioContext` manually to share FFT data across multiple detectors. This demonstrates the two-phase update/fireCallbacks loop.

```cpp
#include "FastLED.h"
#include "fl/audio_input.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/audio_detector.h"

using namespace fl;

#define NUM_LEDS 60
#define DATA_PIN 5

CRGB leds[NUM_LEDS];
shared_ptr<IAudioInput> mic;

// Two simple detectors that share the same AudioContext
class BassMonitor : public AudioDetector {
public:
    float level = 0.0f;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "BassMonitor"; }

    void update(shared_ptr<AudioContext> context) override {
        const FFTBins& bins = context->getFFT(16);
        // Average first 4 bins (low frequencies)
        level = 0.0f;
        for (int i = 0; i < 4; i++) level += bins.bins_raw[i];
        level /= 4.0f;
    }
};

class TrebleMonitor : public AudioDetector {
public:
    float level = 0.0f;
    bool needsFFT() const override { return true; }
    const char* getName() const override { return "TrebleMonitor"; }

    void update(shared_ptr<AudioContext> context) override {
        const FFTBins& bins = context->getFFT(16);
        // Average last 4 bins (high frequencies)
        level = 0.0f;
        for (int i = 12; i < 16; i++) level += bins.bins_raw[i];
        level /= 4.0f;
    }
};

BassMonitor bassMonitor;
TrebleMonitor trebleMonitor;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    auto config = AudioConfig::CreateInmp441(15, 32, 14, Left);
    string errorMsg;
    mic = IAudioInput::create(config, &errorMsg);
    if (!mic) return;
    mic->start();
}

void loop() {
    AudioSample sample = mic->read();
    if (!sample.isValid()) return;

    // Create a shared context - FFT is computed once, shared by both detectors
    auto context = make_shared<AudioContext>(sample);

    // Phase 1: Update all detectors (reads from context, no side effects)
    bassMonitor.update(context);
    trebleMonitor.update(context);

    // Phase 2: Fire callbacks (safe to trigger effects now)
    bassMonitor.fireCallbacks();
    trebleMonitor.fireCallbacks();

    // Use detector results
    uint8_t bassBrightness = bassMonitor.level * 255;
    uint8_t trebleBrightness = trebleMonitor.level * 255;

    for (int i = 0; i < NUM_LEDS / 2; i++) {
        leds[i] = CHSV(0, 255, bassBrightness);  // Red for bass
    }
    for (int i = NUM_LEDS / 2; i < NUM_LEDS; i++) {
        leds[i] = CHSV(160, 255, trebleBrightness);  // Blue for treble
    }

    FastLED.show();
}
```

### Low-Level: Direct FFT Access

For full control, use `AudioSample` and `FFT` directly. This is useful when you want to build your own visualizer or analysis pipeline.

```cpp
#include "FastLED.h"
#include "fl/audio_input.h"
#include "fl/audio.h"
#include "fl/fft.h"

using namespace fl;

#define NUM_LEDS 16
#define DATA_PIN 5

CRGB leds[NUM_LEDS];
shared_ptr<IAudioInput> mic;
FFT fft;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    auto config = AudioConfig::CreateInmp441(15, 32, 14, Left);
    string errorMsg;
    mic = IAudioInput::create(config, &errorMsg);
    if (!mic) return;
    mic->start();
}

void loop() {
    AudioSample sample = mic->read();
    if (!sample.isValid()) return;

    // Run FFT - 16 bins mapped to 16 LEDs
    FFTBins bins(16);
    FFT_Args args(512, 16, 174.6f, 4698.3f, 44100);
    fft.run(sample.pcm(), &bins, args);

    // Map FFT bins directly to LED brightness
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = constrain(bins.bins_raw[i] * 255, 0, 255);
        leds[i] = CHSV(i * 16, 255, brightness);
    }

    FastLED.show();
}
```

### Low-Level: Raw PCM Analysis

For the simplest audio reactivity without FFT, use `AudioSample` properties directly.

```cpp
#include "FastLED.h"
#include "fl/audio_input.h"
#include "fl/audio.h"

using namespace fl;

#define NUM_LEDS 30
#define DATA_PIN 5

CRGB leds[NUM_LEDS];
shared_ptr<IAudioInput> mic;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    auto config = AudioConfig::CreateInmp441(15, 32, 14, Left);
    string errorMsg;
    mic = IAudioInput::create(config, &errorMsg);
    if (!mic) return;
    mic->start();
}

void loop() {
    AudioSample sample = mic->read();
    if (!sample.isValid()) return;

    float rms = sample.rms();       // Volume level (RMS amplitude)
    float zcf = sample.zcf();       // Zero-crossing factor (0.0 - 1.0)

    // High ZCF (> 0.4) = hissing/noise, low ZCF + high RMS = music
    bool isMusic = (zcf < 0.35f) && (rms > 500.0f);

    // Map RMS to LED count (simple VU meter)
    int litCount = map(constrain(rms, 0, 10000), 0, 10000, 0, NUM_LEDS);

    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int i = 0; i < litCount; i++) {
        // Green -> Yellow -> Red gradient
        leds[i] = CHSV(96 - (i * 96 / NUM_LEDS), 255, isMusic ? 255 : 64);
    }

    FastLED.show();
}
```

### Synthesizer: Generating Audio Output

The `Synth` module generates bandlimited waveforms for audio output. Useful for creating tones, alerts, or musical output from your microcontroller.

```cpp
#include "FastLED.h"
#include "fl/audio/synth.h"

using namespace fl;

void setup() {
    Serial.begin(115200);

    // Create a shared engine (holds anti-aliasing tables)
    auto engine = ISynthEngine::create(32, 16);

    // Create oscillators
    auto saw = ISynthOscillator::create(engine, SynthShape::Sawtooth);
    auto sq  = ISynthOscillator::create(engine, SynthShape::Square);
    auto tri = ISynthOscillator::create(engine, SynthShape::Triangle);

    // Generate 256 samples of a 440 Hz tone at 44.1 kHz
    float buffer[256];
    float freq = 440.0f / 44100.0f;  // Normalized frequency
    saw->generateSamples(buffer, 256, freq);

    // Custom waveform via SynthParams(reflect, peakTime, halfHeight, zeroWait)
    SynthParams custom(1, 0.3f, 0.5f, 0.1f);
    auto osc = ISynthOscillator::create(engine, custom);
    osc->generateSamples(buffer, 256, freq);
}

void loop() {
    // Synth runs in setup for this demo
}
```

---

## API Layers

```
┌──────────────────────────────────────────────┐
│  Your Sketch (.ino)                          │
├──────────────────────────────────────────────┤
│  AudioProcessor    (high-level facade)       │  ← Start here
│    callbacks:  onBeat(), onBass(), ...       │
│    polling:    isBeat(), getBassLevel(), ... │
├──────────────────────────────────────────────┤
│  AudioContext      (shared FFT cache)        │  ← Intermediate
│  AudioDetector     (base class)              │
│  Detectors:  Beat, Vocal, Percussion, ...   │
├──────────────────────────────────────────────┤
│  AudioSample       (PCM + RMS + ZCF)         │  ← Low-level
│  FFT / FFTBins     (spectrum analysis)       │
│  SoundLevelMeter   (dBFS / SPL calibration)  │
├──────────────────────────────────────────────┤
│  IAudioInput       (hardware abstraction)    │  ← Platform
│    I2S, PDM, Teensy Audio Library            │
└──────────────────────────────────────────────┘
```

### Choosing Your Level

| Level | Use when... | Key classes |
|-------|-------------|-------------|
| **High (AudioProcessor)** | You want beat/bass/vocal detection without writing DSP code | `AudioProcessor` |
| **Mid (AudioContext)** | You're writing a custom detector or need shared FFT caching | `AudioContext`, `AudioDetector` |
| **Low (AudioSample/FFT)** | You want raw spectrum data or PCM-level control | `AudioSample`, `FFT`, `FFTBins` |
| **Output (Synth)** | You need to generate audio waveforms | `ISynthEngine`, `ISynthOscillator` |

---

## How It Works

### Update Loop (3-Stage Pipeline)

When you call `audio.update(sample)`, three stages run in sequence:

1. **Signal Conditioning** — Raw PCM is cleaned: DC offset removal, spike filtering, noise gate. This stage modifies the sample before any analysis.

2. **Detector Update** — Each active detector's `update(context)` is called with a shared `AudioContext`. Detectors read FFT/PCM data and compute their internal state, but do **not** fire callbacks yet. The FFT is computed lazily on first access and cached — if three detectors all call `context->getFFT(16)`, the FFT runs only once.

3. **Callback Firing** — After all detectors have finished updating, each detector's `fireCallbacks()` is called. This two-phase design prevents callback code from interfering with other detectors' analysis within the same frame.

### Lazy Creation

Detectors are created only when you register a callback or call a polling getter. If you only use `onBeat()` and `getBassLevel()`, only `BeatDetector` and `FrequencyBands` are instantiated. The rest consume zero memory.

### FFT Caching

`AudioContext` caches FFT results per frame. Multiple detectors requesting the same FFT parameters share a single computation. This is why the mid-level API passes a `shared_ptr<AudioContext>` — it's the shared cache.

---

## AudioProcessor Event Reference

### Callbacks

| Category | Callbacks |
|----------|-----------|
| **Beat** | `onBeat(void())`, `onBeatPhase(float)`, `onOnset(float)`, `onTempoChange(float, float)` |
| **Tempo** | `onTempo(float)`, `onTempoWithConfidence(float, float)`, `onTempoStable()`, `onTempoUnstable()` |
| **Frequency** | `onBass(float)`, `onMid(float)`, `onTreble(float)`, `onFrequencyBands(float, float, float)` |
| **Energy** | `onEnergy(float)`, `onNormalizedEnergy(float)`, `onPeak(float)`, `onAverageEnergy(float)` |
| **Transient** | `onTransient()`, `onTransientWithStrength(float)`, `onAttack(float)` |
| **Silence** | `onSilence(u8)`, `onSilenceStart()`, `onSilenceEnd()`, `onSilenceDuration(u32)` |
| **Pitch** | `onPitch(float)`, `onPitchWithConfidence(float, float)`, `onPitchChange(float)`, `onVoiced(u8)` |
| **Note** | `onNoteOn(u8, u8)`, `onNoteOff(u8)`, `onNoteChange(u8, u8)` |
| **Percussion** | `onPercussion(PercussionType)`, `onKick()`, `onSnare()`, `onHiHat()`, `onTom()` |
| **Vocal** | `onVocal(u8)`, `onVocalStart()`, `onVocalEnd()`, `onVocalConfidence(float)` |
| **Dynamics** | `onCrescendo()`, `onDiminuendo()`, `onDynamicTrend(float)`, `onCompressionRatio(float)` |
| **Downbeat** | `onDownbeat()`, `onMeasureBeat(u8)`, `onMeterChange(u8)`, `onMeasurePhase(float)` |
| **Backbeat** | `onBackbeat(u8, float, float)` |
| **Chord** | `onChord(Chord)`, `onChordChange(Chord)`, `onChordEnd()` |
| **Key** | `onKey(Key)`, `onKeyChange(Key)`, `onKeyEnd()` |
| **Mood** | `onMood(Mood)`, `onMoodChange(Mood)`, `onValenceArousal(float, float)` |
| **Buildup** | `onBuildupStart()`, `onBuildupProgress(float)`, `onBuildupPeak()`, `onBuildupEnd()`, `onBuildup(Buildup)` |
| **Drop** | `onDrop()`, `onDropEvent(Drop)`, `onDropImpact(float)` |

### Polling Getters

```cpp
// Beat Detection
audio.isBeat();              // u8 - Beat detected this frame?
audio.getBeatConfidence();   // u8 - Beat confidence 0-255
audio.getBPM();              // float - Estimated BPM

// Frequency Bands (0-255)
audio.getBassLevel();
audio.getMidLevel();
audio.getTrebleLevel();

// Energy
audio.getEnergy();           // u8 - Overall energy 0-255
audio.getPeakLevel();        // u8 - Peak level 0-255

// Percussion
audio.isKick();              // u8 - Kick drum detected?
audio.isSnare();             // u8 - Snare detected?
audio.isHiHat();             // u8 - Hi-hat detected?
audio.isTom();               // u8 - Tom detected?

// Vocal
audio.isVocalActive();       // u8 - Vocals present?
audio.getVocalConfidence();  // u8 - Vocal confidence 0-255

// Silence
audio.isSilent();            // u8 - Silence detected?
audio.getSilenceDuration();  // u32 - Silence duration in ms

// Transient
audio.isTransient();         // u8 - Transient detected?
audio.getTransientStrength();// u8 - Transient strength 0-255

// Dynamics
audio.isCrescendo();         // u8 - Volume increasing?
audio.isDiminuendo();        // u8 - Volume decreasing?
audio.getDynamicTrend();     // u8 - Trend direction 0-255

// Pitch
audio.getPitch();            // float - Pitch in Hz
audio.getPitchConfidence();  // u8 - Confidence 0-255
audio.isVoiced();            // u8 - Pitched sound detected?

// Tempo
audio.getTempoBPM();         // float - Tempo in BPM
audio.getTempoConfidence();  // u8 - Tempo confidence 0-255
audio.isTempoStable();       // u8 - Tempo locked?

// Note
audio.getCurrentNote();      // u8 - MIDI note number
audio.getNoteVelocity();     // u8 - Note velocity 0-255
audio.isNoteActive();        // u8 - Note currently playing?

// Downbeat / Structure
audio.isDownbeat();          // u8 - Downbeat this frame?
audio.getCurrentBeatNumber();// u8 - Beat within measure
audio.getMeasurePhase();     // u8 - Phase through measure 0-255

// Backbeat
audio.getBackbeatConfidence(); // u8 - Backbeat confidence 0-255
audio.getBackbeatStrength();   // u8 - Backbeat strength 0-255

// Buildup / Drop (EDM)
audio.isBuilding();          // u8 - Buildup in progress?
audio.getBuildupProgress();  // u8 - Buildup progress 0-255
audio.getBuildupIntensity(); // u8 - Buildup intensity 0-255
audio.getDropImpact();       // u8 - Drop impact 0-255

// Chord / Key / Mood
audio.hasChord();            // u8 - Chord detected?
audio.getChordConfidence();  // u8 - Chord confidence 0-255
audio.hasKey();              // u8 - Key detected?
audio.getKeyConfidence();    // u8 - Key confidence 0-255
audio.getMoodValence();      // u8 - Happy/sad 0-255
audio.getMoodArousal();      // u8 - Calm/energetic 0-255
```

---

## Writing a Custom Detector

To create your own detector that integrates with `AudioProcessor`:

### Step 1: Subclass `AudioDetector`

```cpp
#include "fl/audio/audio_detector.h"
#include "fl/audio/audio_context.h"

class MyDetector : public fl::AudioDetector {
public:
    // Declare FFT needs (return true if your detector uses frequency data)
    bool needsFFT() const override { return true; }

    // Return true if you need multiple frames of FFT history
    bool needsFFTHistory() const override { return false; }

    // Unique name for debugging
    const char* getName() const override { return "MyDetector"; }

    // Phase 1: Read from context, compute internal state
    // Do NOT fire callbacks or cause side effects here
    void update(fl::shared_ptr<fl::AudioContext> context) override {
        const fl::FFTBins& bins = context->getFFT(16);
        // ... analyze bins, update internal state ...
    }

    // Phase 2: Fire callbacks, set flags, trigger effects
    // Called after ALL detectors have updated
    void fireCallbacks() override {
        // ... notify listeners ...
    }

    // Optional: handle sample rate changes
    void setSampleRate(int rate) override { mSampleRate = rate; }

    // Optional: reset state
    void reset() override { /* ... */ }

private:
    int mSampleRate = 44100;
};
```

### Step 2: Use with AudioContext

Wire your detector into a manual update loop (see the Multi-Detector example above), or use it standalone:

```cpp
auto context = fl::make_shared<fl::AudioContext>(sample);
myDetector.update(context);
myDetector.fireCallbacks();
```

---

## Signal Conditioning

`AudioProcessor` includes a three-stage signal conditioning pipeline. Each stage can be enabled/disabled independently.

### Enable/Disable

```cpp
audio.setSignalConditioningEnabled(true);   // DC removal, spike filter, noise gate
audio.setAutoGainEnabled(true);             // Automatic gain control
audio.setNoiseFloorTrackingEnabled(true);   // Adaptive noise floor
```

### Configuration Structs

**SignalConditionerConfig** — Cleans raw I2S/PCM data:

| Field | Default | Description |
|-------|---------|-------------|
| `enableDCRemoval` | `true` | Remove DC offset via running-average high-pass filter |
| `enableSpikeFilter` | `true` | Reject I2S glitch samples beyond threshold |
| `enableNoiseGate` | `true` | Hysteresis-based noise gate |
| `spikeThreshold` | `10000` | Absolute sample value beyond which samples are rejected |
| `noiseGateOpenThreshold` | `500` | Signal must exceed this to open the gate |
| `noiseGateCloseThreshold` | `300` | Signal must fall below this to close the gate |
| `dcRemovalAlpha` | `0.99f` | Time constant (higher = slower DC adaptation) |

**AutoGainConfig** — Adaptive gain using Robbins-Monro percentile estimation:

| Field | Default | Description |
|-------|---------|-------------|
| `targetPercentile` | `0.9f` | Track P90 of signal distribution |
| `learningRate` | `0.05f` | How quickly the estimate adapts (0.01-0.1 typical) |
| `minGain` | `0.1f` | Minimum gain multiplier |
| `maxGain` | `10.0f` | Maximum gain multiplier |
| `targetRMSLevel` | `8000.0f` | Target RMS level after gain (0-32767) |
| `gainSmoothing` | `0.95f` | Smoothing factor for gain changes |

**NoiseFloorTrackerConfig** — Adaptive noise floor with hysteresis:

| Field | Default | Description |
|-------|---------|-------------|
| `decayRate` | `0.99f` | How slowly the floor decays (higher = more stable) |
| `attackRate` | `0.001f` | How quickly the floor rises when signal is low |
| `hysteresisMargin` | `100.0f` | Floor must drop by this before it can rise again |
| `minFloor` | `10.0f` | Prevents floor from reaching zero |
| `maxFloor` | `5000.0f` | Prevents floor from growing unbounded |
| `crossDomainWeight` | `0.3f` | Blend of time-domain (0.0) vs frequency-domain (1.0) |

### Tuning Example

```cpp
// Noisy environment: raise thresholds, slower adaptation
SignalConditionerConfig scConfig;
scConfig.spikeThreshold = 15000;
scConfig.noiseGateOpenThreshold = 1000;
scConfig.noiseGateCloseThreshold = 700;
audio.configureSignalConditioner(scConfig);

// Quiet venue: aggressive auto-gain, fast adaptation
AutoGainConfig agcConfig;
agcConfig.learningRate = 0.1f;
agcConfig.maxGain = 20.0f;
agcConfig.targetRMSLevel = 12000.0f;
audio.configureAutoGain(agcConfig);

// Outdoor use: wide hysteresis to handle wind noise
NoiseFloorTrackerConfig nfConfig;
nfConfig.hysteresisMargin = 300.0f;
nfConfig.decayRate = 0.995f;
audio.configureNoiseFloorTracker(nfConfig);
```

---

## Detector Quick Reference

| Detector | FFT? | History? | Key Callbacks | Key Polling |
|----------|------|----------|---------------|-------------|
| **BeatDetector** | Yes | No | `onBeat()`, `onOnset(float)` | `isBeat()`, `getBeatConfidence()` |
| **TempoAnalyzer** | No | No | `onTempo(float)`, `onTempoStable()` | `getTempoBPM()`, `isTempoStable()` |
| **FrequencyBands** | Yes | No | `onBass(float)`, `onMid(float)`, `onTreble(float)` | `getBassLevel()`, `getMidLevel()`, `getTrebleLevel()` |
| **EnergyAnalyzer** | No | No | `onEnergy(float)`, `onPeak(float)` | `getEnergy()`, `getPeakLevel()` |
| **TransientDetector** | Yes | No | `onTransient()`, `onAttack(float)` | `isTransient()`, `getTransientStrength()` |
| **SilenceDetector** | No | No | `onSilenceStart()`, `onSilenceEnd()` | `isSilent()`, `getSilenceDuration()` |
| **DynamicsAnalyzer** | No | No | `onCrescendo()`, `onDiminuendo()` | `isCrescendo()`, `isDiminuendo()` |
| **PitchDetector** | Yes | No | `onPitch(float)`, `onVoiced(u8)` | `getPitch()`, `isVoiced()` |
| **NoteDetector** | Yes | No | `onNoteOn(u8, u8)`, `onNoteOff(u8)` | `getCurrentNote()`, `isNoteActive()` |
| **DownbeatDetector** | No | No | `onDownbeat()`, `onMeasureBeat(u8)` | `isDownbeat()`, `getCurrentBeatNumber()` |
| **BackbeatDetector** | No | No | `onBackbeat(u8, float, float)` | `getBackbeatConfidence()` |
| **VocalDetector** | Yes | No | `onVocalStart()`, `onVocalEnd()` | `isVocalActive()`, `getVocalConfidence()` |
| **PercussionDetector** | Yes | No | `onKick()`, `onSnare()`, `onHiHat()`, `onTom()` | `isKick()`, `isSnare()`, `isHiHat()`, `isTom()` |
| **ChordDetector** | Yes | Yes | `onChord(Chord)`, `onChordChange(Chord)` | `hasChord()`, `getChordConfidence()` |
| **KeyDetector** | Yes | Yes | `onKey(Key)`, `onKeyChange(Key)` | `hasKey()`, `getKeyConfidence()` |
| **MoodAnalyzer** | Yes | Yes | `onMood(Mood)`, `onValenceArousal(float, float)` | `getMoodValence()`, `getMoodArousal()` |
| **BuildupDetector** | Yes | No | `onBuildupStart()`, `onBuildupPeak()` | `isBuilding()`, `getBuildupProgress()` |
| **DropDetector** | Yes | No | `onDrop()`, `onDropImpact(float)` | `getDropImpact()` |

---

## Platform Support

| Platform | Microphone | Configuration |
|----------|-----------|---------------|
| **ESP32** | INMP441 (I2S) | `AudioConfig::CreateInmp441(ws, sd, clk, channel)` |
| **ESP32** | PDM mic | `AudioConfig(AudioConfigPdm(din, clk, i2s_num))` |
| **Teensy** | I2S mic | `AudioConfig::CreateTeensyI2S(port, channel)` |

---

## File Structure

```
fl/audio/
├── README.md                    # This file
├── audio_context.h/.cpp.hpp     # Shared FFT cache (lazy evaluation)
├── audio_detector.h             # Base class for all detectors
├── audio_processor.h/.cpp.hpp   # High-level facade (callbacks + polling)
├── synth.h/.cpp.hpp             # Bandlimited waveform synthesizer
├── auto_gain.h/.cpp.hpp         # Automatic gain control
├── signal_conditioner.h/.cpp.hpp # Signal conditioning pipeline
├── noise_floor_tracker.h/.cpp.hpp # Adaptive noise floor
├── frequency_bin_mapper.h/.cpp.hpp # FFT bin frequency mapping
├── spectral_equalizer.h/.cpp.hpp # Spectral equalization
└── detectors/                   # All detector implementations
    ├── beat.h         # Beat detection
    ├── vocal.h        # Vocal presence detection
    ├── percussion.h   # Kick/snare/hihat/tom detection
    ├── pitch.h        # Pitch estimation
    ├── note.h         # MIDI-style note detection
    ├── chord.h        # Chord recognition
    ├── key.h          # Musical key detection
    ├── mood_analyzer.h # Valence/arousal analysis
    ├── tempo_analyzer.h # BPM estimation
    ├── downbeat.h     # Downbeat / measure tracking
    ├── backbeat.h     # Backbeat detection
    ├── buildup.h      # Buildup detection (EDM)
    ├── drop.h         # Drop detection (EDM)
    ├── transient.h    # Transient / attack detection
    ├── silence.h      # Silence detection
    ├── dynamics_analyzer.h # Crescendo / diminuendo
    ├── energy_analyzer.h   # RMS energy tracking
    └── frequency_bands.h   # Bass/mid/treble splitting
```
