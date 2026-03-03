# FastLED Audio

## Quick Start Examples

### Easiest: `FastLED.add(AudioConfig)` (Recommended)

The simplest way to get audio-reactive LEDs. `FastLED.add()` creates the microphone,
wires up a scheduler task that auto-reads samples, and returns an `AudioProcessor`
ready for callbacks. No manual `update()` loop needed — audio is pumped automatically
during `FastLED.show()`.

```cpp
#include "FastLED.h"

#define NUM_LEDS 60
#define LED_PIN 2

// I2S pins for INMP441 microphone (adjust for your board)
#define I2S_WS  7
#define I2S_SD  8
#define I2S_CLK 4

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(128);

    // One line: create mic + auto-pump task
    // FastLED stores the AudioProcessor internally, so no global needed
    auto config = fl::AudioConfig::CreateInmp441(I2S_WS, I2S_SD, I2S_CLK, fl::Right);
    auto audio = FastLED.add(config);
    audio->setAutoGainEnabled(true);

    // Flash white on every beat
    audio->onBeat([] {
        fill_solid(leds, NUM_LEDS, CRGB::White);
    });

    // Map bass level to hue
    audio->onBass([](float level) {
        uint8_t hue = static_cast<uint8_t>(level * 160);
        fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
    });

    // Dim to black on silence
    audio->onSilenceStart([] {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
    });

    // audio goes out of scope here — that's fine, FastLED keeps it alive.
    // Use FastLED.remove(audio) if you ever need to tear it down.
}

void loop() {
    fadeToBlackBy(leds, NUM_LEDS, 20);
    FastLED.show();  // Audio is auto-pumped here
}
```

**How it works:** `FastLED.add(config)` internally calls `IAudioInput::create()`, starts
the mic, stores the `AudioProcessor` in an internal list, and creates a
`fl::task::every_ms(1)` that drains all buffered samples and feeds them to the
`AudioProcessor`. The task runs during `FastLED.show()` via
end-frame → `async_run()` → `Scheduler::update()`. Use `FastLED.remove(audio)` to
tear down a specific processor, or let it live for the lifetime of the program.

**Platform behavior:**
- **ESP32 / Teensy** (`FASTLED_HAS_AUDIO_INPUT == 1`): Real I2S mic is created and pumped.
- **Host / WASM / AVR** (`FASTLED_HAS_AUDIO_INPUT == 0`): Returns a valid but inert
  `AudioProcessor` — callbacks never fire, polling getters return zero. Code compiles
  everywhere without `#ifdef`.

**Test injection:** On any platform, pass a custom `IAudioInput` directly:
```cpp
auto fakeInput = fl::make_shared<MyTestAudioSource>();
auto audio = FastLED.add(fakeInput);  // works on host/stub too
```

### High-Level: Beat-Reactive LEDs (Manual Update)

If you need more control over when samples are read (e.g., reading from a buffer
at a specific rate), you can create the `AudioProcessor` yourself and call
`update()` manually.

```cpp
#include "FastLED.h"
#include "fl/audio/input.h"
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
        audio.update(sample);  // Manual update
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
#include "fl/audio/input.h"
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
#include "fl/audio/input.h"
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

### WLED-Style Equalizer (16-Bin Spectrum)

A dead-simple WLED-compatible equalizer: 16 frequency bins normalized to 0.0-1.0, plus bass/mid/treble/volume/zcf convenience getters. All values are pre-normalized — just multiply by 255 if you want bytes.

```cpp
#include "FastLED.h"

#define NUM_LEDS 16
CRGB leds[NUM_LEDS];
fl::shared_ptr<fl::AudioProcessor> audio;

void setup() {
    FastLED.addLeds<WS2812B, 2, GRB>(leds, NUM_LEDS);

    auto config = fl::AudioConfig::CreateInmp441(7, 8, 4, fl::Right);
    audio = FastLED.add(config);

    // Callback: get everything in one struct
    audio->onEqualizer([](const fl::Equalizer& eq) {
        // eq.bass, eq.mid, eq.treble, eq.volume, eq.zcf — all 0.0-1.0
        // eq.bins — span<const float, 16>, each 0.0-1.0
        for (int i = 0; i < 16; ++i) {
            uint8_t brightness = static_cast<uint8_t>(eq.bins[i] * 255);
            leds[i] = CHSV(i * 16, 255, brightness);
        }
    });
}

