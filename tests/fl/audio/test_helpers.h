/// @file tests/fl/audio/test_helpers.h
/// Centralized test helper functions for audio unit tests
/// Provides common utilities for generating test audio samples, silence, tones, and FFT data

#pragma once

#include "fl/audio/audio.h"
#include "fl/stl/int.h"
#include "fl/stl/math.h"
#include "fl/stl/math.h"
#include "fl/stl/random.h"
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
    return AudioSample(data, timestamp);
}

/// Generate a silence audio sample (all zeros)
/// @param timestamp Sample timestamp in milliseconds
/// @param count Number of samples (default: 512)
/// @return AudioSample containing silence
inline AudioSample makeSilence(fl::u32 timestamp, int count = 512) {
    fl::vector<fl::i16> data(count, 0);
    return AudioSample(data, timestamp);
}

/// Generate a DC offset audio sample (constant value)
/// @param dcValue Constant DC value for all samples
/// @param timestamp Sample timestamp in milliseconds
/// @param count Number of samples (default: 512)
/// @return AudioSample containing DC offset
inline AudioSample makeDC(fl::i16 dcValue, fl::u32 timestamp, int count = 512) {
    fl::vector<fl::i16> data(count, dcValue);
    return AudioSample(data, timestamp);
}

/// Generate a maximum amplitude audio sample (saturated signal)
/// @param timestamp Sample timestamp in milliseconds
/// @param count Number of samples (default: 512)
/// @return AudioSample with maximum i16 amplitude
inline AudioSample makeMaxAmplitude(fl::u32 timestamp, int count = 512) {
    fl::vector<fl::i16> data(count, 32767);
    return AudioSample(data, timestamp);
}

