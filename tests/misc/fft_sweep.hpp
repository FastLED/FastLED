// Frequency sweep tests for the dual-mode FFT system.
// Tests both LogRebin (fast, geometric grouping) and CqOctave (octave-wise CQT)
// modes to verify that each bin responds most strongly to its center frequency.
//
// Also includes continuous sweep diagnostics for issue #2193:
//   - Spectral leakage (energy scattered across many bins)
//   - Jitter (peak bin oscillating back and forth)
//   - Aliasing (energy appearing in wrong bins)
//   - Non-monotonic peak progression
//   - Energy concentration degradation
//   - Bin 0 anomaly
//
// Reference: https://github.com/FastLED/FastLED/issues/2193#issuecomment-4029322668

// Note: fl/audio/fft/fft_impl.h must be included BEFORE FL_TEST_FILE scope
// (in the .cpp file) because it uses unqualified fl:: types that would resolve
// to test::fl:: inside the test namespace.
#include "test.h"
#include "fl/audio/fft/fft.h"
#include "fl/stl/int.h"
#include "fl/log.h"
#include "fl/stl/math.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/warn.h"

namespace {

using namespace ::fl;

::fl::vector<::fl::i16> makeSine(float freq, int count = 512,
                              float sampleRate = 44100.0f,
                              float amplitude = 16000.0f) {
    ::fl::vector<::fl::i16> samples;
    samples.reserve(count);
    for (int i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * freq * i / sampleRate;
        samples.push_back(static_cast<::fl::i16>(amplitude * ::fl::sinf(phase)));
    }
    return samples;
}

// --- Continuous sweep infrastructure for issue #2193 ---

struct SweepFrame {
    float frequency;     // Input frequency (Hz)
    int peakBin;         // Bin with highest energy
    float peakEnergy;    // Energy in the peak bin
    float totalEnergy;   // Sum of all bin energy
    float concentration; // peakEnergy / totalEnergy
    int activeBins;      // Number of bins with >10% of peak energy
    int expectedBin;     // Bin that freqToBin() says this frequency maps to
};

// Run a full logarithmic frequency sweep and collect per-frame diagnostics.
fl::vector<SweepFrame> runContinuousSweep(int numFrames, int bands, float fmin,
                                          float fmax, int sampleRate,
                                          int samplesPerFrame,
                                          fl::FFTMode mode) {
    fl::FFT_Args args(samplesPerFrame, bands, fmin, fmax, sampleRate, mode);
    fl::FFTImpl fft(args);

    fl::vector<SweepFrame> frames;
    frames.reserve(numFrames);

    float logRatio = fl::logf(fmax / fmin);

    for (int f = 0; f < numFrames; ++f) {
        float t = static_cast<float>(f) / static_cast<float>(numFrames - 1);
        float freq = fmin * fl::expf(logRatio * t);

        auto samples = makeSine(freq, samplesPerFrame, static_cast<float>(sampleRate));
        fl::FFTBins bins(bands);
        fft.run(samples, &bins);

        SweepFrame frame;
        frame.frequency = freq;
        frame.peakBin = 0;
        frame.peakEnergy = 0.0f;
        frame.totalEnergy = 0.0f;
        frame.activeBins = 0;

        for (int i = 0; i < bands; ++i) {
            float e = bins.raw()[i];
            frame.totalEnergy += e;
            if (e > frame.peakEnergy) {
                frame.peakEnergy = e;
                frame.peakBin = i;
            }
        }

        float threshold = frame.peakEnergy * 0.1f;
        for (int i = 0; i < bands; ++i) {
            if (bins.raw()[i] > threshold) {
                frame.activeBins++;
            }
        }

        frame.concentration = (frame.totalEnergy > 0.0f)
                                  ? frame.peakEnergy / frame.totalEnergy
                                  : 0.0f;
        frame.expectedBin = bins.freqToBin(freq);
        frames.push_back(frame);
    }

    return frames;
}

int countNonMonotonicSteps(fl::span<const SweepFrame> frames) {
    int count = 0;
    for (fl::size i = 1; i < frames.size(); ++i) {
        if (frames[i].peakBin < frames[i - 1].peakBin) {
            count++;
        }
    }
    return count;
}

int countJitterEvents(fl::span<const SweepFrame> frames) {
    int count = 0;
    for (fl::size i = 2; i < frames.size(); ++i) {
        if (frames[i].peakBin == frames[i - 2].peakBin &&
            frames[i].peakBin != frames[i - 1].peakBin) {
            count++;
        }
    }
    return count;
}

int countMisplacedPeaks(fl::span<const SweepFrame> frames) {
    int count = 0;
    for (fl::size i = 0; i < frames.size(); ++i) {
        int diff = frames[i].peakBin - frames[i].expectedBin;
        if (diff < 0) diff = -diff;
        if (diff > 1) {
            count++;
        }
    }
    return count;
}

float averageConcentration(fl::span<const SweepFrame> frames) {
    float sum = 0.0f;
    for (fl::size i = 0; i < frames.size(); ++i) {
        sum += frames[i].concentration;
    }
    return sum / static_cast<float>(frames.size());
}

float averageActiveBins(fl::span<const SweepFrame> frames) {
    float sum = 0.0f;
    for (fl::size i = 0; i < frames.size(); ++i) {
        sum += static_cast<float>(frames[i].activeBins);
    }
    return sum / static_cast<float>(frames.size());
}

float worstConcentration(fl::span<const SweepFrame> frames) {
    float worst = 1.0f;
    for (fl::size i = 0; i < frames.size(); ++i) {
        if (frames[i].concentration < worst) {
            worst = frames[i].concentration;
        }
    }
    return worst;
}

int maxActiveBins(fl::span<const SweepFrame> frames) {
    int maxBins = 0;
    for (fl::size i = 0; i < frames.size(); ++i) {
        if (frames[i].activeBins > maxBins) {
            maxBins = frames[i].activeBins;
        }
    }
    return maxBins;
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

// ============================================================================
// Continuous sweep diagnostics for issue #2193
// These tests simulate real-time audio sweep behavior and detect the
// jitter, aliasing, and spectral leakage visible in the user's video.
// ============================================================================

// Main diagnostic: 862-frame sweep (~10s audio) through LOG_REBIN 16 bins.
// This is the core reproduction of the jitter reported in issue #2193.
FL_TEST_CASE("FFT sweep - LOG_REBIN 16 bins continuous diagnostic") {
    const int bands = 16;
    const float fmin = fl::FFT_Args::DefaultMinFrequency();
    const float fmax = fl::FFT_Args::DefaultMaxFrequency();
    const int sampleRate = fl::FFT_Args::DefaultSampleRate();
    const int samplesPerFrame = fl::FFT_Args::DefaultSamples();

    // ~10 seconds of audio at 512 samples/frame = 862 frames
    const int numFrames = 862;

    auto frames = runContinuousSweep(numFrames, bands, fmin, fmax, sampleRate,
                                     samplesPerFrame, fl::FFTMode::LOG_REBIN);
    FL_REQUIRE_EQ(static_cast<int>(frames.size()), numFrames);

    int nonMonotonic = countNonMonotonicSteps(frames);
    int jitterCount = countJitterEvents(frames);
    int misplaced = countMisplacedPeaks(frames);
    float avgConc = averageConcentration(frames);
    float avgActive = averageActiveBins(frames);
    float worstConc = worstConcentration(frames);
    int maxActive = maxActiveBins(frames);

    FASTLED_WARN("=== FFT Sweep Diagnostic (LOG_REBIN, " << bands << " bins, "
                 << numFrames << " frames) ===");
    FASTLED_WARN("Non-monotonic steps: " << nonMonotonic << " / "
                 << (numFrames - 1));
    FASTLED_WARN("Jitter events (A->B->A): " << jitterCount);
    FASTLED_WARN("Misplaced peaks (>1 bin off): " << misplaced << " / "
                 << numFrames);
    FASTLED_WARN("Avg energy concentration: " << avgConc);
    FASTLED_WARN("Worst energy concentration: " << worstConc);
    FASTLED_WARN("Avg active bins per frame: " << avgActive);
    FASTLED_WARN("Max active bins in any frame: " << maxActive);

    // Print sample frames for inspection
    int sampleIndices[] = {0,
                           numFrames / 8,
                           numFrames / 4,
                           numFrames / 2,
                           3 * numFrames / 4,
                           numFrames - 1};
    for (int idx : sampleIndices) {
        const auto &fr = frames[idx];
        FASTLED_WARN("  Frame " << idx << ": freq=" << fr.frequency
                     << "Hz peak_bin=" << fr.peakBin
                     << " expected=" << fr.expectedBin
                     << " conc=" << fr.concentration
                     << " active=" << fr.activeBins);
    }

    // --- Assertions for correct behavior ---

    float nonMonotonicRate =
        static_cast<float>(nonMonotonic) / static_cast<float>(numFrames - 1);
    FASTLED_WARN("Non-monotonic rate: " << nonMonotonicRate);
    FL_CHECK_LT(nonMonotonicRate, 0.05f);

    float jitterRate =
        static_cast<float>(jitterCount) / static_cast<float>(numFrames - 2);
    FASTLED_WARN("Jitter rate: " << jitterRate);
    FL_CHECK_LT(jitterRate, 0.03f);

    float misplacedRate =
        static_cast<float>(misplaced) / static_cast<float>(numFrames);
    FASTLED_WARN("Misplaced peak rate: " << misplacedRate);
    FL_CHECK_LT(misplacedRate, 0.05f);

    FL_CHECK_GT(avgConc, 0.40f);
    FL_CHECK_GT(worstConc, 0.15f);
    FL_CHECK_LT(avgActive, 5.0f);
    // Hanning window reduces spectral leakage — max active bins should be ≤8.
    FL_CHECK_LE(maxActive, 8);
}

// Focused test: low-frequency region where jitter is most visible.
// With 512 samples at 44100 Hz, FFT bin width = 86 Hz.
// Log-spaced output bins at low freq are ~40-60 Hz wide — narrower than
// an FFT bin — causing quantization jitter.
FL_TEST_CASE("FFT sweep - low frequency jitter detection") {
    const int bands = 16;
    const float fmin = fl::FFT_Args::DefaultMinFrequency();
    const float fmax = fl::FFT_Args::DefaultMaxFrequency();
    const int sampleRate = fl::FFT_Args::DefaultSampleRate();
    const int samplesPerFrame = fl::FFT_Args::DefaultSamples();

    // Sweep just the bottom bins (174-350 Hz) with fine resolution
    const float lowFmax = 350.0f;
    const int numFrames = 200;

    fl::FFT_Args args(samplesPerFrame, bands, fmin, fmax, sampleRate,
                      fl::FFTMode::LOG_REBIN);
    fl::FFTImpl fft(args);

    int peakBinChanges = 0;
    int lastPeakBin = -1;
    int maxConsecutiveSameBin = 0;
    int currentStreak = 0;

    for (int f = 0; f < numFrames; ++f) {
        float t = static_cast<float>(f) / static_cast<float>(numFrames - 1);
        float freq = fmin + (lowFmax - fmin) * t;

        auto samples =
            makeSine(freq, samplesPerFrame, static_cast<float>(sampleRate));
        fl::FFTBins bins(bands);
        fft.run(samples, &bins);

        int peakBin = 0;
        float peakVal = 0.0f;
        for (int i = 0; i < bands; ++i) {
            if (bins.raw()[i] > peakVal) {
                peakVal = bins.raw()[i];
                peakBin = i;
            }
        }

        if (lastPeakBin >= 0) {
            if (peakBin != lastPeakBin) {
                peakBinChanges++;
                if (currentStreak > maxConsecutiveSameBin) {
                    maxConsecutiveSameBin = currentStreak;
                }
                currentStreak = 1;
            } else {
                currentStreak++;
            }
        } else {
            currentStreak = 1;
        }
        lastPeakBin = peakBin;
    }
    if (currentStreak > maxConsecutiveSameBin) {
        maxConsecutiveSameBin = currentStreak;
    }

    float changeRate =
        static_cast<float>(peakBinChanges) / static_cast<float>(numFrames - 1);
    FASTLED_WARN("Low-freq sweep (174-350Hz): peak bin changed "
                 << peakBinChanges << " times in " << numFrames
                 << " frames (rate=" << changeRate << ")");
    FASTLED_WARN("Max consecutive frames on same bin: "
                 << maxConsecutiveSameBin);

    // Over ~2 output bins, expect ~1-2 clean transitions.
    // More than 10 transitions = oscillation/jitter.
    FL_CHECK_LT(peakBinChanges, 10);
}

// Energy leakage test: a pure tone should concentrate energy in 1-3 bins.
// Without windowing, spectral leakage spreads energy across many bins.
FL_TEST_CASE("FFT sweep - single tone leakage LOG_REBIN") {
    const int bands = 16;
    const int sampleRate = fl::FFT_Args::DefaultSampleRate();
    const int samplesPerFrame = fl::FFT_Args::DefaultSamples();

    float testFreqs[] = {200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f};

    for (float freq : testFreqs) {
        fl::FFT_Args args(samplesPerFrame, bands,
                          fl::FFT_Args::DefaultMinFrequency(),
                          fl::FFT_Args::DefaultMaxFrequency(), sampleRate,
                          fl::FFTMode::LOG_REBIN);
        fl::FFTImpl fft(args);

        auto samples =
            makeSine(freq, samplesPerFrame, static_cast<float>(sampleRate));
        fl::FFTBins bins(bands);
        fft.run(samples, &bins);

        float peakEnergy = 0.0f;
        int peakBin = 0;
        float totalEnergy = 0.0f;
        for (int i = 0; i < bands; ++i) {
            float e = bins.raw()[i];
            totalEnergy += e;
            if (e > peakEnergy) {
                peakEnergy = e;
                peakBin = i;
            }
        }

        float concentration = peakEnergy / totalEnergy;
        int expectedBin = bins.freqToBin(freq);
        int binError = peakBin - expectedBin;
        if (binError < 0) binError = -binError;

        int significantBins = 0;
        for (int i = 0; i < bands; ++i) {
            if (bins.raw()[i] > peakEnergy * 0.05f) {
                significantBins++;
            }
        }

        FASTLED_WARN("Tone " << freq << "Hz: peak_bin=" << peakBin
                     << " expected=" << expectedBin << " conc=" << concentration
                     << " significant_bins=" << significantBins);

        // Hanning window reduces spectral leakage: expect ≤5 significant bins
        // and ≥40% energy concentration for a pure tone.
        FL_CHECK_LE(significantBins, 5);
        FL_CHECK_LE(binError, 1);
        FL_CHECK_GT(concentration, 0.40f);
    }
}

// Bin 0 anomaly test: user reported "Bin 0 of FastLED looks off".
// Tests that bin 0 responds correctly to its own frequency and
// doesn't leak energy from distant frequencies.
FL_TEST_CASE("FFT sweep - bin 0 anomaly") {
    const int bands = 16;
    const float fmin = fl::FFT_Args::DefaultMinFrequency();
    const float fmax = fl::FFT_Args::DefaultMaxFrequency();
    const int sampleRate = fl::FFT_Args::DefaultSampleRate();
    const int samplesPerFrame = fl::FFT_Args::DefaultSamples();

    fl::FFT_Args args(samplesPerFrame, bands, fmin, fmax, sampleRate,
                      fl::FFTMode::LOG_REBIN);
    fl::FFTImpl fft(args);

    // Tone at the center of bin 0
    float bin0CenterFreq =
        fmin * fl::expf(fl::logf(fmax / fmin) * 0.5f /
                        static_cast<float>(bands));

    auto samples =
        makeSine(bin0CenterFreq, samplesPerFrame, static_cast<float>(sampleRate));
    fl::FFTBins bins(bands);
    fft.run(samples, &bins);

    float bin0Energy = bins.raw()[0];
    float totalEnergy = 0.0f;
    float maxOtherBin = 0.0f;
    for (int i = 0; i < bands; ++i) {
        totalEnergy += bins.raw()[i];
        if (i > 0 && bins.raw()[i] > maxOtherBin) {
            maxOtherBin = bins.raw()[i];
        }
    }

    FASTLED_WARN("Bin 0 test: freq=" << bin0CenterFreq << "Hz bin0="
                 << bin0Energy << " total=" << totalEnergy
                 << " max_other=" << maxOtherBin);

    // Bin 0 should have nonzero energy for a tone at its center frequency.
    // Previously bin 0 was dead (energy=0) because FFT bin filtering used
    // center frequency rather than the bin's full frequency range.
    FL_CHECK_GT(bin0Energy, 0.0f);

    // High-frequency tone should NOT leak into bin 0
    auto highSamples =
        makeSine(2000.0f, samplesPerFrame, static_cast<float>(sampleRate));
    fl::FFTBins highBins(bands);
    fft.run(highSamples, &highBins);

    float highToneBin0 = highBins.raw()[0];
    float highToneTotal = 0.0f;
    for (int i = 0; i < bands; ++i) {
        highToneTotal += highBins.raw()[i];
    }

    float bin0LeakFraction = highToneBin0 / highToneTotal;
    FASTLED_WARN("High tone (2000Hz): bin0_fraction=" << bin0LeakFraction);

    FL_CHECK_LT(bin0LeakFraction, 0.05f);
}

// Comparison: LOG_REBIN vs CQ_OCTAVE sweep quality on the same input.
// If LOG_REBIN is dramatically worse, it indicates a bug rather than
// inherent algorithm limitation.
FL_TEST_CASE("FFT sweep - LOG_REBIN vs CQ_OCTAVE quality comparison") {
    const int bands = 16;
    const float fmin = fl::FFT_Args::DefaultMinFrequency();
    const float fmax = fl::FFT_Args::DefaultMaxFrequency();
    const int sampleRate = fl::FFT_Args::DefaultSampleRate();
    const int samplesPerFrame = fl::FFT_Args::DefaultSamples();
    const int numFrames = 100;

    auto logFrames = runContinuousSweep(numFrames, bands, fmin, fmax, sampleRate,
                                        samplesPerFrame, fl::FFTMode::LOG_REBIN);
    auto cqFrames = runContinuousSweep(numFrames, bands, fmin, fmax, sampleRate,
                                       samplesPerFrame, fl::FFTMode::CQ_OCTAVE);

    float logConc = averageConcentration(logFrames);
    float cqConc = averageConcentration(cqFrames);
    int logJitter = countJitterEvents(logFrames);
    int cqJitter = countJitterEvents(cqFrames);
    int logMisplaced = countMisplacedPeaks(logFrames);
    int cqMisplaced = countMisplacedPeaks(cqFrames);
    float logActive = averageActiveBins(logFrames);
    float cqActive = averageActiveBins(cqFrames);

    FASTLED_WARN("=== LOG_REBIN vs CQ_OCTAVE (" << numFrames
                 << " frames) ===");
    FASTLED_WARN("  Avg concentration: LOG=" << logConc << " CQ=" << cqConc);
    FASTLED_WARN("  Jitter events:     LOG=" << logJitter << " CQ=" << cqJitter);
    FASTLED_WARN("  Misplaced peaks:   LOG=" << logMisplaced
                 << " CQ=" << cqMisplaced);
    FASTLED_WARN("  Avg active bins:   LOG=" << logActive << " CQ=" << cqActive);

    // LOG_REBIN concentration should be at least 60% of CQ_OCTAVE
    if (cqConc > 0.0f) {
        float concRatio = logConc / cqConc;
        FASTLED_WARN("  Concentration ratio (LOG/CQ): " << concRatio);
        FL_CHECK_GT(concRatio, 0.60f);
    }

    FL_CHECK_LE(logMisplaced, cqMisplaced + numFrames / 10);
}

// ============================================================================
// Multi-frequency and band independence tests
// These validate that the Hanning window fix resolves the "lockstep"
// issue where all bands rose and fell together.
// ============================================================================

// Two simultaneous tones should be resolved into separate bins.
// This is the core test for the lockstep fix — if spectral leakage
// smears a single tone across all bins, two tones will be indistinguishable.
FL_TEST_CASE("FFT sweep - two-tone separation LOG_REBIN") {
    const int bands = 16;
    const int sampleRate = fl::FFT_Args::DefaultSampleRate();
    const int N = fl::FFT_Args::DefaultSamples();

    fl::FFT_Args args(N, bands, fl::FFT_Args::DefaultMinFrequency(),
                      fl::FFT_Args::DefaultMaxFrequency(), sampleRate,
                      fl::FFTMode::LOG_REBIN);
    fl::FFTImpl fft(args);

    // Two tones well-separated in frequency: ~300 Hz (low bin) and ~2500 Hz (high bin)
    float freqLow = 300.0f;
    float freqHigh = 2500.0f;

    ::fl::vector<::fl::i16> samples;
    samples.resize(N);
    for (int i = 0; i < N; ++i) {
        float phaseLow = 2.0f * FL_M_PI * freqLow * static_cast<float>(i) /
                         static_cast<float>(sampleRate);
        float phaseHigh = 2.0f * FL_M_PI * freqHigh * static_cast<float>(i) /
                          static_cast<float>(sampleRate);
        float val = 8000.0f * (::fl::sinf(phaseLow) + ::fl::sinf(phaseHigh));
        samples[i] = static_cast<::fl::i16>(val);
    }

    fl::FFTBins bins(bands);
    fft.run(samples, &bins);

    int expectedLow = bins.freqToBin(freqLow);
    int expectedHigh = bins.freqToBin(freqHigh);

    FASTLED_WARN("Two-tone test: freqLow=" << freqLow << "Hz (bin "
                 << expectedLow << "), freqHigh=" << freqHigh << "Hz (bin "
                 << expectedHigh << ")");

    // Find the two highest peaks
    float peak1Val = 0.0f, peak2Val = 0.0f;
    int peak1Bin = 0, peak2Bin = 0;
    for (int i = 0; i < bands; ++i) {
        float e = bins.raw()[i];
        if (e > peak1Val) {
            peak2Val = peak1Val;
            peak2Bin = peak1Bin;
            peak1Val = e;
            peak1Bin = i;
        } else if (e > peak2Val) {
            peak2Val = e;
            peak2Bin = i;
        }
    }

    // Ensure the two peaks are in distinct bins (separated by at least 2)
    int peakSeparation = peak1Bin - peak2Bin;
    if (peakSeparation < 0) peakSeparation = -peakSeparation;

    FASTLED_WARN("  Peak 1: bin " << peak1Bin << " energy=" << peak1Val);
    FASTLED_WARN("  Peak 2: bin " << peak2Bin << " energy=" << peak2Val);
    FASTLED_WARN("  Separation: " << peakSeparation << " bins");

    FL_CHECK_GE(peakSeparation, 2);

    // Each peak should be near its expected bin
    int err1Low = peak1Bin - expectedLow;
    if (err1Low < 0) err1Low = -err1Low;
    int err1High = peak1Bin - expectedHigh;
    if (err1High < 0) err1High = -err1High;
    int err2Low = peak2Bin - expectedLow;
    if (err2Low < 0) err2Low = -err2Low;
    int err2High = peak2Bin - expectedHigh;
    if (err2High < 0) err2High = -err2High;

    // One peak near low freq, one near high freq (within ±1 bin each)
    bool peak1IsLow = (err1Low <= 1);
    bool peak1IsHigh = (err1High <= 1);
    bool peak2IsLow = (err2Low <= 1);
    bool peak2IsHigh = (err2High <= 1);

    bool correctAssignment = (peak1IsLow && peak2IsHigh) ||
                             (peak1IsHigh && peak2IsLow);
    FL_CHECK(correctAssignment);
}

// Band independence: simultaneous bass + treble tones should produce
// energy in bass and treble bins but NOT in mid bins.
// This directly tests the "lockstep" issue from #2193.
FL_TEST_CASE("FFT sweep - band independence bass+treble") {
    const int bands = 16;
    const int sampleRate = fl::FFT_Args::DefaultSampleRate();
    const int N = fl::FFT_Args::DefaultSamples();
    const float fmin = fl::FFT_Args::DefaultMinFrequency();
    const float fmax = fl::FFT_Args::DefaultMaxFrequency();

    fl::FFT_Args args(N, bands, fmin, fmax, sampleRate,
                      fl::FFTMode::LOG_REBIN);
    fl::FFTImpl fft(args);

    // Bass tone: ~200 Hz (should land in bins 0-1)
    // Treble tone: ~4000 Hz (should land in bins 14-15)
    float bassToneFreq = 200.0f;
    float trebleToneFreq = 4000.0f;

    ::fl::vector<::fl::i16> samples;
    samples.resize(N);
    for (int i = 0; i < N; ++i) {
        float pBass = 2.0f * FL_M_PI * bassToneFreq * static_cast<float>(i) /
                      static_cast<float>(sampleRate);
        float pTreble = 2.0f * FL_M_PI * trebleToneFreq *
                        static_cast<float>(i) /
                        static_cast<float>(sampleRate);
        float val = 8000.0f * (::fl::sinf(pBass) + ::fl::sinf(pTreble));
        samples[i] = static_cast<::fl::i16>(val);
    }

    fl::FFTBins bins(bands);
    fft.run(samples, &bins);

    // Sum energy in three regions: bass (0-3), mid (5-10), treble (12-15)
    float bassEnergy = 0.0f;
    for (int i = 0; i <= 3; ++i) bassEnergy += bins.raw()[i];

    float midEnergy = 0.0f;
    for (int i = 5; i <= 10; ++i) midEnergy += bins.raw()[i];

    float trebleEnergy = 0.0f;
    for (int i = 12; i <= 15; ++i) trebleEnergy += bins.raw()[i];

    float totalEnergy = 0.0f;
    for (int i = 0; i < bands; ++i) totalEnergy += bins.raw()[i];

    float bassFrac = bassEnergy / totalEnergy;
    float midFrac = midEnergy / totalEnergy;
    float trebleFrac = trebleEnergy / totalEnergy;

    FASTLED_WARN("Band independence test (bass=" << bassToneFreq
                 << "Hz + treble=" << trebleToneFreq << "Hz):");
    FASTLED_WARN("  Bass fraction:   " << bassFrac);
    FASTLED_WARN("  Mid fraction:    " << midFrac);
    FASTLED_WARN("  Treble fraction: " << trebleFrac);

    // Bass and treble should each have significant energy
    FL_CHECK_GT(bassFrac, 0.15f);
    FL_CHECK_GT(trebleFrac, 0.15f);

    // Mid region should have much less energy than bass or treble
    FL_CHECK_LT(midFrac, bassFrac);
    FL_CHECK_LT(midFrac, trebleFrac);

    // Mid should be a small fraction of total (leakage only)
    FL_CHECK_LT(midFrac, 0.25f);
}

// Amplitude linearity: doubling the input amplitude should roughly
// double the FFT magnitude in the peak bin. Windowing should not
// break this linear relationship.
FL_TEST_CASE("FFT sweep - amplitude linearity LOG_REBIN") {
    const int bands = 16;
    const int sampleRate = fl::FFT_Args::DefaultSampleRate();
    const int N = fl::FFT_Args::DefaultSamples();

    fl::FFT_Args args(N, bands, fl::FFT_Args::DefaultMinFrequency(),
                      fl::FFT_Args::DefaultMaxFrequency(), sampleRate,
                      fl::FFTMode::LOG_REBIN);
    fl::FFTImpl fft(args);

    float freq = 1000.0f;
    float ampLow = 4000.0f;
    float ampHigh = 8000.0f; // 2x

    auto samplesLow = makeSine(freq, N, static_cast<float>(sampleRate), ampLow);
    auto samplesHigh = makeSine(freq, N, static_cast<float>(sampleRate), ampHigh);

    fl::FFTBins binsLow(bands), binsHigh(bands);
    fft.run(samplesLow, &binsLow);
    fft.run(samplesHigh, &binsHigh);

    // Find peak bin (should be the same for both)
    int peakBinLow = 0, peakBinHigh = 0;
    float peakValLow = 0.0f, peakValHigh = 0.0f;
    for (int i = 0; i < bands; ++i) {
        if (binsLow.raw()[i] > peakValLow) {
            peakValLow = binsLow.raw()[i];
            peakBinLow = i;
        }
        if (binsHigh.raw()[i] > peakValHigh) {
            peakValHigh = binsHigh.raw()[i];
            peakBinHigh = i;
        }
    }

    FL_CHECK_EQ(peakBinLow, peakBinHigh);

    // Magnitude ratio should be close to the amplitude ratio (2.0)
    float ratio = peakValHigh / peakValLow;
    FASTLED_WARN("Amplitude linearity: amp_ratio=2.0 mag_ratio=" << ratio);

    // Allow 10% tolerance: ratio should be between 1.8 and 2.2
    FL_CHECK_GT(ratio, 1.8f);
    FL_CHECK_LT(ratio, 2.2f);
}
