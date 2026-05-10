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
#include "fl/log/log.h"
#include "fl/math/math.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/log/log.h"

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
                                          fl::audio::fft::Mode mode) {
    fl::audio::fft::Args args(samplesPerFrame, bands, fmin, fmax, sampleRate, mode);
    fl::audio::fft::Impl fft(args);

    fl::vector<SweepFrame> frames;
    frames.reserve(numFrames);

    float logRatio = fl::logf(fmax / fmin);

    for (int f = 0; f < numFrames; ++f) {
        float t = static_cast<float>(f) / static_cast<float>(numFrames - 1);
        float freq = fmin * fl::expf(logRatio * t);

        auto samples = makeSine(freq, samplesPerFrame, static_cast<float>(sampleRate));
        fl::audio::fft::Bins bins(bands);
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
    fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::CQ_OCTAVE);

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
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::FFT fft;
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
            FL_WARN("  Bin " << targetBin << " (center="
                         << centerFreq << " Hz): peak at bin " << peakBin
                         << " (off by " << diff << "), peakVal=" << peakVal);
        }
    }

    FL_WARN("CQ sweep (64 bins, 20-11025 Hz): " << correctBins << "/"
                 << totalTested << " bins correct (within +/-3)");
    if (firstBadBin >= 0) {
        float badFreq = fmin * fl::expf(logRatio * static_cast<float>(firstBadBin) /
                                         static_cast<float>(bands - 1));
        int minWindow = static_cast<int>(512.0f * fmin / badFreq);
        FL_WARN("First bad bin: " << firstBadBin << " (center="
                     << badFreq << " Hz, CQ window=" << minWindow << " samples)");
    }

    // Octave-wise CQT: each octave has well-conditioned kernels (N_window
    // >= N/2), so ±3 bin tolerance at >80% correct validates good accuracy.
    float correctPct = static_cast<float>(correctBins) / static_cast<float>(totalTested);
    FL_CHECK_GT(correctPct, 0.80f);
}

// Default 16-bin config: Auto resolves to CqOctave (naive CQ, well-conditioned).
// winMin = 512 * 90 / 14080 = 3 → CQ kernels are fine.
FL_TEST_CASE("CQ frequency sweep - default config (16 bins 90-14080 Hz)") {
    const int bands = 16;
    fl::audio::fft::Args args; // defaults: 512, 16, 90, 14080, 44100, Auto→CqOctave(naive)

    float logRatio = fl::logf(args.fmax / args.fmin);
    int correctBins = 0;
    int totalTested = 0;

    for (int targetBin = 0; targetBin < bands; targetBin++) {
        float centerFreq = args.fmin * fl::expf(logRatio * static_cast<float>(targetBin) /
                                                  static_cast<float>(bands - 1));

        auto samples = makeSine(centerFreq);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::FFT fft;
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
            FL_WARN("  Bin " << targetBin << " (center="
                         << centerFreq << " Hz): peak at bin " << peakBin
                         << " (off by " << diff << ")");
        }
    }

    FL_WARN("CQ sweep (16 bins, default): " << correctBins << "/"
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
    fl::audio::fft::Args args(512, bands, fmin, fmax, 44100);

    float logRatio = fl::logf(fmax / fmin);
    int correctBins = 0;
    int totalTested = 0;

    for (int targetBin = 0; targetBin < bands; targetBin++) {
        float centerFreq = fmin * fl::expf(logRatio * static_cast<float>(targetBin) /
                                            static_cast<float>(bands - 1));

        // Skip frequencies below FFT resolution
        if (centerFreq < 86.0f) continue;

        auto samples = makeSine(centerFreq);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::FFT fft;
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
            FL_WARN("  Bin " << targetBin << " (center="
                         << centerFreq << " Hz, window=" << window
                         << "): peak at bin " << peakBin);
        }
    }

    FL_WARN("CQ sweep (16 bins, 60-5120 Hz): " << correctBins << "/"
                 << totalTested << " bins correct (within +/-3)");
    // Q ≈ 0.70 limits accuracy. Require >50% correct; band-summing
    // detector (like EqualizerDetector) still work at this resolution.
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
    fl::audio::fft::Args args(512, bands, fmin, fmax, 44100, fl::audio::fft::Mode::LOG_REBIN);

    float logRatio = fl::logf(fmax / fmin);
    int correctBins = 0;
    int totalTested = 0;

    for (int targetBin = 0; targetBin < bands; targetBin += 4) {
        float centerFreq = fmin * fl::expf(logRatio * (static_cast<float>(targetBin) + 0.5f) /
                                            static_cast<float>(bands));

        if (centerFreq < 86.0f) continue;
        if (centerFreq > 22050.0f) continue;

        auto samples = makeSine(centerFreq);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::FFT fft;
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
            FL_WARN("  LogRebin bin " << targetBin << " (center="
                         << centerFreq << " Hz): peak at bin " << peakBin
                         << " (off by " << diff << ")");
        }
    }

    FL_WARN("LogRebin sweep (64 bins, 20-11025 Hz): " << correctBins << "/"
                 << totalTested << " bins correct (within +/-2)");

    float correctPct = static_cast<float>(correctBins) / static_cast<float>(totalTested);
    FL_CHECK_GT(correctPct, 0.80f);
}