/// Generate audio sample from PCM data
/// @param pcm Vector of PCM samples
/// @param timestamp Sample timestamp in milliseconds (default: 0)
/// @return AudioSample wrapping the PCM data
inline AudioSample makeSample(const vector<i16> &pcm, u32 timestamp = 0) {
    return AudioSample(pcm, timestamp);
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

/// Generate synthetic FFT bin data with a peak at specified frequency.
/// Uses linear bin spacing (for use with FrequencyBinMapper and raw FFT data).
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

/// Generate synthetic CQ-spaced FFT bin data with a peak at specified frequency.
/// Uses CQ log-spaced frequency layout matching the actual CQ kernel.
/// @param numBins Number of CQ bins
/// @param peakFrequency Frequency where the peak should occur (Hz)
/// @param fmin CQ minimum frequency
/// @param fmax CQ maximum frequency
/// @return Vector of float magnitudes (one per bin)
inline vector<float> generateSyntheticCQFFT(size numBins, float peakFrequency,
                                             float fmin = 174.6f, float fmax = 4698.3f) {
    vector<float> bins;
    bins.reserve(numBins);
    int bands = static_cast<int>(numBins);
    float logRatio = fl::logf(fmax / fmin);
    for (size i = 0; i < numBins; ++i) {
        float binFreq = fmin * fl::expf(logRatio * static_cast<float>(i) / static_cast<float>(bands - 1));
        // Gaussian-like peak in log-frequency space
        float logDistance = fl::logf(binFreq / peakFrequency) / logRatio * static_cast<float>(bands);
        float magnitude = fl::exp(-logDistance * logDistance / 2.0f);
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

// ============================================================================
// Complex Signal Generators (for vocal detection testing)
// ============================================================================

/// Generate a multi-harmonic signal (fundamental + overtones with geometric decay)
/// Useful for testing harmonic-rich signals that aren't voice (e.g., instruments)
inline AudioSample makeMultiHarmonic(float f0, int numHarmonics, float decay,
                                      u32 timestamp, float amplitude = 16000.0f,
                                      int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data(count, 0);
    for (int h = 1; h <= numHarmonics; ++h) {
        float harmonicAmp = amplitude * fl::powf(decay, static_cast<float>(h - 1));
        float freq = f0 * h;
        if (freq >= sampleRate / 2.0f) break; // Nyquist
        for (int i = 0; i < count; ++i) {
            float phase = 2.0f * FL_M_PI * freq * i / sampleRate;
            data[i] += static_cast<fl::i16>(harmonicAmp * fl::sinf(phase));
        }
    }
    return AudioSample(data, timestamp);
}

/// Generate a synthetic vowel using additive synthesis with Gaussian formant envelopes
/// Simplest physically-motivated voice model: harmonic series with 1/h rolloff shaped by two resonance peaks
inline AudioSample makeSyntheticVowel(float f0, float f1Freq, float f2Freq,
                                       u32 timestamp, float amplitude = 16000.0f,
                                       int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data(count, 0);
    const float f1Bw = 150.0f; // F1 bandwidth
    const float f2Bw = 200.0f; // F2 bandwidth
    const float maxFreq = fl::min(4000.0f, sampleRate / 2.0f);

    for (int h = 1; h * f0 < maxFreq; ++h) {
        float freq = f0 * h;
        // Natural 1/h rolloff
        float naturalAmp = 1.0f / static_cast<float>(h);
        // Gaussian formant envelopes
        float f1Gain = fl::expf(-0.5f * (freq - f1Freq) * (freq - f1Freq) / (f1Bw * f1Bw));
        float f2Gain = fl::expf(-0.5f * (freq - f2Freq) * (freq - f2Freq) / (f2Bw * f2Bw));
        float formantGain = fl::max(f1Gain, f2Gain);
        float harmonicAmp = amplitude * naturalAmp * fl::max(0.05f, formantGain);

        for (int i = 0; i < count; ++i) {
            float phase = 2.0f * FL_M_PI * freq * i / sampleRate;
            data[i] += static_cast<fl::i16>(harmonicAmp * fl::sinf(phase));
        }
    }
    return AudioSample(data, timestamp);
}

/// Generate a jittered vowel: synthetic vowel with per-period amplitude and timing jitter
/// Simulates real vocal cord irregularity — higher envelope jitter and lower autocorrelation
/// than clean synthetic vowels
inline AudioSample makeJitteredVowel(float f0, float f1Freq, float f2Freq,
                                      u32 timestamp, float amplitude = 16000.0f,
                                      int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data(count, 0);
    fl::fl_random rng(123);
    const float f1Bw = 150.0f;
    const float f2Bw = 200.0f;
    const float maxFreq = fl::min(4000.0f, sampleRate / 2.0f);

    // Generate clean vowel base
    for (int h = 1; h * f0 < maxFreq; ++h) {
        float freq = f0 * h;
        float naturalAmp = 1.0f / static_cast<float>(h);
        float f1Gain = fl::expf(-0.5f * (freq - f1Freq) * (freq - f1Freq) / (f1Bw * f1Bw));
        float f2Gain = fl::expf(-0.5f * (freq - f2Freq) * (freq - f2Freq) / (f2Bw * f2Bw));
        float formantGain = fl::max(f1Gain, f2Gain);
        float harmonicAmp = amplitude * naturalAmp * fl::max(0.05f, formantGain);

        for (int i = 0; i < count; ++i) {
            float phase = 2.0f * FL_M_PI * freq * i / sampleRate;
            data[i] += static_cast<fl::i16>(harmonicAmp * fl::sinf(phase));
        }
    }

    // Apply per-period amplitude jitter (simulates vocal cord irregularity)
    float periodSamples = sampleRate / f0;
    float pos = 0.0f;
    float currentJitter = 1.0f;
    for (int i = 0; i < count; ++i) {
        pos += 1.0f;
        if (pos >= periodSamples) {
            pos -= periodSamples;
            // Random amplitude jitter: +/- 20%
            currentJitter = 0.80f + 0.40f * (static_cast<float>(rng.random16()) / 65535.0f);
        }
        float sample = static_cast<float>(data[i]) * currentJitter;
        data[i] = static_cast<fl::i16>(fl::clamp(sample, -32768.0f, 32767.0f));
    }
    return AudioSample(data, timestamp);
}

/// Generate a guitar string decay: harmonic series with 1/h^2 rolloff and exponential decay
/// Very periodic waveform — low envelope jitter, high autocorrelation, low ZC CV
inline AudioSample makeGuitarStringDecay(float f0, u32 timestamp,
                                          float amplitude = 16000.0f,
                                          int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data(count, 0);
    const float maxFreq = fl::min(8000.0f, sampleRate / 2.0f);
    const float decayTime = 0.5f; // 500ms decay

    for (int h = 1; h * f0 < maxFreq; ++h) {
        float freq = f0 * h;
        float harmonicAmp = amplitude / static_cast<float>(h * h);
        float harmonicDecay = decayTime / static_cast<float>(h); // Higher harmonics decay faster

        for (int i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            float decay = fl::expf(-t / harmonicDecay);
            float phase = 2.0f * FL_M_PI * freq * t;
            float sample = harmonicAmp * decay * fl::sinf(phase);
            data[i] += static_cast<fl::i16>(fl::clamp(sample, -32768.0f, 32767.0f));
        }
    }
    return AudioSample(data, timestamp);
}

/// Generate deterministic white noise using fl::fl_random with fixed seed
inline AudioSample makeWhiteNoise(u32 timestamp, float amplitude = 16000.0f,
                                   int count = 512) {
    fl::fl_random rng(12345); // Deterministic seed for reproducible tests
    fl::vector<fl::i16> data;
    data.reserve(count);
    for (int i = 0; i < count; ++i) {
        // Map u16 [0, 65535] to float [-1, 1]
        float sample = (static_cast<float>(rng.random16()) / 32767.5f) - 1.0f;
        data.push_back(static_cast<fl::i16>(amplitude * sample));
    }
    return AudioSample(data, timestamp);
}

/// Generate a linear frequency sweep (chirp) with phase accumulation
/// Tests that a sweeping tone doesn't trigger vocal detection
inline AudioSample makeChirp(float startFreq, float endFreq, u32 timestamp,
                              float amplitude = 16000.0f, int count = 512,
                              float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data;
    data.reserve(count);
    float phase = 0.0f;
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(count);
        float freq = startFreq + (endFreq - startFreq) * t;
        phase += 2.0f * FL_M_PI * freq / sampleRate;
        data.push_back(static_cast<fl::i16>(amplitude * fl::sinf(phase)));
    }
    return AudioSample(data, timestamp);
}

// ============================================================================
// Percussion Signal Generators
// ============================================================================

/// Generate a synthetic kick drum: 60Hz body + 120Hz harmonic (30ms decay) + click transient
/// The click uses multiple high-frequency sines sustained long enough for CQ FFT to resolve
inline AudioSample makeKickDrum(fl::u32 timestamp, float amplitude = 16000.0f,
                                 int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data(count, 0);
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        // Body: 60Hz fundamental with 30ms exponential decay
        float bodyDecay = fl::expf(-t / 0.030f);
        float body = amplitude * 0.7f * bodyDecay * fl::sinf(2.0f * FL_M_PI * 60.0f * t);
        // Second harmonic: 120Hz
        float harm2 = amplitude * 0.3f * bodyDecay * fl::sinf(2.0f * FL_M_PI * 120.0f * t);
        // Click transient: sustained for ~200 samples with fast exponential decay
        // Uses multiple frequencies in the CQ treble range (bins 10-12)
        float clickDecay = fl::expf(-t / 0.003f); // ~3ms decay, covers ~130 samples
        float click = amplitude * 0.8f * clickDecay * (
            0.5f * fl::sinf(2.0f * FL_M_PI * 2000.0f * t) +
            0.3f * fl::sinf(2.0f * FL_M_PI * 3000.0f * t) +
            0.2f * fl::sinf(2.0f * FL_M_PI * 4000.0f * t)
        );
        float sample = body + harm2 + click;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        data[i] = static_cast<fl::i16>(sample);
    }
    return AudioSample(data, timestamp);
}

/// Generate a synthetic snare: 250Hz body (25ms decay) + high-frequency noise (50ms decay)
/// Produces moderate bass + significant treble energy from noise rattles
inline AudioSample makeSnare(fl::u32 timestamp, float amplitude = 16000.0f,
                              int count = 512, float sampleRate = 44100.0f) {
    fl::fl_random rng(42); // Deterministic seed
    fl::vector<fl::i16> data(count, 0);
    float prevNoise = 0.0f;
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        // Tonal body: 250Hz with 25ms decay — reduced to let treble dominate
        float bodyDecay = fl::expf(-t / 0.025f);
        float body = amplitude * 0.20f * bodyDecay * fl::sinf(2.0f * FL_M_PI * 250.0f * t);
        // Noise rattle: high-pass filtered white noise with 50ms decay
        float noiseDecay = fl::expf(-t / 0.050f);
        float rawNoise = (static_cast<float>(rng.random16()) / 32767.5f) - 1.0f;
        // Second-order high-pass approximation for stronger treble content
        float highPassNoise = rawNoise - 0.95f * prevNoise;
        prevNoise = rawNoise;
        float noise = amplitude * 0.80f * noiseDecay * highPassNoise;
        // Also add explicit high-frequency tones to boost CQ treble bins
        float trebleTones = amplitude * 0.25f * bodyDecay * (
            0.4f * fl::sinf(2.0f * FL_M_PI * 2000.0f * t) +
            0.3f * fl::sinf(2.0f * FL_M_PI * 3500.0f * t) +
            0.3f * fl::sinf(2.0f * FL_M_PI * 4500.0f * t)
        );
        float sample = body + noise + trebleTones;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        data[i] = static_cast<fl::i16>(sample);
    }
    return AudioSample(data, timestamp);
}

