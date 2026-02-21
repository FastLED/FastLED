// Unit tests for NoiseFloorTracker - adversarial and boundary tests
// standalone test

#include "fl/audio/noise_floor_tracker.h"
#include "fl/stl/math.h"

using namespace fl;

// NF-1: Floor Decay - Fast Tracking Downward
FL_TEST_CASE("NoiseFloorTracker - floor decays toward lower signal") {
    NoiseFloorTracker tracker;
    NoiseFloorTrackerConfig config;
    config.decayRate = 0.97f;  // 0.97^100 ≈ 0.048 → floor ≈ 143 after 100 frames
    config.minFloor = 10.0f;
    tracker.configure(config);

    // Initialize floor at 1000
    for (int i = 0; i < 5; ++i) tracker.update(1000.0f);
    float highFloor = tracker.getFloor();
    FL_CHECK_GT(highFloor, 500.0f);

    // Feed level=100 for 20 frames - floor should NOT have fully decayed yet
    // 0.97^20 ≈ 0.544 → floor ≈ 100 + 900*0.544 ≈ 590
    for (int i = 0; i < 20; ++i) tracker.update(100.0f);
    float mid = tracker.getFloor();
    FL_CHECK_GT(mid, 400.0f); // Still well above target after only 20 frames

    // Feed 80 more frames (100 total at level=100)
    // 0.97^100 ≈ 0.048 → floor ≈ 100 + 900*0.048 ≈ 143
    for (int i = 0; i < 80; ++i) tracker.update(100.0f);
    float low = tracker.getFloor();
    FL_CHECK_LT(low, 200.0f); // Close to 100 after 100 frames
    FL_CHECK_GE(low, config.minFloor);
}

// NF-2: Floor Attack - Slow Tracking Upward
FL_TEST_CASE("NoiseFloorTracker - floor rises slowly toward higher signal") {
    NoiseFloorTracker tracker;
    NoiseFloorTrackerConfig config;
    config.decayRate = 0.99f;
    config.attackRate = 0.001f;
    config.minFloor = 10.0f;
    tracker.configure(config);

    // Initialize floor at 100
    for (int i = 0; i < 5; ++i) tracker.update(100.0f);
    float startFloor = tracker.getFloor();

    // Feed level=5000 for 50 frames - floor should rise slowly
    for (int i = 0; i < 50; ++i) tracker.update(5000.0f);
    float after50 = tracker.getFloor();

    // With attackRate=0.001, floor rises very slowly
    FL_CHECK_GT(after50, startFloor); // Has moved up
    FL_CHECK_LT(after50, 500.0f);    // Still far from 5000
}

// NF-3: Hysteresis - Exact Margin Test
FL_TEST_CASE("NoiseFloorTracker - isAboveFloor uses margin") {
    NoiseFloorTracker tracker;
    NoiseFloorTrackerConfig config;
    config.hysteresisMargin = 100.0f;
    config.attackRate = 0.05f;
    config.decayRate = 0.99f;
    tracker.configure(config);

    // Establish floor near 500
    for (int i = 0; i < 50; ++i) tracker.update(500.0f);
    float floor = tracker.getFloor();

    // isAboveFloor checks: level > (floor + margin)
    FL_CHECK_FALSE(tracker.isAboveFloor(floor));             // at floor
    FL_CHECK_FALSE(tracker.isAboveFloor(floor + 50.0f));     // within margin
    FL_CHECK_FALSE(tracker.isAboveFloor(floor + 100.0f));    // at margin (not > margin)
    FL_CHECK(tracker.isAboveFloor(floor + 101.0f));          // above margin
}

// NF-4: Cross-Domain Blending - Exact Math
FL_TEST_CASE("NoiseFloorTracker - cross domain blending math") {
    NoiseFloorTracker tracker;
    NoiseFloorTrackerConfig config;
    config.crossDomainWeight = 0.3f;
    tracker.configure(config);

    // First update initializes floor to combined level
    // combined = (1-0.3)*200 + 0.3*800 = 140 + 240 = 380
    tracker.update(200.0f, 800.0f);
    float floor = tracker.getFloor();

    // First update initializes directly to combined level (clamped)
    FL_CHECK_GT(floor, 350.0f);
    FL_CHECK_LT(floor, 410.0f);
}

// NF-5: Floor Clamping - Min Bound
FL_TEST_CASE("NoiseFloorTracker - min floor clamping") {
    NoiseFloorTracker tracker;
    NoiseFloorTrackerConfig config;
    config.minFloor = 100.0f;
    config.decayRate = 0.8f; // fast decay
    tracker.configure(config);

    // Feed very low level repeatedly
    for (int i = 0; i < 100; ++i) tracker.update(1.0f);

    FL_CHECK_GE(tracker.getFloor(), 100.0f);
}

