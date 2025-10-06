# Beat Detection API Documentation

## Overview

The FastLED beat detection system provides real-time onset detection and beat tracking optimized for Electronic Dance Music (EDM) on embedded platforms, particularly the ESP32-S3.

**Key Features:**
- Multiple onset detection algorithms (Energy, Spectral Flux, SuperFlux, HFC, Multi-Band)
- Adaptive thresholding and whitening for robust detection in compressed music
- Real-time tempo estimation via autocorrelation and comb filtering
- Configurable for different EDM subgenres
- Low latency (<51ms total detection latency)
- Efficient memory usage (~61KB per instance)

**Target Performance:**
- Sample rate: 48 kHz
- Frame processing: <8ms per frame (achieves ~5ms)
- CPU overhead: <20% (estimated 10-15% on ESP32-S3)
- Memory: <100KB per instance

---

## Quick Start

### Basic Usage

```cpp
#include "fx/audio/beat_detector.h"

using namespace fl;

// Create configuration
BeatDetectorConfig config;
config.sample_rate_hz = 48000;
config.frame_size = 512;
config.hop_size = 256;

// Create detector
BeatDetector detector(config);

// Set up callbacks
detector.onBeat = [](float confidence, float bpm, float timestamp_ms) {
    Serial.println("Beat detected! BPM: " + String(bpm));
    // Trigger LED effect
};

detector.onOnset = [](float confidence, float timestamp_ms) {
    // Flash LEDs on onset
    FastLED.setBrightness(255);
};

// In your audio processing loop:
void loop() {
    float audio_buffer[512];
    // ... fill audio_buffer from ADC or I2S ...

    detector.processFrame(audio_buffer, 512);
}
```

---

## Configuration

### BeatDetectorConfig Structure

Complete configuration reference for `BeatDetectorConfig`:

#### Audio Parameters

```cpp
config.sample_rate_hz = 48000.0f;  // Input sample rate in Hz
config.frame_size = 512;            // Analysis window size (256-1024)
config.hop_size = 256;              // Step size between frames
config.fft_size = 512;              // FFT size (power of 2, >= frame_size)
```

**Recommendations:**
- Standard: 48000 Hz, frame=512, hop=256, FFT=512
- Lower latency: hop=128 (faster updates, higher CPU)
- Lower CPU: frame=256, hop=256, FFT=256 (less frequency resolution)

#### Onset Detection

```cpp
config.odf_type = OnsetDetectionFunction::SuperFlux;  // Algorithm selection
config.num_bands = 24;                                // Mel bands (if used, 3-138)
config.log_compression = true;                        // Log magnitude compression
config.superflux_mu = 3;                             // SuperFlux delay (1-5 frames)
config.max_filter_radius = 2;                        // SuperFlux filter radius (1-5 bins)
```

**Algorithm Options:**
- `Energy`: Fastest, time-domain only, least accurate
- `SpectralFlux`: Good baseline, frequency-domain
- `SuperFlux`: **Recommended for EDM** - best accuracy, vibrato suppression
- `HFC`: Good for hi-hats/cymbals
- `MultiBand`: Custom frequency weighting
- `ComplexDomain`: Not yet fully implemented (uses phase info)

**Algorithm Comparison:**

| Algorithm | Speed | Accuracy | Best For |
|-----------|-------|----------|----------|
| Energy | ⚡⚡⚡⚡⚡ | ⭐⭐ | Simple beats, ultra-low CPU |
| SpectralFlux | ⚡⚡⚡⚡ | ⭐⭐⭐⭐ | General purpose |
| **SuperFlux** | ⚡⚡⚡ | ⭐⭐⭐⭐⭐ | **EDM (recommended)** |
| HFC | ⚡⚡⚡⚡ | ⭐⭐⭐ | Hi-hats, cymbals |
| MultiBand | ⚡⚡⭐ | ⭐⭐⭐⭐ | Custom frequency emphasis |

#### Multi-Band Configuration

