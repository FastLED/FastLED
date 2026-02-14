// Unit tests for SignalConditioner - adversarial and boundary tests
// standalone test

#include "fl/audio/signal_conditioner.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "test.h"

using namespace fl;

namespace { // signal_conditioner

AudioSample createSample(const vector<i16>& samples, u32 timestamp = 0) {
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), timestamp);
    return AudioSample(impl);
}

} // anonymous namespace

// SC-1: DC Removal - Exact Offset Subtraction
FL_TEST_CASE("SignalConditioner - DC removal exact offset") {
    SignalConditioner conditioner;
    SignalConditionerConfig config;
    config.enableDCRemoval = true;
    config.enableSpikeFilter = false;
    config.enableNoiseGate = false;
    conditioner.configure(config);

    // Pure DC: all samples = 5000
    vector<i16> dcSamples(512, 5000);
    AudioSample raw = createSample(dcSamples, 1000);
    AudioSample cleaned = conditioner.processSample(raw);

    // Mean of all-5000 buffer = 5000, subtracted → all zero
    const auto& pcm = cleaned.pcm();
    FL_CHECK_EQ(pcm.size(), 512u);
    for (size i = 0; i < pcm.size(); ++i) {
        FL_CHECK_EQ(pcm[i], 0);
    }

    const auto& stats = conditioner.getStats();
    // dcOffset should be exactly 5000
    FL_CHECK_EQ(stats.dcOffset, 5000);
    FL_CHECK_EQ(stats.samplesProcessed, 512u);
}

// SC-2: Spike Filtering - Exact Threshold Boundary
// Note: Spike zeroing happens inside removeDCOffset, so DC removal must be
// enabled for spikes to actually be zeroed in the output.
FL_TEST_CASE("SignalConditioner - spike filter threshold boundary") {
    SignalConditioner conditioner;
    SignalConditionerConfig config;
    config.enableDCRemoval = true;   // Required for spike zeroing in output
    config.enableSpikeFilter = true;
    config.enableNoiseGate = false;
    config.spikeThreshold = 10000;
    conditioner.configure(config);

    // Build samples exercising exact boundaries
    // Code: (sample > -threshold) && (sample < threshold) → valid
    vector<i16> samples(512, 1000); // fill with normal values
    samples[0] = 9999;   // 9999 < 10000 → valid
    samples[1] = 10000;  // 10000 < 10000 is false → SPIKE
    samples[2] = 10001;  // 10001 < 10000 is false → SPIKE
    samples[3] = -10000; // -10000 > -10000 is false → SPIKE
    samples[4] = -9999;  // -9999 > -10000 is true → valid

    AudioSample raw = createSample(samples, 2000);
    AudioSample cleaned = conditioner.processSample(raw);

    const auto& pcm = cleaned.pcm();
    // DC offset from valid samples: (507*1000 + 9999 + (-9999)) / 509 ≈ 995
    // Valid boundary samples survive (offset-adjusted but non-zero)
    FL_CHECK_NE(pcm[0], 0);  // 9999 - ~995 = ~9004
    // Spikes are zeroed by removeDCOffset
    FL_CHECK_EQ(pcm[1], 0);  // 10000 spike → zeroed
    FL_CHECK_EQ(pcm[2], 0);  // 10001 spike → zeroed
    FL_CHECK_EQ(pcm[3], 0);  // -10000 spike → zeroed
    // -9999 valid, after DC removal still large negative
    FL_CHECK_NE(pcm[4], 0);  // -9999 - ~995 = ~-10994

    // Exactly 3 spikes
    FL_CHECK_EQ(conditioner.getStats().spikesRejected, 3u);
}

