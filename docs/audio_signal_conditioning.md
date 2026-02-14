# FastLED Audio Signal Conditioning - Phase 1

## Overview

Phase 1 of the FastLED audio middleware adds professional-grade signal conditioning to the audio processing pipeline. This addresses critical gaps identified in the core FastLED audio library by providing:

1. **SignalConditioner** - DC offset removal, spike filtering, and noise gating
2. **AutoGain** - Adaptive gain control using Robbins-Monro percentile estimation
3. **NoiseFloorTracker** - Adaptive noise floor tracking with hysteresis

These components work together to clean and normalize raw audio from I2S microphones before FFT analysis and beat detection.

## Quick Start

### Enable in AudioReactive

```cpp
#include <FastLED.h>

AudioReactive audio;
AudioReactiveConfig config;

// Enable signal conditioning (DC removal, spike filter, noise gate)
config.enableSignalConditioning = true;

// Enable adaptive gain control
config.enableAutoGain = true;

// Enable noise floor tracking
config.enableNoiseFloorTracking = true;

audio.begin(config);

// Process samples - signal conditioning applied automatically
AudioSample sample = ...;  // From I2S microphone
audio.processSample(sample);
```

### Standalone Usage

Each component can also be used independently:

```cpp
#include "fl/audio/signal_conditioner.h"
#include "fl/audio/auto_gain.h"
#include "fl/audio/noise_floor_tracker.h"

// Signal conditioning
SignalConditioner conditioner;
AudioSample cleanSample = conditioner.processSample(rawSample);

// Auto gain
AutoGain agc;
AudioSample amplifiedSample = agc.process(cleanSample);

// Noise floor tracking
NoiseFloorTracker tracker;
tracker.update(sample.rms());
float normalized = tracker.normalize(sample.rms());
```

## Components

### 1. SignalConditioner

Cleans raw PCM audio by removing common artifacts from I2S microphones.

**Features:**
- **DC offset removal**: Subtracts running average to center signal at zero
- **Spike filtering**: Rejects I2S glitches (samples beyond ±threshold)
- **Noise gate**: Applies hysteresis gating to suppress background noise

**Configuration:**
```cpp
SignalConditionerConfig config;
config.enableDCRemoval = true;
config.enableSpikeFilter = true;
config.enableNoiseGate = true;
config.spikeThreshold = 10000;           // Reject samples > ±10000
config.noiseGateOpenThreshold = 500;     // Gate opens when signal > 500
config.noiseGateCloseThreshold = 300;    // Gate closes when signal < 300
config.dcRemovalAlpha = 0.99f;           // DC tracking smoothness
```

**Example:**
```cpp
SignalConditioner conditioner;
conditioner.configure(config);

AudioSample rawSample = ...; // From I2S
AudioSample cleanSample = conditioner.processSample(rawSample);

// Check statistics
const auto& stats = conditioner.getStats();
Serial.printf("DC offset: %d, Spikes rejected: %u\n",
              stats.dcOffset, stats.spikesRejected);
```

### 2. AutoGain

Adaptive gain control using Robbins-Monro percentile estimation for memory-efficient streaming.

**Features:**
- **Percentile tracking**: Estimates P90 (or other percentile) without storing history
- **Adaptive gain**: Adjusts gain to maintain target RMS level
- **Configurable limits**: Min/max gain clamping prevents over-amplification
- **Smooth transitions**: Exponential smoothing prevents abrupt gain changes

**Configuration:**
```cpp
AutoGainConfig config;
config.enabled = true;
config.targetPercentile = 0.9f;      // Track P90 (90th percentile)
config.learningRate = 0.05f;         // Adaptation speed (0.01-0.1)
config.targetRMSLevel = 8000.0f;     // Target RMS after gain
config.minGain = 0.1f;               // Minimum gain multiplier
config.maxGain = 10.0f;              // Maximum gain multiplier
config.gainSmoothing = 0.95f;        // Smoothing factor (0-1)
```

**Example:**
```cpp
AutoGain agc;
agc.configure(config);

// Process samples - AGC adapts over time
for (int i = 0; i < 100; ++i) {
    AudioSample sample = ...;
    AudioSample amplified = agc.process(sample);

    // Monitor gain
    if (i % 10 == 0) {
        const auto& stats = agc.getStats();
        Serial.printf("Gain: %.2f, Input RMS: %.1f, Output RMS: %.1f\n",
                      stats.currentGain, stats.inputRMS, stats.outputRMS);
    }
}
```

### 3. NoiseFloorTracker

Tracks adaptive noise floor with hysteresis to prevent "noise chasing."

**Features:**
- **Exponential decay**: Floor gradually decreases toward minimum
- **Slow attack**: Floor increases slowly when signal is consistently low
- **Hysteresis**: Prevents rapid floor oscillations
- **Cross-domain calibration**: Combines time-domain (RMS) and frequency-domain metrics