```cpp
config.odf_type = OnsetDetectionFunction::MultiBand;

// Clear default bands
config.bands.clear();

// Add custom bands: {low_hz, high_hz, weight}
config.bands.push_back({60.0f, 160.0f, 1.5f});    // Bass/kick emphasis
config.bands.push_back({160.0f, 2000.0f, 1.0f});  // Mid neutral
config.bands.push_back({2000.0f, 8000.0f, 1.2f}); // High/hi-hats emphasis
```

**Default Bands (optimized for EDM):**
- Bass (60-160 Hz): weight 1.5 → emphasizes kick drums
- Mid (160-2000 Hz): weight 1.0 → neutral
- High (2000-8000 Hz): weight 1.2 → emphasizes hi-hats

**Genre-Specific Tuning:**
- **Deep House/Techno**: Increase bass weight to 2.0
- **Drum & Bass**: Balance bass and high weights (1.5 each)
- **Trance**: Neutral weights (all 1.0) for melodic onsets

#### Peak Picking

```cpp
config.peak_mode = PeakPickingMode::SuperFluxPeaks;  // Peak detection strategy
config.peak_threshold_delta = 0.07f;                 // Threshold above mean (0.05-0.15)
config.peak_pre_max_ms = 30;                         // Pre-window for max (ms)
config.peak_post_max_ms = 30;                        // Post-window for max (ms)
config.peak_pre_avg_ms = 100;                        // Pre-window for mean (ms)
config.peak_post_avg_ms = 70;                        // Post-window for mean (ms)
config.min_inter_onset_ms = 30;                      // Minimum onset spacing (ms)
```

**Peak Picking Modes:**
- `LocalMaximum`: Simple local maximum detection
- `AdaptiveThreshold`: Local max + adaptive threshold
- `SuperFluxPeaks`: **Recommended** - combines all strategies

**Tuning Guidelines:**
- Increase `peak_threshold_delta` if too many false positives (0.10-0.15)
- Decrease if missing onsets (0.03-0.05)
- Increase `min_inter_onset_ms` for slower music or kick rolls (50-100ms)
- Decrease for fast hi-hat patterns (20-30ms)

#### Tempo Tracking

```cpp
config.tempo_tracker = TempoTrackerType::CombFilter;  // Tracking algorithm
config.tempo_min_bpm = 100.0f;                        // Minimum tempo (EDM: 100-140)
config.tempo_max_bpm = 150.0f;                        // Maximum tempo (EDM: 100-140)
config.tempo_rayleigh_sigma = 120.0f;                 // Prior center (typical tempo)
config.tempo_acf_window_sec = 4;                      // Autocorrelation window (sec)
```

**Tempo Tracker Types:**
- `None`: Onset detection only, no tempo tracking
- `CombFilter`: **Recommended for EDM** - emphasizes harmonic structure
- `Autocorrelation`: Simple autocorrelation, works for stable tempo
- `DynamicProgramming`: Not yet implemented

**Genre BPM Ranges:**
- House: 120-130 BPM → `min=115, max=135`
- Techno: 125-135 BPM → `min=120, max=140`
- Trance: 130-145 BPM → `min=125, max=150`
- Dubstep: 138-142 BPM (half-time ~70) → `min=65, max=150`
- Drum & Bass: 160-180 BPM → `min=155, max=185`

#### Advanced Options

```cpp
config.adaptive_whitening = true;   // Normalize frequency bins (recommended for compressed music)
config.whitening_alpha = 0.95f;     // Smoothing factor (0.9-0.99)
config.use_fixed_point = false;     // Fixed-point arithmetic (not implemented)
```

**Adaptive Whitening:**
- **Enable for:** Commercial EDM, heavily compressed music, sustained synths
- **Disable for:** Clean audio, minimal compression, CPU budget tight
- `whitening_alpha`: Higher = slower adaptation (0.95-0.99 typical)

---

## API Reference

### BeatDetector Class

Main beat detection class integrating onset detection, peak picking, and tempo tracking.