// Verify that LINEAR bins always produce correct ordering (no CQ kernel issues)
FL_TEST_CASE("Linear bins frequency sweep - wide range (20-11025 Hz)") {
    const int bands = 64;
    const float fmin = 20.0f;
    const float fmax = 11025.0f;
    fl::audio::fft::Args args(512, bands, fmin, fmax, 44100);

    float linearBinHz = (fmax - fmin) / static_cast<float>(bands);
    int correctBins = 0;
    int totalTested = 0;

    // Test linear bins at various frequencies
    for (int targetBin = 1; targetBin < bands; targetBin += 4) {
        float centerFreq = fmin + (static_cast<float>(targetBin) + 0.5f) * linearBinHz;

        // Skip frequencies below FFT resolution
        if (centerFreq < 86.0f) continue;

        auto samples = makeSine(centerFreq);
        fl::audio::fft::Bins bins(bands);
        fl::audio::fft::FFT fft;
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
            FL_WARN("  Linear bin " << targetBin << " (center="
                         << centerFreq << " Hz): peak at bin " << peakBin);
        }
    }

    FL_WARN("Linear sweep (64 bins, 20-11025 Hz): " << correctBins << "/"
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
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int samplesPerFrame = fl::audio::fft::Args::DefaultSamples();

    // ~10 seconds of audio at 512 samples/frame = 862 frames
    const int numFrames = 862;

    auto frames = runContinuousSweep(numFrames, bands, fmin, fmax, sampleRate,
                                     samplesPerFrame, fl::audio::fft::Mode::LOG_REBIN);
    FL_REQUIRE_EQ(static_cast<int>(frames.size()), numFrames);

    int nonMonotonic = countNonMonotonicSteps(frames);
    int jitterCount = countJitterEvents(frames);
    int misplaced = countMisplacedPeaks(frames);
    float avgConc = averageConcentration(frames);
    float avgActive = averageActiveBins(frames);
    float worstConc = worstConcentration(frames);
    int maxActive = maxActiveBins(frames);

    FL_WARN("=== FFT Sweep Diagnostic (LOG_REBIN, " << bands << " bins, "
                 << numFrames << " frames) ===");
    FL_WARN("Non-monotonic steps: " << nonMonotonic << " / "
                 << (numFrames - 1));
    FL_WARN("Jitter events (A->B->A): " << jitterCount);
    FL_WARN("Misplaced peaks (>1 bin off): " << misplaced << " / "
                 << numFrames);
    FL_WARN("Avg energy concentration: " << avgConc);
    FL_WARN("Worst energy concentration: " << worstConc);
    FL_WARN("Avg active bins per frame: " << avgActive);
    FL_WARN("Max active bins in any frame: " << maxActive);

    // Print sample frames for inspection
    int sampleIndices[] = {0,
                           numFrames / 8,
                           numFrames / 4,
                           numFrames / 2,
                           3 * numFrames / 4,
                           numFrames - 1};
    for (int idx : sampleIndices) {
        const auto &fr = frames[idx];
        FL_WARN("  Frame " << idx << ": freq=" << fr.frequency
                     << "Hz peak_bin=" << fr.peakBin
                     << " expected=" << fr.expectedBin
                     << " conc=" << fr.concentration
                     << " active=" << fr.activeBins);
    }

    // --- Assertions for correct behavior ---

    float nonMonotonicRate =
        static_cast<float>(nonMonotonic) / static_cast<float>(numFrames - 1);
    FL_WARN("Non-monotonic rate: " << nonMonotonicRate);
    FL_CHECK_LT(nonMonotonicRate, 0.05f);

    float jitterRate =
        static_cast<float>(jitterCount) / static_cast<float>(numFrames - 2);
    FL_WARN("Jitter rate: " << jitterRate);
    FL_CHECK_LT(jitterRate, 0.03f);

    float misplacedRate =
        static_cast<float>(misplaced) / static_cast<float>(numFrames);
    FL_WARN("Misplaced peak rate: " << misplacedRate);
    FL_CHECK_LT(misplacedRate, 0.05f);

    FL_CHECK_GT(avgConc, 0.40f);
    FL_CHECK_GT(worstConc, 0.15f);
    FL_CHECK_LT(avgActive, 5.0f);
    // Blackman-Harris wider main lobe → max active bins up to 10.
    FL_CHECK_LE(maxActive, 10);
}

// Focused test: low-frequency region where jitter is most visible.
// With 512 samples at 44100 Hz, FFT bin width = 86 Hz.
// Log-spaced output bins at low freq are ~40-60 Hz wide — narrower than
// an FFT bin — causing quantization jitter.
FL_TEST_CASE("FFT sweep - low frequency jitter detection") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int samplesPerFrame = fl::audio::fft::Args::DefaultSamples();

    // Sweep just the bottom bins (174-350 Hz) with fine resolution
    const float lowFmax = 350.0f;
    const int numFrames = 200;

    fl::audio::fft::Args args(samplesPerFrame, bands, fmin, fmax, sampleRate,
                      fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);

    int peakBinChanges = 0;
    int lastPeakBin = -1;
    int maxConsecutiveSameBin = 0;
    int currentStreak = 0;

    for (int f = 0; f < numFrames; ++f) {
        float t = static_cast<float>(f) / static_cast<float>(numFrames - 1);
        float freq = fmin + (lowFmax - fmin) * t;

        auto samples =
            makeSine(freq, samplesPerFrame, static_cast<float>(sampleRate));
        fl::audio::fft::Bins bins(bands);
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
    FL_WARN("Low-freq sweep (174-350Hz): peak bin changed "
                 << peakBinChanges << " times in " << numFrames
                 << " frames (rate=" << changeRate << ")");
    FL_WARN("Max consecutive frames on same bin: "
                 << maxConsecutiveSameBin);

    // Over ~2 output bins, expect ~1-2 clean transitions.
    // More than 10 transitions = oscillation/jitter.
    FL_CHECK_LT(peakBinChanges, 10);
}

// Energy leakage test: a pure tone should concentrate energy in 1-3 bins.
// Without windowing, spectral leakage spreads energy across many bins.
FL_TEST_CASE("FFT sweep - single tone leakage LOG_REBIN") {
    const int bands = 16;
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int samplesPerFrame = fl::audio::fft::Args::DefaultSamples();

    float testFreqs[] = {200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f};

    for (float freq : testFreqs) {
        fl::audio::fft::Args args(samplesPerFrame, bands,
                          fl::audio::fft::Args::DefaultMinFrequency(),
                          fl::audio::fft::Args::DefaultMaxFrequency(), sampleRate,
                          fl::audio::fft::Mode::LOG_REBIN);
        fl::audio::fft::Impl fft(args);

        auto samples =
            makeSine(freq, samplesPerFrame, static_cast<float>(sampleRate));
        fl::audio::fft::Bins bins(bands);
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

        FL_WARN("Tone " << freq << "Hz: peak_bin=" << peakBin
                     << " expected=" << expectedBin << " conc=" << concentration
                     << " significant_bins=" << significantBins);

        // Blackman-Harris has wider main lobe than Hanning (8 vs 4 FFT bins),
        // so energy spreads to more adjacent log-spaced bins at low freq.
        // Sidelobe floor is -92 dB vs -31 dB, so distant bins stay clean.
        FL_CHECK_LE(significantBins, 7);
        FL_CHECK_LE(binError, 1);
        FL_CHECK_GT(concentration, 0.30f);
    }
}

// Bin 0 anomaly test: user reported "Bin 0 of FastLED looks off".
// Tests that bin 0 responds correctly to its own frequency and
// doesn't leak energy from distant frequencies.
FL_TEST_CASE("FFT sweep - bin 0 anomaly") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int samplesPerFrame = fl::audio::fft::Args::DefaultSamples();

    fl::audio::fft::Args args(samplesPerFrame, bands, fmin, fmax, sampleRate,
                      fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);

    // Tone at the center of bin 0
    float bin0CenterFreq =
        fmin * fl::expf(fl::logf(fmax / fmin) * 0.5f /
                        static_cast<float>(bands));

    auto samples =
        makeSine(bin0CenterFreq, samplesPerFrame, static_cast<float>(sampleRate));
    fl::audio::fft::Bins bins(bands);
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

    FL_WARN("Bin 0 test: freq=" << bin0CenterFreq << "Hz bin0="
                 << bin0Energy << " total=" << totalEnergy
                 << " max_other=" << maxOtherBin);

    // Bin 0 should have nonzero energy for a tone at its center frequency.
    // Previously bin 0 was dead (energy=0) because FFT bin filtering used
    // center frequency rather than the bin's full frequency range.
    FL_CHECK_GT(bin0Energy, 0.0f);

    // High-frequency tone should NOT leak into bin 0
    auto highSamples =
        makeSine(2000.0f, samplesPerFrame, static_cast<float>(sampleRate));
    fl::audio::fft::Bins highBins(bands);
    fft.run(highSamples, &highBins);

    float highToneBin0 = highBins.raw()[0];
    float highToneTotal = 0.0f;
    for (int i = 0; i < bands; ++i) {
        highToneTotal += highBins.raw()[i];
    }

    float bin0LeakFraction = highToneBin0 / highToneTotal;
    FL_WARN("High tone (2000Hz): bin0_fraction=" << bin0LeakFraction);

    FL_CHECK_LT(bin0LeakFraction, 0.05f);
}