void loop() {
    FastLED.show();
}
```

Or use polling — no callbacks needed:

```cpp
void loop() {
    // All return 0.0-1.0
    float bass   = audio->getEqBass();
    float mid    = audio->getEqMid();
    float treble = audio->getEqTreble();
    float volume = audio->getEqVolume();
    float zcf    = audio->getEqZcf();
    float bin5   = audio->getEqBin(5);  // 16 bins (0-15)

    fill_solid(leds, NUM_LEDS, CHSV(bass * 160, 255, volume * 255));
    FastLED.show();
}
```

**Bin layout (WLED-compatible):**
| Bins | Range | Getter |
|------|-------|--------|
| 0-3 | ~60-320 Hz (bass) | `getEqBass()` |
| 4-10 | ~320-2560 Hz (mid) | `getEqMid()` |
| 11-15 | ~2560-5120 Hz (treble) | `getEqTreble()` |

### VibeDetector: MilkDrop-Inspired Self-Normalizing Audio Analysis

`VibeDetector` provides self-normalizing, FPS-independent bass/mid/treb levels with
asymmetric attack/decay smoothing. The algorithm is a direct port of Ryan Geiss's
`DoCustomSoundAnalysis()` from [MilkDrop](https://www.geisswerks.com/milkdrop/) v2.25c
— the legendary Winamp visualizer. See `MILK_DROP_AUDIO_REACTIVE.md` for a detailed
technical analysis of the original algorithm.

Key properties:
- **Self-normalizing**: Levels hover around 1.0 regardless of volume, mic sensitivity, or genre
- **Asymmetric smoothing**: Fast attack (beats hit hard), slow decay (graceful fade)
- **Dual timescale**: Short-term average for beat tracking, long-term average for song-level adaptation
- **FPS-independent**: Identical behavior at 30fps, 60fps, or 144fps
- **Spike detection**: `bass > bass_att` means a beat is happening right now

#### Understanding the Values

Vibe levels are **not** 0.0-1.0 like the Equalizer. They are **ratios** against the
long-term average energy of the current song:

| Value | Meaning |
|-------|---------|
| `1.0` | Average level for this song/environment |
| `> 1.0` | Louder than recent average (spike/beat) |
| `< 1.0` | Quieter than recent average |
| `~0.7` | Quiet passage |
| `~1.3` | Loud passage / beat hit |

The self-normalization means the same preset code works on quiet acoustic songs and
loud electronic music without any gain or threshold calibration.

#### Two Sets of Levels: Immediate vs Smoothed

Each band has two relative levels:

- **`bass` / `mid` / `treb`** — Immediate relative level. Reacts instantly to audio changes.
- **`bassAtt` / `midAtt` / `trebAtt`** — Smoothed ("attenuated") relative level. Follows the signal with asymmetric attack/decay: fast attack (80% new signal on beats), slow decay (graceful fadeout).

The relationship between these two is the core of MilkDrop's beat detection:
- When `bass > bassAtt`, energy is rising — **a beat is happening**
- When `bass < bassAtt`, energy is falling — fading out between beats

#### Beat Detection Pattern

The canonical MilkDrop idiom for beat-reactive effects:

```cpp
// In your loop:
float bass = audio->getVibeBass();         // immediate relative
float bassAtt = audio->getVibeBassAtt();   // smoothed relative

// Beat intensity: positive when a beat is hitting, zero/negative otherwise
float beatIntensity = bass - bassAtt;

// Binary beat: true/false
bool beat = bass > bassAtt;
// Or use the convenience method:
bool beat = audio->isVibeBassSpike();

// Scale an effect proportionally to the music's dynamics:
float zoom = 1.0f + 0.1f * (bass - 1.0f);