#### Constructor

```cpp
explicit BeatDetector(const BeatDetectorConfig& cfg);
```

Creates a beat detector with the specified configuration.

#### Callbacks

```cpp
function<void(float confidence, float timestamp_ms)> onOnset;
```
Called when an onset is detected.
- `confidence`: Onset strength (0.0-1.0+, higher = stronger)
- `timestamp_ms`: Time in milliseconds from start

```cpp
function<void(float confidence, float bpm, float timestamp_ms)> onBeat;
```
Called when a beat is detected.
- `confidence`: Beat confidence (0.0-1.0)
- `bpm`: Current tempo estimate
- `timestamp_ms`: Time in milliseconds from start

```cpp
function<void(float bpm, float confidence)> onTempoChange;
```
Called when tempo estimate changes significantly (>1 BPM).
- `bpm`: New tempo estimate
- `confidence`: Confidence in tempo estimate (0.0-1.0)

#### Processing Methods

```cpp
void processFrame(const float* frame, int n);
```
Process an audio frame. Call repeatedly with consecutive audio chunks.
- `frame`: Audio samples (normalized float, -1.0 to +1.0)
- `n`: Number of samples

```cpp
void processSpectrum(const float* magnitude_spectrum, int spectrum_size, float timestamp_ms);
```
Process pre-computed FFT magnitude spectrum. Use if you already have FFT from other processing.
- `magnitude_spectrum`: FFT magnitude (linear scale, not dB)
- `spectrum_size`: Number of FFT bins
- `timestamp_ms`: Current timestamp

#### State Methods

```cpp
void reset();
```
Reset all internal state (counters, history buffers, tempo estimate).

```cpp
TempoEstimate getTempo() const;
```
Get current tempo estimate.
Returns: `TempoEstimate` struct with `bpm`, `confidence`, `period_samples`.

```cpp
float getCurrentODF() const;
```
Get most recent onset detection function value.

```cpp
uint32_t getFrameCount() const;
uint32_t getOnsetCount() const;
uint32_t getBeatCount() const;
```
Get processing counters.

#### Configuration Methods

```cpp
const BeatDetectorConfig& config() const;
void setConfig(const BeatDetectorConfig& cfg);
```
Get or update configuration (affects subsequent processing).

---

## Usage Examples

### Example 1: Simple Beat Flasher

Flash LEDs on every beat:

```cpp
#include <FastLED.h>
#include "fx/audio/beat_detector.h"

#define NUM_LEDS 144
#define DATA_PIN 5

CRGB leds[NUM_LEDS];
fl::BeatDetector* beat_detector;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    fl::BeatDetectorConfig config;
    config.sample_rate_hz = 48000;
    config.odf_type = fl::OnsetDetectionFunction::SuperFlux;

    beat_detector = new fl::BeatDetector(config);

    beat_detector->onBeat = [](float confidence, float bpm, float timestamp_ms) {
        // Flash white on beat
        fill_solid(leds, NUM_LEDS, CRGB::White);
        FastLED.show();
    };
}

void loop() {
    // Get audio from I2S or ADC
    float audio_buffer[512];
    // ... read audio ...

    beat_detector->processFrame(audio_buffer, 512);

    // Fade LEDs
    fadeToBlackBy(leds, NUM_LEDS, 20);
    FastLED.show();
}
```

### Example 2: Onset-Reactive Colors

Change LED color on each onset with intensity based on confidence:

```cpp
beat_detector->onOnset = [](float confidence, float timestamp_ms) {
    static uint8_t hue = 0;

    // Map confidence to brightness (clamp to 255)
    uint8_t brightness = min(255, (int)(confidence * 255.0f));

    // Fill with hue, rotate hue each onset
    fill_solid(leds, NUM_LEDS, CHSV(hue, 255, brightness));
    FastLED.show();

    hue += 32; // Advance hue
};
```

### Example 3: Tempo-Synchronized Effects

