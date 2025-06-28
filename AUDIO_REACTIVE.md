# FastLED Audio Reactive System

## Overview

This document outlines the design and implementation of an audio reactive system for FastLED, based on analysis of the WLED audio reactive usermod. The goal is to provide real-time audio analysis capabilities that can drive LED effects while maintaining FastLED's core principles of simplicity, performance, and cross-platform compatibility.

## Original WLED Analysis

### Architecture

The WLED audio reactive system is a sophisticated audio processing pipeline with several key components:

1. **Audio Input Layer**: Multiple source types (I2S, ADC, PDM, specific audio chips)
2. **FFT Processing**: 512-point FFT with frequency mapping to 16 GEQ channels  
3. **Audio Processing**: AGC (Automatic Gain Control), peak detection, filtering, noise gating
4. **UDP Sync**: Multi-device synchronization for networked installations
5. **Dynamic Processing**: Attack/decay limiting with multiple scaling modes

### Key Features

- **16 Frequency Bands**: Optimized frequency mapping from 43Hz to 9.2kHz
- **AGC System**: Sophisticated PI controller with 3 presets (normal, vivid, lazy)
- **Peak Detection**: Beat detection using FFT bin analysis
- **Noise Gating**: Automatic silence detection and handling
- **UDP Synchronization**: Multi-device audio sync over WiFi
- **Multiple Scaling Modes**: Linear, logarithmic, and square-root scaling options

### Frequency Band Mapping (Original WLED)

The original system maps 512 FFT bins to 16 frequency channels:

| Band | Frequency Range | Description |
|------|----------------|-------------|
| 0 | 43 - 86 Hz | Sub-bass |
| 1 | 86 - 129 Hz | Bass |
| 2 | 129 - 216 Hz | Bass |
| 3 | 216 - 301 Hz | Bass + Midrange |
| 4 | 301 - 430 Hz | Midrange |
| 5 | 430 - 560 Hz | Midrange |
| 6 | 560 - 818 Hz | Midrange |
| 7 | 818 - 1120 Hz | Midrange (1kHz center) |
| 8 | 1120 - 1421 Hz | Midrange |
| 9 | 1421 - 1895 Hz | Midrange |
| 10 | 1895 - 2412 Hz | Midrange + High Mid |
| 11 | 2412 - 3015 Hz | High Mid |
| 12 | 3015 - 3704 Hz | High Mid |
| 13 | 3704 - 4479 Hz | High Mid |
| 14 | 4479 - 7106 Hz | High Mid + High |
| 15 | 7106 - 9259 Hz | High |

## Migration Challenges

### 1. Blocking FFT Task Architecture

**Problem**: WLED runs FFT in a separate FreeRTOS task with blocking operations:
```cpp
void FFTcode(void * parameter) {
    for(;;) {
        delay(1);  // Keep watchdog happy
        if (audioSource) audioSource->getSamples(vReal, samplesFFT);  // BLOCKING
        // ... FFT processing (3-5ms on ESP32)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);  // BLOCKING
    }
}
```

**Solution**: Non-blocking incremental processing that fits FastLED's loop-based architecture.

### 2. Platform-Specific Audio Input

**Problem**: Heavy ESP32 I2S dependencies and hardware-specific initialization code.

**Solution**: Abstract audio interface that accepts standardized signed PCM data externally.

### 3. Complex AGC System

**Problem**: Sophisticated PI controller AGC with 100+ lines of complex floating point math.

**Solution**: Simplified adaptive gain control suitable for embedded systems.

### 4. Large Memory Footprint

**Problem**: Large FFT buffers (4KB+ for vReal/vImag arrays) and complex state.

**Solution**: Use FastLED's existing `fl/fft.h` with configurable FFT sizes.

### 5. UDP Networking Dependencies

**Problem**: WiFi/UDP sync features not available on all platforms.

**Solution**: Optional networking layer with platform-specific implementations.

## FastLED Audio Reactive API Design

### Core Data Structures

