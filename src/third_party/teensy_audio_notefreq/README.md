# Teensy Audio Note Frequency Detection Integration for FastLED

Pulled from: https://github.com/PaulStoffregen/Audio/tree/master (analyze_notefreq.cpp and analyze_notefreq.h)

## Current Status
- ⏸️ **Library Staged**: Awaiting third-party code integration
- ⏸️ **Namespace Wrapping**: To be wrapped in `fl::third_party`
- ⏸️ **C++ Conversion**: Already C++ compatible
- ⏸️ **FastLED Integration**: Bridge implementation pending
- ⏸️ **Audio System Integration**: Integration with FastLED audio analysis pending

## Overview

The Teensy Audio Library's `analyze_notefreq` component provides real-time fundamental frequency detection using the **Yin algorithm**, designed specifically for musical note detection and tuning applications. This component is MIT licensed and authored by Colin Duffy (2015).

### Key Features
- **Yin Algorithm Implementation**: High-quality fundamental frequency estimation
- **Musical Note Detection**: Optimized for guitar/bass tuning and note analysis
- **Real-time Processing**: Designed for embedded audio processing with low overhead
- **Configurable Accuracy**: Adjustable uncertainty threshold and buffer size
- **Low-frequency Capable**: Default configuration detects down to ~29.14 Hz

## Original API

### Class: `AudioAnalyzeNoteFrequency`

#### Public Methods

```cpp
// Initialize frequency detection with threshold and buffer configuration
bool begin(float threshold = 0.15);

// Set detection uncertainty threshold (0.0 to 1.0)
void threshold(float p);

// Check if a valid frequency has been detected
bool available(void);

// Get detected frequency in Hertz
float read(void);

// Get confidence of frequency detection (0.0 to 1.0)
float probability(void);
```

#### Configuration Parameters
- **Buffer Size**: Default 24 audio blocks
  - Each block is typically 128 samples
  - Total buffer: ~3072 samples
  - Minimum detectable frequency: ~(sample_rate / buffer_size)

- **Threshold**: Detection uncertainty parameter
  - Range: 0.0 (most sensitive) to 1.0 (least sensitive)
  - Default: 0.15 (good balance for most applications)
  - Lower values detect quieter/more ambiguous signals

### Algorithm Details

The Yin algorithm is a well-established pitch detection algorithm that:
1. Computes the autocorrelation function using cumulative mean normalized difference
2. Identifies the first local minimum below the threshold
3. Applies parabolic interpolation for sub-sample accuracy
4. Returns frequency estimate with confidence measure

**Advantages**:
- Resistant to octave errors (common in autocorrelation methods)
- Works well with harmonic musical signals
- Provides confidence metric for quality assessment
- Computationally efficient for embedded systems

## FastLED Third-Party Namespace Architecture

FastLED mandates that all third-party libraries be wrapped in the `fl::third_party` namespace to:

### 1. **Prevent Global Namespace Pollution**
```cpp
// Before: Global namespace collision risk
class AudioAnalyzeNoteFrequency { /* ... */ };

// After: Safely namespaced
namespace fl {
namespace third_party {
namespace teensy_audio {
    class AudioAnalyzeNoteFrequency { /* ... */ };
}
}
}
```

### 2. **Clear Ownership and Licensing Boundaries**
The `fl::third_party::teensy_audio` namespace makes it immediately clear:
- **What code is external**: Everything in `fl::third_party::teensy_audio` is from Teensy Audio Library
- **Licensing considerations**: MIT licensed third-party code
- **Maintenance responsibility**: Upstream maintained by PJRC (Paul Stoffregen)
- **API stability**: Third-party APIs may change independently of FastLED

### 3. **Controlled Integration Points**
```cpp
// Third-party code stays isolated
namespace fl::third_party::teensy_audio {
    class AudioAnalyzeNoteFrequency { /* Original Teensy Audio API */ };
}

// FastLED provides clean wrappers
namespace fl {
    class NoteFrequencyAnalyzer {  // Clean C++ API following FastLED conventions
        static float detectFrequency(const AnalyzerConfig& config,
                                     fl::span<const float> audio_samples,
                                     float* confidence = nullptr);
    private:
        fl::third_party::teensy_audio::AudioAnalyzeNoteFrequency analyzer_;
    };
}
```