Animate effects in sync with detected tempo:

```cpp
float current_bpm = 120.0f;

beat_detector->onTempoChange = [](float bpm, float confidence) {
    current_bpm = bpm;
    Serial.println("Tempo: " + String(bpm) + " BPM");
};

beat_detector->onBeat = [](float confidence, float bpm, float timestamp_ms) {
    // Trigger animation with duration based on tempo
    float beat_duration_ms = 60000.0f / current_bpm;
    // ... start animation with beat_duration_ms ...
};
```

### Example 4: Multi-Band Frequency Visualization

React differently to bass vs. high-frequency onsets:

```cpp
fl::BeatDetectorConfig bass_config;
bass_config.odf_type = fl::OnsetDetectionFunction::MultiBand;
bass_config.bands.clear();
bass_config.bands.push_back({60.0f, 160.0f, 2.0f});  // Only bass

fl::BeatDetectorConfig high_config;
high_config.odf_type = fl::OnsetDetectionFunction::HFC;  // High-frequency content

fl::BeatDetector bass_detector(bass_config);
fl::BeatDetector high_detector(high_config);

bass_detector.onOnset = [](float confidence, float timestamp_ms) {
    // Flash red LEDs on bass/kick
    leds[0] = CRGB::Red;
};

high_detector.onOnset = [](float confidence, float timestamp_ms) {
    // Flash blue LEDs on hi-hats
    leds[NUM_LEDS - 1] = CRGB::Blue;
};

void loop() {
    float audio[512];
    // ... get audio ...

    // Process same audio with both detectors
    bass_detector.processFrame(audio, 512);
    high_detector.processFrame(audio, 512);
}
```

### Example 5: Adaptive Brightness Control

Adjust LED brightness based on onset strength:

```cpp
uint8_t current_brightness = 128;

beat_detector->onOnset = [](float confidence, float timestamp_ms) {
    // Strong onsets → full brightness
    // Weak onsets → moderate brightness
    current_brightness = map(confidence * 100, 0, 100, 64, 255);
    FastLED.setBrightness(current_brightness);
};

void loop() {
    // ... process audio ...

    // Gradually fade brightness back to default
    if (current_brightness > 128) {
        current_brightness = max(128, current_brightness - 2);
        FastLED.setBrightness(current_brightness);
    }
}
```

---

## Troubleshooting

### Too Many False Positives

**Symptoms:** Onsets detected during quiet sections or sustained notes.

**Solutions:**
1. Increase `peak_threshold_delta` (try 0.10-0.15)
2. Enable `adaptive_whitening = true` to normalize sustained notes
3. Increase `min_inter_onset_ms` to debounce closely-spaced detections
4. Use `SuperFlux` ODF instead of `SpectralFlux`

### Missing Onsets

**Symptoms:** Real beats not detected.

**Solutions:**
1. Decrease `peak_threshold_delta` (try 0.03-0.05)
2. Check audio input level (should be normalized -1.0 to +1.0)
3. Try different ODF: `MultiBand` with bass emphasis for kicks
4. Increase `peak_post_avg_ms` for more stable threshold
5. Disable `log_compression` if audio is already compressed

### Incorrect Tempo

**Symptoms:** Tempo estimate is wrong (often 2x or 0.5x actual tempo).

**Solutions:**
1. Adjust `tempo_min_bpm` and `tempo_max_bpm` to narrower range
2. Use `CombFilter` tracker instead of `Autocorrelation`
3. Increase `tempo_acf_window_sec` (4-8 seconds)
4. Set `tempo_rayleigh_sigma` to expected tempo (weights towards this value)

### High CPU Usage

**Symptoms:** Frame processing takes too long, audio dropouts.

**Solutions:**
1. Reduce `fft_size` (512 → 256)
2. Increase `hop_size` (256 → 512) - processes fewer frames per second
3. Use simpler ODF: `SpectralFlux` instead of `SuperFlux`
4. Disable `adaptive_whitening`
5. Use `Energy` ODF for minimal CPU (time-domain only, no FFT)