/// Generate a synthetic hi-hat: high-pass shaped white noise
/// @param open If true, use 80ms decay (open hi-hat); if false, use 5ms decay (closed)
inline AudioSample makeHiHat(fl::u32 timestamp, bool open = false, float amplitude = 16000.0f,
                              int count = 512, float sampleRate = 44100.0f) {
    fl::fl_random rng(99); // Deterministic seed
    float decayTime = open ? 0.080f : 0.005f;
    fl::vector<fl::i16> data(count, 0);
    float prevSample = 0.0f;
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float decay = fl::expf(-t / decayTime);
        // White noise
        float noiseVal = (static_cast<float>(rng.random16()) / 32767.5f) - 1.0f;
        // Simple high-pass: subtract previous sample (first-order difference)
        float raw = amplitude * decay * noiseVal;
        float highPassed = raw - prevSample * 0.85f;
        prevSample = raw;
        if (highPassed > 32767.0f) highPassed = 32767.0f;
        if (highPassed < -32768.0f) highPassed = -32768.0f;
        data[i] = static_cast<fl::i16>(highPassed);
    }
    return AudioSample(data, timestamp);
}

/// Generate a synthetic tom drum: single sine at tuning frequency, 60ms decay, NO click
/// Discriminated from kick by absence of click transient
inline AudioSample makeTom(fl::u32 timestamp, float tuningHz = 160.0f, float amplitude = 16000.0f,
                            int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data(count, 0);
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float decay = fl::expf(-t / 0.060f);
        float sample = amplitude * decay * fl::sinf(2.0f * FL_M_PI * tuningHz * t);
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        data[i] = static_cast<fl::i16>(sample);
    }
    return AudioSample(data, timestamp);
}

