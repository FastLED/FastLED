#include "fl/audio/detectors/multiband_beat_detector.h"
#include "fl/stl/array.h"

using namespace fl;

namespace {

// Helper: Create 16-element frequency bin array with specified band energies
static array<float, 16> createFrequencyBins(float bassLevel, float midLevel, float trebleLevel) {
    array<float, 16> bins;
    bins.fill(0.1f);  // Background noise level

    // Bass bins (0-1)
    bins[0] = bassLevel;
    bins[1] = bassLevel;

    // Mid bins (6-7)
    bins[6] = midLevel;
    bins[7] = midLevel;

    // Treble bins (14-15)
    bins[14] = trebleLevel;
    bins[15] = trebleLevel;

    return bins;
}

} // anonymous namespace

FL_TEST_CASE("MultiBandBeatDetector - Basic configuration") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.midThreshold = 0.12f;
    config.trebleThreshold = 0.08f;
    detector.configure(config);

    // Initial state
    FL_CHECK_FALSE(detector.isBassBeat());
    FL_CHECK_FALSE(detector.isMidBeat());
    FL_CHECK_FALSE(detector.isTrebleBeat());
    FL_CHECK_EQ(detector.getBassEnergy(), 0.0f);
    FL_CHECK_EQ(detector.getMidEnergy(), 0.0f);
    FL_CHECK_EQ(detector.getTrebleEnergy(), 0.0f);
}

FL_TEST_CASE("MultiBandBeatDetector - Bass beat detection") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.beatCooldownFrames = 5;
    detector.configure(config);

    // Frame 1: Low bass energy (establish baseline)
    array<float, 16> bins1 = createFrequencyBins(0.5f, 0.3f, 0.2f);
    detector.detectBeats(bins1);
    FL_CHECK_FALSE(detector.isBassBeat());  // No beat on first frame

    // Frame 2: Bass spike (increase > 15%)
    array<float, 16> bins2 = createFrequencyBins(0.8f, 0.3f, 0.2f);  // +60% bass
    detector.detectBeats(bins2);
    FL_CHECK(detector.isBassBeat());  // Should detect bass beat

    // Verify energy levels
    FL_CHECK(detector.getBassEnergy() > 0.7f);

    // Stats should track bass beat
    const auto& stats = detector.getStats();
    FL_CHECK_EQ(stats.bassBeats, 1u);
}

FL_TEST_CASE("MultiBandBeatDetector - Mid beat detection") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.midThreshold = 0.12f;
    config.beatCooldownFrames = 5;
    detector.configure(config);

    // Frame 1: Low mid energy
    array<float, 16> bins1 = createFrequencyBins(0.3f, 0.4f, 0.2f);
    detector.detectBeats(bins1);
    FL_CHECK_FALSE(detector.isMidBeat());

    // Frame 2: Mid spike (snare drum)
    array<float, 16> bins2 = createFrequencyBins(0.3f, 0.6f, 0.2f);  // +50% mid
    detector.detectBeats(bins2);
    FL_CHECK(detector.isMidBeat());  // Should detect mid beat

    // Verify energy levels
    FL_CHECK(detector.getMidEnergy() > 0.5f);

    const auto& stats = detector.getStats();
    FL_CHECK_EQ(stats.midBeats, 1u);
}

FL_TEST_CASE("MultiBandBeatDetector - Treble beat detection") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.trebleThreshold = 0.08f;
    config.beatCooldownFrames = 5;
    detector.configure(config);

    // Frame 1: Low treble energy
    array<float, 16> bins1 = createFrequencyBins(0.3f, 0.3f, 0.3f);
    detector.detectBeats(bins1);
    FL_CHECK_FALSE(detector.isTrebleBeat());

    // Frame 2: Treble spike (hi-hat)
    array<float, 16> bins2 = createFrequencyBins(0.3f, 0.3f, 0.5f);  // +67% treble
    detector.detectBeats(bins2);
    FL_CHECK(detector.isTrebleBeat());  // Should detect treble beat

    // Verify energy levels
    FL_CHECK(detector.getTrebleEnergy() > 0.4f);

    const auto& stats = detector.getStats();
    FL_CHECK_EQ(stats.trebleBeats, 1u);
}

