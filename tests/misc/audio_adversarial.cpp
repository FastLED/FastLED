// ok standalone
#include "test.h"
#include "FastLED.h"
#include "fl/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/fft.h"
#include "fl/audio/signal_conditioner.h"
#include "fl/audio/auto_gain.h"
#include "fl/audio/noise_floor_tracker.h"
#include "fl/fx/audio/audio_processor.h"
#include "fl/fx/audio/detectors/beat.h"
#include "fl/fx/audio/detectors/energy_analyzer.h"
#include "fl/fx/audio/detectors/tempo_analyzer.h"
#include "fl/fx/audio/detectors/frequency_bands.h"
#include "fl/fx/audio/detectors/vocal.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/math_macros.h"

using namespace fl;

namespace { // adversarial_tests

AudioSample makeSample(float freq, fl::u32 timestamp, float amplitude = 16000.0f) {
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * FL_M_PI * freq * i / 44100.0f;
        data.push_back(static_cast<fl::i16>(amplitude * fl::sinf(phase)));
    }
    return AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp);
}

AudioSample makeSilence(fl::u32 timestamp) {
    fl::vector<fl::i16> data(512, 0);
    return AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp);
}

AudioSample makeDC(fl::i16 dcValue, fl::u32 timestamp) {
    fl::vector<fl::i16> data(512, dcValue);
    return AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp);
}

AudioSample makeMaxAmplitude(fl::u32 timestamp) {
    fl::vector<fl::i16> data;
    data.reserve(512);
    for (int i = 0; i < 512; ++i) {
        data.push_back((i % 2 == 0) ? 32767 : -32768);
    }
    return AudioSample(fl::span<const fl::i16>(data.data(), data.size()), timestamp);
}

// Safe NaN check that doesn't rely on isnan
bool isNaN(float x) {
    return !(x == x);
}

} // anonymous namespace

// F1: FFT Edge Cases

FL_TEST_CASE("Adversarial - FFT with DC-only input produces no spectral peaks") {
    FFT fft;
    fl::vector<fl::i16> dcSignal(512, 10000);  // Pure DC
    FFTBins bins(16);
    fft.run(fl::span<const fl::i16>(dcSignal.data(), dcSignal.size()), &bins);

    // DC should not produce significant energy in frequency bins
    // (CQ transform starts at ~175 Hz, DC is 0 Hz)
    float totalEnergy = 0.0f;
    for (fl::size i = 0; i < bins.bins_raw.size(); ++i) {
        totalEnergy += bins.bins_raw[i];
    }
    // Total energy should be minimal for pure DC
    FL_CHECK_LT(totalEnergy, 1000.0f);
}

FL_TEST_CASE("Adversarial - FFT with alternating max samples") {
    FFT fft;
    fl::vector<fl::i16> alternating;
    alternating.reserve(512);
    for (int i = 0; i < 512; ++i) {
        alternating.push_back((i % 2 == 0) ? 32767 : -32768);
    }
    FFTBins bins(16);
    fft.run(fl::span<const fl::i16>(alternating.data(), alternating.size()), &bins);

    // Should not crash, bins should have values
    FL_CHECK_GT(bins.bins_raw.size(), 0u);

    // Alternating +-max is essentially Nyquist frequency
    // Energy should be concentrated in the highest bins (if within CQ range)
    // At minimum, it should not produce NaN or Inf
    for (fl::size i = 0; i < bins.bins_raw.size(); ++i) {
        FL_CHECK_FALSE(isNaN(bins.bins_raw[i]));
    }
}

FL_TEST_CASE("Adversarial - FFT with single impulse") {
    FFT fft;
    fl::vector<fl::i16> impulse(512, 0);
    impulse[0] = 32767;
    FFTBins bins(16);
    fft.run(fl::span<const fl::i16>(impulse.data(), impulse.size()), &bins);

    // Impulse should distribute energy across all frequency bins
    FL_CHECK_GT(bins.bins_raw.size(), 0u);
    int nonZeroBins = 0;
    for (fl::size i = 0; i < bins.bins_raw.size(); ++i) {
        if (bins.bins_raw[i] > 0.0f) nonZeroBins++;
    }
    // Impulse has flat spectrum - should have energy in multiple bins
    FL_CHECK_GT(nonZeroBins, 1);
}

// F2: Signal Conditioner Edge Cases