```cpp
namespace fl {

// Audio data structure - matches original WLED output
struct AudioData {
    float volume;                    // Overall volume level (0-255)
    float volumeRaw;                 // Raw volume without smoothing
    float peak;                      // Peak level (0-255) 
    bool beatDetected;               // Beat detection flag
    float frequencyBins[16];         // 16 frequency bins (matches WLED NUM_GEQ_CHANNELS)
    float dominantFrequency;         // Major peak frequency (Hz)
    float magnitude;                 // FFT magnitude of dominant frequency
    uint32_t timestamp;              // millis() when data was captured
};

struct AudioConfig {
    uint8_t gain = 128;              // Input gain (0-255)
    uint8_t sensitivity = 128;       // AGC sensitivity
    bool agcEnabled = true;          // Auto gain control
    bool noiseGate = true;           // Noise gate
    uint8_t attack = 50;             // Attack time (ms)
    uint8_t decay = 200;             // Decay time (ms)
    uint16_t sampleRate = 22050;     // Sample rate (Hz)
    uint8_t scalingMode = 3;         // 0=none, 1=log, 2=linear, 3=sqrt
};

class AudioReactive {
public:
    // Setup
    void begin(const AudioConfig& config = AudioConfig{});
    void setConfig(const AudioConfig& config);
    
    // External audio input interface
    void addSample(int16_t sample);
    void addSamples(const int16_t* samples, size_t count);
    
    // Non-blocking update - call from loop()
    void update();
    
    // Data access
    const AudioData& getData() const;
    const AudioData& getSmoothedData() const;
    
    // Convenience accessors
    float getVolume() const;
    float getBass() const;    // Average of bins 0-1
    float getMid() const;     // Average of bins 6-7 
    float getTreble() const;  // Average of bins 14-15
    bool isBeat() const;
    
    // Effect helpers
    uint8_t volumeToScale255() const;
    CRGB volumeToColor(const CRGBPalette16& palette) const;
    uint8_t frequencyToScale255(uint8_t binIndex) const;
};

// Global instance
extern AudioReactive Audio;

} // namespace fl
```

### Usage Examples

```cpp
// Example 1: Basic setup with external audio
void setup() {
    FastLED.addLeds<WS2812, DATA_PIN>(leds, NUM_LEDS);
    fl::Audio.begin({.sampleRate = 22050, .agcEnabled = true});
}

void loop() {
    // External audio feeding (I2S, file, etc.)
    int16_t samples[32];
    size_t count = getAudioSamples(samples, 32);  // External function
    fl::Audio.addSamples(samples, count);
    
    // Update audio processing (non-blocking)
    fl::Audio.update();
    
    // Use audio data in effects
    uint8_t brightness = fl::Audio.volumeToScale255();
    fill_solid(leds, NUM_LEDS, CHSV(fl::Audio.getMid(), 255, brightness));
    
    if (fl::Audio.isBeat()) {
        // Flash white on beat
        fill_solid(leds, NUM_LEDS, CRGB::White);
    }
    
    FastLED.show();
}

// Example 2: Simulated audio for testing
void loop() {
    static float phase = 0;
    phase += 0.1f;
    int16_t sample = (int16_t)(sin(phase) * 1000 + random(-100, 100));
    
    fl::Audio.addSample(sample);
    fl::Audio.update();
    
    // Your effects here...
    FastLED.show();
}
```

## Implementation Strategy

### Phase 1: Core Audio Processing
1. **AudioReactive class** - Main coordinator using `fl/fft.h`
2. **Frequency mapping** - WLED-compatible 16-band mapping
3. **Simple AGC** - Lightweight automatic gain control
4. **Peak detection** - Basic beat detection algorithm
5. **Non-blocking processing** - Incremental FFT and filtering

### Phase 2: Advanced Features  
1. **Multiple scaling modes** - Linear, logarithmic, square-root
2. **Noise gating** - Automatic silence detection
3. **Advanced filtering** - Band-pass filters, smoothing
4. **Performance optimization** - Memory usage and processing speed

### Phase 3: Testing & Validation
1. **Unit test framework** - Comprehensive test suite
2. **Audio simulation** - Sine wave generation for testing
3. **Frequency response validation** - Verify bin mapping accuracy
4. **Performance benchmarking** - Real-time processing validation

### Phase 4: Integration
1. **Effect helper functions** - Easy audio-to-visual mapping
2. **Documentation & examples** - Getting started guides
3. **Platform testing** - Cross-platform compatibility verification

## Unit Testing Framework

### Test Approach

The unit testing framework uses synthetic audio generation to validate the audio reactive system:

