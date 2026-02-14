// Unit tests for AudioContext

#include "test.h"
#include "FastLED.h"
#include "fl/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/fft.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/math_macros.h"

using namespace fl;

namespace { // audio_context

AudioSample makeSineAudioSample(float freq, fl::u32 timestamp) {
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * FL_M_PI * freq * i / 44100.0f;
        data.push_back(static_cast<fl::i16>(16000.0f * fl::sinf(phase)));
    }
    return AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp);
}

} // anonymous namespace

FL_TEST_CASE("AudioContext - basic accessors") {
    AudioSample sample = makeSineAudioSample(440.0f, 5000);
    AudioContext ctx(sample);
    FL_CHECK_EQ(ctx.getTimestamp(), 5000u);
    FL_CHECK_GT(ctx.getRMS(), 0.0f);
    // A 440Hz sine at 44100 Hz has ~20 zero crossings per 512 samples.
    // ZCF = crossings / samples ≈ 0.04. Verify it's in a meaningful range.
    float zcf = ctx.getZCF();
    FL_CHECK_GT(zcf, 0.01f);
    FL_CHECK_LT(zcf, 0.15f);
    FL_CHECK_EQ(ctx.getPCM().size(), 512u);
    FL_CHECK_EQ(ctx.getSampleRate(), 44100);
}

FL_TEST_CASE("AudioContext - lazy FFT computation") {
    AudioSample sample = makeSineAudioSample(1000.0f, 1000);
    AudioContext ctx(sample);
    FL_CHECK_FALSE(ctx.hasFFT());
    const FFTBins& bins = ctx.getFFT(16);
    FL_CHECK(ctx.hasFFT());
    FL_CHECK_GT(bins.bins_raw.size(), 0u);

    // The input is a 1kHz sine wave with amplitude 16000 (Q15 scale).
    // The FFT should contain meaningful energy, not just zeros.
    float maxBin = 0.0f;
    for (fl::size i = 0; i < bins.bins_raw.size(); i++) {
        if (bins.bins_raw[i] > maxBin) {
            maxBin = bins.bins_raw[i];
        }
    }
    // With a strong 1kHz sine input, the peak FFT bin should have
    // significant energy (well above noise floor)
    FL_CHECK_GT(maxBin, 100.0f);
}

FL_TEST_CASE("AudioContext - FFT history") {
    AudioSample sample1 = makeSineAudioSample(440.0f, 1000);
    AudioContext ctx(sample1);
    ctx.getFFT(16);  // Compute FFT for first sample

    // Initialize history tracking BEFORE calling setSample
    ctx.getFFTHistory(4);
    FL_CHECK(ctx.hasFFTHistory());

    // setSample pushes the current FFT into history
    AudioSample sample2 = makeSineAudioSample(880.0f, 2000);
    ctx.setSample(sample2);
    ctx.getFFT(16);  // Compute FFT for second sample

    AudioSample sample3 = makeSineAudioSample(1200.0f, 3000);
    ctx.setSample(sample3);
    ctx.getFFT(16);  // Compute FFT for third sample

    const auto& history = ctx.getFFTHistory(4);
    FL_CHECK_EQ(history.size(), 2u);  // Two FFTs were pushed via setSample
}

FL_TEST_CASE("AudioContext - getHistoricalFFT") {
    AudioSample sample1 = makeSineAudioSample(440.0f, 1000);
    AudioContext ctx(sample1);
    ctx.getFFT(16);

    // Initialize history tracking before pushing samples
    ctx.getFFTHistory(4);

    // Push sample1's FFT into history via setSample
    AudioSample sample2 = makeSineAudioSample(880.0f, 2000);
    ctx.setSample(sample2);
    ctx.getFFT(16);

    // Index 0 = most recent in history (sample1's FFT)
    const FFTBins* recent = ctx.getHistoricalFFT(0);
    FL_CHECK(recent != nullptr);

    // Index 1 = no second entry yet, should be null
    const FFTBins* prev = ctx.getHistoricalFFT(1);
    FL_CHECK(prev == nullptr);
}

FL_TEST_CASE("AudioContext - clearCache resets FFT state") {
    AudioSample sample = makeSineAudioSample(440.0f, 1000);
    AudioContext ctx(sample);
    ctx.getFFT(16);
    FL_CHECK(ctx.hasFFT());
    ctx.clearCache();
    FL_CHECK_FALSE(ctx.hasFFT());
}

FL_TEST_CASE("AudioContext - setSampleRate round-trip") {
    AudioSample sample = makeSineAudioSample(440.0f, 1000);
    AudioContext ctx(sample);
    FL_CHECK_EQ(ctx.getSampleRate(), 44100);
    ctx.setSampleRate(22050);
    FL_CHECK_EQ(ctx.getSampleRate(), 22050);
}

FL_TEST_CASE("AudioContext - setSample updates state") {
    AudioSample sample1 = makeSineAudioSample(440.0f, 1000);
    AudioContext ctx(sample1);
    FL_CHECK_EQ(ctx.getTimestamp(), 1000u);

    AudioSample sample2 = makeSineAudioSample(880.0f, 2000);
    ctx.setSample(sample2);
    FL_CHECK_EQ(ctx.getTimestamp(), 2000u);
}