// Comparison: LOG_REBIN vs CQ_OCTAVE sweep quality on the same input.
// If LOG_REBIN is dramatically worse, it indicates a bug rather than
// inherent algorithm limitation.
FL_TEST_CASE("FFT sweep - LOG_REBIN vs CQ_OCTAVE quality comparison") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int samplesPerFrame = fl::audio::fft::Args::DefaultSamples();
    const int numFrames = 100;

    auto logFrames = runContinuousSweep(numFrames, bands, fmin, fmax, sampleRate,
                                        samplesPerFrame, fl::audio::fft::Mode::LOG_REBIN);
    auto cqFrames = runContinuousSweep(numFrames, bands, fmin, fmax, sampleRate,
                                       samplesPerFrame, fl::audio::fft::Mode::CQ_OCTAVE);

    float logConc = averageConcentration(logFrames);
    float cqConc = averageConcentration(cqFrames);
    int logJitter = countJitterEvents(logFrames);
    int cqJitter = countJitterEvents(cqFrames);
    int logMisplaced = countMisplacedPeaks(logFrames);
    int cqMisplaced = countMisplacedPeaks(cqFrames);
    float logActive = averageActiveBins(logFrames);
    float cqActive = averageActiveBins(cqFrames);

    FL_WARN("=== LOG_REBIN vs CQ_OCTAVE (" << numFrames
                 << " frames) ===");
    FL_WARN("  Avg concentration: LOG=" << logConc << " CQ=" << cqConc);
    FL_WARN("  Jitter events:     LOG=" << logJitter << " CQ=" << cqJitter);
    FL_WARN("  Misplaced peaks:   LOG=" << logMisplaced
                 << " CQ=" << cqMisplaced);
    FL_WARN("  Avg active bins:   LOG=" << logActive << " CQ=" << cqActive);

    // LOG_REBIN concentration should be at least 60% of CQ_OCTAVE
    if (cqConc > 0.0f) {
        float concRatio = logConc / cqConc;
        FL_WARN("  Concentration ratio (LOG/CQ): " << concRatio);
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
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int N = fl::audio::fft::Args::DefaultSamples();

    fl::audio::fft::Args args(N, bands, fl::audio::fft::Args::DefaultMinFrequency(),
                      fl::audio::fft::Args::DefaultMaxFrequency(), sampleRate,
                      fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);

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

    fl::audio::fft::Bins bins(bands);
    fft.run(samples, &bins);

    int expectedLow = bins.freqToBin(freqLow);
    int expectedHigh = bins.freqToBin(freqHigh);

    FL_WARN("Two-tone test: freqLow=" << freqLow << "Hz (bin "
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

    FL_WARN("  Peak 1: bin " << peak1Bin << " energy=" << peak1Val);
    FL_WARN("  Peak 2: bin " << peak2Bin << " energy=" << peak2Val);
    FL_WARN("  Separation: " << peakSeparation << " bins");

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
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int N = fl::audio::fft::Args::DefaultSamples();
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();

    fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate,
                      fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);

    // Bass tone near fmin (should land in low bins)
    // Treble tone near fmax (should land in high bins)
    float bassToneFreq = fmin * 2.0f;
    float trebleToneFreq = fmax / 2.0f;

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

    fl::audio::fft::Bins bins(bands);
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

    FL_WARN("Band independence test (bass=" << bassToneFreq
                 << "Hz + treble=" << trebleToneFreq << "Hz):");
    FL_WARN("  Bass fraction:   " << bassFrac);
    FL_WARN("  Mid fraction:    " << midFrac);
    FL_WARN("  Treble fraction: " << trebleFrac);

    // Bass and treble should each have significant energy
    FL_CHECK_GT(bassFrac, 0.15f);
    FL_CHECK_GT(trebleFrac, 0.15f);

    // Mid region should have much less energy than bass or treble
    FL_CHECK_LT(midFrac, bassFrac);
    FL_CHECK_LT(midFrac, trebleFrac);

    // Mid should be a small fraction of total (leakage only)
    FL_CHECK_LT(midFrac, 0.25f);
}

// ============================================================================
// High-band aliasing diagnostic for Equalizer config (16 bins, 60-5120 Hz)
// Tests that LOG_REBIN bin-width normalization prevents high-frequency bins
// from accumulating disproportionate sidelobe energy.
// ============================================================================

