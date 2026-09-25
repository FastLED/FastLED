// FFT CQ Mode Performance & Accuracy Comparison
//
// Benchmarks LOG_REBIN, CQ_NAIVE, CQ_HYBRID, and CQ_OCTAVE to compare
// speed, spectral leakage, and accuracy.
//
// Usage:
//   ./fft_cq_compare.exe baseline    # JSON output
//   ./fft_cq_compare.exe report      # Human-readable report

// IWYU pragma: begin_keep
#include "third_party/cq_kernel/cq_kernel.h"
#include "third_party/cq_kernel/kiss_fftr.h"
// IWYU pragma: end_keep
#include "fl/audio/fft/fft.h"
#include "fl/audio/fft/fft_impl.h"
#include "fl/stl/int.h"
#include "fl/math/math.h"
#include "fl/math/random.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "profile_result.h"

namespace fl {

using audio::fft::Args;
using audio::fft::Bins;
using audio::fft::Impl;
using audio::fft::Mode;

// Generate a synthetic audio buffer: mix of sine waves spanning the
// frequency range to exercise all CQ bins.
static void generateTestSignal(fl::vector<fl::i16> &buf, int samples,
                               int sampleRate) {
    buf.resize(samples);
    const float freqs[] = {200.0f, 500.0f, 1200.0f, 3000.0f};
    const float amps[] = {0.4f, 0.3f, 0.2f, 0.1f};
    for (int i = 0; i < samples; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float signal = 0.0f;
        for (int f = 0; f < 4; f++) {
            signal +=
                amps[f] * fl::sinf(2.0f * static_cast<float>(FL_M_PI) * freqs[f] * t);
        }
        buf[i] = static_cast<fl::i16>(signal * 25000.0f);
    }
}

// Generate white noise (flat spectrum, equal energy per Hz)
static void generateWhiteNoise(fl::vector<fl::i16> &buf, int samples) {
    buf.resize(samples);
    fl_random rng(12345);
    for (int i = 0; i < samples; i++) {
        fl::i32 val = static_cast<fl::i32>(rng.random16()) - 32768;
        buf[i] = static_cast<fl::i16>(val * 20000 / 32768);
    }
}

// Generate pink noise (1/f spectrum, equal energy per octave)
static void generatePinkNoise(fl::vector<fl::i16> &buf, int samples) {
    buf.resize(samples);
    fl_random rng(54321);
    const int NUM_OCTAVES = 8;
    fl::i32 octaveValues[NUM_OCTAVES] = {};
    for (int o = 0; o < NUM_OCTAVES; o++) {
        octaveValues[o] = static_cast<fl::i32>(rng.random16()) - 32768;
    }
    for (int i = 0; i < samples; i++) {
        for (int o = 0; o < NUM_OCTAVES; o++) {
            if ((i & ((1 << o) - 1)) == 0) {
                octaveValues[o] =
                    static_cast<fl::i32>(rng.random16()) - 32768;
            }
        }
        fl::i32 sum = 0;
        for (int o = 0; o < NUM_OCTAVES; o++) {
            sum += octaveValues[o];
        }
        buf[i] = static_cast<fl::i16>(sum / NUM_OCTAVES * 20000 / 32768);
    }
}

// Generate a pure sine wave at a specific frequency
static void generateSine(fl::vector<fl::i16> &buf, int samples,
                         int sampleRate, float freq) {
    buf.resize(samples);
    for (int i = 0; i < samples; i++) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float s = fl::sinf(2.0f * static_cast<float>(FL_M_PI) * freq * t);
        buf[i] = static_cast<fl::i16>(s * 30000.0f);
    }
}

struct BenchResult {
    const char *name;
    fl::i64 totalNs;
    int iterations;
    double nsPerCall() const {
        return static_cast<double>(totalNs) / iterations;
    }
};

static BenchResult benchMode(Mode mode, const char *name,
                             fl::span<const fl::i16> signal, int bands,
                             float fmin, float fmax, int sampleRate,
                             int iterations) {
    Args args(static_cast<int>(signal.size()), bands, fmin, fmax,
                  sampleRate, mode);
    Impl fft(args);
    Bins out(bands);

    for (int i = 0; i < 10; i++) {
        fft.run(signal, &out);
    }

    fl::u32 t0 = fl::micros();
    for (int i = 0; i < iterations; i++) {
        fft.run(signal, &out);
    }
    fl::u32 t1 = fl::micros();

    fl::i64 ns = static_cast<fl::i64>(t1 - t0) * 1000LL;
    return {name, ns, iterations};
}