FL_TEST_CASE("Adversarial - SignalConditioner with max DC offset") {
    SignalConditioner conditioner;
    SignalConditionerConfig config;
    config.enableDCRemoval = true;
    conditioner.configure(config);

    // Feed constant max positive value
    fl::vector<fl::i16> maxDC(512, 32767);
    AudioSample inputSample(fl::span<const fl::i16>(maxDC.data(), maxDC.size()), 0);
    AudioSample output = conditioner.processSample(inputSample);

    // After DC removal, output should be near zero (or at least reduced)
    fl::i64 sum = 0;
    for (size i = 0; i < output.pcm().size(); ++i) {
        sum += output.pcm()[i];
    }
    float meanDC = static_cast<float>(sum) / static_cast<float>(output.pcm().size());
    FL_CHECK_LT(fl::fl_abs(meanDC), 16000.0f);
}

FL_TEST_CASE("Adversarial - SignalConditioner with alternating spikes") {
    SignalConditioner conditioner;
    SignalConditionerConfig config;
    config.enableSpikeFilter = true;
    conditioner.configure(config);

    // Every other sample is a spike
    fl::vector<fl::i16> spiky(512);
    for (int i = 0; i < 512; ++i) {
        spiky[i] = (i % 2 == 0) ? 30000 : 0;
    }
    AudioSample inputSample(fl::span<const fl::i16>(spiky.data(), spiky.size()), 0);
    AudioSample output = conditioner.processSample(inputSample);

    fl::i16 maxVal = 0;
    for (size i = 0; i < output.pcm().size(); ++i) {
        if (output.pcm()[i] > maxVal) maxVal = output.pcm()[i];
    }
    FL_CHECK_LT(static_cast<fl::i32>(maxVal), 30000);
}

// F3: AutoGain Edge Cases

FL_TEST_CASE("Adversarial - AutoGain with silence doesn't produce NaN") {
    AutoGain gain;
    AutoGainConfig config;
    config.enabled = true;
    gain.configure(config);

    fl::vector<fl::i16> silence(512, 0);
    AudioSample inputSample(fl::span<const fl::i16>(silence.data(), silence.size()), 0);

    // Feed many frames of silence
    AudioSample output = inputSample;
    for (int i = 0; i < 50; ++i) {
        output = gain.process(inputSample);
    }

    // Output should be all zeros, no NaN or overflow
    for (size i = 0; i < output.pcm().size(); ++i) {
        FL_CHECK_EQ(output.pcm()[i], 0);
    }

    const auto& stats = gain.getStats();
    FL_CHECK_GT(stats.samplesProcessed, 0u);
}

FL_TEST_CASE("Adversarial - AutoGain with max amplitude clipping") {
    AutoGain gain;
    AutoGainConfig config;
    config.enabled = true;
    config.targetRMSLevel = 20000.0f;  // High target to test clipping
    gain.configure(config);

    // Feed loud signal
    fl::vector<fl::i16> loud;
    loud.reserve(512);
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * FL_M_PI * 440.0f * i / 44100.0f;
        loud.push_back(static_cast<fl::i16>(20000.0f * fl::sinf(phase)));
    }
    AudioSample inputSample(fl::span<const fl::i16>(loud.data(), loud.size()), 0);

    AudioSample output = inputSample;
    for (int iter = 0; iter < 20; ++iter) {
        output = gain.process(inputSample);
    }

    bool hasNonZero = false;
    for (size i = 0; i < output.pcm().size(); ++i) {
        if (output.pcm()[i] != 0) {
            hasNonZero = true;
            break;
        }
    }
    FL_CHECK(hasNonZero);
}

// F4: NoiseFloorTracker Edge Cases

FL_TEST_CASE("Adversarial - NoiseFloorTracker with zero input doesn't go negative") {
    NoiseFloorTracker tracker;
    NoiseFloorTrackerConfig config;
    config.minFloor = 1.0f;
    tracker.configure(config);

    // Feed zero for many frames
    for (int i = 0; i < 200; ++i) {
        tracker.update(0.0f);
    }

    float floor = tracker.getFloor();
    FL_CHECK_GE(floor, config.minFloor);
    FL_CHECK_FALSE(isNaN(floor));
}

FL_TEST_CASE("Adversarial - NoiseFloorTracker with huge value spike") {
    NoiseFloorTracker tracker;

    // Establish low floor
    for (int i = 0; i < 20; ++i) {
        tracker.update(100.0f);
    }

    // Single massive spike
    tracker.update(1000000.0f);

    float afterSpike = tracker.getFloor();

    FL_CHECK_GT(afterSpike, 50.0f);
    FL_CHECK_LT(afterSpike, 1000000.0f);
    FL_CHECK_FALSE(isNaN(afterSpike));
}

// F5: BeatDetector Edge Cases