// Decay that responds to bass hits:
uint8_t decay = 240 + static_cast<uint8_t>(bass * 10);
```

#### Choosing Between Vibe and Other Band APIs

| API | Range | Use when... |
|-----|-------|-------------|
| `getVibeBass()` | ~1.0 (unbounded) | You want self-normalizing, beat-reactive effects that adapt to any song |
| `getBassLevel()` | 0.0-1.0 | You want simple normalized levels for brightness/color mapping |
| `getEqBass()` | 0.0-1.0 | You want WLED-compatible spectrum analysis |
| `getBassRaw()` | 0+ (absolute) | You need raw FFT energy for custom algorithms |

#### Example: Callback API

```cpp
#include "FastLED.h"

#define NUM_LEDS 60
#define LED_PIN 2

// I2S pins for INMP441 microphone
#define I2S_WS  7
#define I2S_SD  8
#define I2S_CLK 4

CRGB leds[NUM_LEDS];
fl::shared_ptr<fl::AudioProcessor> audio;

void setup() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(128);

    auto config = fl::AudioConfig::CreateInmp441(I2S_WS, I2S_SD, I2S_CLK, fl::Right);
    audio = FastLED.add(config);

    // Flash white on bass spike (rising edge only)
    audio->onVibeBassSpike([] {
        fill_solid(leds, NUM_LEDS, CRGB::White);
    });

    // React to all three bands every frame
    audio->onVibeLevels([](const fl::VibeLevels& v) {
        // v.bass/mid/treb hover around 1.0; >1 means louder than recent average
        uint8_t hue = static_cast<uint8_t>(v.mid * 80);
        uint8_t brightness = static_cast<uint8_t>(constrain(v.vol * 200, 0, 255));
        fill_solid(leds, NUM_LEDS, CHSV(hue, 255, brightness));
    });
}

void loop() {
    fadeToBlackBy(leds, NUM_LEDS, 20);
    FastLED.show();  // Audio is auto-pumped here
}
```

#### Polling API (Complete)

```cpp
// ---- Immediate relative levels (~1.0 = average, unbounded) ----
float bass = audio->getVibeBass();       // Immediate relative bass
float mid  = audio->getVibeMid();        // Immediate relative mid
float treb = audio->getVibeTreb();       // Immediate relative treble
float vol  = audio->getVibeVol();        // Average of bass/mid/treb

// ---- Smoothed relative levels (for beat comparison) ----
float bassAtt = audio->getVibeBassAtt(); // Smoothed relative bass
float midAtt  = audio->getVibeMidAtt();  // Smoothed relative mid
float trebAtt = audio->getVibeTrebAtt(); // Smoothed relative treble
float volAtt  = audio->getVibeVolAtt();  // Average of smoothed bands

// ---- Spike detection (bass > bassAtt = beat) ----
bool bassBeat = audio->isVibeBassSpike();  // true when bass is rising
bool midBeat  = audio->isVibeMidSpike();   // true when mid is rising
bool trebBeat = audio->isVibeTrebSpike();  // true when treb is rising
```

#### VibeLevels Callback Struct

The `onVibeLevels` callback provides everything in one struct per frame:

```cpp
struct VibeLevels {
    // Self-normalizing relative levels (~1.0 = average)
    float bass, mid, treb;     // Immediate relative
    float vol;                 // (bass + mid + treb) / 3

    // Spike detection
    bool bassSpike, midSpike, trebSpike;  // true when energy is rising

    // Absolute values (for advanced use)
    float bassRaw, midRaw, trebRaw;           // Immediate absolute energy
    float bassAvg, midAvg, trebAvg;           // Short-term smoothed absolute
    float bassLongAvg, midLongAvg, trebLongAvg; // Long-term average absolute
};
```

#### Spike Callbacks

Spike callbacks fire on the **rising edge** only (transition from no-spike to spike),
so you get one event per beat rather than continuous firing:

```cpp
audio->onVibeBassSpike([] { /* bass beat! */ });
audio->onVibeMidSpike([] { /* mid-range transient */ });
audio->onVibeTrebSpike([] { /* high-frequency hit (hi-hat, cymbal) */ });
```

### Mid-Level: Custom Detector

Create your own detector by subclassing `AudioDetector`. This gives you direct access to `AudioContext` for FFT data while integrating into the update loop.

```cpp
#include "FastLED.h"
#include "fl/audio/input.h"
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
#include "fl/audio/input.h"
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
#include "fl/audio/input.h"
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
#include "fl/audio/input.h"
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
│  FastLED.add(AudioConfig)  (integration)     │  ← Easiest
│    auto-pump via fl::task::every_ms(1)       │
│    returns shared_ptr<AudioProcessor>        │
├──────────────────────────────────────────────┤
│  AudioProcessor    (high-level facade)       │  ← Manual control
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
| **Easiest (FastLED.add)** | You want audio-reactive LEDs with zero boilerplate | `FastLED.add(AudioConfig)` |
| **High (AudioProcessor)** | You want manual control over when samples are processed | `AudioProcessor` |
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
| **Equalizer** | `onEqualizer(const Equalizer&)` |

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