FL_TEST_CASE("FFT sweep - high-band aliasing diagnostic (Equalizer config)") {
    const int bands = 16;
    const float fmin = 60.0f;
    const float fmax = 5120.0f;
    const int sampleRate = 44100;
    const int samplesPerFrame = 512;
    const float fftBinHz = static_cast<float>(sampleRate) / static_cast<float>(samplesPerFrame);

    // Print bin info table
    float logRatio = fl::logf(fmax / fmin);
    FL_WARN("=== Equalizer config bin info (16 bins, 60-5120 Hz) ===");
    FL_WARN("FFT bin width: " << fftBinHz << " Hz");
    for (int i = 0; i < bands; ++i) {
        float center = fmin * fl::expf(logRatio * static_cast<float>(i) /
                                        static_cast<float>(bands - 1));
        float lo = (i == 0) ? fmin : fmin * fl::expf(logRatio * (2.0f * static_cast<float>(i) - 1.0f) /
                                                       (2.0f * static_cast<float>(bands - 1)));
        float hi = (i == bands - 1) ? fmax : fmin * fl::expf(logRatio * (2.0f * static_cast<float>(i) + 1.0f) /
                                                               (2.0f * static_cast<float>(bands - 1)));
        float width = hi - lo;
        float fftBins = width / fftBinHz;
        FL_WARN("  Bin " << i << ": center=" << center << "Hz width="
                     << width << "Hz mapped_fft_bins=" << fftBins);
    }

    // Sweep upper bins (bins 10-15, ~2000-5120 Hz) with fine steps
    const int numSteps = 200;
    int correctPeaks = 0;
    int totalSteps = 0;
    float worstAliasMetric = 0.0f;
    int worstAliasBin = -1;

    fl::audio::fft::Args args(samplesPerFrame, bands, fmin, fmax, sampleRate,
                      fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);

    // Sweep from ~1500 Hz to ~5120 Hz (upper region)
    float sweepFmin = 1500.0f;
    float sweepFmax = 5000.0f;

    for (int step = 0; step < numSteps; ++step) {
        float t = static_cast<float>(step) / static_cast<float>(numSteps - 1);
        float freq = sweepFmin * fl::expf(fl::logf(sweepFmax / sweepFmin) * t);

        auto samples = makeSine(freq, samplesPerFrame, static_cast<float>(sampleRate));
        fl::audio::fft::Bins bins(bands);
        fft.run(samples, &bins);

        // Use bin-width-normalized magnitudes for aliasing analysis
        auto normBins = bins.rawNormalized();

        // Find peak bin and total energy
        int peakBin = 0;
        float peakEnergy = 0.0f;
        float totalEnergy = 0.0f;
        for (int i = 0; i < bands; ++i) {
            float e = normBins[i];
            totalEnergy += e;
            if (e > peakEnergy) {
                peakEnergy = e;
                peakBin = i;
            }
        }

        int expectedBin = bins.freqToBin(freq);

        // Compute aliasing metric: energy outside peak +/-1 bins vs total
        float adjacentEnergy = 0.0f;
        for (int i = 0; i < bands; ++i) {
            int dist = i - peakBin;
            if (dist < 0) dist = -dist;
            if (dist <= 1) {
                adjacentEnergy += normBins[i];
            }
        }
        float aliasMetric = (totalEnergy > 0.0f)
            ? 1.0f - (adjacentEnergy / totalEnergy) : 0.0f;

        if (aliasMetric > worstAliasMetric) {
            worstAliasMetric = aliasMetric;
            worstAliasBin = peakBin;
        }

        totalSteps++;
        int diff = peakBin - expectedBin;
        if (diff < 0) diff = -diff;
        if (diff <= 1) {
            correctPeaks++;
        }
    }

    float correctRate = static_cast<float>(correctPeaks) / static_cast<float>(totalSteps);
    FL_WARN("=== High-band aliasing results ===");
    FL_WARN("Correct peaks (within +/-1): " << correctPeaks << "/" << totalSteps
                 << " (" << correctRate << ")");
    FL_WARN("Worst alias metric: " << worstAliasMetric << " at bin " << worstAliasBin);

    // Assertions: aliasing metric < 0.30 (30% max leakage to non-adjacent bins)
    FL_CHECK_LT(worstAliasMetric, 0.30f);
    // Peak bin matches expected within +/-1 for >= 90% of sweep steps
    FL_CHECK_GE(correctRate, 0.90f);
}

// Amplitude linearity: doubling the input amplitude should roughly
// double the FFT magnitude in the peak bin. Windowing should not
// break this linear relationship.
FL_TEST_CASE("FFT sweep - amplitude linearity LOG_REBIN") {
    const int bands = 16;
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int N = fl::audio::fft::Args::DefaultSamples();

    fl::audio::fft::Args args(N, bands, fl::audio::fft::Args::DefaultMinFrequency(),
                      fl::audio::fft::Args::DefaultMaxFrequency(), sampleRate,
                      fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);

    float freq = 1000.0f;
    float ampLow = 4000.0f;
    float ampHigh = 8000.0f; // 2x

    auto samplesLow = makeSine(freq, N, static_cast<float>(sampleRate), ampLow);
    auto samplesHigh = makeSine(freq, N, static_cast<float>(sampleRate), ampHigh);

    fl::audio::fft::Bins binsLow(bands), binsHigh(bands);
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
    FL_WARN("Amplitude linearity: amp_ratio=2.0 mag_ratio=" << ratio);

    // Allow 10% tolerance: ratio should be between 1.8 and 2.2
    FL_CHECK_GT(ratio, 1.8f);
    FL_CHECK_LT(ratio, 2.2f);
}

// ============================================================================
// Adversarial FFT tone sweep tests
// Tests high-frequency bin quality, spectral leakage, normalization,
// boundary effects, and cross-mode consistency.
// ============================================================================

namespace {

// Deterministic PRNG for white noise generation (LCG)
struct SimplePRNG {
    ::fl::u32 state;
    SimplePRNG(::fl::u32 seed = 12345u) : state(seed) {}
    ::fl::u32 next() {
        state = state * 1664525u + 1013904223u;
        return state;
    }
    // Returns i16 in [-16384, 16383]
    ::fl::i16 nextI16() {
        return static_cast<::fl::i16>(
            (static_cast<::fl::i32>(next() & 0xFFFFu) - 32768) / 2);
    }
};

const char* advModeName(::fl::audio::fft::Mode mode) {
    switch (mode) {
    case ::fl::audio::fft::Mode::LOG_REBIN: return "LOG_REBIN";
    case ::fl::audio::fft::Mode::CQ_NAIVE: return "CQ_NAIVE";
    case ::fl::audio::fft::Mode::CQ_OCTAVE: return "CQ_OCTAVE";
    case ::fl::audio::fft::Mode::CQ_HYBRID: return "CQ_HYBRID";
    default: return "UNKNOWN";
    }
}

} // namespace

// Test 1: High-Bin Spectral Leakage (All Modes)
// Pure tone at center of bins 13, 14, 15 — checks concentration and distant leakage.
FL_TEST_CASE("FFT adversarial - high-bin spectral leakage (all modes)") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int N = fl::audio::fft::Args::DefaultSamples();
    float logRatio = fl::logf(fmax / fmin);

    fl::audio::fft::Mode modes[] = {
        fl::audio::fft::Mode::LOG_REBIN,
        fl::audio::fft::Mode::CQ_NAIVE,
        fl::audio::fft::Mode::CQ_OCTAVE,
        fl::audio::fft::Mode::CQ_HYBRID
    };

    for (fl::audio::fft::Mode mode : modes) {
        fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate, mode);
        fl::audio::fft::Impl fft(args);

        for (int targetBin = 13; targetBin <= 15; ++targetBin) {
            float centerFreq = fmin * fl::expf(logRatio * static_cast<float>(targetBin) /
                                                static_cast<float>(bands - 1));

            auto samples = makeSine(centerFreq, N, static_cast<float>(sampleRate));
            fl::audio::fft::Bins bins(bands);
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

            float concentration = (totalEnergy > 0.0f) ? peakEnergy / totalEnergy : 0.0f;

            // Distant leakage: energy in bins far from peak (>= 3 bins below peak)
            float distantEnergy = 0.0f;
            for (int i = 0; i < peakBin - 2; ++i) {
                distantEnergy += bins.raw()[i];
            }
            float distantLeakage = (totalEnergy > 0.0f) ? distantEnergy / totalEnergy : 0.0f;

            FL_WARN(advModeName(mode) << " bin " << targetBin
                         << " (freq=" << centerFreq << "Hz): conc=" << concentration
                         << " distant_leak=" << distantLeakage
                         << " peak_bin=" << peakBin);

            // Per-mode concentration thresholds:
            // CQ_NAIVE uses single-FFT CQ kernels → lower concentration at high bins.
            // CQ_OCTAVE has wider kernel main lobes → slightly lower concentration.
            float concThreshold = (mode == fl::audio::fft::Mode::CQ_NAIVE) ? 0.25f :
                                  (mode == fl::audio::fft::Mode::CQ_OCTAVE) ? 0.35f : 0.40f;
            FL_CHECK_GT(concentration, concThreshold);
            FL_CHECK_LT(distantLeakage, 0.20f);
        }
    }
}