FL_TEST_CASE("MultiBandBeatDetector - Multi-band correlation") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.midThreshold = 0.12f;
    config.trebleThreshold = 0.08f;
    config.enableCrossBandCorrelation = true;
    detector.configure(config);

    // Frame 1: Establish baseline
    array<float, 16> bins1 = createFrequencyBins(0.5f, 0.4f, 0.3f);
    detector.detectBeats(bins1);

    // Frame 2: Bass + mid spike simultaneously (kick + snare)
    array<float, 16> bins2 = createFrequencyBins(0.8f, 0.6f, 0.3f);
    detector.detectBeats(bins2);

    // Both bands should trigger
    FL_CHECK(detector.isBassBeat());
    FL_CHECK(detector.isMidBeat());

    // Multi-band beat detected
    FL_CHECK(detector.isMultiBandBeat());

    const auto& stats = detector.getStats();
    FL_CHECK_EQ(stats.multiBandBeats, 1u);
}

FL_TEST_CASE("MultiBandBeatDetector - Beat cooldown prevents double-trigger") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.beatCooldownFrames = 5;
    detector.configure(config);

    // Frame 1: Establish baseline
    array<float, 16> bins1 = createFrequencyBins(0.5f, 0.3f, 0.2f);
    detector.detectBeats(bins1);

    // Frame 2: Bass spike - should trigger
    array<float, 16> bins2 = createFrequencyBins(0.8f, 0.3f, 0.2f);
    detector.detectBeats(bins2);
    FL_CHECK(detector.isBassBeat());

    u32 firstBeatCount = detector.getStats().bassBeats;

    // Frames 3-7: Cooldown period - should NOT trigger even with high bass
    for (u32 i = 0; i < 5; ++i) {
        array<float, 16> bins = createFrequencyBins(0.9f, 0.3f, 0.2f);
        detector.detectBeats(bins);
        FL_CHECK_FALSE(detector.isBassBeat());  // Cooldown active
    }

    // Beat count should not increase during cooldown
    FL_CHECK_EQ(detector.getStats().bassBeats, firstBeatCount);

    // Frame 8: After cooldown - should be able to trigger again
    array<float, 16> bins8 = createFrequencyBins(0.5f, 0.3f, 0.2f);  // Drop energy
    detector.detectBeats(bins8);

    array<float, 16> bins9 = createFrequencyBins(0.8f, 0.3f, 0.2f);  // Spike again
    detector.detectBeats(bins9);
    FL_CHECK(detector.isBassBeat());  // Should trigger after cooldown
}

FL_TEST_CASE("MultiBandBeatDetector - Independent band cooldowns") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.midThreshold = 0.12f;
    config.beatCooldownFrames = 3;
    detector.configure(config);

    // Frame 1: Establish baseline
    array<float, 16> bins1 = createFrequencyBins(0.5f, 0.4f, 0.2f);
    detector.detectBeats(bins1);

    // Frame 2: Bass beat only
    array<float, 16> bins2 = createFrequencyBins(0.8f, 0.4f, 0.2f);
    detector.detectBeats(bins2);
    FL_CHECK(detector.isBassBeat());
    FL_CHECK_FALSE(detector.isMidBeat());

    // Frame 3: Mid beat - bass still in cooldown
    array<float, 16> bins3 = createFrequencyBins(0.8f, 0.6f, 0.2f);
    detector.detectBeats(bins3);
    FL_CHECK_FALSE(detector.isBassBeat());  // Bass cooldown active
    FL_CHECK(detector.isMidBeat());         // Mid can still trigger

    // Cooldowns are independent
    const auto& stats = detector.getStats();
    FL_CHECK_EQ(stats.bassBeats, 1u);
    FL_CHECK_EQ(stats.midBeats, 1u);
}