// Find the bin with the highest magnitude
static int peakBin(fl::span<const float> bins) {
    int best = 0;
    float bestVal = 0.0f;
    for (int i = 0; i < static_cast<int>(bins.size()); i++) {
        if (bins[i] > bestVal) {
            bestVal = bins[i];
            best = i;
        }
    }
    return best;
}

// Spectral leakage: fraction of total energy NOT in peak bin +/- 1
static float leakageRatio(fl::span<const float> bins, int peak) {
    float total = 0.0f;
    float peakEnergy = 0.0f;
    for (int i = 0; i < static_cast<int>(bins.size()); i++) {
        float e = bins[i] * bins[i];
        total += e;
        if (i >= peak - 1 && i <= peak + 1) {
            peakEnergy += e;
        }
    }
    if (total <= 0.0f) return 1.0f;
    return 1.0f - (peakEnergy / total);
}

struct AccuracyResult {
    const char *modeName;
    float testFreq;
    float expectedBinFreq;
    float actualBinFreq;
    int expectedBin;
    int actualBin;
    int binError;
    float leakage;
};

static void runAccuracyTest(Mode mode, const char *modeName,
                            int samples, int bands, float fmin, float fmax,
                            int sampleRate, const float *testFreqs, int nFreqs,
                            fl::vector<AccuracyResult> &results) {
    Args args(samples, bands, fmin, fmax, sampleRate, mode);
    Impl fft(args);
    Bins out(bands);
    fl::vector<fl::i16> signal;

    for (int f = 0; f < nFreqs; f++) {
        generateSine(signal, samples, sampleRate, testFreqs[f]);
        fft.run(signal, &out);

        int expected = out.freqToBin(testFreqs[f]);
        int actual = peakBin(out.raw());
        float leak = leakageRatio(out.raw(), actual);

        AccuracyResult r;
        r.modeName = modeName;
        r.testFreq = testFreqs[f];
        r.expectedBinFreq = out.binToFreq(expected);
        r.actualBinFreq = out.binToFreq(actual);
        r.expectedBin = expected;
        r.actualBin = actual;
        r.binError = (actual > expected) ? (actual - expected)
                                         : (expected - actual);
        r.leakage = leak;
        results.push_back(r);
    }
}

struct ModeEntry {
    Mode mode;
    const char *label;
};

static const ModeEntry MODES[] = {
    {Mode::LOG_REBIN, "LOG_REB"},
    {Mode::CQ_NAIVE, "NAIVE"},
    {Mode::CQ_HYBRID, "HYBRID"},
    {Mode::CQ_OCTAVE, "OCTAVE"},
};
static const int NUM_MODES = 4;

static void printAccuracyReport(int samples, int bands, float fmin,
                                float fmax, int sampleRate) {
    const float testFreqs[] = {200.0f, 330.0f, 440.0f, 660.0f, 880.0f,
                               1000.0f, 1500.0f, 2000.0f, 3000.0f, 4000.0f};
    const int nFreqs = 10;

    fl::vector<AccuracyResult> results;

    fl::printf("======================================================\n");
    fl::printf("    FFT Mode Accuracy Comparison\n");
    fl::printf("======================================================\n");

    for (int m = 0; m < NUM_MODES; m++) {
        runAccuracyTest(MODES[m].mode, MODES[m].label, samples, bands,
                        fmin, fmax, sampleRate, testFreqs, nFreqs, results);
    }

    fl::printf("\n--- %.1f-%.1f Hz, %d bins ---\n", fmin, fmax, bands);
    fl::printf("%-12s %8s %8s %8s %6s %8s\n",
               "Mode", "SineHz", "ExpHz", "GotHz", "BinErr", "Leakage");
    fl::printf("-------------------------------------------------------------\n");
    for (const auto &r : results) {
        fl::printf("%-12s %7.0f %8.1f %8.1f %5d  %6.1f%%\n",
                   r.modeName, r.testFreq, r.expectedBinFreq,
                   r.actualBinFreq, r.binError, r.leakage * 100.0f);
    }

    // Average leakage per mode
    fl::printf("\n--- Average Leakage by Mode ---\n");
    for (int m = 0; m < NUM_MODES; m++) {
        float totalLeak = 0.0f;
        int count = 0;
        for (const auto &r : results) {
            if (fl::strcmp(r.modeName, MODES[m].label) == 0) {
                totalLeak += r.leakage;
                count++;
            }
        }
        if (count > 0) {
            fl::printf("  %-12s avg leakage: %5.1f%%\n", MODES[m].label,
                       totalLeak / static_cast<float>(count) * 100.0f);
        }
    }
    fl::printf("\n");
}