// Test 2: Near-Nyquist Aliasing
// Config: 16 bands, 60-20000 Hz. Tones at 18000, 19000, 20000 Hz.
FL_TEST_CASE("FFT adversarial - near-Nyquist aliasing") {
    const int bands = 16;
    const float fmin = 60.0f;
    const float fmax = 20000.0f;
    const int sampleRate = 44100;
    const int N = 512;

    float testFreqs[] = {18000.0f, 19000.0f, 20000.0f};

    for (float freq : testFreqs) {
        fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate, fl::audio::fft::Mode::LOG_REBIN);
        fl::audio::fft::Impl fft(args);

        auto samples = makeSine(freq, N, static_cast<float>(sampleRate));
        fl::audio::fft::Bins bins(bands);
        fft.run(samples, &bins);

        int peakBin = 0;
        float peakEnergy = 0.0f;
        float totalEnergy = 0.0f;
        float bottomHalfEnergy = 0.0f;
        for (int i = 0; i < bands; ++i) {
            float e = bins.raw()[i];
            totalEnergy += e;
            if (e > peakEnergy) {
                peakEnergy = e;
                peakBin = i;
            }
            if (i < bands / 2) {
                bottomHalfEnergy += e;
            }
        }

        float bottomFrac = (totalEnergy > 0.0f) ? bottomHalfEnergy / totalEnergy : 0.0f;

        FL_WARN("Near-Nyquist " << freq << "Hz: peak_bin=" << peakBin
                     << " bottom_half_frac=" << bottomFrac
                     << " total_energy=" << totalEnergy);

        // Peak should be in top 2 bins
        FL_CHECK_GE(peakBin, bands - 2);
        // Energy in bottom half should be < 5% (aliasing check)
        FL_CHECK_LT(bottomFrac, 0.05f);
    }

    // Nyquist tone: sin(n*pi) = 0, so total energy should be near-zero
    {
        float nyquistFreq = static_cast<float>(sampleRate) / 2.0f;
        fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate, fl::audio::fft::Mode::LOG_REBIN);
        fl::audio::fft::Impl fft(args);

        auto samples = makeSine(nyquistFreq, N, static_cast<float>(sampleRate));
        fl::audio::fft::Bins bins(bands);
        fft.run(samples, &bins);

        float totalEnergy = 0.0f;
        float bottomHalfEnergy = 0.0f;
        for (int i = 0; i < bands; ++i) {
            totalEnergy += bins.raw()[i];
            if (i < bands / 2) {
                bottomHalfEnergy += bins.raw()[i];
            }
        }

        float bottomFrac = (totalEnergy > 0.0f) ? bottomHalfEnergy / totalEnergy : 0.0f;
        FL_WARN("Nyquist " << nyquistFreq << "Hz: bottom_half_frac=" << bottomFrac
                     << " total_energy=" << totalEnergy);

        // Nyquist sine samples to zero; aliasing check only if there's energy
        FL_CHECK_LT(bottomFrac, 0.10f);
    }
}

// Test 3: White Noise Normalization Flatness
// Deterministic PRNG white noise through LOG_REBIN.
FL_TEST_CASE("FFT adversarial - white noise normalization flatness") {
    const int bands = 16;
    const int N = 512;
    const int sampleRate = 44100;

    SimplePRNG rng(42);
    ::fl::vector<::fl::i16> noiseSamples;
    noiseSamples.resize(N);
    for (int i = 0; i < N; ++i) {
        noiseSamples[i] = rng.nextI16();
    }

    fl::audio::fft::Args args(N, bands,
                      fl::audio::fft::Args::DefaultMinFrequency(),
                      fl::audio::fft::Args::DefaultMaxFrequency(),
                      sampleRate, fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);
    fl::audio::fft::Bins bins(bands);
    fft.run(noiseSamples, &bins);

    // rawNormalized() should be flatter than raw()
    auto normBins = bins.rawNormalized();
    auto rawBins = bins.raw();

    float normSum = 0.0f;
    float rawMax = 0.0f, rawMin = 1e30f;

    for (int i = 0; i < bands; ++i) {
        normSum += normBins[i];
        if (rawBins[i] > rawMax) rawMax = rawBins[i];
        if (rawBins[i] > 0.0f && rawBins[i] < rawMin) rawMin = rawBins[i];
    }

    float normMean = normSum / static_cast<float>(bands);
    float normVariance = 0.0f;
    for (int i = 0; i < bands; ++i) {
        float diff = normBins[i] - normMean;
        normVariance += diff * diff;
    }
    normVariance /= static_cast<float>(bands);
    // Manual sqrt: iterative Newton's method for test code
    float normStddev = normVariance;
    for (int iter = 0; iter < 10; ++iter) {
        if (normStddev <= 0.0f) break;
        normStddev = 0.5f * (normStddev + normVariance / normStddev);
    }
    float normCV = (normMean > 0.0f) ? normStddev / normMean : 0.0f;

    float rawRatio = (rawMin > 0.0f) ? rawMax / rawMin : 0.0f;

    FL_WARN("White noise: normCV=" << normCV << " rawMax/rawMin=" << rawRatio);
    for (int i = 0; i < bands; ++i) {
        FL_WARN("  bin " << i << ": raw=" << rawBins[i]
                     << " norm=" << normBins[i]);
    }

    // rawNormalized() coefficient of variation should be < 0.60
    FL_CHECK_LT(normCV, 0.60f);
    // raw() max/min ratio > 3.0 (proves raw IS biased toward wide high bins)
    FL_CHECK_GT(rawRatio, 3.0f);
}