FL_TEST_CASE("MultiBandBeatDetector - Bass-heavy pattern (kick drums)") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.beatCooldownFrames = 10;
    detector.configure(config);

    // Simulate kick drum pattern: 4 kicks with gaps
    array<float, 16> quiet = createFrequencyBins(0.3f, 0.2f, 0.1f);
    array<float, 16> kick = createFrequencyBins(0.8f, 0.2f, 0.1f);

    u32 bassBeats = 0;

    // Kick 1
    detector.detectBeats(quiet);
    detector.detectBeats(kick);
    if (detector.isBassBeat()) bassBeats++;

    // Gap
    for (u32 i = 0; i < 12; ++i) {
        detector.detectBeats(quiet);
    }

    // Kick 2
    detector.detectBeats(kick);
    if (detector.isBassBeat()) bassBeats++;

    // Gap
    for (u32 i = 0; i < 12; ++i) {
        detector.detectBeats(quiet);
    }

    // Kick 3
    detector.detectBeats(kick);
    if (detector.isBassBeat()) bassBeats++;

    // Should detect all kicks
    FL_CHECK(bassBeats >= 3);
    FL_CHECK_EQ(detector.getStats().midBeats, 0u);   // No mid beats
    FL_CHECK_EQ(detector.getStats().trebleBeats, 0u); // No treble beats
}

FL_TEST_CASE("MultiBandBeatDetector - Complex rhythm pattern") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.midThreshold = 0.12f;
    config.trebleThreshold = 0.08f;
    config.beatCooldownFrames = 8;
    detector.configure(config);

    // Simulate drum pattern: kick, hi-hat, snare, hi-hat
    array<float, 16> quiet = createFrequencyBins(0.3f, 0.3f, 0.2f);
    array<float, 16> kick = createFrequencyBins(0.8f, 0.3f, 0.2f);
    array<float, 16> hihat = createFrequencyBins(0.3f, 0.3f, 0.5f);
    array<float, 16> snare = createFrequencyBins(0.3f, 0.6f, 0.2f);

    // Establish baseline
    detector.detectBeats(quiet);

    // Kick
    detector.detectBeats(kick);
    bool kick1 = detector.isBassBeat();

    // Gap + hi-hat
    for (u32 i = 0; i < 10; ++i) {
        detector.detectBeats(quiet);
    }
    detector.detectBeats(hihat);
    bool hihat1 = detector.isTrebleBeat();

    // Gap + snare
    for (u32 i = 0; i < 10; ++i) {
        detector.detectBeats(quiet);
    }
    detector.detectBeats(snare);
    bool snare1 = detector.isMidBeat();

    // Gap + hi-hat
    for (u32 i = 0; i < 10; ++i) {
        detector.detectBeats(quiet);
    }
    detector.detectBeats(hihat);
    bool hihat2 = detector.isTrebleBeat();

    // Verify pattern detected correctly
    FL_CHECK(kick1);   // Kick triggered bass
    FL_CHECK(hihat1);  // Hi-hat triggered treble
    FL_CHECK(snare1);  // Snare triggered mid
    FL_CHECK(hihat2);  // Second hi-hat triggered treble

    const auto& stats = detector.getStats();
    FL_CHECK(stats.bassBeats >= 1);
    FL_CHECK(stats.midBeats >= 1);
    FL_CHECK(stats.trebleBeats >= 2);
}

FL_TEST_CASE("MultiBandBeatDetector - Energy calculation accuracy") {
    MultiBandBeatDetector detector;

    // Test bass energy
    array<float, 16> bins1 = createFrequencyBins(0.8f, 0.4f, 0.2f);
    detector.detectBeats(bins1);

    float bassEnergy = detector.getBassEnergy();
    float midEnergy = detector.getMidEnergy();
    float trebleEnergy = detector.getTrebleEnergy();

    // Bass should be highest
    FL_CHECK(bassEnergy > midEnergy);
    FL_CHECK(bassEnergy > trebleEnergy);
    FL_CHECK(bassEnergy >= 0.7f);  // Should be close to 0.8

    // Test mid energy
    array<float, 16> bins2 = createFrequencyBins(0.2f, 0.9f, 0.3f);
    detector.detectBeats(bins2);

    bassEnergy = detector.getBassEnergy();
    midEnergy = detector.getMidEnergy();
    trebleEnergy = detector.getTrebleEnergy();

    // Mid should be highest
    FL_CHECK(midEnergy > bassEnergy);
    FL_CHECK(midEnergy > trebleEnergy);
    FL_CHECK(midEnergy >= 0.8f);

    // Test treble energy
    array<float, 16> bins3 = createFrequencyBins(0.2f, 0.3f, 0.7f);
    detector.detectBeats(bins3);

    bassEnergy = detector.getBassEnergy();
    midEnergy = detector.getMidEnergy();
    trebleEnergy = detector.getTrebleEnergy();

    // Treble should be highest
    FL_CHECK(trebleEnergy > bassEnergy);
    FL_CHECK(trebleEnergy > midEnergy);
    FL_CHECK(trebleEnergy >= 0.6f);
}

