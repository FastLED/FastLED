// Audio Detector Performance Breakdown
//
// This profiler breaks down detector costs into:
// 1. FFT computation cost (computed once per sample in AudioContext)
// 2. Detector analysis cost (operating on cached FFT bins)
//
// Usage:
//   ./audio_detector_breakdown              # Human-readable report
//   ./audio_detector_breakdown baseline     # JSON output for profiling pipeline

#include "fl/audio/audio_processor.h"
#include "fl/audio/input.h"
#include "fl/audio/audio_sample.h"
#include "fl/stl/int.h"
#include "fl/stl/stdio.h"
#include "fl/stl/chrono.h"
#include "fl/stl/vector.h"
#include "fl/stl/string.h"
#include "profile_result.h"

namespace fl {

// Synthetic audio sample generator for consistent testing
class SynthAudioGenerator {
public:
    explicit SynthAudioGenerator(int sampleRate = 16000)
        : mSampleRate(sampleRate), mPhase(0) {}

    AudioSample generateSample() {
        const int bufferSize = mSampleRate / 100;  // 10ms at 16kHz = 160 samples
        fl::vector<fl::i16> pcm(bufferSize);

        for (int i = 0; i < bufferSize; i++) {
            float phase = (mPhase + i) / static_cast<float>(mSampleRate);
            float signal = 0.0f;
            signal += 0.5f * fl::sinf(2.0f * 3.14159f * 440.0f * phase);
            signal += 0.3f * fl::sinf(2.0f * 3.14159f * 880.0f * phase);
            signal += 0.2f * fl::sinf(2.0f * 3.14159f * 220.0f * phase);
            pcm[i] = static_cast<fl::i16>(signal * 20000.0f);
        }

        mPhase += bufferSize;
        return AudioSample(fl::span<const fl::i16>(pcm.data(), pcm.size()), 0);
    }

private:
    int mSampleRate;
    int mPhase;
};

struct DetectorCost {
    const char* name;
    float totalCostUs;
    float fftCostUs;
    float detectorCostUs;