1. **Sine Wave Generation**: Create pure tones at specific frequencies
2. **Mixed Frequency Content**: Test multiple simultaneous frequencies
3. **Volume Detection**: Verify amplitude-to-volume mapping
4. **Beat Detection**: Test peak detection algorithms
5. **Frequency Mapping**: Validate 16-band frequency distribution

### Key Test Cases

```cpp
// Test 1: Single frequency detection
TEST_F(AudioReactiveTest, DetectsSingleFrequencies) {
    // Generate 1kHz sine wave
    auto samples = generateSineWave(1000.0f, 10000.0f, 22050, 512);
    feedSamplesAndProcess(samples);
    
    // Verify it appears in bin 7 (818-1120Hz range)
    verifyFrequencyBinHasEnergy(7, 20.0f);
}

// Test 2: Mixed frequency content
TEST_F(AudioReactiveTest, DetectsMixedFrequencies) {
    // Generate bass + mid + treble
    fl::vector<float> frequencies = {100.0f, 1000.0f, 4000.0f};
    fl::vector<float> amplitudes = {8000.0f, 6000.0f, 4000.0f};
    
    auto samples = generateMixedFrequencies(frequencies, amplitudes, 22050, 512);
    feedSamplesAndProcess(samples);
    
    // Verify each frequency band has energy
    verifyFrequencyBinHasEnergy(1, 15.0f);   // 100Hz -> Bin 1
    verifyFrequencyBinHasEnergy(7, 15.0f);   // 1kHz -> Bin 7  
    verifyFrequencyBinHasEnergy(13, 15.0f);  // 4kHz -> Bin 13
}

// Test 3: Beat detection
TEST_F(AudioReactiveTest, DetectsBeats) {
    // Generate quiet period followed by loud burst
    // ... test implementation
    EXPECT_TRUE(audio.isBeat()) << "Should detect beat after amplitude increase";
}
```

### Synthetic Audio Generation

```cpp
// Generate sine wave at specific frequency
fl::vector<int16_t> generateSineWave(float frequency, float amplitude, 
                                      uint32_t sampleRate, size_t numSamples) {
    fl::vector<int16_t> samples;
    samples.reserve(numSamples);
    
    float phaseIncrement = 2.0f * FL_PI * frequency / sampleRate;
    
    for (size_t i = 0; i < numSamples; ++i) {
        float phase = i * phaseIncrement;
        float sample = amplitude * sinf(phase);
        samples.push_back((int16_t)sample);
    }
    
    return samples;
}

// Generate mixed frequency content
fl::vector<int16_t> generateMixedFrequencies(const fl::vector<float>& frequencies,
                                              const fl::vector<float>& amplitudes,
                                              uint32_t sampleRate, size_t numSamples) {
    fl::vector<int16_t> samples(numSamples, 0);
    
    for (size_t freqIdx = 0; freqIdx < frequencies.size(); ++freqIdx) {
        float freq = frequencies[freqIdx];
        float amp = amplitudes[freqIdx];
        float phaseIncrement = 2.0f * FL_PI * freq / sampleRate;
        
        for (size_t i = 0; i < numSamples; ++i) {
            float phase = i * phaseIncrement;
            samples[i] += (int16_t)(amp * sinf(phase));
        }
    }
    
    return samples;
}
```

## Implementation Details

### FFT Processing

Uses FastLED's existing `fl/fft.h`:

```cpp
void AudioReactive::processFFT() {
    if (sampleBuffer.size() < 512) return;
    
    // Use FastLED's FFT with WLED-compatible parameters
    FFT_Args args(512, 16, 43.0f, 9259.0f, config.sampleRate);
    fft.run(Slice<const int16_t>(sampleBuffer.data(), 512), &fftBins, args);
    
    // Map FFT bins to 16 frequency channels (matches WLED mapping)
    mapFFTBinsToFrequencyChannels();
}
```

### Frequency Band Mapping

Exact replication of WLED's frequency mapping:

