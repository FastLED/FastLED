// Frequency sweep tests for the dual-mode FFT system.
// Tests both LogRebin (fast, geometric grouping) and CqOctave (octave-wise CQT)
// modes to verify that each bin responds most strongly to its center frequency.

#include "test.h"
#include "fl/audio/fft/fft.h"
#include "fl/int.h"
#include "fl/log.h"
#include "fl/stl/math.h"
#include "fl/stl/vector.h"

namespace {

fl::vector<fl::i16> makeSine(float freq, int count = 512,
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

} // namespace

// CqOctave mode with wide range (64 bins, 20-11025 Hz).
// Octave-wise CQT keeps kernels well-conditioned (N_window >= N/2 per octave).
FL_TEST_CASE("CqOctave frequency sweep (64 bins 20-11025 Hz)") {
    const int bands = 64;
    const float fmin = 20.0f;
    const float fmax = 11025.0f;
    fl::FFT_Args args(512, bands, fmin, fmax, 44100, fl::FFTMode::CQ_OCTAVE);

    // Test a subset of bins across the range
    // Bins are log-spaced: bin i center = fmin * exp(log(fmax/fmin) * i / (bands-1))
    float logRatio = fl::logf(fmax / fmin);

    int correctBins = 0;
    int totalTested = 0;
    int firstBadBin = -1;

    // Test every 4th bin to keep test fast
    for (int targetBin = 0; targetBin < bands; targetBin += 4) {
        float centerFreq = fmin * fl::expf(logRatio * static_cast<float>(targetBin) /
                                            static_cast<float>(bands - 1));

        // Skip frequencies below FFT resolution (44100/512 ≈ 86 Hz)
        if (centerFreq < 86.0f) continue;

        // Skip frequencies above Nyquist
        if (centerFreq > 22050.0f) continue;

        auto samples = makeSine(centerFreq);
        fl::FFTBins bins(bands);
        fl::FFT fft;
        fft.run(samples, &bins, args);

        // Find the peak CQ bin
        int peakBin = 0;
        float peakVal = 0.0f;
        for (int i = 0; i < bands; ++i) {
            if (bins.raw()[i] > peakVal) {
                peakVal = bins.raw()[i];
                peakBin = i;
            }
        }

        totalTested++;
        int diff = peakBin - targetBin;
        if (diff < 0) diff = -diff;
        if (diff <= 3) {
            correctBins++;
        } else {
            if (firstBadBin < 0) firstBadBin = targetBin;
            FASTLED_WARN("  Bin " << targetBin << " (center="
                         << centerFreq << " Hz): peak at bin " << peakBin
                         << " (off by " << diff << "), peakVal=" << peakVal);
        }
    }

    FASTLED_WARN("CQ sweep (64 bins, 20-11025 Hz): " << correctBins << "/"
                 << totalTested << " bins correct (within +/-3)");
    if (firstBadBin >= 0) {
        float badFreq = fmin * fl::expf(logRatio * static_cast<float>(firstBadBin) /
                                         static_cast<float>(bands - 1));
        int minWindow = static_cast<int>(512.0f * fmin / badFreq);
        FASTLED_WARN("First bad bin: " << firstBadBin << " (center="
                     << badFreq << " Hz, CQ window=" << minWindow << " samples)");
    }

    // Octave-wise CQT: each octave has well-conditioned kernels (N_window
    // >= N/2), so ±3 bin tolerance at >80% correct validates good accuracy.
    float correctPct = static_cast<float>(correctBins) / static_cast<float>(totalTested);
    FL_CHECK_GT(correctPct, 0.80f);
}

// Default 16-bin config: Auto resolves to CqOctave (naive CQ, well-conditioned).
// winMin = 512 * 174.6 / 4698.3 = 19 → CQ kernels are fine.
FL_TEST_CASE("CQ frequency sweep - default config (16 bins 174.6-4698.3 Hz)") {
    const int bands = 16;
    fl::FFT_Args args; // defaults: 512, 16, 174.6, 4698.3, 44100, Auto→CqOctave(naive)

    float logRatio = fl::logf(args.fmax / args.fmin);
    int correctBins = 0;
    int totalTested = 0;

    for (int targetBin = 0; targetBin < bands; targetBin++) {
        float centerFreq = args.fmin * fl::expf(logRatio * static_cast<float>(targetBin) /
                                                  static_cast<float>(bands - 1));

        auto samples = makeSine(centerFreq);
        fl::FFTBins bins(bands);
        fl::FFT fft;
        fft.run(samples, &bins, args);

        int peakBin = 0;
        float peakVal = 0.0f;
        for (int i = 0; i < bands; ++i) {
            if (bins.raw()[i] > peakVal) {
                peakVal = bins.raw()[i];
                peakBin = i;
            }
        }

        totalTested++;
        int diff = peakBin - targetBin;
        if (diff < 0) diff = -diff;
        if (diff <= 1) {
            correctBins++;
        } else {
            FASTLED_WARN("  Bin " << targetBin << " (center="
                         << centerFreq << " Hz): peak at bin " << peakBin
                         << " (off by " << diff << ")");
        }
    }

    FASTLED_WARN("CQ sweep (16 bins, default): " << correctBins << "/"
                 << totalTested << " bins correct (within +/-1)");

    // Default config should work well
    float correctPct = static_cast<float>(correctBins) / static_cast<float>(totalTested);
    FL_CHECK_GT(correctPct, 0.90f);
}

// Equalizer config (16 bins, 60-5120 Hz): Auto resolves to CqOctave (naive CQ).
// winMin = 512 * 60 / 5120 = 6 → CQ kernels work but Q is marginal.
// With 16 widely-spaced bins, ±3 bin accuracy is acceptable.
FL_TEST_CASE("CQ frequency sweep - Equalizer config (16 bins 60-5120 Hz)") {
    const int bands = 16;
    const float fmin = 60.0f;
    const float fmax = 5120.0f;
    fl::FFT_Args args(512, bands, fmin, fmax, 44100);

    float logRatio = fl::logf(fmax / fmin);
    int correctBins = 0;
    int totalTested = 0;

    for (int targetBin = 0; targetBin < bands; targetBin++) {
        float centerFreq = fmin * fl::expf(logRatio * static_cast<float>(targetBin) /
                                            static_cast<float>(bands - 1));

        // Skip frequencies below FFT resolution
        if (centerFreq < 86.0f) continue;

        auto samples = makeSine(centerFreq);
        fl::FFTBins bins(bands);
        fl::FFT fft;
        fft.run(samples, &bins, args);

        int peakBin = 0;
        float peakVal = 0.0f;
        for (int i = 0; i < bands; ++i) {
            if (bins.raw()[i] > peakVal) {
                peakVal = bins.raw()[i];
                peakBin = i;
            }
        }

        totalTested++;
        int diff = peakBin - targetBin;
        if (diff < 0) diff = -diff;
        if (diff <= 3) {
            correctBins++;
        } else {
            float window = 512.0f * fmin / centerFreq;
            FASTLED_WARN("  Bin " << targetBin << " (center="
                         << centerFreq << " Hz, window=" << window
                         << "): peak at bin " << peakBin);
        }
    }

    FASTLED_WARN("CQ sweep (16 bins, 60-5120 Hz): " << correctBins << "/"
                 << totalTested << " bins correct (within +/-3)");
    // Q ≈ 0.70 limits accuracy. Require >50% correct; band-summing
    // detectors (like EqualizerDetector) still work at this resolution.
    float correctPct = static_cast<float>(correctBins) / static_cast<float>(totalTested);
    FL_CHECK_GT(correctPct, 0.50f);
}

// LogRebin with wide range (64 bins, 20-11025 Hz).
// Verifies that the fast log-rebin path produces correct bin ordering
// even for the wide frequency range that breaks naive CQ kernels.
FL_TEST_CASE("LogRebin frequency sweep - wide range (64 bins 20-11025 Hz)") {
    const int bands = 64;
    const float fmin = 20.0f;
    const float fmax = 11025.0f;
    fl::FFT_Args args(512, bands, fmin, fmax, 44100, fl::FFTMode::LOG_REBIN);

    float logRatio = fl::logf(fmax / fmin);
    int correctBins = 0;
    int totalTested = 0;

    for (int targetBin = 0; targetBin < bands; targetBin += 4) {
        float centerFreq = fmin * fl::expf(logRatio * (static_cast<float>(targetBin) + 0.5f) /
                                            static_cast<float>(bands));

        if (centerFreq < 86.0f) continue;
        if (centerFreq > 22050.0f) continue;

        auto samples = makeSine(centerFreq);
        fl::FFTBins bins(bands);
        fl::FFT fft;
        fft.run(samples, &bins, args);

        int peakBin = 0;
        float peakVal = 0.0f;
        for (int i = 0; i < bands; ++i) {
            if (bins.raw()[i] > peakVal) {
                peakVal = bins.raw()[i];
                peakBin = i;
            }
        }

        totalTested++;
        int diff = peakBin - targetBin;
        if (diff < 0) diff = -diff;
        if (diff <= 2) {
            correctBins++;
        } else {
            FASTLED_WARN("  LogRebin bin " << targetBin << " (center="
                         << centerFreq << " Hz): peak at bin " << peakBin
                         << " (off by " << diff << ")");
        }
    }

    FASTLED_WARN("LogRebin sweep (64 bins, 20-11025 Hz): " << correctBins << "/"
                 << totalTested << " bins correct (within +/-2)");

    float correctPct = static_cast<float>(correctBins) / static_cast<float>(totalTested);
    FL_CHECK_GT(correctPct, 0.80f);
}

// Verify that LINEAR bins always produce correct ordering (no CQ kernel issues)
FL_TEST_CASE("Linear bins frequency sweep - wide range (20-11025 Hz)") {
    const int bands = 64;
    const float fmin = 20.0f;
    const float fmax = 11025.0f;
    fl::FFT_Args args(512, bands, fmin, fmax, 44100);

    float linearBinHz = (fmax - fmin) / static_cast<float>(bands);
    int correctBins = 0;
    int totalTested = 0;

    // Test linear bins at various frequencies
    for (int targetBin = 1; targetBin < bands; targetBin += 4) {
        float centerFreq = fmin + (static_cast<float>(targetBin) + 0.5f) * linearBinHz;

        // Skip frequencies below FFT resolution
        if (centerFreq < 86.0f) continue;

        auto samples = makeSine(centerFreq);
        fl::FFTBins bins(bands);
        fl::FFT fft;
        fft.run(samples, &bins, args);

        // Find peak in LINEAR bins
        fl::span<const float> linear = bins.linear();
        int peakBin = 0;
        float peakVal = 0.0f;
        for (int i = 0; i < bands; ++i) {
            if (linear[i] > peakVal) {
                peakVal = linear[i];
                peakBin = i;
            }
        }

        totalTested++;
        int diff = peakBin - targetBin;
        if (diff < 0) diff = -diff;
        if (diff <= 1) {
            correctBins++;
        } else {
            FASTLED_WARN("  Linear bin " << targetBin << " (center="
                         << centerFreq << " Hz): peak at bin " << peakBin);
        }
    }

    FASTLED_WARN("Linear sweep (64 bins, 20-11025 Hz): " << correctBins << "/"
                 << totalTested << " bins correct (within +/-1)");

    // Linear bins should always work correctly
    float correctPct = static_cast<float>(correctBins) / static_cast<float>(totalTested);
    FL_CHECK_GT(correctPct, 0.90f);
}