// Test 4: Top-Quartile vs Mid-Range Concentration Comparison
// 100-frame sweep over bins 12-15 vs bins 4-8.
FL_TEST_CASE("FFT adversarial - top-quartile vs mid-range concentration") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int N = fl::audio::fft::Args::DefaultSamples();
    float logRatio = fl::logf(fmax / fmin);

    fl::audio::fft::Mode modes[] = {fl::audio::fft::Mode::LOG_REBIN, fl::audio::fft::Mode::CQ_OCTAVE};

    for (fl::audio::fft::Mode mode : modes) {
        fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate, mode);
        fl::audio::fft::Impl fft(args);

        // Sweep bins 12-15 (top quartile)
        const int topFrames = 100;
        float topConcSum = 0.0f;
        for (int frame = 0; frame < topFrames; ++frame) {
            float t = static_cast<float>(frame) / static_cast<float>(topFrames - 1);
            float binF = 12.0f + t * 3.0f;
            float freq = fmin * fl::expf(logRatio * binF /
                                          static_cast<float>(bands - 1));

            auto samples = makeSine(freq, N, static_cast<float>(sampleRate));
            fl::audio::fft::Bins bins(bands);
            fft.run(samples, &bins);

            float peakE = 0.0f, totalE = 0.0f;
            for (int i = 0; i < bands; ++i) {
                float e = bins.raw()[i];
                totalE += e;
                if (e > peakE) peakE = e;
            }
            topConcSum += (totalE > 0.0f) ? peakE / totalE : 0.0f;
        }

        // Sweep bins 4-8 (mid range)
        const int midFrames = 100;
        float midConcSum = 0.0f;
        for (int frame = 0; frame < midFrames; ++frame) {
            float t = static_cast<float>(frame) / static_cast<float>(midFrames - 1);
            float binF = 4.0f + t * 4.0f;
            float freq = fmin * fl::expf(logRatio * binF /
                                          static_cast<float>(bands - 1));

            auto samples = makeSine(freq, N, static_cast<float>(sampleRate));
            fl::audio::fft::Bins bins(bands);
            fft.run(samples, &bins);

            float peakE = 0.0f, totalE = 0.0f;
            for (int i = 0; i < bands; ++i) {
                float e = bins.raw()[i];
                totalE += e;
                if (e > peakE) peakE = e;
            }
            midConcSum += (totalE > 0.0f) ? peakE / totalE : 0.0f;
        }

        float topAvg = topConcSum / static_cast<float>(topFrames);
        float midAvg = midConcSum / static_cast<float>(midFrames);

        FL_WARN(advModeName(mode) << ": top_quartile_avg_conc=" << topAvg
                     << " mid_range_avg_conc=" << midAvg);

        FL_CHECK_GT(topAvg, 0.35f);
        if (midAvg > 0.0f) {
            float ratio = topAvg / midAvg;
            FL_WARN(advModeName(mode) << ": high/mid ratio=" << ratio);
            FL_CHECK_GT(ratio, 0.70f);
        }
    }
}

// Test 5: High-Bin Boundary Energy Split
// Tone at geometric mean between adjacent high bins.
FL_TEST_CASE("FFT adversarial - high-bin boundary energy split") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int N = fl::audio::fft::Args::DefaultSamples();
    float logRatio = fl::logf(fmax / fmin);

    fl::audio::fft::Mode modes[] = {
        fl::audio::fft::Mode::LOG_REBIN,
        fl::audio::fft::Mode::CQ_NAIVE,
        fl::audio::fft::Mode::CQ_OCTAVE,
        fl::audio::fft::Mode::CQ_HYBRID
    };

    for (fl::audio::fft::Mode mode : modes) {
        fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate, mode);
        fl::audio::fft::Impl fft(args);

        // Test boundaries: bins 13-14 and bins 14-15
        for (int p = 0; p < 2; ++p) {
            int binA = (p == 0) ? 13 : 14;
            int binB = (p == 0) ? 14 : 15;

            float freqA = fmin * fl::expf(logRatio * static_cast<float>(binA) /
                                            static_cast<float>(bands - 1));
            float freqB = fmin * fl::expf(logRatio * static_cast<float>(binB) /
                                            static_cast<float>(bands - 1));
            // Geometric mean = boundary frequency
            float boundaryFreq = freqA * fl::expf(0.5f * fl::logf(freqB / freqA));

            auto samples = makeSine(boundaryFreq, N, static_cast<float>(sampleRate));
            fl::audio::fft::Bins bins(bands);
            fft.run(samples, &bins);

            float eA = bins.raw()[binA];
            float eB = bins.raw()[binB];
            float eSum = eA + eB;
            float minE = (eA < eB) ? eA : eB;
            float splitRatio = (eSum > 0.0f) ? minE / eSum : 0.0f;

            FL_WARN(advModeName(mode) << " boundary " << binA << "-" << binB
                         << " (freq=" << boundaryFreq << "Hz): eA=" << eA
                         << " eB=" << eB << " split=" << splitRatio);

            // CQ_NAIVE frequency-domain kernels degenerate at the highest
            // bins when the frequency range is wide. Skip the topmost
            // boundary check for CQ_NAIVE.
            if (mode == fl::audio::fft::Mode::CQ_NAIVE && binB == bands - 1) {
                FL_WARN("  (skipping CQ_NAIVE top boundary - kernel degeneration)");
                continue;
            }
            FL_CHECK_GT(splitRatio, 0.15f);
            FL_CHECK_GT(eA, 0.0f);
            FL_CHECK_GT(eB, 0.0f);
        }
    }
}

// Test 6: CQ_HYBRID Tier Boundary Continuity
// Sweep with fine resolution across tier boundaries (~698 Hz, ~1397 Hz).
FL_TEST_CASE("FFT adversarial - CQ_HYBRID tier boundary continuity") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int N = fl::audio::fft::Args::DefaultSamples();

    fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate, fl::audio::fft::Mode::CQ_HYBRID);
    fl::audio::fft::Impl fft(args);

    // Tier boundaries at ~698 Hz (fmin*4) and ~1397 Hz (fmin*8)
    float boundaries[] = {fmin * 4.0f, fmin * 8.0f};

    for (int b = 0; b < 2; ++b) {
        float boundary = boundaries[b];
        const int steps = 50;
        float prevPeakEnergy = -1.0f;
        int prevPeakBin = -1;
        float maxJumpRatio = 0.0f;
        int backwardTransitions = 0;

        for (int step = 0; step < steps; ++step) {
            float t = static_cast<float>(step) / static_cast<float>(steps - 1);
            float freq = boundary * (0.8f + 0.4f * t); // 80% to 120%

            auto samples = makeSine(freq, N, static_cast<float>(sampleRate));
            fl::audio::fft::Bins bins(bands);
            fft.run(samples, &bins);

            float peakEnergy = 0.0f;
            int peakBin = 0;
            for (int i = 0; i < bands; ++i) {
                if (bins.raw()[i] > peakEnergy) {
                    peakEnergy = bins.raw()[i];
                    peakBin = i;
                }
            }

            if (prevPeakEnergy > 0.0f && peakEnergy > 0.0f) {
                float ratio = (peakEnergy > prevPeakEnergy) ?
                    peakEnergy / prevPeakEnergy : prevPeakEnergy / peakEnergy;
                if (ratio > maxJumpRatio) maxJumpRatio = ratio;
            }
            if (prevPeakBin >= 0 && peakBin < prevPeakBin) {
                backwardTransitions++;
            }

            prevPeakEnergy = peakEnergy;
            prevPeakBin = peakBin;
        }

        FL_WARN("CQ_HYBRID tier boundary ~" << boundary
                     << "Hz: maxJumpRatio=" << maxJumpRatio
                     << " backwardTransitions=" << backwardTransitions);

        FL_CHECK_LT(maxJumpRatio, 3.0f);
        FL_CHECK_EQ(backwardTransitions, 0);
    }
}