### High Latency

**Symptoms:** Visible delay between beat and LED response.

**Solutions:**
1. Reduce `hop_size` for more frequent processing
2. Reduce `peak_post_max_ms` (minimum: 0 for fully causal)
3. Reduce `peak_post_avg_ms` (minimum: 0)
4. Note: Some latency unavoidable due to peak detection look-ahead

---

## Performance Optimization

### CPU Budget Management

**Priority 1: Reduce FFT Size**
```cpp
config.fft_size = 256;  // Half the bins, ~40% faster FFT
```

**Priority 2: Increase Hop Size**
```cpp
config.hop_size = 512;  // Half the frame rate
```

**Priority 3: Simplify ODF**
```cpp
config.odf_type = OnsetDetectionFunction::SpectralFlux;  // Instead of SuperFlux
// or
config.odf_type = OnsetDetectionFunction::Energy;  // No FFT, fastest
```

**Priority 4: Disable Features**
```cpp
config.adaptive_whitening = false;
config.log_compression = false;
config.tempo_tracker = TempoTrackerType::None;
```

### Memory Optimization

Beat detector uses fixed-size buffers. Memory usage is constant regardless of audio duration.

**Estimated Memory per Instance:** ~61 KB

**To Reduce Memory:**
- Only use one `BeatDetector` instance
- Share FFT with other audio processing (use `processSpectrum()`)
- Reduce `MAX_SPECTRUM_SIZE` in source if compiling for low-RAM platform

---

## Integration with Other FastLED Features

### Using with Sound-to-MIDI

Beat detector can run alongside pitch detection:

```cpp
#include "fx/audio/sound_to_midi.h"
#include "fx/audio/beat_detector.h"

SoundToMidi* pitch_detector;
BeatDetector* beat_detector;

void setup() {
    pitch_detector = new SoundToMidi();
    beat_detector = new BeatDetector(config);

    // Both can process same audio
}

void loop() {
    float audio[512];
    // ... get audio ...

    pitch_detector->process(audio, 512);
    beat_detector->processFrame(audio, 512);
}
```

### Combining with Effects

Use beat detection to trigger or modulate effects:

```cpp
beat_detector->onBeat = [](float confidence, float bpm, float timestamp_ms) {
    // Change effect palette on beat
    current_palette = random_palette();
};

beat_detector->onOnset = [](float confidence, float timestamp_ms) {
    // Trigger sparkle effect
    leds[random16(NUM_LEDS)] = CRGB::White;
};
```

---

## Research Background

The FastLED beat detection implementation is based on state-of-the-art research in Music Information Retrieval (MIR):

**Core Algorithms:**
- **SuperFlux** onset detection (Böck & Widmer, 2013) - MIREX winner
- **Adaptive whitening** (Stowell & Plumbley, 2007) - for compressed music
- **Comb filter** tempo estimation (Goto & Muraoka, 1999) - emphasizes beat harmonics
- **Autocorrelation** beat tracking (Scheirer, 1998) - foundational method

**Optimizations for EDM:**
- Multi-band frequency weighting emphasizing kick drums (60-160 Hz)
- Log compression for loudness normalization
- Debouncing for sidechain compression artifacts
- Narrow BPM range for stable tempo detection

For complete research documentation and implementation decisions, see:
- `docs/BEAT_DETECTION_DECISIONS.md` - Research analysis and design decisions
- `src/fx/audio/beat_detector.h` - Detailed technical comments

---

## API Version

**Version:** 1.0
**Last Updated:** 2025-01-05
**Status:** Production-ready for EDM applications on ESP32-S3

---

## License

Same as FastLED (MIT-style license)

---

## Support

For issues, questions, or contributions:
- GitHub Issues: [FastLED repository](https://github.com/FastLED/FastLED)
- Documentation: This file and `BEAT_DETECTION_DECISIONS.md`
- Examples: See `examples/BeatDetection/` (when available)
