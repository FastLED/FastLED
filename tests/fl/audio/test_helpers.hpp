/// @file test_helpers.h
/// @brief Shared helper functions for audio tests
/// Common utilities for creating test signals and AudioSample objects

#pragma once

#include "fl/audio.h"
#include "fl/int.h"
#include "fl/math_macros.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"

namespace fl {
namespace test {

/// Create an AudioSample from a vector of samples
inline AudioSample createSample(const vector<i16>& samples, u32 timestamp = 0) {
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), timestamp);
    return AudioSample(impl);
}

/// Create an AudioSample from a span of samples
inline AudioSample createSample(span<const i16> samples, u32 timestamp = 0) {
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), timestamp);
    return AudioSample(impl);
}

/// Generate a sine wave signal
inline vector<i16> generateSineWave(size count, float frequency, float sampleRate, i16 amplitude) {
    vector<i16> samples;
    samples.reserve(count);
    for (size i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * frequency * static_cast<float>(i) / sampleRate;
        i16 sample = static_cast<i16>(amplitude * fl::sinf(phase));
        samples.push_back(sample);
    }
    return samples;
}

/// Alias for generateSineWave (some tests use "generateTone")
inline vector<i16> generateTone(size count, float frequency, float sampleRate, i16 amplitude) {
    return generateSineWave(count, frequency, sampleRate, amplitude);
}

/// Generate a sine wave and create an AudioSample in one step
inline AudioSample makeSample(float frequency, u32 timestamp,
                               float amplitude = 16000.0f,
                               size count = 512,
                               float sampleRate = 44100.0f) {
    vector<i16> data = generateSineWave(count, frequency, sampleRate,
                                        static_cast<i16>(amplitude));
    return createSample(data, timestamp);
}

/// Generate a constant amplitude signal (for level testing)
inline AudioSample makeSample(i16 amplitude, u32 timestamp, size count = 512) {
    vector<i16> data(count, amplitude);
    return createSample(data, timestamp);
}

/// Generate silence
inline AudioSample makeSilence(u32 timestamp, size count = 512) {
    return makeSample(static_cast<i16>(0), timestamp, count);
}

/// Generate white noise
inline vector<i16> generateNoise(size count, i16 amplitude) {
    vector<i16> samples;
    samples.reserve(count);
    for (size i = 0; i < count; ++i) {
        // Simple pseudo-random noise (not cryptographically secure)
        i32 noise = ((i * 1103515245 + 12345) >> 16) & 0x7FFF;
        noise = (noise * amplitude) / 0x7FFF;
        samples.push_back(static_cast<i16>(noise));
    }
    return samples;
}

} // namespace test
} // namespace fl
