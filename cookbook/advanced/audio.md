# Audio Reactive Patterns

> **⚠️ EXPERIMENTAL**: This documentation covers experimental audio features currently in development. The FastLED beat detection APIs were introduced in October 2025 and are still evolving. Code examples shown here demonstrate general audio-reactive concepts but may not reflect the final API.

Create LED visualizations that respond to audio input. This guide covers reading audio signals and creating reactive effects.

## Basic Audio Setup

To read audio, you'll need an analog input connected to a microphone or audio line.

### Hardware Requirements
- Microcontroller with analog input (most Arduino-compatible boards)
- Electret microphone module or audio line-in circuit
- Optional: MAX4466 or similar microphone amplifier for better sensitivity

### Reading Audio Levels

```cpp
#define MIC_PIN A0
#define SAMPLE_WINDOW 50  // Sample window width in ms

uint16_t readAudioLevel() {
    unsigned long startMillis = millis();
    uint16_t peakToPeak = 0;
    uint16_t signalMax = 0;
    uint16_t signalMin = 1024;

    // Collect data for sample window
    while (millis() - startMillis < SAMPLE_WINDOW) {
        uint16_t sample = analogRead(MIC_PIN);
        if (sample > signalMax) signalMax = sample;
        if (sample < signalMin) signalMin = sample;
    }

    peakToPeak = signalMax - signalMin;
    return peakToPeak;
}
```

This function measures the peak-to-peak amplitude of the audio signal over a short time window, giving you a reading of the current volume level.

## VU Meter Effect

A classic visualization that displays audio level as a growing/shrinking bar.

```cpp
void vuMeter() {
    uint16_t audioLevel = readAudioLevel();

    // Map to LED count (0 to NUM_LEDS)
    uint8_t numLEDsToLight = map(audioLevel, 0, 1023, 0, NUM_LEDS);

    // Clear all
    fill_solid(leds, NUM_LEDS, CRGB::Black);

    // Light up LEDs based on level
    for (int i = 0; i < numLEDsToLight; i++) {
        // Color gradient: green -> yellow -> red
        uint8_t hue = map(i, 0, NUM_LEDS, 96, 0);  // Green to red
        leds[i] = CHSV(hue, 255, 255);
    }
}
```

### VU Meter Variations

**With Peak Hold**:
```cpp
void vuMeterWithPeak() {
    static uint8_t peak = 0;
    static unsigned long lastPeakTime = 0;

    uint16_t audioLevel = readAudioLevel();
    uint8_t numLEDsToLight = map(audioLevel, 0, 1023, 0, NUM_LEDS);

    // Update peak
    if (numLEDsToLight > peak) {
        peak = numLEDsToLight;
        lastPeakTime = millis();
    }

    // Peak decay
    if (millis() - lastPeakTime > 1000) {
        if (peak > 0) peak--;
        lastPeakTime = millis();
    }

    // Draw VU meter
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int i = 0; i < numLEDsToLight; i++) {
        uint8_t hue = map(i, 0, NUM_LEDS, 96, 0);
        leds[i] = CHSV(hue, 255, 255);
    }

    // Draw peak indicator
    if (peak < NUM_LEDS) {
        leds[peak] = CRGB::White;
    }
}
```

## Beat Detection

Detect beats/kicks in music for reactive effects.

```cpp
class BeatDetector {
private:
    uint16_t samples[32];
    uint8_t sampleIndex = 0;
    uint32_t lastBeatTime = 0;

public:
    bool detectBeat(uint16_t audioLevel) {
        // Store sample
        samples[sampleIndex] = audioLevel;
        sampleIndex = (sampleIndex + 1) % 32;

        // Calculate average
        uint32_t sum = 0;
        for (int i = 0; i < 32; i++) sum += samples[i];
        uint16_t average = sum / 32;

        // Detect beat (current level significantly above average)
        bool isBeat = (audioLevel > average * 1.5) &&
                      (millis() - lastBeatTime > 300);  // Minimum 300ms between beats

        if (isBeat) lastBeatTime = millis();
        return isBeat;
    }
};

BeatDetector beatDetector;

void beatEffect() {
    uint16_t audioLevel = readAudioLevel();

    if (beatDetector.detectBeat(audioLevel)) {
        // Flash on beat
        fill_solid(leds, NUM_LEDS, CRGB::White);
    } else {
        // Fade out
        fadeToBlackBy(leds, NUM_LEDS, 10);
    }
}
```