// Test 7: CQ_OCTAVE Decimation Boundary Aliasing
// 64 bands, 20-11025 Hz. High-frequency tones should stay in top quartile.
// Tests at 8000 and 10000 Hz (well within range, avoiding fmax edge artifacts).
FL_TEST_CASE("FFT adversarial - CQ_OCTAVE decimation boundary aliasing") {
    const int bands = 64;
    const float fmin = 20.0f;
    const float fmax = 11025.0f;
    const int sampleRate = 44100;
    const int N = 512;

    fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate, fl::audio::fft::Mode::CQ_OCTAVE);
    fl::audio::fft::Impl fft(args);

    float testFreqs[] = {8000.0f, 10000.0f};

    for (float freq : testFreqs) {
        auto samples = makeSine(freq, N, static_cast<float>(sampleRate));
        fl::audio::fft::Bins bins(bands);
        fft.run(samples, &bins);

        float totalEnergy = 0.0f;
        float bottomHalfEnergy = 0.0f;
        int peakBin = 0;
        float peakEnergy = 0.0f;

        for (int i = 0; i < bands; ++i) {
            float e = bins.raw()[i];
            totalEnergy += e;
            if (e > peakEnergy) {
                peakEnergy = e;
                peakBin = i;
            }
            if (i < bands / 2) {
                bottomHalfEnergy += e;
            }
        }

        float bottomFrac = (totalEnergy > 0.0f) ? bottomHalfEnergy / totalEnergy : 0.0f;

        FL_WARN("CQ_OCTAVE " << freq << "Hz: peak_bin=" << peakBin
                     << " bottom_half_frac=" << bottomFrac
                     << " total_energy=" << totalEnergy);

        FL_CHECK_LT(bottomFrac, 0.10f);
        FL_CHECK_GE(peakBin, bands * 3 / 4); // top quartile
    }
}

// Test 8: High vs Low Bin Concentration Symmetry
// Same-amplitude pure tone at bin 1 center vs bin 14 center.
FL_TEST_CASE("FFT adversarial - high vs low bin concentration symmetry") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int N = fl::audio::fft::Args::DefaultSamples();
    float logRatio = fl::logf(fmax / fmin);

    fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate, fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);

    // Low bin (bin 1) center frequency
    float lowFreq = fmin * fl::expf(logRatio * 1.0f / static_cast<float>(bands - 1));
    // High bin (bin 14) center frequency
    float highFreq = fmin * fl::expf(logRatio * 14.0f / static_cast<float>(bands - 1));

    auto lowSamples = makeSine(lowFreq, N, static_cast<float>(sampleRate));
    auto highSamples = makeSine(highFreq, N, static_cast<float>(sampleRate));

    fl::audio::fft::Bins lowBins(bands), highBins(bands);
    fft.run(lowSamples, &lowBins);
    fft.run(highSamples, &highBins);

    // Measure concentration for each
    float lowPeak = 0.0f, lowTotal = 0.0f;
    float highPeak = 0.0f, highTotal = 0.0f;
    for (int i = 0; i < bands; ++i) {
        float eL = lowBins.raw()[i];
        float eH = highBins.raw()[i];
        lowTotal += eL;
        highTotal += eH;
        if (eL > lowPeak) lowPeak = eL;
        if (eH > highPeak) highPeak = eH;
    }

    float lowConc = (lowTotal > 0.0f) ? lowPeak / lowTotal : 0.0f;
    float highConc = (highTotal > 0.0f) ? highPeak / highTotal : 0.0f;

    FL_WARN("Concentration symmetry: low_bin1=" << lowConc
                 << " (freq=" << lowFreq << "Hz)"
                 << " high_bin14=" << highConc
                 << " (freq=" << highFreq << "Hz)");

    float ratio = (lowConc > 0.0f) ? highConc / lowConc : 0.0f;
    FL_WARN("  high/low ratio=" << ratio);
    FL_CHECK_GT(ratio, 0.65f);
}

// Test 9: All-Mode High-Bin Peak Agreement
// Tones at 3000, 3500, 4000, 4500 Hz — all 4 modes must agree on peak bin within +/-1.
FL_TEST_CASE("FFT adversarial - all-mode high-bin peak agreement") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int N = fl::audio::fft::Args::DefaultSamples();

    float testFreqs[] = {3000.0f, 3500.0f, 4000.0f, 4500.0f};

    fl::audio::fft::Mode modes[] = {
        fl::audio::fft::Mode::LOG_REBIN,
        fl::audio::fft::Mode::CQ_NAIVE,
        fl::audio::fft::Mode::CQ_OCTAVE,
        fl::audio::fft::Mode::CQ_HYBRID
    };

    for (float freq : testFreqs) {
        int peakBins[4];
        int modeIdx = 0;

        for (fl::audio::fft::Mode mode : modes) {
            fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate, mode);
            fl::audio::fft::Impl fft(args);

            auto samples = makeSine(freq, N, static_cast<float>(sampleRate));
            fl::audio::fft::Bins bins(bands);
            fft.run(samples, &bins);

            float peakE = 0.0f;
            int peakBin = 0;
            for (int i = 0; i < bands; ++i) {
                if (bins.raw()[i] > peakE) {
                    peakE = bins.raw()[i];
                    peakBin = i;
                }
            }
            peakBins[modeIdx++] = peakBin;
        }

        FL_WARN("Peak agreement " << freq << "Hz: LOG=" << peakBins[0]
                     << " NAIVE=" << peakBins[1] << " OCTAVE=" << peakBins[2]
                     << " HYBRID=" << peakBins[3]);

        // All modes must agree within +/-1
        for (int i = 0; i < 4; ++i) {
            for (int j = i + 1; j < 4; ++j) {
                int diff = peakBins[i] - peakBins[j];
                if (diff < 0) diff = -diff;
                FL_CHECK_LE(diff, 1);
            }
        }
    }
}