FL_TEST_CASE("MultiBandBeatDetector - Reset functionality") {
    MultiBandBeatDetector detector;

    // Generate some beats
    array<float, 16> quiet = createFrequencyBins(0.3f, 0.3f, 0.2f);
    array<float, 16> loud = createFrequencyBins(0.8f, 0.7f, 0.6f);

    detector.detectBeats(quiet);
    detector.detectBeats(loud);

    // Verify beats detected
    const auto& stats1 = detector.getStats();
    u32 totalBeats = stats1.bassBeats + stats1.midBeats + stats1.trebleBeats;
    FL_CHECK(totalBeats > 0);

    // Reset
    detector.reset();

    // Verify state cleared
    FL_CHECK_FALSE(detector.isBassBeat());
    FL_CHECK_FALSE(detector.isMidBeat());
    FL_CHECK_FALSE(detector.isTrebleBeat());

    const auto& stats2 = detector.getStats();
    FL_CHECK_EQ(stats2.bassBeats, 0u);
    FL_CHECK_EQ(stats2.midBeats, 0u);
    FL_CHECK_EQ(stats2.trebleBeats, 0u);
    FL_CHECK_EQ(stats2.multiBandBeats, 0u);
}

FL_TEST_CASE("MultiBandBeatDetector - Invalid input handling") {
    MultiBandBeatDetector detector;

    // Test with empty array
    array<float, 8> shortBins;
    shortBins.fill(0.5f);

    detector.detectBeats(shortBins);

    // Should not crash, all beats should be false
    FL_CHECK_FALSE(detector.isBassBeat());
    FL_CHECK_FALSE(detector.isMidBeat());
    FL_CHECK_FALSE(detector.isTrebleBeat());
}

// MBD-1: Exact Threshold Boundary
FL_TEST_CASE("MultiBandBeatDetector - exact threshold boundary") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.beatCooldownFrames = 0; // No cooldown for this test
    detector.configure(config);

    // Frame 1: Establish baseline (bass = 1.0)
    array<float, 16> baseline = createFrequencyBins(1.0f, 0.3f, 0.2f);
    detector.detectBeats(baseline);
    FL_CHECK_FALSE(detector.isBassBeat()); // First frame, no previous

    // Frame 2: Exactly at threshold (bass = 1.15, 15% increase)
    // relativeIncrease = 0.15/1.0 = 0.15. Code uses strict >: 0.15 > 0.15 is false
    array<float, 16> atThreshold = createFrequencyBins(1.15f, 0.3f, 0.2f);
    detector.detectBeats(atThreshold);
    FL_CHECK_FALSE(detector.isBassBeat()); // Exactly at threshold - NOT a beat

    // Frame 3: Just above threshold (bass = 1.16, 0.87% increase from 1.15)
    // relativeIncrease = 0.01/1.15 = 0.0087. 0.0087 > 0.15 is false!
    // Need to drop and spike again. Reset baseline first.
    array<float, 16> dropBaseline = createFrequencyBins(1.0f, 0.3f, 0.2f);
    detector.detectBeats(dropBaseline);

    // Frame 4: Above threshold from baseline (bass = 1.16, 16% increase)
    // relativeIncrease = 0.16/1.0 = 0.16. 0.16 > 0.15 is true!
    array<float, 16> aboveThreshold = createFrequencyBins(1.16f, 0.3f, 0.2f);
    detector.detectBeats(aboveThreshold);
    FL_CHECK(detector.isBassBeat()); // Above threshold - IS a beat
}

// MBD-4: Energy Decrease Never Triggers
FL_TEST_CASE("MultiBandBeatDetector - energy decrease never triggers") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.midThreshold = 0.12f;
    config.trebleThreshold = 0.08f;
    config.beatCooldownFrames = 0;
    detector.configure(config);

    // Frame 1: High bass energy (establish baseline)
    array<float, 16> highBass = createFrequencyBins(1.0f, 1.0f, 1.0f);
    detector.detectBeats(highBass);

    // Frame 2: Bass drops to 0.5 (energy decrease)
    // energyIncrease = 0.5 - 1.0 = -0.5, which is <= 0 -> no beat
    array<float, 16> lowBass = createFrequencyBins(0.5f, 0.5f, 0.5f);
    detector.detectBeats(lowBass);

    FL_CHECK_FALSE(detector.isBassBeat());
    FL_CHECK_FALSE(detector.isMidBeat());
    FL_CHECK_FALSE(detector.isTrebleBeat());
}