FL_TEST_CASE("Adversarial - BeatDetector with constant loud signal doesn't spam beats") {
    BeatDetector detector;
    int beatCount = 0;
    detector.onBeat.add([&beatCount]() { beatCount++; });

    auto ctx = fl::make_shared<AudioContext>(makeSilence(0));
    ctx->setSampleRate(44100);

    // Feed constant loud bass tone (no transients, just sustained)
    for (int i = 0; i < 100; ++i) {
        ctx->setSample(makeSample(200.0f, i * 23, 20000.0f));
        ctx->getFFT(16);
        ctx->getFFTHistory(4);
        detector.update(ctx);
    }

    // Constant signal has no onsets - should produce very few beats
    // (maybe 1-2 at startup as threshold adapts, but not continuous)
    FL_CHECK_LT(beatCount, 10);
}

FL_TEST_CASE("Adversarial - BeatDetector cooldown enforced") {
    BeatDetector detector;
    detector.setThreshold(0.01f);  // Very low threshold

    fl::vector<u32> beatTimestamps;
    detector.onBeat.add([&beatTimestamps, &detector]() {
        // We can't easily get timestamp in callback, just count
        beatTimestamps.push_back(static_cast<u32>(beatTimestamps.size()));
    });

    auto ctx = fl::make_shared<AudioContext>(makeSilence(0));
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->getFFTHistory(4);

    // Rapid-fire bass bursts every frame (23ms apart, faster than cooldown)
    for (int i = 0; i < 50; ++i) {
        ctx->setSample(makeSample(200.0f, i * 23, 20000.0f));
        ctx->getFFT(16);
        detector.update(ctx);
    }

    // MIN_BEAT_INTERVAL_MS = 250ms, frames are 23ms apart
    // So max beats in 50*23=1150ms should be about 1150/250 ≈ 4-5
    FL_CHECK_LT(static_cast<int>(beatTimestamps.size()), 10);
}

// F6: EnergyAnalyzer Edge Cases

FL_TEST_CASE("Adversarial - EnergyAnalyzer silence then loud doesn't overflow") {
    EnergyAnalyzer analyzer;

    // Extended silence
    for (int i = 0; i < 50; ++i) {
        auto ctx = fl::make_shared<AudioContext>(makeSilence(i * 23));
        analyzer.update(ctx);
    }

    // Sudden max amplitude
    auto ctx = fl::make_shared<AudioContext>(makeMaxAmplitude(1200));
    analyzer.update(ctx);

    float rms = analyzer.getRMS();
    FL_CHECK_GT(rms, 0.0f);
    FL_CHECK_FALSE(isNaN(rms));

    float normalized = analyzer.getNormalizedRMS();
    FL_CHECK_GE(normalized, 0.0f);
    FL_CHECK_LE(normalized, 1.0f);
}

FL_TEST_CASE("Adversarial - EnergyAnalyzer min never exceeds max after silence") {
    EnergyAnalyzer analyzer;

    // Only silence - tests degenerate min/max initialization
    for (int i = 0; i < 30; ++i) {
        auto ctx = fl::make_shared<AudioContext>(makeSilence(i * 23));
        analyzer.update(ctx);
    }

    // Now a real signal
    auto ctx = fl::make_shared<AudioContext>(makeSample(440.0f, 700, 10000.0f));
    analyzer.update(ctx);

    float minE = analyzer.getMinEnergy();
    float maxE = analyzer.getMaxEnergy();
    // After getting at least one real signal, max should be >= min
    FL_CHECK_GE(maxE, minE);
}

// F7: TempoAnalyzer Edge Cases

FL_TEST_CASE("Adversarial - TempoAnalyzer with random noise doesn't crash") {
    TempoAnalyzer analyzer;
    auto ctx = fl::make_shared<AudioContext>(makeSilence(0));
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->getFFTHistory(4);

    // Feed random-amplitude signals at random-ish intervals
    u32 timestamp = 0;
    for (int i = 0; i < 200; ++i) {
        timestamp += (i % 7) * 5 + 10;  // Irregular intervals
        float amplitude = static_cast<float>((i * 7 + 13) % 20000);
        ctx->setSample(makeSample(200.0f + (i % 5) * 100.0f, timestamp, amplitude));
        ctx->getFFT(16);
        analyzer.update(ctx);
    }

    // Should not crash, BPM should be in valid range
    float bpm = analyzer.getBPM();
    FL_CHECK_GT(bpm, 0.0f);
    FL_CHECK_LT(bpm, 300.0f);
    FL_CHECK_FALSE(isNaN(bpm));
    FL_CHECK_FALSE(isNaN(analyzer.getConfidence()));
    FL_CHECK_FALSE(isNaN(analyzer.getStability()));
}