## Proposed FastLED Integration

### Phase 1: Library Staging and Namespace Wrapping

#### 1.1 File Structure
```
src/third_party/teensy_audio_notefreq/
├── README.md                    (this file)
├── LICENSE.txt                  (MIT license from original)
├── analyze_notefreq.h           (wrapped in fl::third_party::teensy_audio)
├── analyze_notefreq.cpp         (wrapped in fl::third_party::teensy_audio)
└── AudioStream.h                (minimal stub for Teensy Audio dependency)
```

#### 1.2 Namespace Wrapping
```cpp
// analyze_notefreq.h
namespace fl {
namespace third_party {
namespace teensy_audio {
    class AudioAnalyzeNoteFrequency {
        // Original API preserved
    };
}
}
}
```

#### 1.3 Dependency Handling

The original code depends on Teensy Audio's `AudioStream` class for audio block management. Options:

**Option A: Minimal Stub** (Recommended)
- Create minimal `AudioStream.h` stub with required interfaces
- Replace Teensy-specific audio block management with FastLED equivalents
- Maintain same buffering semantics

**Option B: Direct Port**
- Extract and adapt only the core Yin algorithm implementation
- Remove `AudioStream` dependency entirely
- Implement buffer management directly in FastLED wrapper

### Phase 2: FastLED API Wrapper

#### 2.1 Configuration Structure
```cpp
namespace fl {

struct NoteFrequencyConfig {
    // Detection parameters
    float threshold = 0.15f;        // Uncertainty threshold (0.0 to 1.0)
    float minFrequency = 29.14f;    // Minimum detectable frequency (Hz)
    float maxFrequency = 5000.0f;   // Maximum detectable frequency (Hz)

    // Buffer configuration
    uint32_t sampleRate = 44100;    // Audio sample rate
    uint32_t bufferSize = 3072;     // Analysis buffer size (samples)

    // Output options
    bool requireHighConfidence = false;  // Only return high-confidence results
    float minConfidence = 0.5f;          // Minimum confidence threshold
};

}
```

#### 2.2 Clean FastLED API
```cpp
namespace fl {

class NoteFrequencyAnalyzer {
public:
    // Static detection method
    static bool detectFrequency(const NoteFrequencyConfig& config,
                               fl::span<const float> audio_samples,
                               float* frequency,
                               float* confidence = nullptr,
                               fl::string* error_message = nullptr);

    // Platform support detection
    static bool isSupported();

    // Stateful analyzer for streaming audio
    class StreamingAnalyzer {
    public:
        explicit StreamingAnalyzer(const NoteFrequencyConfig& config);

        // Process audio samples incrementally
        void processSamples(fl::span<const float> samples);

        // Check if frequency is available
        bool available() const;

        // Get detected frequency
        float frequency() const;

        // Get detection confidence
        float confidence() const;

    private:
        fl::third_party::teensy_audio::AudioAnalyzeNoteFrequency analyzer_;
        NoteFrequencyConfig config_;
    };
};

}
```

### Phase 3: Integration Implementation

#### 3.1 Bridge Implementation (`fl/audio/note_frequency.cpp`)
```cpp
namespace fl {

class NoteFrequencyAnalyzerImpl {
public:
    NoteFrequencyAnalyzerImpl(const NoteFrequencyConfig& config)
        : config_(config) {
        analyzer_.begin(config.threshold);
    }

    bool detect(fl::span<const float> samples, float* freq, float* conf) {
        // Feed samples to analyzer
        feedSamples(samples);

        // Check if frequency detected
        if (!analyzer_.available()) {
            return false;
        }

        // Get results
        float detected_freq = analyzer_.read();
        float detected_conf = analyzer_.probability();

        // Apply configuration filters
        if (detected_freq < config_.minFrequency ||
            detected_freq > config_.maxFrequency) {
            return false;
        }

        if (config_.requireHighConfidence &&
            detected_conf < config_.minConfidence) {
            return false;
        }

        // Return results
        if (freq) *freq = detected_freq;
        if (conf) *conf = detected_conf;

        return true;
    }

private:
    void feedSamples(fl::span<const float> samples);

    fl::third_party::teensy_audio::AudioAnalyzeNoteFrequency analyzer_;
    NoteFrequencyConfig config_;
};

}
```