// SC-3: Noise Gate Hysteresis - Per-Sample Boundary
FL_TEST_CASE("SignalConditioner - noise gate hysteresis per-sample") {
    SignalConditioner conditioner;
    SignalConditionerConfig config;
    config.enableDCRemoval = false;
    config.enableSpikeFilter = false;
    config.enableNoiseGate = true;
    config.noiseGateOpenThreshold = 500;
    config.noiseGateCloseThreshold = 300;
    conditioner.configure(config);

    // Gate starts closed.
    // 128 @ 600 (abs >= 500 → opens), 128 @ 400 (abs >= 300, stays open),
    // 128 @ 250 (abs < 300, closes), 128 @ 450 (abs < 500, stays closed)
    vector<i16> samples;
    samples.reserve(512);
    for (int i = 0; i < 128; ++i) samples.push_back(600);
    for (int i = 0; i < 128; ++i) samples.push_back(400);
    for (int i = 0; i < 128; ++i) samples.push_back(250);
    for (int i = 0; i < 128; ++i) samples.push_back(450);

    AudioSample raw = createSample(samples, 3000);
    AudioSample cleaned = conditioner.processSample(raw);
    const auto& pcm = cleaned.pcm();

    // Region 1 (0-127): gate opens at first sample, all pass through
    FL_CHECK_EQ(pcm[0], 600);
    FL_CHECK_EQ(pcm[64], 600);
    FL_CHECK_EQ(pcm[127], 600);

    // Region 2 (128-255): gate stays open (400 >= closeThreshold=300)
    FL_CHECK_EQ(pcm[128], 400);
    FL_CHECK_EQ(pcm[192], 400);
    FL_CHECK_EQ(pcm[255], 400);

    // Region 3 (256-383): gate closes (250 < closeThreshold=300)
    FL_CHECK_EQ(pcm[256], 0);
    FL_CHECK_EQ(pcm[320], 0);
    FL_CHECK_EQ(pcm[383], 0);

    // Region 4 (384-511): gate stays closed (450 < openThreshold=500)
    FL_CHECK_EQ(pcm[384], 0);
    FL_CHECK_EQ(pcm[448], 0);
    FL_CHECK_EQ(pcm[511], 0);
}

// SC-4: DC Removal With Spikes - Spikes Excluded From Mean
FL_TEST_CASE("SignalConditioner - DC removal excludes spikes from mean") {
    SignalConditioner conditioner;
    SignalConditionerConfig config;
    config.enableDCRemoval = true;
    config.enableSpikeFilter = true;
    config.enableNoiseGate = false;
    config.spikeThreshold = 10000;
    conditioner.configure(config);

    // 510 samples at 1000, 2 spikes at 30000
    vector<i16> samples(512, 1000);
    samples[100] = 30000;
    samples[200] = 30000;

    AudioSample raw = createSample(samples, 4000);
    AudioSample cleaned = conditioner.processSample(raw);
    const auto& pcm = cleaned.pcm();

    // DC offset should be ~1000 (spikes excluded from mean calculation)
    const auto& stats = conditioner.getStats();
    FL_CHECK_EQ(stats.spikesRejected, 2u);
    // dcOffset should be close to 1000 (not pulled up by spikes)
    FL_CHECK_GE(stats.dcOffset, 990);
    FL_CHECK_LE(stats.dcOffset, 1010);

    // Spike positions should be zeroed
    FL_CHECK_EQ(pcm[100], 0);
    FL_CHECK_EQ(pcm[200], 0);

    // Valid samples: 1000 - dcOffset(~1000) ≈ 0
    for (size i = 0; i < pcm.size(); ++i) {
        if (i == 100 || i == 200) continue;
        FL_CHECK_GE(pcm[i], -5);
        FL_CHECK_LE(pcm[i], 5);
    }
}

// SC-5: Empty and Invalid Samples
FL_TEST_CASE("SignalConditioner - empty and invalid samples") {
    SignalConditioner conditioner;

    AudioSample emptySample;
    AudioSample result1 = conditioner.processSample(emptySample);
    FL_CHECK_FALSE(result1.isValid());

    vector<i16> emptyVec;
    AudioSample zeroSizeSample = createSample(emptyVec, 5000);
    AudioSample result2 = conditioner.processSample(zeroSizeSample);
    FL_CHECK_FALSE(result2.isValid());

    FL_CHECK_EQ(conditioner.getStats().samplesProcessed, 0u);
    FL_CHECK_EQ(conditioner.getStats().spikesRejected, 0u);
}

// SC-6: All-Spike Buffer
FL_TEST_CASE("SignalConditioner - all spike buffer") {
    SignalConditioner conditioner;
    SignalConditionerConfig config;
    config.enableDCRemoval = true;
    config.enableSpikeFilter = true;
    config.enableNoiseGate = false;
    config.spikeThreshold = 10000;
    conditioner.configure(config);

    // All samples above spike threshold
    vector<i16> spikes(512, 32000);
    AudioSample raw = createSample(spikes, 6000);
    AudioSample cleaned = conditioner.processSample(raw);
    const auto& pcm = cleaned.pcm();

    // All should be zeroed
    for (size i = 0; i < pcm.size(); ++i) {
        FL_CHECK_EQ(pcm[i], 0);
    }

    FL_CHECK_EQ(conditioner.getStats().spikesRejected, 512u);
    // dcOffset should be 0 (no valid samples to calculate mean)
    FL_CHECK_EQ(conditioner.getStats().dcOffset, 0);
}