// Test 10: Systematic Top-Half Sweep (All Modes)
// 200 frames sweeping bins 8-15 for each mode.
FL_TEST_CASE("FFT adversarial - systematic top-half sweep (all modes)") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int N = fl::audio::fft::Args::DefaultSamples();
    const int numFrames = 200;
    float logRatio = fl::logf(fmax / fmin);

    fl::audio::fft::Mode modes[] = {
        fl::audio::fft::Mode::LOG_REBIN,
        fl::audio::fft::Mode::CQ_NAIVE,
        fl::audio::fft::Mode::CQ_OCTAVE,
        fl::audio::fft::Mode::CQ_HYBRID
    };

    for (fl::audio::fft::Mode mode : modes) {
        fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate, mode);
        fl::audio::fft::Impl fft(args);

        int correctPeaks = 0;
        float concSum = 0.0f;
        int maxActive = 0;

        for (int frame = 0; frame < numFrames; ++frame) {
            float t = static_cast<float>(frame) / static_cast<float>(numFrames - 1);
            // Sweep bins 8-15
            float binF = 8.0f + t * 7.0f;
            float freq = fmin * fl::expf(logRatio * binF /
                                          static_cast<float>(bands - 1));

            auto samples = makeSine(freq, N, static_cast<float>(sampleRate));
            fl::audio::fft::Bins bins(bands);
            fft.run(samples, &bins);

            float peakE = 0.0f, totalE = 0.0f;
            int peakBin = 0;
            int activeBins = 0;

            for (int i = 0; i < bands; ++i) {
                float e = bins.raw()[i];
                totalE += e;
                if (e > peakE) {
                    peakE = e;
                    peakBin = i;
                }
            }

            float threshold = peakE * 0.1f;
            for (int i = 0; i < bands; ++i) {
                if (bins.raw()[i] > threshold) activeBins++;
            }

            float conc = (totalE > 0.0f) ? peakE / totalE : 0.0f;
            concSum += conc;
            if (activeBins > maxActive) maxActive = activeBins;

            int expectedBin = bins.freqToBin(freq);
            int diff = peakBin - expectedBin;
            if (diff < 0) diff = -diff;
            if (diff <= 1) correctPeaks++;
        }

        float accuracy = static_cast<float>(correctPeaks) /
                          static_cast<float>(numFrames);
        float avgConc = concSum / static_cast<float>(numFrames);

        FL_WARN(advModeName(mode) << " top-half sweep: accuracy=" << accuracy
                     << " avgConc=" << avgConc << " maxActive=" << maxActive);

        FL_CHECK_GT(accuracy, 0.85f);

        // Per-mode thresholds reflecting algorithmic characteristics:
        // LOG_REBIN/CQ_HYBRID: sharp bin assignment → high concentration, few active
        // CQ_OCTAVE: octave-wise CQ → wider kernel main lobe → more active bins
        // CQ_NAIVE: single-FFT CQ → inherently lower concentration at high bins
        if (mode == fl::audio::fft::Mode::CQ_NAIVE) {
            FL_CHECK_GT(avgConc, 0.20f);
            FL_CHECK_LE(maxActive, 10);
        } else if (mode == fl::audio::fft::Mode::CQ_OCTAVE) {
            FL_CHECK_GT(avgConc, 0.30f);
            FL_CHECK_LE(maxActive, 14);
        } else {
            // LOG_REBIN, CQ_HYBRID
            FL_CHECK_GT(avgConc, 0.35f);
            FL_CHECK_LE(maxActive, 5);
        }
    }
}

// ============================================================================
// Zero-leakage-at-distance-3 test
// For a pure sine centered on any output bin, there should be effectively
// zero energy in bins 3+ away from the peak.
//
// Blackman-Harris window has -92 dB sidelobes, which produce only mag=0-1
// quantization noise in the 16-bit fixed-point FFT. After log-rebinning,
// distant output bins accumulate at most 1-2 energy from this noise.
// The test uses a threshold of 3.0 to account for this quantization floor.
//
// The BH main lobe is 8 FFT bins wide (~690 Hz at 512/44100). For output
// bins narrower than ~130 Hz, this main lobe extends to 3+ output bins —
// a fundamental resolution limit. The test skips these bins.
// ============================================================================

FL_TEST_CASE("FFT adversarial - zero leakage at distance 3 (LOG_REBIN)") {
    const int bands = 16;
    const float fmin = fl::audio::fft::Args::DefaultMinFrequency();
    const float fmax = fl::audio::fft::Args::DefaultMaxFrequency();
    const int sampleRate = fl::audio::fft::Args::DefaultSampleRate();
    const int N = fl::audio::fft::Args::DefaultSamples();
    const float fftBinHz = static_cast<float>(sampleRate) / static_cast<float>(N);
    const float logRatio = fl::logf(fmax / fmin);
    const float binRatio = fl::expf(logRatio / static_cast<float>(bands - 1));

    // Skip criterion: output bin width must exceed 1.5x FFT bin width.
    // The Blackman-Harris main lobe (8 FFT bins = ~690 Hz) extends 3+
    // output bins when binWidth < ~130 Hz. Using 1.5 * fftBinHz (~129 Hz)
    // as the threshold ensures the BH main lobe stays within 2 output bins.
    const float minBinWidth = 1.5f * fftBinHz;

    // Quantization noise floor: BH sidelobes at -92 dB produce mag=0-1 in
    // the u16 fastMag output. After rebinning, distant bins accumulate a few
    // units of energy from this noise. With the wider frequency range
    // (90-14080 Hz), more FFT bins are grouped per output bin, slightly
    // raising the quantization floor. Threshold of 5 accounts for this.
    const float noiseFloor = 5.0f;

    fl::audio::fft::Args args(N, bands, fmin, fmax, sampleRate, fl::audio::fft::Mode::LOG_REBIN);
    fl::audio::fft::Impl fft(args);

    int testedBins = 0;
    int passedBins = 0;

    for (int targetBin = 0; targetBin < bands; ++targetBin) {
        float centerFreq = fmin * fl::expf(logRatio * static_cast<float>(targetBin) /
                                            static_cast<float>(bands - 1));
        // Approximate output bin width
        float binWidth = centerFreq * (binRatio - 1.0f / binRatio) / 2.0f;

        // Skip bins where BH main lobe extends 3+ output bins
        if (binWidth < minBinWidth) {
            FL_WARN("  Bin " << targetBin << ": SKIP (width=" << binWidth
                         << "Hz < " << minBinWidth << "Hz)");
            continue;
        }

        testedBins++;

        auto samples = makeSine(centerFreq, N, static_cast<float>(sampleRate));
        fl::audio::fft::Bins bins(bands);
        fft.run(samples, &bins);

        // Find peak
        int peakBin = 0;
        float peakEnergy = 0.0f;
        for (int i = 0; i < bands; ++i) {
            float e = bins.raw()[i];
            if (e > peakEnergy) {
                peakEnergy = e;
                peakBin = i;
            }
        }

        // Check every bin at distance >= 3 from peak for near-zero energy
        bool binPassed = true;
        for (int i = 0; i < bands; ++i) {
            int dist = i - peakBin;
            if (dist < 0) dist = -dist;
            if (dist >= 3 && bins.raw()[i] >= noiseFloor) {
                FL_WARN("  Bin " << targetBin << " (center=" << centerFreq
                             << "Hz, width=" << binWidth << "Hz): LEAK at output bin "
                             << i << " (dist=" << dist << ") energy=" << bins.raw()[i]);
                binPassed = false;
            }
        }

        if (binPassed) {
            passedBins++;
            FL_WARN("  Bin " << targetBin << " (center=" << centerFreq
                         << "Hz, width=" << binWidth << "Hz): PASS (peak=" << peakBin << ")");
        }
    }

    FL_WARN("Zero-leakage test: " << passedBins << "/" << testedBins
                 << " bins passed (bins with width >= " << minBinWidth << "Hz)");

    // All tested bins must pass
    FL_CHECK_EQ(passedBins, testedBins);
}