#### 3.2 Memory Management
- Use `fl::scoped_array` for temporary buffers
- Replace any dynamic allocation with FastLED memory patterns
- Ensure proper cleanup in destructors

#### 3.3 Type Conversions
```cpp
// Teensy Audio uses float samples in range [-1.0, 1.0]
// FastLED may use int16_t or other formats

void convertSamples(fl::span<const int16_t> input,
                   fl::span<float> output) {
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] / 32768.0f;
    }
}
```

## Integration Challenges and Solutions

### Challenge 1: AudioStream Dependency
**Problem**: Original code inherits from `AudioStream` class for Teensy Audio framework integration.

**Solutions**:
1. **Minimal Stub Approach**: Create lightweight `AudioStream` base class with required virtual methods
2. **Direct Algorithm Port**: Extract Yin algorithm core, remove streaming framework dependency
3. **Adapter Pattern**: Wrap analyzer and translate between FastLED and Teensy Audio semantics

**Recommendation**: Use Minimal Stub for fastest integration, preserving original code quality.

### Challenge 2: Audio Block Management
**Problem**: Teensy Audio uses specific audio block structure (128 samples per block, reference counting).

**Solutions**:
1. **Emulate Blocks**: Create compatible block structure in stub
2. **Buffer Conversion**: Convert FastLED audio buffers to block format on-the-fly
3. **Algorithm Refactor**: Modify to accept arbitrary buffer sizes

**Recommendation**: Emulate blocks in stub for minimal code changes.

### Challenge 3: Platform Dependencies
**Problem**: Original code may have ARM-specific optimizations or Teensy hardware assumptions.

**Solutions**:
1. **Portable Fallbacks**: Ensure C++ implementation works on all platforms
2. **Feature Detection**: Use `#ifdef` guards for platform-specific optimizations
3. **FastLED Portability**: Leverage existing FastLED cross-platform patterns

**Recommendation**: Test on multiple platforms early, add portable fallbacks as needed.

### Challenge 4: Real-time Requirements
**Problem**: Musical note detection requires consistent low-latency processing.

**Solutions**:
1. **Performance Profiling**: Benchmark on target embedded platforms
2. **Buffer Size Tuning**: Allow configuration of latency vs. accuracy trade-off
3. **Optimization**: Consider fixed-point math for microcontrollers if needed

## Testing Strategy

### Unit Tests (`tests/audio_note_frequency.cpp`)

#### Phase 1: Basic Functionality
```cpp
TEST_CASE("NoteFrequency initialization") {
    fl::NoteFrequencyConfig config;
    fl::NoteFrequencyAnalyzer::StreamingAnalyzer analyzer(config);
    CHECK(analyzer.confidence() == 0.0f);
}

TEST_CASE("NoteFrequency known frequency detection") {
    // Generate pure 440Hz sine wave (A4)
    std::vector<float> samples = generateSineWave(440.0f, 1.0f, 44100, 4096);

    float freq, conf;
    bool detected = fl::NoteFrequencyAnalyzer::detectFrequency(
        fl::NoteFrequencyConfig{}, samples, &freq, &conf);

    CHECK(detected);
    CHECK(freq == Approx(440.0f).epsilon(0.01));  // Within 1%
    CHECK(conf > 0.8f);  // High confidence
}
```

#### Phase 2: Musical Note Range
```cpp
TEST_CASE("NoteFrequency musical note range") {
    // Test standard guitar tuning: E2, A2, D3, G3, B3, E4
    float notes[] = {82.41f, 110.0f, 146.83f, 196.0f, 246.94f, 329.63f};

    for (float expected : notes) {
        auto samples = generateSineWave(expected, 1.0f, 44100, 4096);
        float detected;

        bool success = fl::NoteFrequencyAnalyzer::detectFrequency(
            fl::NoteFrequencyConfig{}, samples, &detected);

        CHECK(success);
        CHECK(detected == Approx(expected).epsilon(0.02));
    }
}
```

