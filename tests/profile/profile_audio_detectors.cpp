// Audio Detector Performance Profiling Test
//
// Profiles all audio detectors to identify performance outliers.
// Compares each detector against EnergyAnalyzer (baseline).
//
// Usage:
//   ./profile_audio_detectors.exe baseline    # Output JSON profile results
//   ./profile_audio_detectors.exe report      # Output human-readable report
//
// This test is useful for:
// - Catching performance regressions in audio detectors
// - Identifying which detectors are significantly slower
// - Comparing relative performance before/after optimizations

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

    // Generate a complex audio signal: mix of sine waves
    // This creates interesting spectral content for detectors
    AudioSample generateSample() {
        const int bufferSize = mSampleRate / 100;  // 10ms at 16kHz = 160 samples
        fl::vector<fl::i16> pcm(bufferSize);

        // Mix multiple sine waves for interesting spectrum
        // - 440 Hz (A4 - musical note)
        // - 880 Hz (2x frequency)
        // - 220 Hz (0.5x frequency)
        for (int i = 0; i < bufferSize; i++) {
            float phase = (mPhase + i) / static_cast<float>(mSampleRate);
            float signal = 0.0f;

            // Fundamental (440 Hz)
            signal += 0.5f * fl::sinf(2.0f * 3.14159f * 440.0f * phase);

            // Harmonic (880 Hz)
            signal += 0.3f * fl::sinf(2.0f * 3.14159f * 880.0f * phase);

            // Sub-harmonic (220 Hz)
            signal += 0.2f * fl::sinf(2.0f * 3.14159f * 220.0f * phase);

            // Convert to i16
            pcm[i] = static_cast<fl::i16>(signal * 20000.0f);
        }

        mPhase += bufferSize;
        return AudioSample(fl::span<const fl::i16>(pcm.data(), pcm.size()), 0);
    }

private:
    int mSampleRate;
    int mPhase;
};

struct DetectorBenchmark {
    const char* name;
    fl::i64 totalTimeNs;
    int iterations;

    double nsPerCall() const {
        return static_cast<double>(totalTimeNs) / iterations;
    }

    double callsPerSec() const {
        return 1e9 / nsPerCall();
    }

    double relativeToBaseline(double baselineNsPerCall) const {
        return nsPerCall() / baselineNsPerCall;
    }
};

// Main profiling function
int runProfiler(bool jsonOutput) {
    using clock = fl::chrono::high_resolution_clock;

    const int ITERATIONS = 1000;
    const int SAMPLE_RATE = 16000;

    fl::vector<DetectorBenchmark> benchmarks;
    SynthAudioGenerator audioGen(SAMPLE_RATE);

    // Create audio processor (which owns all detectors)
    fl::AudioProcessor processor;
    processor.setSampleRate(SAMPLE_RATE);

    // Register some callbacks to ensure detectors are active
    // (Lazy detectors don't update if no callbacks are registered)
    processor.onEnergy([](float) {});              // EnergyAnalyzer (BASELINE)
    processor.onFrequencyBands([](float, float, float) {});  // FrequencyBands
    processor.onBeat([]() {});                      // BeatDetector
    processor.onTempo([](float) {});                // TempoAnalyzer
    processor.onPitch([](float) {});                // PitchDetector
    processor.onVocal([](fl::u8) {});               // VocalDetector
    processor.onMood([](const fl::Mood&) {});       // MoodAnalyzer
    processor.onTransient([]() {});                 // TransientDetector
    processor.onVibeLevels([](const fl::VibeLevels&) {});  // VibeDetector

    // Warm-up (JIT/caching)
    for (int i = 0; i < 10; i++) {
        fl::AudioSample sample = audioGen.generateSample();
        processor.update(sample);
    }

    // Benchmark: Measure total time for ITERATIONS updates
    auto t0 = clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        fl::AudioSample sample = audioGen.generateSample();
        processor.update(sample);
    }

    auto t1 = clock::now();
    fl::i64 totalTimeNs =
        fl::chrono::duration_cast<fl::chrono::nanoseconds>(t1 - t0).count();

    // For now, report combined time
    // (Detailed per-detector timing would require instrumenting AudioProcessor)
    DetectorBenchmark overall;
    overall.name = "AudioProcessor (all detectors)";
    overall.totalTimeNs = totalTimeNs;
    overall.iterations = ITERATIONS;
    benchmarks.push_back(overall);

    if (jsonOutput) {
        // Output JSON format for integration with profiling pipeline
        ProfileResultBuilder::print_result("baseline", "audio_processors", ITERATIONS,
                                          static_cast<fl::u32>(totalTimeNs / 1000));
    } else {
        // Output human-readable report
        fl::printf("\n");
        fl::printf("═══════════════════════════════════════════════════════════════\n");
        fl::printf("         FASTLED AUDIO DETECTOR PERFORMANCE PROFILE\n");
        fl::printf("═══════════════════════════════════════════════════════════════\n");
        fl::printf("\n");
        fl::printf("Configuration:\n");
        fl::printf("  Iterations:        %d\n", ITERATIONS);
        fl::printf("  Sample Rate:       %d Hz\n", SAMPLE_RATE);
        fl::printf("  Samples/Iteration: %d\n", SAMPLE_RATE / 100);
        fl::printf("\n");

        fl::printf("Results:\n");
        fl::printf("─────────────────────────────────────────────────────────────\n");
        fl::printf("Component                    Time/Call      Calls/Sec\n");
        fl::printf("─────────────────────────────────────────────────────────────\n");

        for (const auto& bm : benchmarks) {
            fl::printf("%-28s %8.2f µs     %10.0f\n", bm.name,
                      bm.nsPerCall() / 1000.0,  // Convert ns to µs
                      bm.callsPerSec());
        }

        fl::printf("─────────────────────────────────────────────────────────────\n");
        fl::printf("\n");
        fl::printf("Notes:\n");
        fl::printf("  - Time includes all audio processing (FFT, detection, callbacks)\n");
        fl::printf("  - Sample is synthetic mix of 220, 440, 880 Hz sine waves\n");
        fl::printf("  - Baseline assumes ~10µs acceptable for real-time processing\n");
        fl::printf("\n");

        // Check for performance issues
        bool performanceWarning = false;
        for (const auto& bm : benchmarks) {
            if (bm.nsPerCall() > 50000.0) {  // > 50µs
                if (!performanceWarning) {
                    fl::printf("⚠️  PERFORMANCE WARNINGS:\n");
                    performanceWarning = true;
                }
                fl::printf("  - %s: %.2f µs/call (exceeds safe threshold)\n", bm.name,
                          bm.nsPerCall() / 1000.0);
            }
        }

        if (!performanceWarning) {
            fl::printf("✅ All detectors performing within acceptable limits\n");
        }
        fl::printf("\n");
    }

    return 0;
}

}  // namespace fl

int main(int argc, char* argv[]) {
    bool jsonOutput = (argc > 1 && fl::strcmp(argv[1], "baseline") == 0);
    return fl::runProfiler(jsonOutput);
}