// MBD-7: Sub-Threshold Increase
FL_TEST_CASE("MultiBandBeatDetector - sub-threshold increase") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.beatCooldownFrames = 0;
    detector.configure(config);

    // Frame 1: Baseline bass = 1.0
    array<float, 16> baseline = createFrequencyBins(1.0f, 0.3f, 0.2f);
    detector.detectBeats(baseline);

    // Frame 2: Bass = 1.10 (10% increase, below 15% threshold)
    // relativeIncrease = 0.10/1.0 = 0.10. 0.10 > 0.15 is false
    array<float, 16> smallIncrease = createFrequencyBins(1.10f, 0.3f, 0.2f);
    detector.detectBeats(smallIncrease);
    FL_CHECK_FALSE(detector.isBassBeat()); // Below threshold, no beat
}

// MBD-8: Reset Clears Cooldowns (tighter version)
FL_TEST_CASE("MultiBandBeatDetector - reset clears cooldowns") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.beatCooldownFrames = 100; // Long cooldown
    detector.configure(config);

    // Trigger bass beat
    array<float, 16> quiet = createFrequencyBins(0.5f, 0.3f, 0.2f);
    detector.detectBeats(quiet);
    array<float, 16> loud = createFrequencyBins(0.8f, 0.3f, 0.2f);
    detector.detectBeats(loud);
    FL_CHECK(detector.isBassBeat()); // Beat triggered

    // Verify cooldown is active (same spike pattern doesn't trigger)
    detector.detectBeats(quiet);
    detector.detectBeats(loud);
    FL_CHECK_FALSE(detector.isBassBeat()); // Cooldown active

    // Reset should clear cooldowns
    detector.reset();

    // Same pattern should trigger again after reset
    detector.detectBeats(quiet);
    detector.detectBeats(loud);
    FL_CHECK(detector.isBassBeat()); // Cooldown cleared by reset
}

FL_TEST_CASE("MultiBandBeatDetector - Statistics tracking") {
    MultiBandBeatDetector detector;
    MultiBandBeatDetectorConfig config;
    config.bassThreshold = 0.15f;
    config.midThreshold = 0.12f;
    config.trebleThreshold = 0.08f;
    config.beatCooldownFrames = 8;
    config.enableCrossBandCorrelation = true;
    detector.configure(config);

    // Pattern: bass, mid, bass+mid, treble
    array<float, 16> quiet = createFrequencyBins(0.3f, 0.3f, 0.2f);

    // Establish baseline
    detector.detectBeats(quiet);

    // Bass beat
    array<float, 16> bass = createFrequencyBins(0.8f, 0.3f, 0.2f);
    detector.detectBeats(bass);

    for (u32 i = 0; i < 10; ++i) detector.detectBeats(quiet);

    // Mid beat
    array<float, 16> mid = createFrequencyBins(0.3f, 0.6f, 0.2f);
    detector.detectBeats(mid);

    for (u32 i = 0; i < 10; ++i) detector.detectBeats(quiet);

    // Bass + mid (multi-band)
    array<float, 16> bassMid = createFrequencyBins(0.8f, 0.6f, 0.2f);
    detector.detectBeats(bassMid);

    for (u32 i = 0; i < 10; ++i) detector.detectBeats(quiet);

    // Treble beat
    array<float, 16> treble = createFrequencyBins(0.3f, 0.3f, 0.5f);
    detector.detectBeats(treble);

    // Check statistics
    const auto& stats = detector.getStats();
    FL_CHECK(stats.bassBeats >= 2);      // At least 2 bass beats
    FL_CHECK(stats.midBeats >= 2);       // At least 2 mid beats
    FL_CHECK(stats.trebleBeats >= 1);    // At least 1 treble beat
    FL_CHECK(stats.multiBandBeats >= 1); // At least 1 multi-band beat
}