#### Phase 3: Edge Cases
```cpp
TEST_CASE("NoteFrequency low SNR handling") {
    // Test with noisy signal
    auto signal = generateSineWave(440.0f, 0.5f, 44100, 4096);
    auto noise = generateWhiteNoise(0.3f, 4096);

    for (size_t i = 0; i < signal.size(); ++i) {
        signal[i] += noise[i];
    }

    float freq, conf;
    bool detected = fl::NoteFrequencyAnalyzer::detectFrequency(
        fl::NoteFrequencyConfig{}, signal, &freq, &conf);

    // Should still detect but with lower confidence
    if (detected) {
        CHECK(freq == Approx(440.0f).epsilon(0.05));
        CHECK(conf < 0.9f);  // Lower confidence expected
    }
}

TEST_CASE("NoteFrequency harmonic-rich signal") {
    // Test with square wave (odd harmonics)
    auto samples = generateSquareWave(440.0f, 1.0f, 44100, 4096);

    float freq;
    bool detected = fl::NoteFrequencyAnalyzer::detectFrequency(
        fl::NoteFrequencyConfig{}, samples, &freq);

    CHECK(detected);
    CHECK(freq == Approx(440.0f).epsilon(0.02));  // Should find fundamental
}
```

### Test Data Requirements
- **Pure Tones**: Sine waves at musical note frequencies
- **Harmonic Signals**: Square, sawtooth, triangle waves
- **Noisy Signals**: Signal + white/pink noise at various SNR levels
- **Edge Cases**: Sub-threshold signals, octave ambiguity tests

### Integration Tests
- **Streaming Analysis**: Feed continuous audio, verify state updates
- **Configuration Impact**: Test different threshold/buffer size settings
- **Performance**: Benchmark processing time on embedded targets

## Build System Integration

### CMake Configuration
```cmake
# src/third_party/teensy_audio_notefreq/CMakeLists.txt

add_library(teensy_audio_notefreq
    analyze_notefreq.cpp
    # AudioStream.cpp (if needed)
)

target_include_directories(teensy_audio_notefreq
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add to FastLED build
target_link_libraries(FastLED PRIVATE teensy_audio_notefreq)
```

### Platform Support
```cpp
// fl/audio/note_frequency.h
namespace fl {

inline bool NoteFrequencyAnalyzer::isSupported() {
    #if defined(ESP32) || defined(ARDUINO) || defined(__linux__) || defined(_WIN32)
        return true;  // Algorithm is portable
    #else
        return false;
    #endif
}

}
```

## Documentation Requirements

### API Documentation (`fl/audio/note_frequency.h`)
```cpp
/// @brief Real-time musical note frequency detection using the Yin algorithm
///
/// This analyzer detects the fundamental frequency of musical notes in audio
/// signals, optimized for guitar/bass tuning and pitch detection applications.
///
/// Based on the Teensy Audio Library's analyze_notefreq component by Colin Duffy.
/// Uses the Yin algorithm for high-quality pitch estimation resistant to octave errors.
///
/// @note Requires buffering audio samples for analysis. Minimum detectable frequency
///       is determined by buffer size: f_min ≈ sample_rate / buffer_size
///
/// Example usage:
/// @code
/// fl::NoteFrequencyConfig config;
/// config.threshold = 0.15f;
/// config.sampleRate = 44100;
///
/// float frequency, confidence;
/// if (fl::NoteFrequencyAnalyzer::detectFrequency(config, audio_samples,
///                                                 &frequency, &confidence)) {
///     printf("Detected %.2f Hz (%.0f%% confidence)\n", frequency, confidence * 100);
/// }
/// @endcode
class NoteFrequencyAnalyzer { /* ... */ };
```