/// Generate a synthetic crash cymbal: broadband noise with 80ms decay
/// All treble bins have similar energy (high flatness)
inline AudioSample makeCrashCymbal(fl::u32 timestamp, float amplitude = 16000.0f,
                                    int count = 512, float sampleRate = 44100.0f) {
    fl::fl_random rng(77); // Deterministic seed
    fl::vector<fl::i16> data(count, 0);
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float decay = fl::expf(-t / 0.080f);
        float noiseVal = (static_cast<float>(rng.random16()) / 32767.5f) - 1.0f;
        float sample = amplitude * decay * noiseVal;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        data[i] = static_cast<fl::i16>(sample);
    }
    return AudioSample(data, timestamp);
}

// ============================================================================
// Realistic Instrument Generators (calibrated to match real audio features)
//
// These generators produce feature distributions that match real MP3 audio
// recordings within ~30%. They serve as permanent synthetic substitutes for
// real-audio calibration files, enabling weight-locking tests that catch
// scoring parameter drift without requiring MP3 fixtures.
//
// Target feature distributions (from 3-way MP3 calibration):
//   Real backing (guitar+drums):  flat=0.63 form=0.67 pres=0.13 zcCV=1.12
//   Real voice+all (0dB):         flat=0.61 form=0.38 pres=0.03 zcCV=1.46
// ============================================================================

/// Acoustic guitar with body resonances.
/// Unlike makeGuitarStringDecay (smooth 1/h^2 rolloff), this models the guitar
/// body as three Gaussian resonances at ~300Hz, ~800Hz, ~1500Hz. These create
/// energy peaks in both F1 (250-900Hz) and F2 (1000-3000Hz) vocal formant bands,
/// producing formant ratio ~0.5-0.7 (matching real guitar recordings).
inline AudioSample makeAcousticGuitar(float f0, fl::u32 timestamp,
                                       float amplitude = 16000.0f,
                                       int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data(count, 0);
    const float maxFreq = fl::min(6000.0f, sampleRate / 2.0f);

    // Body resonance frequencies and bandwidths (Hz)
    const float bodyFreqs[] = {300.0f, 800.0f, 1500.0f};
    const float bodyBws[] = {120.0f, 200.0f, 300.0f};
    const float bodyGains[] = {1.0f, 0.6f, 0.3f};

    for (int h = 1; h * f0 < maxFreq; ++h) {
        float freq = f0 * static_cast<float>(h);
        // Base amplitude: 1/h^1.3 rolloff (gentler than string decay)
        float baseAmp = amplitude * 0.15f / fl::powf(static_cast<float>(h), 1.3f);

        // Apply body resonance envelope: sum of Gaussian peaks
        float bodyEnvelope = 0.05f; // Minimum floor so all harmonics are present
        for (int r = 0; r < 3; ++r) {
            float dist = freq - bodyFreqs[r];
            bodyEnvelope += bodyGains[r] * fl::expf(-0.5f * dist * dist
                                                     / (bodyBws[r] * bodyBws[r]));
        }

        float harmonicAmp = baseAmp * bodyEnvelope;
        for (int i = 0; i < count; ++i) {
            float phase = 2.0f * FL_M_PI * freq * static_cast<float>(i) / sampleRate;
            data[i] += static_cast<fl::i16>(harmonicAmp * fl::sinf(phase));
        }
    }
    return AudioSample(data, timestamp);
}