### Beat-Synchronized Effects

**Color Change on Beat**:
```cpp
void beatColorChange() {
    static uint8_t currentHue = 0;
    uint16_t audioLevel = readAudioLevel();

    if (beatDetector.detectBeat(audioLevel)) {
        currentHue += 32;  // Change color on beat
    }

    // Fade to current color
    CRGB targetColor = CHSV(currentHue, 255, 255);
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = blend(leds[i], targetColor, 50);
    }
}
```

**Pulse on Beat**:
```cpp
void beatPulse() {
    static uint8_t pulseIntensity = 0;
    uint16_t audioLevel = readAudioLevel();

    if (beatDetector.detectBeat(audioLevel)) {
        pulseIntensity = 255;
    }

    // Draw pulsing effect
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(160, 255, pulseIntensity);
    }

    // Fade out
    if (pulseIntensity > 0) pulseIntensity -= 5;
}
```

## Advanced Audio Effects

### Frequency Bands (requires FFT)

For more sophisticated audio reactivity, use FFT (Fast Fourier Transform) to analyze frequency bands. This requires additional libraries like:
- arduinoFFT
- fix_fft

Example concept (requires FFT library):
```cpp
// Pseudo-code - requires FFT library
void frequencyBands() {
    // Perform FFT on audio samples
    // Extract bass, mid, treble

    uint8_t bass = getBassLevel();    // 0-255
    uint8_t mid = getMidLevel();      // 0-255
    uint8_t treble = getTrebleLevel(); // 0-255

    // Map to different parts of strip
    int bassLEDs = NUM_LEDS / 3;
    int midLEDs = NUM_LEDS / 3;
    int trebleLEDs = NUM_LEDS / 3;

    // Bass: red
    fill_solid(&leds[0], bassLEDs, CRGB::Red);
    fadeToBlackBy(&leds[0], bassLEDs, 255 - bass);

    // Mid: green
    fill_solid(&leds[bassLEDs], midLEDs, CRGB::Green);
    fadeToBlackBy(&leds[bassLEDs], midLEDs, 255 - mid);

    // Treble: blue
    fill_solid(&leds[bassLEDs + midLEDs], trebleLEDs, CRGB::Blue);
    fadeToBlackBy(&leds[bassLEDs + midLEDs], trebleLEDs, 255 - treble);
}
```

## Tips for Audio Reactive Effects

### Calibration

Adjust sensitivity based on your audio source:
```cpp
// For quiet environments
uint16_t calibratedLevel = map(audioLevel, 0, 200, 0, 1023);

// For loud environments
uint16_t calibratedLevel = map(audioLevel, 100, 800, 0, 1023);
```

### Smoothing

Smooth audio readings to avoid jitter:
```cpp
uint16_t smoothAudio(uint16_t newValue) {
    static uint16_t lastValue = 0;
    lastValue = (lastValue * 7 + newValue) / 8;  // Exponential smoothing
    return lastValue;
}
```

### Performance

Audio processing can be CPU-intensive:
- Use shorter sample windows for faster response
- Limit FFT resolution if using frequency analysis
- Consider using interrupts for audio sampling
- Test frame rates to ensure smooth LED updates

## Troubleshooting

**No audio response**:
- Check microphone wiring and power
- Verify analog pin in code matches hardware
- Test with Serial.println(audioLevel) to see raw readings
- Adjust calibration thresholds

**Too sensitive/not sensitive enough**:
- Adjust map() ranges in calibration
- Check microphone gain if using amplified mic
- Modify beat detection threshold (currently 1.5x average)

**Jerky motion**:
- Add smoothing to audio readings
- Increase sample window size
- Use exponential smoothing or running average

**False beat detection**:
- Increase minimum time between beats (currently 300ms)
- Adjust threshold multiplier (currently 1.5)
- Increase sample buffer size for better average calculation

---

**Next Steps**:
- Experiment with different threshold values for your environment
- Combine audio reactivity with other effects (noise, palettes)
- Try multiple LED strips reacting to different frequency bands