### User Guide Section
Add to FastLED documentation:
- **Audio Analysis Overview**: Introduction to pitch detection
- **Configuration Guide**: Choosing threshold, buffer size for application
- **Performance Considerations**: Latency vs. accuracy trade-offs
- **Example Applications**: Tuner, pitch-to-MIDI, reactive lighting

## Timeline and Milestones

### Milestone 1: Library Integration (Est. 1-2 days)
- [ ] Copy source files to `src/third_party/teensy_audio_notefreq/`
- [ ] Add MIT license file
- [ ] Wrap in `fl::third_party::teensy_audio` namespace
- [ ] Create minimal `AudioStream` stub
- [ ] Verify compilation on multiple platforms

### Milestone 2: FastLED API Wrapper (Est. 2-3 days)
- [ ] Implement `NoteFrequencyConfig` structure
- [ ] Create `NoteFrequencyAnalyzer` static API
- [ ] Implement `StreamingAnalyzer` for stateful processing
- [ ] Add type conversion utilities (int16_t ↔ float)
- [ ] Implement error handling and validation

### Milestone 3: Testing and Validation (Est. 2-3 days)
- [ ] Write unit tests for basic functionality
- [ ] Create test signal generators (sine, square, noise)
- [ ] Add musical note range tests
- [ ] Implement edge case tests (noise, harmonics)
- [ ] Performance benchmarking on ESP32/Arduino

### Milestone 4: Documentation and Examples (Est. 1-2 days)
- [ ] API documentation in headers
- [ ] Add user guide section
- [ ] Create example sketches (tuner, reactive lighting)
- [ ] Update FastLED documentation index

## Success Criteria

### Functional Requirements
✅ Detects fundamental frequency of musical notes (E2-E6: 82Hz-1318Hz)
✅ Accuracy within 2% of true frequency for clean signals
✅ Processes audio in real-time on ESP32/Teensy platforms
✅ Provides confidence metric for detection quality
✅ Handles harmonic-rich signals (guitar, bass, voice)

### Code Quality
✅ All code in `fl::third_party::teensy_audio` namespace
✅ Clean FastLED API following project conventions
✅ Comprehensive error handling and validation
✅ Full test coverage with unit and integration tests
✅ Documented API with usage examples

### Performance
✅ Processing latency < 100ms for typical configurations
✅ Memory footprint < 20KB for analyzer instance
✅ No dynamic allocation in real-time processing path
✅ Consistent performance across FastLED-supported platforms

## License and Attribution

**Original Work**:
- **Library**: Teensy Audio Library
- **Component**: analyze_notefreq (Note Frequency Detection)
- **Author**: Colin Duffy
- **Copyright**: © 2015 Colin Duffy
- **License**: MIT License
- **Source**: https://github.com/PaulStoffregen/Audio

**MIT License Summary**:
Permission is hereby granted, free of charge, to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, subject to including the copyright notice and permission notice in all copies.

**Integration into FastLED**:
- Third-party code wrapped in `fl::third_party::teensy_audio` namespace
- Original license preserved in `LICENSE.txt`
- FastLED wrapper code follows FastLED license (MIT)
- Clear attribution maintained in documentation

## References

### Yin Algorithm
- **Paper**: "YIN, a fundamental frequency estimator for speech and music" by Alain de Cheveigné and Hideki Kawahara (2002)
- **Algorithm**: Autocorrelation-based pitch detection with cumulative mean normalized difference function
- **Advantages**: Resistant to octave errors, works well with harmonic signals

### Teensy Audio Library
- **Repository**: https://github.com/PaulStoffregen/Audio
- **Documentation**: https://www.pjrc.com/teensy/td_libs_Audio.html
- **Forum**: https://forum.pjrc.com/forums/4-Audio-Projects

### FastLED Integration Patterns
- **TJpg_Decoder Integration**: `src/third_party/TJpg_Decoder/README.md` (reference implementation)
- **Third-Party Guidelines**: `CLAUDE.md` (project-wide integration rules)
- **Namespace Conventions**: `fl::third_party::*` pattern for all external libraries

---

*This integration document serves as a blueprint for incorporating Teensy Audio's note frequency detection into FastLED, following established third-party integration patterns and maintaining code quality standards.*