**Configuration:**
```cpp
NoiseFloorTrackerConfig config;
config.enabled = true;
config.decayRate = 0.99f;            // How fast floor decreases (0-1)
config.attackRate = 0.001f;          // How fast floor increases (0-1)
config.hysteresisMargin = 100.0f;    // Prevents noise chasing
config.minFloor = 10.0f;             // Minimum floor value
config.maxFloor = 5000.0f;           // Maximum floor value
config.crossDomainWeight = 0.3f;     // Blend time/freq domains (0-1)
```

**Example:**
```cpp
NoiseFloorTracker tracker;
tracker.configure(config);

// Update with each sample
AudioSample sample = ...;
float rms = sample.rms();
tracker.update(rms);

// Use normalized values
float normalized = tracker.normalize(rms);
bool isSignal = tracker.isAboveFloor(rms);

// Check statistics
const auto& stats = tracker.getStats();
Serial.printf("Floor: %.1f, Min: %.1f, Max: %.1f\n",
              stats.currentFloor, stats.minObserved, stats.maxObserved);
```

## Integration with AudioReactive

The signal conditioning pipeline is integrated into AudioReactive's `processSample()` method:

```
Raw AudioSample (from I2S)
    ↓
[SignalConditioner] → DC removal, spike filter, noise gate
    ↓
[AutoGain] → Adaptive gain normalization
    ↓
[NoiseFloorTracker] → Update noise floor estimate (passive)
    ↓
FFT Analysis → Beat Detection → Output
```

Access statistics from AudioReactive:

```cpp
AudioReactive audio;
// ... configure and process samples ...

// Get signal conditioning stats
const auto* scStats = audio.getSignalConditionerStats();
if (scStats) {
    Serial.printf("DC offset: %d\n", scStats->dcOffset);
}

const auto* agStats = audio.getAutoGainStats();
if (agStats) {
    Serial.printf("Current gain: %.2f\n", agStats->currentGain);
}

const auto* nfStats = audio.getNoiseFloorStats();
if (nfStats) {
    Serial.printf("Noise floor: %.1f\n", nfStats->currentFloor);
}
```

## Performance Characteristics

### SignalConditioner
- **Memory**: ~2KB per instance (working buffers)
- **CPU**: ~50-100 µs per 1000-sample buffer (ESP32-S3 @ 240MHz)
- **Latency**: Zero-latency (per-buffer processing)

### AutoGain
- **Memory**: ~1KB per instance
- **CPU**: ~30-60 µs per 1000-sample buffer
- **Convergence**: 20-50 samples (depending on learning rate)

### NoiseFloorTracker
- **Memory**: <1KB per instance
- **CPU**: <10 µs per update
- **Convergence**: 50-100 samples for stable floor estimate

## Testing

Comprehensive unit tests are provided:

```bash
# Test individual components
bash test signal_conditioner --cpp
bash test auto_gain --cpp
bash test noise_floor_tracker --cpp

# Test integration with AudioReactive
bash test audio_signal_conditioning_integration --cpp

# Run all audio tests
bash test audio --cpp
```

## Troubleshooting

### Signal is over-amplified
- Reduce `AutoGainConfig::maxGain`
- Increase `AutoGainConfig::targetRMSLevel`
- Check if spikes are being filtered correctly

### Floor tracking is unstable
- Increase `NoiseFloorTrackerConfig::hysteresisMargin`
- Increase `NoiseFloorTrackerConfig::decayRate` (slower decay)
- Decrease `NoiseFloorTrackerConfig::attackRate` (slower rise)

### Noise gate cutting off signal
- Lower `SignalConditionerConfig::noiseGateCloseThreshold`
- Increase hysteresis gap (open - close threshold)

### DC offset not removed
- Increase `SignalConditionerConfig::dcRemovalAlpha` for slower tracking
- Decrease for faster adaptation to DC changes

## Coming in Phase 2: Enhanced FFT Analysis

- **FrequencyBinMapper**: Runtime-switchable bin configurations (16/32 bands)
- **SpectralEqualizer**: Frequency-dependent gain correction
- **Logarithmic spacing**: Better bass/mid/treble separation (20-16000 Hz)

## Coming in Phase 3: Musical Beat Detection

- **MusicalBeatDetector**: True beat tracking with BPM estimation
- **Temporal pattern recognition**: Distinguish beats from random onsets
- **Multi-band beat validation**: Bass/mid/treble beat separation

## References

- [Robbins-Monro Algorithm](https://en.wikipedia.org/wiki/Stochastic_approximation)
- [Noise Gate Design](https://en.wikipedia.org/wiki/Noise_gate)
- [DC Offset Removal](https://en.wikipedia.org/wiki/DC_bias)

## Contributing

Signal conditioning middleware is contributed to FastLED core. For issues or improvements, please open a GitHub issue.