```cpp
void AudioReactive::mapFFTBinsToFrequencyChannels() {
    float fftCalc[16];
    
    // WLED-compatible frequency mapping
    fftCalc[0] = mapFrequencyBin(1, 2);     // 43-86 Hz sub-bass
    fftCalc[1] = mapFrequencyBin(2, 3);     // 86-129 Hz bass  
    fftCalc[2] = mapFrequencyBin(3, 5);     // 129-216 Hz bass
    fftCalc[3] = mapFrequencyBin(5, 7);     // 216-301 Hz bass + midrange
    fftCalc[4] = mapFrequencyBin(7, 10);    // 301-430 Hz midrange
    fftCalc[5] = mapFrequencyBin(10, 13);   // 430-560 Hz midrange
    fftCalc[6] = mapFrequencyBin(13, 19);   // 560-818 Hz midrange
    fftCalc[7] = mapFrequencyBin(19, 26);   // 818-1120 Hz midrange (1kHz center)
    fftCalc[8] = mapFrequencyBin(26, 33);   // 1120-1421 Hz midrange
    fftCalc[9] = mapFrequencyBin(33, 44);   // 1421-1895 Hz midrange
    fftCalc[10] = mapFrequencyBin(44, 56);  // 1895-2412 Hz midrange + high mid
    fftCalc[11] = mapFrequencyBin(56, 70);  // 2412-3015 Hz high mid
    fftCalc[12] = mapFrequencyBin(70, 86);  // 3015-3704 Hz high mid
    fftCalc[13] = mapFrequencyBin(86, 104); // 3704-4479 Hz high mid
    fftCalc[14] = mapFrequencyBin(104, 165) * 0.88f; // 4479-7106 Hz high mid + high
    fftCalc[15] = mapFrequencyBin(165, 215) * 0.70f; // 7106-9259 Hz high
    
    // Apply pink noise compensation and scaling
    for (int i = 0; i < 16; ++i) {
        fftCalc[i] *= PINK_NOISE_COMPENSATION[i];
        // Apply scaling mode (linear, log, sqrt)
        // Store in currentData.frequencyBins[i]
    }
}
```

### Pink Noise Compensation

Original WLED compensation factors:

```cpp
static constexpr float PINK_NOISE_COMPENSATION[16] = {
    1.70f, 1.71f, 1.73f, 1.78f, 1.68f, 1.56f, 1.55f, 1.63f,
    1.79f, 1.62f, 1.80f, 2.06f, 2.47f, 3.35f, 6.83f, 9.55f
};
```

### Non-Blocking Processing

```cpp
void AudioReactive::update() {
    // Only process FFT periodically (~50Hz update rate)
    if (millis() - lastProcessTime > PROCESS_INTERVAL) {
        if (sampleBuffer.size() >= 512) {
            processFFT();
            updateVolumeAndPeak();
            detectBeat();
            smoothResults();
            lastProcessTime = millis();
        }
    }
}
```

## Key Advantages

1. **Platform Agnostic**: Works with any audio source providing signed PCM data
2. **Non-Blocking**: Fits FastLED's loop-based architecture
3. **WLED Compatible**: Maintains the same 16-frequency-band output structure
4. **Testable**: Comprehensive unit test framework with synthetic audio
5. **Lightweight**: Uses FastLED's existing FFT implementation
6. **Simple Integration**: Just call `addSample()` and `update()`

## Performance Considerations

- **Memory Usage**: ~2KB for sample buffers (vs 4KB+ in original)
- **Processing Time**: <5ms per FFT cycle on ESP32 (target <20ms)
- **Update Rate**: ~50Hz frequency analysis (vs 21ms minimum in WLED)
- **CPU Usage**: Configurable FFT size for performance vs quality tradeoffs

## Future Enhancements

1. **Multiple FFT Sizes**: 128, 256, 512, 1024 point options
2. **Advanced AGC**: Multiple presets (normal, vivid, lazy)
3. **UDP Sync**: Multi-device synchronization (WiFi platforms only)
4. **Audio File Support**: WAV/MP3 playback for testing
5. **Real-time Visualization**: Debug output for frequency analysis
6. **Effect Libraries**: Pre-built audio-reactive animations

## Conclusion

This design provides a solid foundation for audio-reactive LED effects in FastLED while maintaining the core principles of simplicity, performance, and cross-platform compatibility. The system eliminates the complexity of hardware-specific audio input while preserving the sophisticated frequency analysis capabilities of the original WLED implementation.

The unit test framework ensures reliable operation across different audio scenarios, while the non-blocking architecture fits naturally into FastLED's existing event loop model. The result is a powerful yet accessible audio reactive system that can drive stunning visual effects synchronized to music. 