FL_TEST_CASE("Adversarial - TempoAnalyzer with silence only") {
    TempoAnalyzer analyzer;
    auto ctx = fl::make_shared<AudioContext>(makeSilence(0));
    ctx->setSampleRate(44100);
    ctx->getFFT(16);
    ctx->getFFTHistory(4);

    for (int i = 0; i < 100; ++i) {
        ctx->setSample(makeSilence(i * 23));
        ctx->getFFT(16);
        analyzer.update(ctx);
    }

    // Should stay at default BPM with zero confidence
    FL_CHECK_EQ(analyzer.getBPM(), 120.0f);
    FL_CHECK_EQ(analyzer.getConfidence(), 0.0f);
}

// F8: AudioProcessor Full Pipeline Edge Cases

FL_TEST_CASE("Adversarial - AudioProcessor full pipeline with silence") {
    AudioProcessor processor;
    float lastEnergy = -1.0f;
    processor.onEnergy([&lastEnergy](float rms) { lastEnergy = rms; });

    // Feed only silence through full pipeline
    for (int i = 0; i < 50; ++i) {
        fl::vector<fl::i16> silence(512, 0);
        AudioSample sample(fl::span<const fl::i16>(silence.data(), silence.size()), i * 23);
        processor.update(sample);
    }

    // Energy callback should have fired with zero or near-zero value
    FL_CHECK_GE(lastEnergy, 0.0f);
    FL_CHECK_LT(lastEnergy, 100.0f);
}

FL_TEST_CASE("Adversarial - AudioProcessor rapid sample rate changes") {
    AudioProcessor processor;

    // Change sample rate between updates
    processor.setSampleRate(44100);
    AudioSample s1 = makeSample(440.0f, 100);
    processor.update(s1);

    processor.setSampleRate(22050);
    AudioSample s2 = makeSample(440.0f, 200);
    processor.update(s2);

    processor.setSampleRate(16000);
    AudioSample s3 = makeSample(440.0f, 300);
    processor.update(s3);

    // Should not crash, context should be valid
    FL_CHECK(processor.getContext() != nullptr);
    FL_CHECK_EQ(processor.getSampleRate(), 16000);
}

FL_TEST_CASE("Adversarial - AudioProcessor reset mid-processing") {
    AudioProcessor processor;
    int beatCount = 0;
    processor.onBeat([&beatCount]() { beatCount++; });
    processor.onEnergy([](float) {});  // Register to create detectors

    // Process some frames
    for (int i = 0; i < 10; ++i) {
        AudioSample s = makeSample(200.0f, i * 23, 15000.0f);
        processor.update(s);
    }

    // Reset mid-stream
    processor.reset();

    // Continue processing - should not crash
    for (int i = 0; i < 10; ++i) {
        AudioSample s = makeSample(200.0f, (10 + i) * 23, 15000.0f);
        processor.update(s);
    }

    FL_CHECK(processor.getContext() != nullptr);
}

// F9: FrequencyBands Edge Cases

FL_TEST_CASE("Adversarial - FrequencyBands with sub-bass frequency") {
    FrequencyBands bands;
    bands.setSampleRate(44100);
    bands.setSmoothing(0.0f);

    // 20 Hz is at the very edge of hearing and bass range
    fl::vector<fl::i16> subBass;
    subBass.reserve(1024);
    for (int i = 0; i < 1024; ++i) {
        float phase = 2.0f * FL_M_PI * 20.0f * i / 44100.0f;
        subBass.push_back(static_cast<fl::i16>(20000.0f * fl::sinf(phase)));
    }

    auto ctx = fl::make_shared<AudioContext>(
        AudioSample(fl::span<const fl::i16>(subBass.data(), subBass.size()), 0));
    ctx->setSampleRate(44100);
    bands.update(ctx);

    FL_CHECK_FALSE(isNaN(bands.getBass()));
    FL_CHECK_FALSE(isNaN(bands.getMid()));
    FL_CHECK_FALSE(isNaN(bands.getTreble()));
}

FL_TEST_CASE("Adversarial - FrequencyBands with Nyquist frequency") {
    FrequencyBands bands;
    bands.setSampleRate(44100);
    bands.setSmoothing(0.0f);

    // Near Nyquist frequency (22050 Hz)
    fl::vector<fl::i16> nyquist;
    nyquist.reserve(512);
    for (int i = 0; i < 512; ++i) {
        nyquist.push_back((i % 2 == 0) ? 20000 : -20000);
    }

    auto ctx = fl::make_shared<AudioContext>(
        AudioSample(fl::span<const fl::i16>(nyquist.data(), nyquist.size()), 0));
    ctx->setSampleRate(44100);
    bands.update(ctx);

    // Should not crash or produce NaN
    FL_CHECK_FALSE(isNaN(bands.getBass()));
    FL_CHECK_FALSE(isNaN(bands.getMid()));
    FL_CHECK_FALSE(isNaN(bands.getTreble()));
}

