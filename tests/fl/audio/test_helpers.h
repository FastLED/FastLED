/// @file tests/fl/audio/test_helpers.h
/// Centralized test helper functions for audio unit tests
/// Provides common utilities for generating test audio samples, silence, tones, and FFT data

#pragma once

#include "fl/audio.h"
#include "fl/int.h"
#include "fl/math_macros.h"
#include "fl/stl/math.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"

namespace fl {
namespace audio {
namespace test {

// ============================================================================
// Audio Sample Generators
// ============================================================================

/// Generate a sine wave audio sample with specified frequency, timestamp, and amplitude
/// @param freq Frequency in Hz (e.g., 440.0f for A4)
/// @param timestamp Sample timestamp in milliseconds
/// @param amplitude Peak amplitude (default: 16000 for typical test signal)
/// @param count Number of samples (default: 512)
/// @param sampleRate Sample rate in Hz (default: 44100)
/// @return AudioSample containing the generated sine wave
inline AudioSample makeSample(float freq, fl::u32 timestamp, float amplitude = 16000.0f,
                              int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data;
    data.reserve(count);
    for (int i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * freq * i / sampleRate;
        data.push_back(static_cast<fl::i16>(amplitude * fl::sinf(phase)));
    }
    return AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp);
}

/// Generate a silence audio sample (all zeros)
/// @param timestamp Sample timestamp in milliseconds
/// @param count Number of samples (default: 512)
/// @return AudioSample containing silence
inline AudioSample makeSilence(fl::u32 timestamp, int count = 512) {
    fl::vector<fl::i16> data(count, 0);
    return AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp);
}

/// Generate a DC offset audio sample (constant value)
/// @param dcValue Constant DC value for all samples
/// @param timestamp Sample timestamp in milliseconds
/// @param count Number of samples (default: 512)
/// @return AudioSample containing DC offset
inline AudioSample makeDC(fl::i16 dcValue, fl::u32 timestamp, int count = 512) {
    fl::vector<fl::i16> data(count, dcValue);
    return AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp);
}

/// Generate a maximum amplitude audio sample (saturated signal)
/// @param timestamp Sample timestamp in milliseconds
/// @param count Number of samples (default: 512)
/// @return AudioSample with maximum i16 amplitude
inline AudioSample makeMaxAmplitude(fl::u32 timestamp, int count = 512) {
    fl::vector<fl::i16> data(count, 32767);
    return AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp);
}

/// Generate audio sample from PCM data
/// @param pcm Vector of PCM samples
/// @param timestamp Sample timestamp in milliseconds (default: 0)
/// @return AudioSample wrapping the PCM data
inline AudioSample makeSample(const vector<i16> &pcm, u32 timestamp = 0) {
    return AudioSample(fl::span<const fl::i16>(pcm.data(), pcm.size()), timestamp);
}

// ============================================================================
// Raw PCM Generators (for direct vector manipulation)
// ============================================================================

/// Generate a sine wave as raw PCM vector
/// @param freq Frequency in Hz
/// @param count Number of samples (default: 512)
/// @param sampleRate Sample rate in Hz (default: 44100)
/// @param amplitude Peak amplitude (default: 16000)
/// @return Vector of i16 samples
inline fl::vector<fl::i16> generateSine(float freq, int count = 512,
                                        float sampleRate = 44100.0f,
                                        float amplitude = 16000.0f) {
    fl::vector<fl::i16> samples;
    samples.reserve(count);
    for (int i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * freq * i / sampleRate;
        samples.push_back(static_cast<fl::i16>(amplitude * fl::sinf(phase)));
    }
    return samples;
}

/// Generate a tone as raw PCM vector (in-place version)
/// @param out Output vector to append samples to
/// @param count Number of samples to generate
/// @param frequency Frequency in Hz
/// @param sampleRate Sample rate in Hz
/// @param amplitude Peak amplitude
inline void generateSine(vector<i16> &out, int count, float frequency,
                        float sampleRate, i16 amplitude) {
    out.reserve(out.size() + count);
    for (int i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * frequency * i / sampleRate;
        out.push_back(static_cast<i16>(amplitude * fl::sinf(phase)));
    }
}

/// Generate a tone as raw PCM vector
/// @param count Number of samples
/// @param frequency Frequency in Hz
/// @param sampleRate Sample rate in Hz
/// @param amplitude Peak amplitude
/// @return Vector of i16 samples
inline vector<i16> generateTone(size count, float frequency,
                                float sampleRate, i16 amplitude) {
    vector<i16> samples;
    generateSine(samples, count, frequency, sampleRate, amplitude);
    return samples;
}

/// Generate a constant signal (all same value)
/// @param count Number of samples
/// @param amplitude Constant value for all samples
/// @return Vector of i16 samples
inline vector<i16> generateConstantSignal(size count, i16 amplitude) {
    return vector<i16>(count, amplitude);
}

/// Generate DC offset as raw PCM vector (in-place version)
/// @param out Output vector to append samples to
/// @param count Number of samples to generate
/// @param dcOffset Constant DC value
inline void generateDC(vector<i16> &out, int count, i16 dcOffset) {
    out.reserve(out.size() + count);
    for (int i = 0; i < count; ++i) {
        out.push_back(dcOffset);
    }
}

// ============================================================================
// FFT Test Data Generators
// ============================================================================

/// Generate synthetic FFT bin data with a peak at specified frequency
/// @param numBins Number of frequency bins
/// @param peakFrequency Frequency where the peak should occur (Hz)
/// @param sampleRate Sample rate in Hz
/// @return Vector of float magnitudes (one per bin)
inline vector<float> generateSyntheticFFT(size numBins, float peakFrequency, u32 sampleRate) {
    vector<float> bins;
    bins.reserve(numBins);
    float binWidth = sampleRate / (2.0f * numBins);
    for (size i = 0; i < numBins; ++i) {
        float binFreq = i * binWidth;
        // Gaussian-like peak centered at peakFrequency
        float distance = fl::abs(binFreq - peakFrequency) / binWidth;
        float magnitude = fl::exp(-distance * distance / 2.0f);
        bins.push_back(magnitude);
    }
    return bins;
}

/// Generate uniform magnitude bins (all same value)
/// @param count Number of bins
/// @param magnitude Value for all bins
/// @return Vector of float magnitudes
inline vector<float> generateUniformBins(size count, float magnitude) {
    return vector<float>(count, magnitude);
}

} // namespace test
} // namespace audio
} // namespace fl
