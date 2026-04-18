// Unit tests for audio::Context

#include "fl/audio/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/fft/fft.h"
#include "fl/stl/int.h"
#include "fl/math/math.h"
#include "fl/math/math.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"

using namespace fl;

namespace {

static audio::Sample makeSineAudioSample(float freq, fl::u32 timestamp) {
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * FL_M_PI * freq * i / 44100.0f;
        data.push_back(static_cast<fl::i16>(16000.0f * fl::sinf(phase)));
    }
    return audio::Sample(data, timestamp);
}

} // anonymous namespace

FL_TEST_CASE("audio::Context - basic accessors") {
    audio::Sample sample = makeSineAudioSample(440.0f, 5000);
    audio::Context ctx(sample);
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

FL_TEST_CASE("audio::Context - lazy audio::fft::FFT computation") {
    audio::Sample sample = makeSineAudioSample(1000.0f, 1000);
    audio::Context ctx(sample);
    FL_CHECK_FALSE(ctx.hasFFT());
    auto binsPtr = ctx.getFFT(16);
    const audio::fft::Bins& bins = *binsPtr;
    FL_CHECK(ctx.hasFFT());
    FL_CHECK_GT(bins.raw().size(), 0u);

    // The input is a 1kHz sine wave with amplitude 16000 (Q15 scale).
    // The audio::fft::FFT should contain meaningful energy, not just zeros.
    float maxBin = 0.0f;
    for (fl::size i = 0; i < bins.raw().size(); i++) {
        if (bins.raw()[i] > maxBin) {
            maxBin = bins.raw()[i];
        }
    }
    // With a strong 1kHz sine input, the peak audio::fft::FFT bin should have
    // significant energy (well above noise floor)
    FL_CHECK_GT(maxBin, 100.0f);
}

FL_TEST_CASE("audio::Context - audio::fft::FFT history") {
    audio::Sample sample1 = makeSineAudioSample(440.0f, 1000);
    audio::Context ctx(sample1);
    auto fft1 = ctx.getFFT(16);  // Retain shared_ptr so weak cache is alive for setSample

    // Initialize history tracking BEFORE calling setSample
    ctx.setFFTHistoryDepth(4);
    FL_CHECK(ctx.hasFFTHistory());

    // setSample pushes the current audio::fft::FFT into history (locks weak_ptr)
    audio::Sample sample2 = makeSineAudioSample(880.0f, 2000);
    ctx.setSample(sample2);
    auto fft2 = ctx.getFFT(16);  // Retain for next setSample

    audio::Sample sample3 = makeSineAudioSample(1200.0f, 3000);
    ctx.setSample(sample3);
    ctx.getFFT(16);  // Compute audio::fft::FFT for third sample

    const auto& history = ctx.getFFTHistory();
    FL_CHECK_EQ(history.size(), 2u);  // Two FFTs were pushed via setSample
}

FL_TEST_CASE("audio::Context - getHistoricalFFT") {
    audio::Sample sample1 = makeSineAudioSample(440.0f, 1000);
    audio::Context ctx(sample1);
    auto fft1 = ctx.getFFT(16);  // Retain so setSample can lock weak_ptr

    // Initialize history tracking before pushing samples
    ctx.setFFTHistoryDepth(4);

    // Push sample1's audio::fft::FFT into history via setSample
    audio::Sample sample2 = makeSineAudioSample(880.0f, 2000);
    ctx.setSample(sample2);
    ctx.getFFT(16);

    // Index 0 = most recent in history (sample1's audio::fft::FFT)
    const audio::fft::Bins* recent = ctx.getHistoricalFFT(0);
    FL_CHECK(recent != nullptr);

    // Index 1 = no second entry yet, should be null
    const audio::fft::Bins* prev = ctx.getHistoricalFFT(1);
    FL_CHECK(prev == nullptr);
}

FL_TEST_CASE("audio::Context - clearCache resets audio::fft::FFT state") {
    audio::Sample sample = makeSineAudioSample(440.0f, 1000);
    audio::Context ctx(sample);
    ctx.getFFT(16);
    FL_CHECK(ctx.hasFFT());
    ctx.clearCache();
    FL_CHECK_FALSE(ctx.hasFFT());
}

FL_TEST_CASE("audio::Context - setSampleRate round-trip") {
    audio::Sample sample = makeSineAudioSample(440.0f, 1000);
    audio::Context ctx(sample);
    FL_CHECK_EQ(ctx.getSampleRate(), 44100);
    ctx.setSampleRate(22050);
    FL_CHECK_EQ(ctx.getSampleRate(), 22050);
}

FL_TEST_CASE("audio::Context - setSample updates state") {
    audio::Sample sample1 = makeSineAudioSample(440.0f, 1000);
    audio::Context ctx(sample1);
    FL_CHECK_EQ(ctx.getTimestamp(), 1000u);

    audio::Sample sample2 = makeSineAudioSample(880.0f, 2000);
    ctx.setSample(sample2);
    FL_CHECK_EQ(ctx.getTimestamp(), 2000u);
}

FL_TEST_CASE("audio::Context - new sample produces fresh audio::fft::FFT data") {
    audio::Sample sample1 = makeSineAudioSample(440.0f, 1000);
    audio::Context ctx(sample1);

    // Get audio::fft::FFT for first sample, capture a bin value, then release
    float firstMaxBin;
    {
        auto fft1 = ctx.getFFT(16);
        firstMaxBin = 0.0f;
        for (fl::size i = 0; i < fft1->raw().size(); i++) {
            if (fft1->raw()[i] > firstMaxBin) firstMaxBin = fft1->raw()[i];
        }
    }

    // New sample with different frequency
    audio::Sample sample2 = makeSineAudioSample(2000.0f, 2000);
    ctx.setSample(sample2);

    // This should recycle the bins but populate with NEW data
    auto fft2 = ctx.getFFT(16);
    FL_CHECK_GT(fft2->raw().size(), 0u);

    // Verify it has valid data (non-zero energy)
    float secondMaxBin = 0.0f;
    for (fl::size i = 0; i < fft2->raw().size(); i++) {
        if (fft2->raw()[i] > secondMaxBin) secondMaxBin = fft2->raw()[i];
    }
    FL_CHECK_GT(secondMaxBin, 100.0f);
}

FL_TEST_CASE("audio::Context - silence flag") {
    audio::Sample sample = makeSineAudioSample(440.0f, 1000);
    audio::Context ctx(sample);

    // Default: no silence claim until pipeline populates the flag.
    FL_CHECK_FALSE(ctx.isSilent());

    // Pipeline owner (Processor/Reactive) sets it after NFT update.
    ctx.setSilent(true);
    FL_CHECK(ctx.isSilent());

    // Next frame — setSample() must reset the flag so a stale claim can't
    // survive a frame where NFT is disabled or the pipeline forgot to populate.
    audio::Sample sample2 = makeSineAudioSample(880.0f, 2000);
    ctx.setSample(sample2);
    FL_CHECK_FALSE(ctx.isSilent());

    // And re-populating works after setSample.
    ctx.setSilent(true);
    FL_CHECK(ctx.isSilent());
}