// NF-6: Floor Clamping - Max Bound
FL_TEST_CASE("NoiseFloorTracker - max floor clamping") {
    NoiseFloorTracker tracker;
    NoiseFloorTrackerConfig config;
    config.maxFloor = 1000.0f;
    config.attackRate = 0.1f; // fast attack for test
    tracker.configure(config);

    // Feed very high level repeatedly
    for (int i = 0; i < 200; ++i) tracker.update(50000.0f);

    FL_CHECK_LE(tracker.getFloor(), 1000.0f);
}

// NF-7: Normalize - Floor Subtraction
FL_TEST_CASE("NoiseFloorTracker - normalize subtracts floor") {
    NoiseFloorTracker tracker;
    NoiseFloorTrackerConfig config;
    config.minFloor = 10.0f;
    config.attackRate = 0.05f;
    tracker.configure(config);

    // Establish floor near 300
    for (int i = 0; i < 50; ++i) tracker.update(300.0f);
    float floor = tracker.getFloor();

    // normalize(500) = max(0, 500 - floor) ≈ 200
    float n1 = tracker.normalize(500.0f);
    FL_CHECK_GT(n1, 100.0f); // 500 - ~300 > 100
    FL_CHECK_LT(n1, 300.0f); // but < 300

    // normalize(value below floor) = 0
    float n2 = tracker.normalize(floor - 10.0f);
    FL_CHECK_EQ(n2, 0.0f);

    // normalize(floor) = 0
    float n3 = tracker.normalize(floor);
    FL_CHECK_EQ(n3, 0.0f);
}

// NF-8: First Update Initialization
FL_TEST_CASE("NoiseFloorTracker - first update initializes floor") {
    NoiseFloorTracker tracker;
    NoiseFloorTrackerConfig config;
    config.minFloor = 10.0f;
    config.maxFloor = 5000.0f;
    tracker.configure(config);

    // First update should initialize floor to observed level
    tracker.update(750.0f);
    float floor = tracker.getFloor();
    FL_CHECK_GT(floor, 700.0f);
    FL_CHECK_LT(floor, 800.0f);
}

// Keep: Basic init
FL_TEST_CASE("NoiseFloorTracker - basic initialization") {
    NoiseFloorTracker tracker;
    FL_CHECK_GT(tracker.getFloor(), 0.0f);
    FL_CHECK_EQ(tracker.getStats().samplesProcessed, 0u);
}

// Keep: Above floor detection
FL_TEST_CASE("NoiseFloorTracker - above floor detection") {
    NoiseFloorTracker tracker;
    NoiseFloorTrackerConfig config;
    config.hysteresisMargin = 100.0f;
    tracker.configure(config);

    for (int i = 0; i < 20; ++i) tracker.update(200.0f);
    float floor = tracker.getFloor();

    FL_CHECK_FALSE(tracker.isAboveFloor(floor + 50.0f));
    FL_CHECK(tracker.isAboveFloor(floor + 150.0f));
    FL_CHECK_FALSE(tracker.isAboveFloor(floor - 10.0f));
}

// Keep: Time domain only
FL_TEST_CASE("NoiseFloorTracker - time domain only") {
    NoiseFloorTracker tracker;
    for (int i = 0; i < 20; ++i) tracker.update(250.0f);
    float floor = tracker.getFloor();
    FL_CHECK_GT(floor, 50.0f);
    FL_CHECK_LT(floor, 400.0f);
}

// Keep: Statistics
FL_TEST_CASE("NoiseFloorTracker - statistics tracking") {
    NoiseFloorTracker tracker;
    tracker.update(100.0f);
    tracker.update(500.0f);
    tracker.update(50.0f);
    tracker.update(300.0f);

    const auto& stats = tracker.getStats();
    FL_CHECK_EQ(stats.samplesProcessed, 4u);
    FL_CHECK_EQ(stats.minObserved, 50.0f);
    FL_CHECK_EQ(stats.maxObserved, 500.0f);
}

// Keep: Reset
FL_TEST_CASE("NoiseFloorTracker - reset clears state") {
    NoiseFloorTracker tracker;
    for (int i = 0; i < 20; ++i) tracker.update(300.0f);
    FL_CHECK_GT(tracker.getStats().samplesProcessed, 0u);

    tracker.reset();
    FL_CHECK_EQ(tracker.getStats().samplesProcessed, 0u);
    FL_CHECK_EQ(tracker.getStats().minObserved, 0.0f);
    FL_CHECK_FALSE(tracker.getStats().inHysteresis);
}

// Keep: Disabled mode
FL_TEST_CASE("NoiseFloorTracker - disabled mode") {
    NoiseFloorTracker tracker;
    NoiseFloorTrackerConfig config;
    config.enabled = false;
    tracker.configure(config);

    float initialFloor = tracker.getFloor();
    for (int i = 0; i < 20; ++i) tracker.update(1000.0f);

    FL_CHECK_EQ(tracker.getFloor(), initialFloor);
    FL_CHECK_EQ(tracker.getStats().samplesProcessed, 0u);
}