// SC-7: Config Change Mid-Stream
FL_TEST_CASE("SignalConditioner - config change affects spike count") {
    SignalConditioner conditioner;

    // Pass 1: threshold=10000, signal at 8000 (no spikes)
    SignalConditionerConfig config1;
    config1.enableDCRemoval = false;
    config1.enableSpikeFilter = true;
    config1.enableNoiseGate = false;
    config1.spikeThreshold = 10000;
    conditioner.configure(config1);

    vector<i16> samples(100, 8000);
    conditioner.processSample(createSample(samples, 7000));
    u32 spikes1 = conditioner.getStats().spikesRejected;
    FL_CHECK_EQ(spikes1, 0u); // 8000 < 10000, no spikes

    // Pass 2: lower threshold to 5000, same 8000 signal → all spikes
    conditioner.reset();
    SignalConditionerConfig config2;
    config2.enableDCRemoval = false;
    config2.enableSpikeFilter = true;
    config2.enableNoiseGate = false;
    config2.spikeThreshold = 5000;
    conditioner.configure(config2);

    conditioner.processSample(createSample(samples, 7100));
    u32 spikes2 = conditioner.getStats().spikesRejected;
    FL_CHECK_EQ(spikes2, 100u); // 8000 >= 5000, all spikes
}

// SC-8: Noise Gate Reopening
FL_TEST_CASE("SignalConditioner - noise gate reopening") {
    SignalConditioner conditioner;
    SignalConditionerConfig config;
    config.enableDCRemoval = false;
    config.enableSpikeFilter = false;
    config.enableNoiseGate = true;
    config.noiseGateOpenThreshold = 500;
    config.noiseGateCloseThreshold = 300;
    conditioner.configure(config);

    // 128 @ 600 (opens), 128 @ 200 (closes), 128 @ 600 (reopens)
    vector<i16> samples;
    samples.reserve(384);
    for (int i = 0; i < 128; ++i) samples.push_back(600);
    for (int i = 0; i < 128; ++i) samples.push_back(200);
    for (int i = 0; i < 128; ++i) samples.push_back(600);

    AudioSample raw = createSample(samples, 8000);
    AudioSample cleaned = conditioner.processSample(raw);
    const auto& pcm = cleaned.pcm();

    FL_CHECK_EQ(pcm[64], 600);   // gate open
    FL_CHECK_EQ(pcm[192], 0);    // gate closed (200 < 300)
    FL_CHECK_EQ(pcm[320], 600);  // gate reopened (600 >= 500)
}

// Keep: Full pipeline test
FL_TEST_CASE("SignalConditioner - full pipeline") {
    SignalConditioner conditioner;
    SignalConditionerConfig config;
    config.enableDCRemoval = true;
    config.enableSpikeFilter = true;
    config.enableNoiseGate = true;
    config.spikeThreshold = 10000;
    config.noiseGateOpenThreshold = 1000;
    config.noiseGateCloseThreshold = 500;
    conditioner.configure(config);

    // Signal: 2000 amplitude sine + 3000 DC bias + occasional spikes
    vector<i16> samples;
    samples.reserve(1000);
    for (size i = 0; i < 1000; ++i) {
        float phase = 2.0f * FL_M_PI * 440.0f * static_cast<float>(i) / 22050.0f;
        i32 biased = static_cast<i32>(2000 * fl::sinf(phase)) + 3000;
        if (i % 150 == 0) biased = 25000; // spike
        if (biased > 32767) biased = 32767;
        if (biased < -32768) biased = -32768;
        samples.push_back(static_cast<i16>(biased));
    }

    AudioSample raw = createSample(samples, 9000);
    AudioSample cleaned = conditioner.processSample(raw);
    const auto& stats = conditioner.getStats();

    FL_CHECK(cleaned.isValid());
    FL_CHECK_EQ(cleaned.size(), samples.size());
    FL_CHECK(stats.spikesRejected >= 6);
    FL_CHECK(stats.noiseGateOpen);
}

// Keep: Reset
FL_TEST_CASE("SignalConditioner - reset clears state") {
    SignalConditioner conditioner;
    vector<i16> samples(100, 20000);
    conditioner.processSample(createSample(samples, 10000));
    FL_CHECK_GT(conditioner.getStats().spikesRejected, 0u);

    conditioner.reset();
    const auto& stats = conditioner.getStats();
    FL_CHECK_EQ(stats.dcOffset, 0);
    FL_CHECK_FALSE(stats.noiseGateOpen);
    FL_CHECK_EQ(stats.spikesRejected, 0u);
    FL_CHECK_EQ(stats.samplesProcessed, 0u);
}

// Keep: Timestamp preservation
FL_TEST_CASE("SignalConditioner - timestamp preserved") {
    SignalConditioner conditioner;
    vector<i16> samples(500, 5000);
    AudioSample raw = createSample(samples, 123456);
    AudioSample cleaned = conditioner.processSample(raw);
    FL_CHECK_EQ(cleaned.timestamp(), 123456u);
}