/// Full-band music mix: acoustic guitar + kick + snare + hi-hat + optional voice.
/// Produces feature distributions matching real 3-way MP3 recordings:
///   backing: flat~0.5-0.7 form~0.4-0.8 pres~0.05-0.2 zcCV~0.8-1.5
///   vocal:   flat~0.5-0.7 form~0.3-0.6 pres~0.01-0.05 zcCV~1.0-2.0
///
/// The guitar body resonances create realistic formant-band energy, and the
/// drum pattern produces realistic time-domain features (jitter, ACF, zcCV).
///
/// @param vocalRatio 0.0 = backing only, 1.0 = voice at equal level
inline AudioSample makeFullBandMix(float vocalRatio, fl::u32 timestamp,
                                    float amplitude = 16000.0f,
                                    int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> mixed(count, 0);

    // 1. Acoustic guitar (dominant — provides harmonic/formant structure)
    auto guitar = makeAcousticGuitar(196.0f, timestamp, amplitude * 0.7f,
                                      count, sampleRate);
    const auto& guitarPcm = guitar.pcm();
    for (int i = 0; i < count; ++i) {
        mixed[i] += guitarPcm[i];
    }

    // 2. Kick drum (low-frequency body)
    auto kick = makeKickDrum(timestamp, amplitude * 0.25f, count, sampleRate);
    const auto& kickPcm = kick.pcm();
    for (int i = 0; i < count; ++i) {
        mixed[i] = static_cast<fl::i16>(fl::clamp(
            static_cast<float>(mixed[i]) + static_cast<float>(kickPcm[i]),
            -32768.0f, 32767.0f));
    }

    // 3. Snare (mid-frequency body + noise rattle)
    auto snare = makeSnare(timestamp, amplitude * 0.15f, count, sampleRate);
    const auto& snarePcm = snare.pcm();
    for (int i = 0; i < count; ++i) {
        mixed[i] = static_cast<fl::i16>(fl::clamp(
            static_cast<float>(mixed[i]) + static_cast<float>(snarePcm[i]),
            -32768.0f, 32767.0f));
    }

    // 4. Hi-hat (high-frequency noise — reduced amplitude to avoid
    //    inflating zcCV beyond real-audio levels)
    auto hat = makeHiHat(timestamp, false, amplitude * 0.08f, count, sampleRate);
    const auto& hatPcm = hat.pcm();
    for (int i = 0; i < count; ++i) {
        mixed[i] = static_cast<fl::i16>(fl::clamp(
            static_cast<float>(mixed[i]) + static_cast<float>(hatPcm[i]),
            -32768.0f, 32767.0f));
    }

    // 5. Voice (jittered vowel if ratio > 0)
    if (vocalRatio > 0.001f) {
        auto vocal = makeJitteredVowel(150.0f, 700.0f, 1200.0f, timestamp,
                                        amplitude * vocalRatio, count, sampleRate);
        const auto& vocalPcm = vocal.pcm();
        for (int i = 0; i < count; ++i) {
            mixed[i] = static_cast<fl::i16>(fl::clamp(
                static_cast<float>(mixed[i]) + static_cast<float>(vocalPcm[i]),
                -32768.0f, 32767.0f));
        }
    }

    return AudioSample(mixed, timestamp);
}

// ============================================================================
// Degenerate-Case Signal Generators (for false-positive testing)
// ============================================================================