static void benchSignal(const char *signalName,
                        fl::span<const fl::i16> signal, int bands,
                        float fmin, float fmax, int sampleRate,
                        int iterations) {
    fl::printf("  %-8s", signalName);
    for (int m = 0; m < NUM_MODES; m++) {
        BenchResult r = benchMode(MODES[m].mode, MODES[m].label, signal,
                                  bands, fmin, fmax, sampleRate, iterations);
        fl::printf(" %8.1f", r.nsPerCall() / 1000.0);
    }
    fl::printf("\n");
}

int runBenchmark(bool jsonOutput) {
    const int SAMPLES = 512;
    const int SAMPLE_RATE = 44100;
    const int ITERATIONS = 2000;
    const float fmin = Args::DefaultMinFrequency();
    const float fmax = Args::DefaultMaxFrequency();
    const int bands = 64;

    fl::vector<fl::i16> sines;
    generateTestSignal(sines, SAMPLES, SAMPLE_RATE);
    fl::vector<fl::i16> white;
    generateWhiteNoise(white, SAMPLES);
    fl::vector<fl::i16> pink;
    generatePinkNoise(pink, SAMPLES);

    if (jsonOutput) {
        fl::span<const fl::i16> signals[] = {sines, white, pink};
        const char *names[] = {"sines", "white", "pink"};
        for (int s = 0; s < 3; s++) {
            BenchResult r = benchMode(Mode::CQ_HYBRID, names[s], signals[s],
                                      bands, fmin, fmax, SAMPLE_RATE,
                                      ITERATIONS);
            ProfileResultBuilder::print_result(
                "baseline", r.name, r.iterations,
                static_cast<fl::u32>(r.totalNs / 1000));
        }
    } else {
        fl::printf("\n");
        fl::printf("======================================================\n");
        fl::printf("    FFT Mode Performance Comparison\n");
        fl::printf("======================================================\n");
        fl::printf("\n");
        fl::printf("Config: %d samples, %d Hz, %d bands, %.0f-%.0fHz\n",
                   SAMPLES, SAMPLE_RATE, bands, fmin, fmax);
        fl::printf("Iterations: %d per measurement\n", ITERATIONS);
        fl::printf("\n");

        fl::printf("  %-8s", "Signal");
        for (int m = 0; m < NUM_MODES; m++) {
            fl::printf(" %8s", MODES[m].label);
        }
        fl::printf("\n");
        fl::printf("  --------------------------------------------------\n");

        benchSignal("sines", sines, bands, fmin, fmax, SAMPLE_RATE,
                    ITERATIONS);
        benchSignal("white", white, bands, fmin, fmax, SAMPLE_RATE,
                    ITERATIONS);
        benchSignal("pink", pink, bands, fmin, fmax, SAMPLE_RATE,
                    ITERATIONS);

        fl::printf("\n  (all values in us/call)\n");

        // Head-to-head: LOG_REBIN vs HYBRID in same timing window
        {
            Args argsLR(SAMPLES, bands, fmin, fmax, SAMPLE_RATE,
                            Mode::LOG_REBIN);
            Args argsHY(SAMPLES, bands, fmin, fmax, SAMPLE_RATE,
                            Mode::CQ_HYBRID);
            Impl fftLR(argsLR);
            Impl fftHY(argsHY);
            Bins outLR(bands), outHY(bands);

            // Warm-up
            for (int i = 0; i < 20; i++) {
                fftLR.run(sines, &outLR);
                fftHY.run(sines, &outHY);
            }

            fl::u32 t0 = fl::micros();
            for (int i = 0; i < ITERATIONS; i++)
                fftLR.run(sines, &outLR);
            fl::u32 t1 = fl::micros();
            for (int i = 0; i < ITERATIONS; i++)
                fftHY.run(sines, &outHY);
            fl::u32 t2 = fl::micros();

            float lrUs = static_cast<float>(t1 - t0) / ITERATIONS;
            float hyUs = static_cast<float>(t2 - t1) / ITERATIONS;
            fl::printf("\n  Head-to-head (sines, %d iters):\n", ITERATIONS);
            fl::printf("    LOG_REBIN: %.1f us/call\n", lrUs);
            fl::printf("    HYBRID:    %.1f us/call\n", hyUs);
            fl::printf("    Ratio:     %.2fx\n", hyUs / lrUs);
        }

        fl::printf("\n");

        // Accuracy comparison
        printAccuracyReport(SAMPLES, bands, fmin, fmax, SAMPLE_RATE);
    }

    return 0;
}

} // namespace fl

extern "C" void runner_setup_crash_handler();

int main(int argc, char *argv[]) {
    runner_setup_crash_handler();
    bool jsonOutput = (argc > 1 && fl::strcmp(argv[1], "baseline") == 0);
    return fl::runBenchmark(jsonOutput);
}