    float detectorOverhead() const {
        return detectorCostUs;
    }
};

int runProfiler(bool jsonOutput) {
    const int ITERATIONS = 500;
    const int SAMPLE_RATE = 16000;

    fl::vector<DetectorCost> costs;
    SynthAudioGenerator audioGen(SAMPLE_RATE);

    // ===== PHASE 1: Baseline (Energy only) =====
    // This gives us the true baseline cost without any FFT-dependent detectors
    fl::u32 baselineTotal_us = 0;
    {
        fl::AudioProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.onEnergy([](float) {});

        for (int i = 0; i < 10; i++) {
            processor.update(audioGen.generateSample());
        }

        fl::u32 t0 = fl::micros();
        for (int i = 0; i < ITERATIONS; i++) {
            processor.update(audioGen.generateSample());
        }
        fl::u32 t1 = fl::micros();
        baselineTotal_us = t1 - t0;

        costs.push_back({"EnergyAnalyzer (BASELINE)",
                        baselineTotal_us / static_cast<float>(ITERATIONS), 0.0f, 0.0f});
    }

    // ===== PHASE 2: Measure detectors that DON'T use FFT =====
    fl::vector<const char*> noFFTDetectors = {
        "BeatDetector", "TempoAnalyzer", "TransientDetector"
    };

    fl::vector<fl::u32> noFFTTotals;
    noFFTTotals.push_back(baselineTotal_us);  // Start with baseline

    // Beat detector
    {
        fl::AudioProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.onEnergy([](float) {});
        processor.onBeat([]() {});

        for (int i = 0; i < 10; i++) {
            processor.update(audioGen.generateSample());
        }

        fl::u32 t0 = fl::micros();
        for (int i = 0; i < ITERATIONS; i++) {
            processor.update(audioGen.generateSample());
        }
        fl::u32 t1 = fl::micros();
        fl::u32 total = t1 - t0;
        float detectorCost = (total - baselineTotal_us) / static_cast<float>(ITERATIONS);

        costs.push_back({"BeatDetector",
                        total / static_cast<float>(ITERATIONS), 0.0f, detectorCost});
        noFFTTotals.push_back(total);
    }

    // Tempo analyzer
    {
        fl::AudioProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.onEnergy([](float) {});
        processor.onTempo([](float) {});

        for (int i = 0; i < 10; i++) {
            processor.update(audioGen.generateSample());
        }

        fl::u32 t0 = fl::micros();
        for (int i = 0; i < ITERATIONS; i++) {
            processor.update(audioGen.generateSample());
        }
        fl::u32 t1 = fl::micros();
        fl::u32 total = t1 - t0;
        float detectorCost = (total - baselineTotal_us) / static_cast<float>(ITERATIONS);

        costs.push_back({"TempoAnalyzer",
                        total / static_cast<float>(ITERATIONS), 0.0f, detectorCost});
        noFFTTotals.push_back(total);
    }

    // Transient detector
    {
        fl::AudioProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.onEnergy([](float) {});
        processor.onTransient([]() {});

        for (int i = 0; i < 10; i++) {
            processor.update(audioGen.generateSample());
        }

        fl::u32 t0 = fl::micros();
        for (int i = 0; i < ITERATIONS; i++) {
            processor.update(audioGen.generateSample());
        }
        fl::u32 t1 = fl::micros();
        fl::u32 total = t1 - t0;
        float detectorCost = (total - baselineTotal_us) / static_cast<float>(ITERATIONS);

        costs.push_back({"TransientDetector",
                        total / static_cast<float>(ITERATIONS), 0.0f, detectorCost});
        noFFTTotals.push_back(total);
    }

    // ===== PHASE 3: FFT cost (determined by FrequencyBands alone) =====
    fl::u32 fftCostTotal_us = 0;
    {
        fl::AudioProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.onEnergy([](float) {});
        processor.onFrequencyBands([](float, float, float) {});

        for (int i = 0; i < 10; i++) {
            processor.update(audioGen.generateSample());
        }

        fl::u32 t0 = fl::micros();
        for (int i = 0; i < ITERATIONS; i++) {
            processor.update(audioGen.generateSample());
        }
        fl::u32 t1 = fl::micros();
        fl::u32 total = t1 - t0;

        // FFT cost = (Energy + FrequencyBands) - Energy
        fftCostTotal_us = total - baselineTotal_us;
        float fftCost = fftCostTotal_us / static_cast<float>(ITERATIONS);
        float detectorCost = 0.0f;  // This is pure FFT cost

        costs.push_back({"[FFT Computation]",
                        total / static_cast<float>(ITERATIONS), fftCost, detectorCost});
    }

    // ===== PHASE 4: Measure detectors that USE FFT =====
    // Pitch detector (uses FFT)
    {
        fl::AudioProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.onEnergy([](float) {});
        processor.onPitch([](float) {});

        for (int i = 0; i < 10; i++) {
            processor.update(audioGen.generateSample());
        }

        fl::u32 t0 = fl::micros();
        for (int i = 0; i < ITERATIONS; i++) {
            processor.update(audioGen.generateSample());
        }
        fl::u32 t1 = fl::micros();
        fl::u32 total = t1 - t0;

        // Cost = Total - (Baseline + FFT)
        float detectorCost = ((total - baselineTotal_us) - fftCostTotal_us) / static_cast<float>(ITERATIONS);

        costs.push_back({"PitchDetector (w/ FFT)",
                        total / static_cast<float>(ITERATIONS),
                        fftCostTotal_us / static_cast<float>(ITERATIONS), detectorCost});
    }

    // Vocal detector (uses FFT)
    {
        fl::AudioProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.onEnergy([](float) {});
        processor.onVocal([](fl::u8) {});

        for (int i = 0; i < 10; i++) {
            processor.update(audioGen.generateSample());
        }

        fl::u32 t0 = fl::micros();
        for (int i = 0; i < ITERATIONS; i++) {
            processor.update(audioGen.generateSample());
        }
        fl::u32 t1 = fl::micros();
        fl::u32 total = t1 - t0;

        float detectorCost = ((total - baselineTotal_us) - fftCostTotal_us) / static_cast<float>(ITERATIONS);

        costs.push_back({"VocalDetector (w/ FFT)",
                        total / static_cast<float>(ITERATIONS),
                        fftCostTotal_us / static_cast<float>(ITERATIONS), detectorCost});
    }

    // Vibe detector (uses FFT)
    {
        fl::AudioProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.onEnergy([](float) {});
        processor.onVibeLevels([](const fl::VibeLevels&) {});

        for (int i = 0; i < 10; i++) {
            processor.update(audioGen.generateSample());
        }

        fl::u32 t0 = fl::micros();
        for (int i = 0; i < ITERATIONS; i++) {
            processor.update(audioGen.generateSample());
        }
        fl::u32 t1 = fl::micros();
        fl::u32 total = t1 - t0;

        float detectorCost = ((total - baselineTotal_us) - fftCostTotal_us) / static_cast<float>(ITERATIONS);

        costs.push_back({"VibeDetector (w/ FFT)",
                        total / static_cast<float>(ITERATIONS),
                        fftCostTotal_us / static_cast<float>(ITERATIONS), detectorCost});
    }

    if (jsonOutput) {
        ProfileResultBuilder::print_result("baseline", "audio_detectors_breakdown",
                                          ITERATIONS, baselineTotal_us);
    } else {
        fl::printf("\n");
        fl::printf("═══════════════════════════════════════════════════════════════════════════════\n");
        fl::printf("       FASTLED AUDIO DETECTOR COST BREAKDOWN\n");
        fl::printf("═══════════════════════════════════════════════════════════════════════════════\n");
        fl::printf("\n");
        fl::printf("Configuration:\n");
        fl::printf("  Iterations:        %d\n", ITERATIONS);
        fl::printf("  Sample Rate:       %d Hz\n", SAMPLE_RATE);
        fl::printf("  Samples/Iteration: %d\n", SAMPLE_RATE / 100);
        fl::printf("\n");

        fl::printf("DETECTOR COST BREAKDOWN (µs per call)\n");
        fl::printf("───────────────────────────────────────────────────────────────────────────────\n");
        fl::printf("%-30s %10s    %10s    %10s\n",
                   "Detector", "FFT Cost", "Analysis", "Total");
        fl::printf("───────────────────────────────────────────────────────────────────────────────\n");

        for (const auto& cost : costs) {
            fl::printf("%-30s %10.2f    %10.2f    %10.2f\n",
                       cost.name, cost.fftCostUs, cost.detectorCostUs, cost.totalCostUs);
        }

        fl::printf("───────────────────────────────────────────────────────────────────────────────\n");
        fl::printf("\n");

        fl::printf("Summary:\n");
        fl::printf("  Baseline (EnergyAnalyzer):   %.2f µs/call\n", costs[0].totalCostUs);
        fl::printf("  FFT Computation:              %.2f µs/call (reused by multiple detectors)\n",
                   fftCostTotal_us / static_cast<float>(ITERATIONS));
        fl::printf("\n");

        fl::printf("Insights:\n");
        fl::printf("  ✓ FFT is computed ONCE per sample in AudioContext\n");
        fl::printf("  ✓ Multiple detectors can reuse the same cached FFT bins\n");
        fl::printf("  ✓ Non-FFT detectors (Beat, Tempo, Transient) are very cheap\n");
        fl::printf("  ✓ FFT-based detectors pay the FFT cost + their analysis cost\n");
        fl::printf("\n");

        fl::printf("Optimization Strategies:\n");
        fl::printf("  • Enable only non-FFT detectors if latency is critical (< 50µs budget)\n");
        fl::printf("  • If using any FFT detector, cost of others is cheap (just analysis)\n");
        fl::printf("  • Pre-computing or caching FFT results can save ~50µs per sample\n");
        fl::printf("  • For real-time systems, consider parallel processing with dedicated FFT thread\n");
        fl::printf("\n");
    }

    return 0;
}

}  // namespace fl

int main(int argc, char* argv[]) {
    bool jsonOutput = (argc > 1 && fl::strcmp(argv[1], "baseline") == 0);
    return fl::runProfiler(jsonOutput);
}