/// Generate a sawtooth wave using Fourier series: sum((-1)^(h+1) * sin(2*pi*h*f*t) / h)
/// Rich harmonics at 1/h amplitude but NO formant peaks (smooth spectral envelope).
/// Tests that a spectrally rich but structurally non-vocal signal is rejected.
inline AudioSample makeSawtoothWave(float freq, fl::u32 timestamp,
                                     float amplitude = 16000.0f, int count = 512,
                                     float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data(count, 0);
    const float nyquist = sampleRate / 2.0f;
    const int maxH = static_cast<int>(nyquist / freq);
    for (int h = 1; h <= maxH; ++h) {
        float sign = ((h % 2) == 0) ? -1.0f : 1.0f;
        float hAmp = amplitude * sign / static_cast<float>(h);
        float hFreq = freq * static_cast<float>(h);
        for (int i = 0; i < count; ++i) {
            float phase = 2.0f * FL_M_PI * hFreq * static_cast<float>(i) / sampleRate;
            float sample = static_cast<float>(data[i]) + hAmp * fl::sinf(phase);
            data[i] = static_cast<fl::i16>(fl::clamp(sample, -32768.0f, 32767.0f));
        }
    }
    return AudioSample(data, timestamp);
}

/// Generate an AM-modulated sine (tremolo tone):
///   amp * (1 - depth + depth*sin(2*pi*modRate*t)) * sin(2*pi*freq*t)
/// Creates envelope jitter without formant structure. Tests jitter boost false positives.
inline AudioSample makeTremoloTone(float freq, float modRate, float modDepth,
                                    fl::u32 timestamp, float amplitude = 16000.0f,
                                    int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data;
    data.reserve(count);
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float envelope = 1.0f - modDepth + modDepth * fl::sinf(2.0f * FL_M_PI * modRate * t);
        float sample = amplitude * envelope * fl::sinf(2.0f * FL_M_PI * freq * t);
        data.push_back(static_cast<fl::i16>(fl::clamp(sample, -32768.0f, 32767.0f)));
    }
    return AudioSample(data, timestamp);
}

/// Generate bandpass-filtered noise: ~40 random-phase sinusoids between lowFreq-highFreq.
/// Use makeFilteredNoise(200, 4000, ts) = speech-band noise.
inline AudioSample makeFilteredNoise(float lowFreq, float highFreq, fl::u32 timestamp,
                                      float amplitude = 16000.0f, int count = 512,
                                      float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data(count, 0);
    fl::fl_random rng(54321);
    const int numComponents = 40;
    for (int c = 0; c < numComponents; ++c) {
        float t = static_cast<float>(rng.random16()) / 65535.0f;
        float freq = lowFreq + (highFreq - lowFreq) * t;
        float phase0 = static_cast<float>(rng.random16()) / 65535.0f * 2.0f * FL_M_PI;
        float compAmp = amplitude / static_cast<float>(numComponents);
        for (int i = 0; i < count; ++i) {
            float phase = phase0 + 2.0f * FL_M_PI * freq * static_cast<float>(i) / sampleRate;
            float sample = static_cast<float>(data[i]) + compAmp * fl::sinf(phase);
            data[i] = static_cast<fl::i16>(fl::clamp(sample, -32768.0f, 32767.0f));
        }
    }
    return AudioSample(data, timestamp);
}

/// Generate a pitched tom drum: 3 harmonics at tuningHz/2x/3x with 60ms decay + noise attack.
/// Sits at male vocal F0 frequency (~150Hz) but lacks formant structure.
inline AudioSample makePitchedTom(float tuningHz, fl::u32 timestamp,
                                   float amplitude = 16000.0f, int count = 512,
                                   float sampleRate = 44100.0f) {
    fl::fl_random rng(9876);
    fl::vector<fl::i16> data(count, 0);
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float bodyDecay = fl::expf(-t / 0.060f);
        // 3 harmonics
        float body = 0.0f;
        for (int h = 1; h <= 3; ++h) {
            float hFreq = tuningHz * static_cast<float>(h);
            body += amplitude * 0.5f / static_cast<float>(h) * bodyDecay
                    * fl::sinf(2.0f * FL_M_PI * hFreq * t);
        }
        // Noise attack burst (3ms decay)
        float attackDecay = fl::expf(-t / 0.003f);
        float noise = (static_cast<float>(rng.random16()) / 32767.5f) - 1.0f;
        float attack = amplitude * 0.4f * attackDecay * noise;
        float sample = body + attack;
        data[i] = static_cast<fl::i16>(fl::clamp(sample, -32768.0f, 32767.0f));
    }
    return AudioSample(data, timestamp);
}

} // namespace test
} // namespace audio
} // namespace fl