// Equalizer (WLED-style, all 0.0-1.0)
audio.getEqBass();           // float - Bass level 0.0-1.0
audio.getEqMid();            // float - Mid level 0.0-1.0
audio.getEqTreble();         // float - Treble level 0.0-1.0
audio.getEqVolume();         // float - Volume 0.0-1.0 (AGC-normalized)
audio.getEqZcf();            // float - Zero-crossing factor 0.0-1.0
audio.getEqBin(0);           // float - Bin 0 level 0.0-1.0 (16 bins total)
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

**AutoGainConfig** — Adaptive gain using PI controller with peak envelope tracking (WLED-style):

| Field | Default | Description |
|-------|---------|-------------|
| `preset` | `AGCPreset_Normal` | Behavior preset: Normal, Vivid, Lazy, or Custom |
| `minGain` | `1/64` | Minimum gain multiplier |
| `maxGain` | `32.0f` | Maximum gain multiplier |
| `targetRMSLevel` | `8000.0f` | Target RMS level after gain (0-32767) |
| `peakDecayTau` | `3.3f` | Peak envelope decay (seconds, Custom only) |
| `kp` | `0.6f` | PI proportional gain (Custom only) |
| `ki` | `1.7f` | PI integral gain (Custom only) |
| `gainFollowSlowTau` | `12.3f` | Slow gain-follow tau (seconds, Custom only) |
| `gainFollowFastTau` | `0.38f` | Fast gain-follow tau (seconds, Custom only) |

**AGC Presets:**

| Parameter | Normal | Vivid | Lazy |
|-----------|--------|-------|------|
| peakDecayTau | 3.3s | 1.3s | 6.7s |
| kp | 0.6 | 1.5 | 0.65 |
| ki | 1.7 | 1.85 | 1.2 |
| gainFollowSlowTau | 12.3s | 8.2s | 16.4s |
| gainFollowFastTau | 0.38s | 0.26s | 0.51s |

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

// Quiet venue: vivid preset for faster adaptation
AutoGainConfig agcConfig;
agcConfig.preset = AGCPreset_Vivid;
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
| **EqualizerDetector** | Yes | No | `onEqualizer(const Equalizer&)` | `getEqBass()`, `getEqMid()`, `getEqTreble()`, `getEqVolume()`, `getEqZcf()`, `getEqBin(int)` |
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
| **VibeDetector** | Yes | No | `onVibeLevels(const VibeLevels&)`, `onVibeBassSpike()`, `onVibeMidSpike()`, `onVibeTrebSpike()` | `getVibeBass()`, `getVibeMid()`, `getVibeTreb()`, `getVibeVol()`, `isVibeBassSpike()` |

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
├── input.h                      # AudioConfig, IAudioInput (hardware abstraction)
├── audio_context.h/.cpp.hpp     # Shared FFT cache (lazy evaluation)
├── audio_detector.h             # Base class for all detectors
├── audio_processor.h/.cpp.hpp   # High-level facade (callbacks + polling + auto-pump)
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
    ├── vibe.h/.cpp.hpp # MilkDrop-inspired self-normalizing audio analysis
    ├── transient.h    # Transient / attack detection
    ├── silence.h      # Silence detection
    ├── dynamics_analyzer.h # Crescendo / diminuendo
    ├── energy_analyzer.h   # RMS energy tracking
    ├── equalizer.h    # WLED-style 16-bin equalizer (0.0-1.0)
    └── frequency_bands.h   # Bass/mid/treble splitting
```