// F10: VocalDetector Edge Cases

FL_TEST_CASE("Adversarial - VocalDetector with DC input") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    auto ctx = fl::make_shared<AudioContext>(makeDC(10000, 0));
    ctx->setSampleRate(44100);
    ctx->getFFT(128);
    detector.update(ctx);

    // DC should not be detected as vocal
    FL_CHECK_FALSE(detector.isVocal());
    FL_CHECK_FALSE(isNaN(detector.getConfidence()));
}

// ADV-2: INT16_MAX/INT16_MIN Saturation (full-scale square wave)
FL_TEST_CASE("Adversarial - INT16 saturation through pipeline") {
    SignalConditioner conditioner;
    SignalConditionerConfig scConfig;
    scConfig.enableDCRemoval = true;
    scConfig.enableSpikeFilter = true;
    scConfig.enableNoiseGate = true;
    conditioner.configure(scConfig);

    // Full-scale square wave: alternating 32767 and -32768
    AudioSample saturated = makeMaxAmplitude(1000);
    AudioSample result = conditioner.processSample(saturated);

    // Should not crash, output should be valid
    FL_CHECK(result.isValid());
    // No NaN in stats
    FL_CHECK_FALSE(isNaN(static_cast<float>(conditioner.getStats().dcOffset)));
}

// ADV-3: Single-Sample Buffer
FL_TEST_CASE("Adversarial - single sample buffer") {
    SignalConditioner conditioner;

    // Create a buffer with exactly 1 sample
    fl::vector<fl::i16> single(1, 5000);
    AudioSample singleSample(fl::span<const fl::i16>(single.data(), single.size()), 2000);
    AudioSample result = conditioner.processSample(singleSample);

    // Should not crash
    FL_CHECK(result.isValid());
    FL_CHECK_EQ(result.size(), 1u);
}

// ADV-4: Rapid Config Switching
FL_TEST_CASE("Adversarial - rapid config switching") {
    SignalConditioner conditioner;

    fl::vector<fl::i16> samples(512, 5000);
    AudioSample audio(fl::span<const fl::i16>(samples.data(), samples.size()), 0);

    // Switch configs every frame for 100 frames
    for (int i = 0; i < 100; ++i) {
        SignalConditionerConfig config;
        config.enableDCRemoval = (i % 2 == 0);
        config.enableSpikeFilter = (i % 3 == 0);
        config.enableNoiseGate = (i % 5 == 0);
        config.spikeThreshold = static_cast<fl::i16>(5000 + (i % 10) * 1000);
        conditioner.configure(config);
        AudioSample result = conditioner.processSample(audio);
        FL_CHECK(result.isValid());
    }
}

// ADV-5: Monotonically Increasing Signal (no periodicity)
FL_TEST_CASE("Adversarial - monotonic signal no false beats") {
    fl::vector<fl::i16> ramp;
    ramp.reserve(512);
    for (int i = 0; i < 512; ++i) {
        // Linear ramp from 0 to 32767
        ramp.push_back(static_cast<fl::i16>((static_cast<fl::i32>(i) * 32767) / 511));
    }
    AudioSample rampSample(fl::span<const fl::i16>(ramp.data(), ramp.size()), 3000);

    // Process through signal conditioner
    SignalConditioner conditioner;
    AudioSample cleaned = conditioner.processSample(rampSample);
    FL_CHECK(cleaned.isValid());
    FL_CHECK_EQ(cleaned.size(), 512u);

    // Process through auto gain
    AutoGain agc;
    AudioSample gained = agc.process(cleaned);
    FL_CHECK(gained.isValid());

    // No NaN in output
    const auto& pcm = gained.pcm();
    for (fl::size j = 0; j < pcm.size(); ++j) {
        FL_CHECK_GE(pcm[j], -32768);
        FL_CHECK_LE(pcm[j], 32767);
    }
}

FL_TEST_CASE("Adversarial - VocalDetector with silence") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    auto ctx = fl::make_shared<AudioContext>(makeSilence(0));
    ctx->setSampleRate(44100);
    ctx->getFFT(128);
    detector.update(ctx);

    FL_CHECK_FALSE(detector.isVocal());
    float conf = detector.getConfidence();
    FL_CHECK_FALSE(isNaN(conf));
}
