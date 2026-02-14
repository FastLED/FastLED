// Unit tests for AudioProcessor

#include "test.h"
#include "FastLED.h"
#include "fl/audio.h"
#include "fl/fx/audio/audio_processor.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"

using namespace fl;

namespace { // audio_processor

AudioSample makeSample(float freq, fl::u32 timestamp, float amplitude = 16000.0f) {
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * FL_M_PI * freq * i / 44100.0f;
        data.push_back(static_cast<fl::i16>(amplitude * fl::sinf(phase)));
    }
    return AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp);
}

} // anonymous namespace

FL_TEST_CASE("AudioProcessor - update with valid sample doesn't crash") {
    AudioProcessor processor;
    AudioSample sample = makeSample(440.0f, 1000);
    processor.update(sample);
    // After update, the processor should have a valid context
    FL_CHECK(processor.getContext() != nullptr);
}

FL_TEST_CASE("AudioProcessor - setSampleRate / getSampleRate round-trip") {
    AudioProcessor processor;
    FL_CHECK_EQ(processor.getSampleRate(), 44100);
    processor.setSampleRate(22050);
    FL_CHECK_EQ(processor.getSampleRate(), 22050);
}

FL_TEST_CASE("AudioProcessor - onEnergy callback fires") {
    AudioProcessor processor;
    float lastRMS = -1.0f;
    processor.onEnergy([&lastRMS](float rms) { lastRMS = rms; });

    AudioSample sample = makeSample(440.0f, 1000, 10000.0f);
    processor.update(sample);

    FL_CHECK_GT(lastRMS, 0.0f);
}

FL_TEST_CASE("AudioProcessor - onBass/onMid/onTreble callbacks fire") {
    AudioProcessor processor;
    float lastBass = -1.0f;
    float lastMid = -1.0f;
    float lastTreble = -1.0f;
    processor.onBass([&lastBass](float level) { lastBass = level; });
    processor.onMid([&lastMid](float level) { lastMid = level; });
    processor.onTreble([&lastTreble](float level) { lastTreble = level; });

    // Feed a multi-frequency signal
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float t = static_cast<float>(i) / 44100.0f;
        float bass = 5000.0f * fl::sinf(2.0f * FL_M_PI * 200.0f * t);
        float mid = 5000.0f * fl::sinf(2.0f * FL_M_PI * 1000.0f * t);
        float treble = 5000.0f * fl::sinf(2.0f * FL_M_PI * 4000.0f * t);
        fl::i32 combined = static_cast<fl::i32>(bass + mid + treble);
        if (combined > 32767) combined = 32767;
        if (combined < -32768) combined = -32768;
        data.push_back(static_cast<fl::i16>(combined));
    }
    AudioSample sample(fl::span<const fl::i16>(data.data(), data.size()), 1000);
    processor.update(sample);

    // The signal contains explicit 200Hz bass + 1000Hz mid + 4000Hz treble energy.
    // All three bands should report positive energy individually.
    FL_CHECK_GT(lastBass, 0.0f);
    FL_CHECK_GT(lastMid, 0.0f);
    FL_CHECK_GT(lastTreble, 0.0f);
}

FL_TEST_CASE("AudioProcessor - signal conditioning enabled by default") {
    AudioProcessor processor;
    // Signal conditioning is enabled by default
    const auto& stats = processor.getSignalConditionerStats();
    FL_CHECK_EQ(stats.samplesProcessed, 0u);

    AudioSample sample = makeSample(440.0f, 1000);
    processor.update(sample);

    FL_CHECK_GT(processor.getSignalConditionerStats().samplesProcessed, 0u);
}

FL_TEST_CASE("AudioProcessor - reset clears all state") {
    AudioProcessor processor;
    float lastRMS = -1.0f;
    processor.onEnergy([&lastRMS](float rms) { lastRMS = rms; });

    AudioSample sample = makeSample(440.0f, 1000);
    processor.update(sample);
    FL_CHECK_GT(lastRMS, 0.0f);

    processor.reset();
    // After reset, stats should be cleared
    FL_CHECK_EQ(processor.getSignalConditionerStats().samplesProcessed, 0u);
}

FL_TEST_CASE("AudioProcessor - lazy detector creation") {
    AudioProcessor processor;
    // Without callbacks registered, just update() should work fine
    AudioSample sample = makeSample(440.0f, 1000);
    processor.update(sample);

    // Now register a callback - should trigger detector creation
    int beatCount = 0;
    processor.onBeat([&beatCount]() { beatCount++; });

    processor.update(sample);
    // After registering onBeat and updating, context should still be valid
    FL_CHECK(processor.getContext() != nullptr);
}

FL_TEST_CASE("AudioProcessor - onBeat callback fires with periodic bass") {
    AudioProcessor processor;
    int beatCount = 0;
    processor.onBeat([&beatCount]() { beatCount++; });

    // Feed silence to establish baseline
    fl::vector<fl::i16> silenceData(512, 0);
    for (int i = 0; i < 20; ++i) {
        AudioSample silence(fl::span<const fl::i16>(silenceData.data(), silenceData.size()), i * 23);
        processor.update(silence);
    }

    // Feed periodic bass bursts at ~120 BPM
    fl::u32 timestamp = 500;
    for (int beat = 0; beat < 12; ++beat) {
        // Bass burst
        fl::vector<fl::i16> bassData;
        bassData.reserve(512);
        for (int s = 0; s < 512; ++s) {
            float phase = 2.0f * FL_M_PI * 200.0f * s / 44100.0f;
            bassData.push_back(static_cast<fl::i16>(20000.0f * fl::sinf(phase)));
        }
        AudioSample bassSample(fl::span<const fl::i16>(bassData.data(), bassData.size()), timestamp);
        processor.update(bassSample);

        // Silence between beats
        for (int frame = 1; frame < 22; ++frame) {
            timestamp += 23;
            AudioSample silence(fl::span<const fl::i16>(silenceData.data(), silenceData.size()), timestamp);
            processor.update(silence);
        }
        timestamp += 23;
    }

    // At least some beats should have been detected
    FL_CHECK_GT(beatCount, 2);
}
