// Unit tests for smoothing filters: ExponentialSmoother, MovingAverage, MedianFilter

#include "test.h"
#include "fl/filter.h"
#include "fl/stl/fixed_point.h"

using namespace fl;

namespace { // Anonymous namespace for smoothing tests

// --- Float tests (circular_buffer, default) ---

FL_TEST_CASE("ExponentialSmoother - converges to constant input") {
    ExponentialSmoother<float> ema(0.1f, 0.0f); // tau=0.1s, start at 0
    float v = 0.0f;
    for (int i = 0; i < 100; ++i) {
        v = ema.update(1.0f, 0.01f);
    }
    // After 1 second (10x tau), should be very close to 1.0
    FL_CHECK_GT(v, 0.99f);
    FL_CHECK_LT(v, 1.01f);
}

FL_TEST_CASE("ExponentialSmoother - reset works") {
    ExponentialSmoother<float> ema(0.1f, 5.0f);
    FL_CHECK_EQ(ema.value(), 5.0f);
    ema.reset(0.0f);
    FL_CHECK_EQ(ema.value(), 0.0f);
}

FL_TEST_CASE("ExponentialSmoother - large tau is slower") {
    ExponentialSmoother<float> fast(0.01f, 0.0f);
    ExponentialSmoother<float> slow(1.0f, 0.0f);
    for (int i = 0; i < 10; ++i) {
        fast.update(1.0f, 0.01f);
        slow.update(1.0f, 0.01f);
    }
    FL_CHECK_GT(fast.value(), slow.value());
}

FL_TEST_CASE("MovingAverage - basic average") {
    MovingAverage<float, 4> ma;
    ma.update(2.0f);
    ma.update(4.0f);
    ma.update(6.0f);
    ma.update(8.0f);
    float v = ma.value();
    FL_CHECK_GT(v, 4.99f);
    FL_CHECK_LT(v, 5.01f);
}

FL_TEST_CASE("MovingAverage - sliding window") {
    MovingAverage<float, 3> ma;
    ma.update(1.0f);
    ma.update(2.0f);
    ma.update(3.0f);
    FL_CHECK_TRUE(ma.full());
    float v1 = ma.value();
    FL_CHECK_GT(v1, 1.99f);
    FL_CHECK_LT(v1, 2.01f);

    ma.update(6.0f);
    // Window: {2, 3, 6}, avg = 3.666...
    float v2 = ma.value();
    FL_CHECK_GT(v2, 3.6f);
    FL_CHECK_LT(v2, 3.7f);
}

FL_TEST_CASE("MovingAverage - reset") {
    MovingAverage<float, 4> ma;
    ma.update(10.0f);
    ma.reset();
    FL_CHECK_FALSE(ma.full());
    FL_CHECK_EQ(ma.value(), 0.0f);
}

FL_TEST_CASE("MedianFilter - basic median") {
    MedianFilter<float, 5> mf;
    mf.update(3.0f);
    mf.update(1.0f);
    mf.update(4.0f);
    mf.update(1.0f);
    mf.update(5.0f);
    // Sorted: {1, 1, 3, 4, 5}, median = 3.0
    FL_CHECK_EQ(mf.value(), 3.0f);
}

FL_TEST_CASE("MedianFilter - rejects outlier") {
    MedianFilter<float, 3> mf;
    mf.update(10.0f);
    mf.update(10.0f);
    mf.update(10.0f);
    mf.update(1000.0f);
    // Window: {10, 10, 1000}, median = 10
    FL_CHECK_EQ(mf.value(), 10.0f);
}

FL_TEST_CASE("MedianFilter - reset") {
    MedianFilter<float, 3> mf;
    mf.update(5.0f);
    mf.update(5.0f);
    mf.update(5.0f);
    FL_CHECK_EQ(mf.value(), 5.0f);
    mf.reset();
    FL_CHECK_EQ(mf.value(), 0.0f);
}

FL_TEST_CASE("MedianFilter - sliding eviction") {
    MedianFilter<float, 3> mf;
    mf.update(5.0f);
    mf.update(3.0f);
    mf.update(7.0f);
    // Window: {5,3,7}, sorted: {3,5,7}, median = 5
    FL_CHECK_EQ(mf.value(), 5.0f);

    mf.update(1.0f);
    // Evicts 5, window: {3,7,1}, sorted: {1,3,7}, median = 3
    FL_CHECK_EQ(mf.value(), 3.0f);

    mf.update(9.0f);
    // Evicts 3, window: {7,1,9}, sorted: {1,7,9}, median = 7
    FL_CHECK_EQ(mf.value(), 7.0f);

    mf.update(9.0f);
    // Evicts 7, window: {1,9,9}, sorted: {1,9,9}, median = 9
    FL_CHECK_EQ(mf.value(), 9.0f);
}

FL_TEST_CASE("MovingAverage - integer type") {
    MovingAverage<int, 3> ma;
    ma.update(3);
    ma.update(6);
    ma.update(9);
    FL_CHECK_EQ(ma.value(), 6);
}

// --- Dynamic buffer tests ---

FL_TEST_CASE("MovingAverage - dynamic circular_buffer") {
    MovingAverage<float, 0> ma(4);
    ma.update(2.0f);
    ma.update(4.0f);
    ma.update(6.0f);
    ma.update(8.0f);
    float v = ma.value();
    FL_CHECK_GT(v, 4.99f);
    FL_CHECK_LT(v, 5.01f);
    FL_CHECK_EQ(ma.capacity(), 4);
}

FL_TEST_CASE("MovingAverage - dynamic resize") {
    MovingAverage<float, 0> ma(2);
    ma.update(10.0f);
    ma.update(20.0f);
    FL_CHECK_TRUE(ma.full());
    float v1 = ma.value();
    FL_CHECK_GT(v1, 14.9f);
    FL_CHECK_LT(v1, 15.1f);

    // Resize to larger window — resets state.
    ma.resize(4);
    FL_CHECK_EQ(ma.size(), 0);
    FL_CHECK_EQ(ma.capacity(), 4);
    ma.update(1.0f);
    ma.update(2.0f);
    ma.update(3.0f);
    ma.update(4.0f);
    float v2 = ma.value();
    FL_CHECK_GT(v2, 2.49f);
    FL_CHECK_LT(v2, 2.51f);
}

FL_TEST_CASE("MedianFilter - dynamic circular_buffer") {
    MedianFilter<float, 0> mf(5);
    mf.update(3.0f);
    mf.update(1.0f);
    mf.update(4.0f);
    mf.update(1.0f);
    mf.update(5.0f);
    FL_CHECK_EQ(mf.value(), 3.0f);
}

FL_TEST_CASE("MedianFilter - dynamic resize") {
    MedianFilter<float, 0> mf(3);
    mf.update(10.0f);
    mf.update(20.0f);
    mf.update(30.0f);
    FL_CHECK_EQ(mf.value(), 20.0f);

    mf.resize(5);
    FL_CHECK_EQ(mf.size(), 0);
    mf.update(1.0f);
    mf.update(2.0f);
    mf.update(3.0f);
    mf.update(4.0f);
    mf.update(5.0f);
    FL_CHECK_EQ(mf.value(), 3.0f);
}

// --- Fixed-point tests ---

using FP = fl::fixed_point<16, 16>;

FL_TEST_CASE("MovingAverage - fixed_point s16x16") {
    MovingAverage<FP, 4> ma;
    ma.update(FP(2.0f));
    ma.update(FP(4.0f));
    ma.update(FP(6.0f));
    ma.update(FP(8.0f));
    float v = ma.value().to_float();
    FL_CHECK_GT(v, 4.9f);
    FL_CHECK_LT(v, 5.1f);
}

FL_TEST_CASE("MovingAverage - fixed_point sliding window") {
    MovingAverage<FP, 3> ma;
    ma.update(FP(1.0f));
    ma.update(FP(2.0f));
    ma.update(FP(3.0f));
    FL_CHECK_TRUE(ma.full());
    float v1 = ma.value().to_float();
    FL_CHECK_GT(v1, 1.9f);
    FL_CHECK_LT(v1, 2.1f);

    ma.update(FP(6.0f));
    float v2 = ma.value().to_float();
    FL_CHECK_GT(v2, 3.5f);
    FL_CHECK_LT(v2, 3.8f);
}

FL_TEST_CASE("MedianFilter - fixed_point s16x16") {
    MedianFilter<FP, 5> mf;
    mf.update(FP(3.0f));
    mf.update(FP(1.0f));
    mf.update(FP(4.0f));
    mf.update(FP(1.0f));
    mf.update(FP(5.0f));
    float v = mf.value().to_float();
    FL_CHECK_GT(v, 2.9f);
    FL_CHECK_LT(v, 3.1f);
}

FL_TEST_CASE("MedianFilter - fixed_point rejects outlier") {
    MedianFilter<FP, 3> mf;
    mf.update(FP(10.0f));
    mf.update(FP(10.0f));
    mf.update(FP(10.0f));
    mf.update(FP(1000.0f));
    float v = mf.value().to_float();
    FL_CHECK_GT(v, 9.9f);
    FL_CHECK_LT(v, 10.1f);
}

FL_TEST_CASE("ExponentialSmoother - fixed_point s16x16 converges") {
    FP tau(0.5f);
    FP zero(0.0f);
    FP one(1.0f);
    FP dt(0.05f);
    ExponentialSmoother<FP> ema(tau, zero);
    FP v = zero;
    for (int i = 0; i < 200; ++i) {
        v = ema.update(one, dt);
    }
    float vf = v.to_float();
    FL_CHECK_GT(vf, 0.9f);
    FL_CHECK_LT(vf, 1.1f);
}

// ============================================================================
// New filter tests
// ============================================================================

// --- LeakyIntegrator ---

FL_TEST_CASE("LeakyIntegrator - converges to constant") {
    LeakyIntegrator<float, 2> li; // alpha = 1/4
    float v = 0.0f;
    for (int i = 0; i < 100; ++i) {
        v = li.update(1.0f);
    }
    FL_CHECK_GT(v, 0.99f);
    FL_CHECK_LT(v, 1.01f);
}

FL_TEST_CASE("LeakyIntegrator - reset") {
    LeakyIntegrator<float, 3> li(5.0f);
    FL_CHECK_GT(li.value(), 4.9f);
    li.reset(0.0f);
    FL_CHECK_EQ(li.value(), 0.0f);
}

FL_TEST_CASE("LeakyIntegrator - integer shift") {
    LeakyIntegrator<int, 1> li; // alpha = 1/2
    li.update(100);
    // y = 0 + (100 - 0) >> 1 = 50
    FL_CHECK_EQ(li.value(), 50);
    li.update(100);
    // y = 50 + (100 - 50) >> 1 = 75
    FL_CHECK_EQ(li.value(), 75);
}

// --- CascadedEMA ---

FL_TEST_CASE("CascadedEMA - converges to constant") {
    CascadedEMA<float, 2> cema(0.1f, 0.0f);
    float v = 0.0f;
    for (int i = 0; i < 200; ++i) {
        v = cema.update(1.0f, 0.01f);
    }
    FL_CHECK_GT(v, 0.98f);
    FL_CHECK_LT(v, 1.02f);
}

FL_TEST_CASE("CascadedEMA - more stages = slower") {
    CascadedEMA<float, 1> fast(0.1f, 0.0f);
    CascadedEMA<float, 3> slow(0.1f, 0.0f);
    for (int i = 0; i < 20; ++i) {
        fast.update(1.0f, 0.01f);
        slow.update(1.0f, 0.01f);
    }
    FL_CHECK_GT(fast.value(), slow.value());
}

FL_TEST_CASE("CascadedEMA - reset") {
    CascadedEMA<float, 2> cema(0.1f, 5.0f);
    FL_CHECK_GT(cema.value(), 4.9f);
    cema.reset(0.0f);
    FL_CHECK_EQ(cema.value(), 0.0f);
}

// --- WeightedMovingAverage ---

FL_TEST_CASE("WeightedMovingAverage - weights newer samples more") {
    WeightedMovingAverage<float, 3> wma;
    wma.update(1.0f);
    wma.update(2.0f);
    wma.update(3.0f);
    // Weights: [1,2,3], weighted avg = (1*1 + 2*2 + 3*3)/6 = 14/6 ≈ 2.33
    float v = wma.value();
    FL_CHECK_GT(v, 2.3f);
    FL_CHECK_LT(v, 2.4f);
}

FL_TEST_CASE("WeightedMovingAverage - reset") {
    WeightedMovingAverage<float, 4> wma;
    wma.update(5.0f);
    wma.reset();
    FL_CHECK_EQ(wma.value(), 0.0f);
}

// --- TriangularFilter ---

FL_TEST_CASE("TriangularFilter - symmetric weights") {
    TriangularFilter<float, 5> tf;
    tf.update(1.0f);
    tf.update(2.0f);
    tf.update(3.0f);
    tf.update(2.0f);
    tf.update(1.0f);
    // Weights for N=5: [1,2,3,2,1], total=9
    // Weighted sum = 1*1 + 2*2 + 3*3 + 2*2 + 1*1 = 1+4+9+4+1 = 19
    // 19/9 ≈ 2.11
    float v = tf.value();
    FL_CHECK_GT(v, 2.0f);
    FL_CHECK_LT(v, 2.2f);
}

FL_TEST_CASE("TriangularFilter - reset") {
    TriangularFilter<float, 3> tf;
    tf.update(5.0f);
    tf.reset();
    FL_CHECK_EQ(tf.value(), 0.0f);
}

// --- GaussianFilter ---

FL_TEST_CASE("GaussianFilter - constant signal") {
    GaussianFilter<float, 5> gf;
    for (int i = 0; i < 5; ++i) gf.update(7.0f);
    FL_CHECK_GT(gf.value(), 6.99f);
    FL_CHECK_LT(gf.value(), 7.01f);
}

FL_TEST_CASE("GaussianFilter - weights center more") {
    GaussianFilter<float, 5> gf;
    gf.update(0.0f);
    gf.update(0.0f);
    gf.update(10.0f);
    gf.update(0.0f);
    gf.update(0.0f);
    // Weights: [1,4,6,4,1]/16. Center gets 6/16 = 37.5% of weight.
    // Result = 10*6/16 = 3.75
    float v = gf.value();
    FL_CHECK_GT(v, 3.7f);
    FL_CHECK_LT(v, 3.8f);
}

FL_TEST_CASE("GaussianFilter - reset") {
    GaussianFilter<float, 3> gf;
    gf.update(5.0f);
    gf.reset();
    FL_CHECK_EQ(gf.value(), 0.0f);
}

// --- AlphaTrimmedMean ---

FL_TEST_CASE("AlphaTrimmedMean - trims extremes") {
    AlphaTrimmedMean<float, 5> atm(1);
    atm.update(1.0f);
    atm.update(5.0f);
    atm.update(5.0f);
    atm.update(5.0f);
    atm.update(100.0f);
    // Sorted: [1, 5, 5, 5, 100], trim 1 from each end → [5, 5, 5], avg = 5.0
    FL_CHECK_GT(atm.value(), 4.99f);
    FL_CHECK_LT(atm.value(), 5.01f);
}

FL_TEST_CASE("AlphaTrimmedMean - zero trim = moving average") {
    AlphaTrimmedMean<float, 3> atm(0);
    atm.update(3.0f);
    atm.update(6.0f);
    atm.update(9.0f);
    // No trimming: avg of [3,6,9] = 6.0
    FL_CHECK_GT(atm.value(), 5.99f);
    FL_CHECK_LT(atm.value(), 6.01f);
}

FL_TEST_CASE("AlphaTrimmedMean - reset") {
    AlphaTrimmedMean<float, 5> atm(1);
    atm.update(5.0f);
    atm.reset();
    FL_CHECK_EQ(atm.value(), 0.0f);
}

// --- HampelFilter ---

FL_TEST_CASE("HampelFilter - passes normal values") {
    HampelFilter<float, 5> hf(3.0f);
    hf.update(9.0f);
    hf.update(10.0f);
    hf.update(11.0f);
    hf.update(10.0f);
    // Window has variance: MAD > 0. A value within 3*MAD should pass.
    float v = hf.update(10.5f);
    FL_CHECK_GT(v, 10.4f);
    FL_CHECK_LT(v, 10.6f);
}

FL_TEST_CASE("HampelFilter - rejects outlier") {
    HampelFilter<float, 5> hf(3.0f);
    hf.update(10.0f);
    hf.update(10.0f);
    hf.update(10.0f);
    hf.update(10.0f);
    float v = hf.update(1000.0f);
    // 1000.0 deviates wildly from median 10.0, replaced with median.
    FL_CHECK_GT(v, 9.9f);
    FL_CHECK_LT(v, 10.1f);
}

FL_TEST_CASE("HampelFilter - reset") {
    HampelFilter<float, 3> hf;
    hf.update(5.0f);
    hf.reset();
    FL_CHECK_EQ(hf.value(), 0.0f);
}

// --- BiquadFilter ---

FL_TEST_CASE("BiquadFilter - Butterworth converges") {
    auto lpf = BiquadFilter<float>::butterworth(100.0f, 1000.0f);
    float v = 0.0f;
    for (int i = 0; i < 200; ++i) {
        v = lpf.update(1.0f);
    }
    FL_CHECK_GT(v, 0.98f);
    FL_CHECK_LT(v, 1.02f);
}

FL_TEST_CASE("BiquadFilter - reset") {
    auto lpf = BiquadFilter<float>::butterworth(50.0f, 1000.0f);
    lpf.update(5.0f);
    lpf.update(5.0f);
    lpf.reset();
    FL_CHECK_EQ(lpf.value(), 0.0f);
}

FL_TEST_CASE("BiquadFilter - low cutoff smooths more") {
    auto low = BiquadFilter<float>::butterworth(10.0f, 1000.0f);
    auto high = BiquadFilter<float>::butterworth(200.0f, 1000.0f);
    // Step response: feed 1.0 and check after a few samples.
    for (int i = 0; i < 10; ++i) {
        low.update(1.0f);
        high.update(1.0f);
    }
    // High cutoff should respond faster.
    FL_CHECK_GT(high.value(), low.value());
}

// --- KalmanFilter ---

FL_TEST_CASE("KalmanFilter - converges to constant") {
    KalmanFilter<float> kf(0.01f, 0.1f, 0.0f);
    float v = 0.0f;
    for (int i = 0; i < 100; ++i) {
        v = kf.update(5.0f);
    }
    FL_CHECK_GT(v, 4.9f);
    FL_CHECK_LT(v, 5.1f);
}

FL_TEST_CASE("KalmanFilter - smooth noise") {
    KalmanFilter<float> kf(0.01f, 1.0f, 0.0f);
    // Feed alternating noisy values around 10.0
    for (int i = 0; i < 50; ++i) {
        float noise = (i % 2 == 0) ? 12.0f : 8.0f;
        kf.update(noise);
    }
    // Should converge near 10.0
    FL_CHECK_GT(kf.value(), 9.5f);
    FL_CHECK_LT(kf.value(), 10.5f);
}

FL_TEST_CASE("KalmanFilter - reset") {
    KalmanFilter<float> kf(0.01f, 0.1f, 5.0f);
    FL_CHECK_GT(kf.value(), 4.9f);
    kf.reset(0.0f);
    FL_CHECK_EQ(kf.value(), 0.0f);
}

// --- OneEuroFilter ---

FL_TEST_CASE("OneEuroFilter - converges to constant") {
    OneEuroFilter<float> oef(1.0f, 0.0f);
    float v = 0.0f;
    for (int i = 0; i < 100; ++i) {
        v = oef.update(5.0f, 0.016f);
    }
    FL_CHECK_GT(v, 4.9f);
    FL_CHECK_LT(v, 5.1f);
}

FL_TEST_CASE("OneEuroFilter - adapts to velocity") {
    // With beta > 0, fast changes should have less smoothing.
    OneEuroFilter<float> oef(1.0f, 0.5f);
    // Settle at 0.
    for (int i = 0; i < 50; ++i) oef.update(0.0f, 0.016f);
    // Jump to 100 — should track quickly due to high beta.
    float v = 0.0f;
    for (int i = 0; i < 20; ++i) {
        v = oef.update(100.0f, 0.016f);
    }
    FL_CHECK_GT(v, 50.0f); // Should have tracked a good portion.
}

FL_TEST_CASE("OneEuroFilter - reset") {
    OneEuroFilter<float> oef(1.0f, 0.0f);
    oef.update(5.0f, 0.016f);
    oef.reset(0.0f);
    FL_CHECK_EQ(oef.value(), 0.0f);
}

// --- SavitzkyGolayFilter ---

FL_TEST_CASE("SavitzkyGolayFilter - constant signal") {
    SavitzkyGolayFilter<float, 5> sg;
    for (int i = 0; i < 5; ++i) sg.update(3.0f);
    FL_CHECK_GT(sg.value(), 2.99f);
    FL_CHECK_LT(sg.value(), 3.01f);
}

FL_TEST_CASE("SavitzkyGolayFilter - preserves linear trend") {
    SavitzkyGolayFilter<float, 5> sg;
    sg.update(1.0f);
    sg.update(2.0f);
    sg.update(3.0f);
    sg.update(4.0f);
    sg.update(5.0f);
    // SG quadratic fit on a linear ramp should return the center value (3.0).
    FL_CHECK_GT(sg.value(), 2.9f);
    FL_CHECK_LT(sg.value(), 3.1f);
}

FL_TEST_CASE("SavitzkyGolayFilter - reset") {
    SavitzkyGolayFilter<float, 5> sg;
    sg.update(5.0f);
    sg.reset();
    FL_CHECK_EQ(sg.value(), 0.0f);
}

// --- BilateralFilter ---

FL_TEST_CASE("BilateralFilter - constant signal") {
    BilateralFilter<float, 5> bf(1.0f);
    for (int i = 0; i < 5; ++i) bf.update(4.0f);
    FL_CHECK_GT(bf.value(), 3.99f);
    FL_CHECK_LT(bf.value(), 4.01f);
}

FL_TEST_CASE("BilateralFilter - preserves edge") {
    BilateralFilter<float, 5> bf(0.5f);
    // Fill with values near 10, then one outlier at 100.
    bf.update(10.0f);
    bf.update(10.0f);
    bf.update(10.0f);
    bf.update(10.0f);
    bf.update(100.0f);
    // With small sigma, the outlier (100) has low weight relative to the 10s.
    // Result should be pulled toward 10.
    float v = bf.value();
    // The newest sample (100) gets full weight (diff=0), but the 10s get
    // exp(-(90^2)/(2*0.25)) ≈ 0 weight. So result ≈ 100.
    // Actually bilateral weights relative to the NEWEST sample. Let me fix:
    // v ≈ 100 (since the 10s are very far from 100 with sigma=0.5)
    FL_CHECK_GT(v, 90.0f);
}

FL_TEST_CASE("BilateralFilter - large sigma = box-like") {
    BilateralFilter<float, 3> bf(1000.0f);
    bf.update(1.0f);
    bf.update(2.0f);
    bf.update(3.0f);
    // With huge sigma, all weights ≈ 1, so result ≈ average = 2.0
    FL_CHECK_GT(bf.value(), 1.9f);
    FL_CHECK_LT(bf.value(), 2.1f);
}

FL_TEST_CASE("BilateralFilter - reset") {
    BilateralFilter<float, 3> bf(1.0f);
    bf.update(5.0f);
    bf.reset();
    FL_CHECK_EQ(bf.value(), 0.0f);
}

// ============================================================================
// Fixed-point tests for new filters
// ============================================================================

FL_TEST_CASE("LeakyIntegrator - fixed_point converges") {
    LeakyIntegrator<FP, 2> li;
    FP v(0.0f);
    for (int i = 0; i < 200; ++i) {
        v = li.update(FP(1.0f));
    }
    FL_CHECK_GT(v.to_float(), 0.99f);
    FL_CHECK_LT(v.to_float(), 1.01f);
}

FL_TEST_CASE("CascadedEMA - fixed_point converges") {
    CascadedEMA<FP, 2> cema(FP(0.1f), FP(0.0f));
    FP v(0.0f);
    for (int i = 0; i < 200; ++i) {
        v = cema.update(FP(1.0f), FP(0.01f));
    }
    FL_CHECK_GT(v.to_float(), 0.9f);
    FL_CHECK_LT(v.to_float(), 1.1f);
}

FL_TEST_CASE("WeightedMovingAverage - fixed_point") {
    WeightedMovingAverage<FP, 3> wma;
    wma.update(FP(1.0f));
    wma.update(FP(2.0f));
    wma.update(FP(3.0f));
    float v = wma.value().to_float();
    FL_CHECK_GT(v, 2.2f);
    FL_CHECK_LT(v, 2.5f);
}

FL_TEST_CASE("GaussianFilter - fixed_point constant") {
    GaussianFilter<FP, 5> gf;
    for (int i = 0; i < 5; ++i) gf.update(FP(7.0f));
    float v = gf.value().to_float();
    FL_CHECK_GT(v, 6.9f);
    FL_CHECK_LT(v, 7.1f);
}

FL_TEST_CASE("AlphaTrimmedMean - fixed_point") {
    AlphaTrimmedMean<FP, 5> atm(1);
    atm.update(FP(1.0f));
    atm.update(FP(5.0f));
    atm.update(FP(5.0f));
    atm.update(FP(5.0f));
    atm.update(FP(100.0f));
    float v = atm.value().to_float();
    FL_CHECK_GT(v, 4.9f);
    FL_CHECK_LT(v, 5.1f);
}

FL_TEST_CASE("KalmanFilter - fixed_point converges") {
    KalmanFilter<FP> kf(FP(0.01f), FP(0.1f), FP(0.0f));
    FP v(0.0f);
    for (int i = 0; i < 100; ++i) {
        v = kf.update(FP(5.0f));
    }
    FL_CHECK_GT(v.to_float(), 4.8f);
    FL_CHECK_LT(v.to_float(), 5.2f);
}

FL_TEST_CASE("SavitzkyGolayFilter - fixed_point constant") {
    SavitzkyGolayFilter<FP, 5> sg;
    for (int i = 0; i < 5; ++i) sg.update(FP(3.0f));
    float v = sg.value().to_float();
    FL_CHECK_GT(v, 2.9f);
    FL_CHECK_LT(v, 3.1f);
}

// ============================================================================
// Adversarial / edge-case tests
// ============================================================================

FL_TEST_CASE("GaussianFilter - large N binomial overflow") {
    // N=35: C(34,17) = 2,333,606,220 > 2^31.
    // If fl::size is 32-bit, the binomial coefficient overflows silently
    // and the filter produces garbage weights.
    // Even on 64-bit, the intermediate multiplication can overflow for
    // sufficiently large N.
    // With N=35 and a constant signal, the output should still be that constant.
    GaussianFilter<float, 35> gf;
    for (int i = 0; i < 35; ++i) gf.update(5.0f);
    float v = gf.value();
    FL_CHECK_GT(v, 4.9f);
    FL_CHECK_LT(v, 5.1f);
}

FL_TEST_CASE("SavitzkyGolayFilter - odd window size N=7") {
    SavitzkyGolayFilter<float, 7> sg;
    for (int i = 0; i < 7; ++i) sg.update(4.0f);
    float v = sg.value();
    FL_CHECK_GT(v, 3.9f);
    FL_CHECK_LT(v, 4.1f);
}

FL_TEST_CASE("SavitzkyGolayFilter - odd window size N=9") {
    SavitzkyGolayFilter<float, 9> sg;
    for (int i = 0; i < 9; ++i) sg.update(7.0f);
    float v = sg.value();
    FL_CHECK_GT(v, 6.9f);
    FL_CHECK_LT(v, 7.1f);
}

FL_TEST_CASE("BilateralFilter - zero sigma") {
    // sigma_range = 0 causes two_sigma_sq = 0. The filter should handle
    // this gracefully by returning the input (Dirac delta limit).
    BilateralFilter<float, 3> bf(0.0f);
    bf.update(1.0f);
    bf.update(2.0f);
    float v = bf.update(3.0f);
    FL_CHECK_EQ(v, v); // not NaN
    FL_CHECK_EQ(v, 3.0f); // returns input when sigma=0
}

FL_TEST_CASE("MovingAverage - integer overflow with large values") {
    // With int type and window of 4, sum of 4 * (INT_MAX/2) shouldn't overflow
    // but sum of 4 * INT_MAX will. The filter should still be usable with
    // reasonable values.
    MovingAverage<int, 4> ma;
    ma.update(1000000);
    ma.update(1000000);
    ma.update(1000000);
    ma.update(1000000);
    FL_CHECK_EQ(ma.value(), 1000000);
}

FL_TEST_CASE("SavitzkyGolayFilter - negative weight cancellation on spike") {
    // SG filters have negative edge weights. An adversarial input
    // with a single spike should not produce a wildly wrong result.
    SavitzkyGolayFilter<float, 7> sg;
    sg.update(0.0f);
    sg.update(0.0f);
    sg.update(0.0f);
    sg.update(100.0f); // spike
    sg.update(0.0f);
    sg.update(0.0f);
    sg.update(0.0f);
    // The SG filter should smooth the spike. The output should not be
    // negative or wildly amplified beyond the input range [0, 100].
    float v = sg.value();
    FL_CHECK_GT(v, -50.0f);  // shouldn't invert too much
    FL_CHECK_LT(v, 150.0f);  // shouldn't amplify beyond input
}

FL_TEST_CASE("ExponentialSmoother - zero tau") {
    // tau=0 causes dt/tau = infinity, exp(-inf)=0, so decay=0.
    // Output should snap to input immediately.
    ExponentialSmoother<float> ema(0.0f, 0.0f);
    float v = ema.update(5.0f, 0.01f);
    FL_CHECK_EQ(v, v); // not NaN
}

FL_TEST_CASE("ExponentialSmoother - negative dt") {
    // Negative dt could cause exp(+value) which blows up.
    ExponentialSmoother<float> ema(0.1f, 0.0f);
    float v = ema.update(1.0f, -0.01f);
    // Should not produce NaN or inf
    FL_CHECK_EQ(v, v); // not NaN
}

FL_TEST_CASE("KalmanFilter - zero measurement noise") {
    // R=0 causes division by zero in k = P / (P + R).
    // Should not crash or produce NaN.
    KalmanFilter<float> kf(0.01f, 0.0f, 0.0f);
    float v = kf.update(5.0f);
    FL_CHECK_EQ(v, v); // not NaN
}

FL_TEST_CASE("OneEuroFilter - zero dt") {
    // dt=0 causes division by zero in dx = (input - mX) / dt.
    OneEuroFilter<float> oef(1.0f, 0.0f);
    oef.update(1.0f, 0.016f); // first call OK
    float v = oef.update(2.0f, 0.0f); // second call: dt=0
    FL_CHECK_EQ(v, v); // not NaN
}

FL_TEST_CASE("WeightedMovingAverage - constant signal returns constant") {
    // Regardless of weights, a constant signal should return that constant.
    WeightedMovingAverage<float, 8> wma;
    for (int i = 0; i < 8; ++i) wma.update(42.0f);
    FL_CHECK_GT(wma.value(), 41.99f);
    FL_CHECK_LT(wma.value(), 42.01f);
}

// ============================================================================
// AttackDecayFilter tests
// ============================================================================

FL_TEST_CASE("AttackDecayFilter - converges to constant input") {
    AttackDecayFilter<float> adf(0.1f, 0.1f, 0.0f); // same tau = same as EMA
    float v = 0.0f;
    for (int i = 0; i < 100; ++i) {
        v = adf.update(1.0f, 0.01f);
    }
    FL_CHECK_GT(v, 0.99f);
    FL_CHECK_LT(v, 1.01f);
}

FL_TEST_CASE("AttackDecayFilter - fast attack slow decay") {
    AttackDecayFilter<float> adf(0.01f, 1.0f, 0.0f);
    // Rising: should track quickly (attack tau = 10ms)
    float v = 0.0f;
    for (int i = 0; i < 10; ++i) {
        v = adf.update(1.0f, 0.01f);
    }
    FL_CHECK_GT(v, 0.95f); // should be very close after 100ms with 10ms tau

    // Falling: should decay slowly (decay tau = 1s)
    for (int i = 0; i < 10; ++i) {
        v = adf.update(0.0f, 0.01f);
    }
    FL_CHECK_GT(v, 0.8f); // should still be high after 100ms with 1s tau
}

FL_TEST_CASE("AttackDecayFilter - slow attack fast decay") {
    AttackDecayFilter<float> adf(1.0f, 0.01f, 0.0f);
    // Rising: should track slowly (attack tau = 1s)
    float v = 0.0f;
    for (int i = 0; i < 10; ++i) {
        v = adf.update(1.0f, 0.01f);
    }
    FL_CHECK_LT(v, 0.15f); // slow rise

    // Now set value high and let it decay fast
    adf.reset(1.0f);
    for (int i = 0; i < 10; ++i) {
        v = adf.update(0.0f, 0.01f);
    }
    FL_CHECK_LT(v, 0.05f); // should have decayed quickly
}

FL_TEST_CASE("AttackDecayFilter - reset") {
    AttackDecayFilter<float> adf(0.1f, 0.5f, 5.0f);
    FL_CHECK_GT(adf.value(), 4.9f);
    adf.reset(0.0f);
    FL_CHECK_EQ(adf.value(), 0.0f);
}

FL_TEST_CASE("AttackDecayFilter - setAttackTau and setDecayTau") {
    AttackDecayFilter<float> adf(0.1f, 0.1f, 0.0f);
    adf.setAttackTau(0.01f);
    adf.setDecayTau(1.0f);
    // Fast attack
    float v = 0.0f;
    for (int i = 0; i < 10; ++i) {
        v = adf.update(1.0f, 0.01f);
    }
    FL_CHECK_GT(v, 0.95f);
}

// ============================================================================
// DCBlocker tests
// ============================================================================

FL_TEST_CASE("DCBlocker - removes DC offset") {
    DCBlocker<float> dc(0.995f);
    // Feed constant DC signal — output should converge to zero.
    float v = 0.0f;
    for (int i = 0; i < 1000; ++i) {
        v = dc.update(5.0f);
    }
    FL_CHECK_GT(v, -0.1f);
    FL_CHECK_LT(v, 0.1f);
}

FL_TEST_CASE("DCBlocker - passes AC signal") {
    DCBlocker<float> dc(0.995f);
    // Feed alternating signal (AC-like). After settling, peaks should
    // be close to the original amplitude.
    for (int i = 0; i < 100; ++i) {
        dc.update((i % 2 == 0) ? 1.0f : -1.0f);
    }
    float v_high = dc.update(1.0f);
    float v_low = dc.update(-1.0f);
    // Peak-to-peak should be close to 2.0 (the original amplitude).
    float pp = v_high - v_low;
    FL_CHECK_GT(pp, 1.8f);
}

FL_TEST_CASE("DCBlocker - reset") {
    DCBlocker<float> dc;
    dc.update(5.0f);
    dc.update(5.0f);
    dc.reset();
    FL_CHECK_EQ(dc.value(), 0.0f);
}

FL_TEST_CASE("DCBlocker - first sample passes through") {
    DCBlocker<float> dc(0.995f);
    // First sample: y = x - 0 + R*0 = x
    float v = dc.update(3.0f);
    FL_CHECK_GT(v, 2.99f);
    FL_CHECK_LT(v, 3.01f);
}

// ============================================================================
// BiquadFilter highpass / bandpass / notch tests
// ============================================================================

FL_TEST_CASE("BiquadFilter::highpass - blocks DC") {
    auto hpf = BiquadFilter<float>::highpass(100.0f, 1000.0f);
    // Feed DC signal — output should converge to zero.
    float v = 0.0f;
    for (int i = 0; i < 200; ++i) {
        v = hpf.update(1.0f);
    }
    FL_CHECK_GT(v, -0.05f);
    FL_CHECK_LT(v, 0.05f);
}

FL_TEST_CASE("BiquadFilter::highpass - passes high frequency") {
    auto hpf = BiquadFilter<float>::highpass(10.0f, 1000.0f);
    // Feed high-frequency alternating signal (500 Hz at 1kHz sample rate).
    for (int i = 0; i < 50; ++i) {
        hpf.update((i % 2 == 0) ? 1.0f : -1.0f);
    }
    float v_high = hpf.update(1.0f);
    float v_low = hpf.update(-1.0f);
    float pp = v_high - v_low;
    FL_CHECK_GT(pp, 1.5f); // should pass most of the signal
}

FL_TEST_CASE("BiquadFilter::bandpass - attenuates off-center") {
    // Center at 250 Hz, sample rate 1000 Hz, Q=2
    auto bpf = BiquadFilter<float>::bandpass(250.0f, 1000.0f, 2.0f);
    // Feed DC (0 Hz) — should be attenuated.
    float v = 0.0f;
    for (int i = 0; i < 200; ++i) {
        v = bpf.update(1.0f);
    }
    FL_CHECK_GT(v, -0.05f);
    FL_CHECK_LT(v, 0.05f);
}

FL_TEST_CASE("BiquadFilter::notch - passes DC") {
    // Notch at 60 Hz, sample rate 1000 Hz.
    auto nf = BiquadFilter<float>::notch(60.0f, 1000.0f, 1.0f);
    float v = 0.0f;
    for (int i = 0; i < 200; ++i) {
        v = nf.update(1.0f);
    }
    // DC should pass through a notch filter.
    FL_CHECK_GT(v, 0.95f);
    FL_CHECK_LT(v, 1.05f);
}

FL_TEST_CASE("BiquadFilter::notch - rejects center frequency") {
    // Notch at 250 Hz with high Q, sample rate 1000 Hz.
    // 250 Hz at 1000 Hz sample rate = period of 4 samples.
    auto nf = BiquadFilter<float>::notch(250.0f, 1000.0f, 10.0f);
    // Feed 250 Hz sine: sin(2*pi*250/1000 * n) = sin(pi/2 * n) = [0, 1, 0, -1, ...]
    float max_abs = 0.0f;
    for (int i = 0; i < 200; ++i) {
        float input = 0.0f;
        switch (i % 4) {
            case 0: input = 0.0f; break;
            case 1: input = 1.0f; break;
            case 2: input = 0.0f; break;
            case 3: input = -1.0f; break;
        }
        float v = nf.update(input);
        if (i > 100) { // after settling
            float av = (v < 0) ? -v : v;
            if (av > max_abs) max_abs = av;
        }
    }
    // Should be heavily attenuated.
    FL_CHECK_LT(max_abs, 0.15f);
}

// ============================================================================
// Corner-case and stress tests
// ============================================================================

// --- MovingAverage corner cases ---

FL_TEST_CASE("MovingAverage - single element window") {
    MovingAverage<float, 1> ma;
    ma.update(7.0f);
    FL_CHECK_CLOSE(ma.value(), 7.0f, 0.01f);
    ma.update(3.0f);
    FL_CHECK_CLOSE(ma.value(), 3.0f, 0.01f);
}

FL_TEST_CASE("MovingAverage - value before full") {
    MovingAverage<float, 4> ma;
    ma.update(10.0f);
    // Only 1 sample, average should be 10
    FL_CHECK_CLOSE(ma.value(), 10.0f, 0.01f);
    ma.update(20.0f);
    // 2 samples, average should be 15
    FL_CHECK_CLOSE(ma.value(), 15.0f, 0.01f);
}

FL_TEST_CASE("MovingAverage - running sum drift correction") {
    // After many updates with float, the incremental sum can drift.
    // Feed a constant and verify the average stays accurate.
    MovingAverage<float, 8> ma;
    for (int i = 0; i < 10000; ++i) {
        ma.update(1.0f / 3.0f); // irrational-ish in float
    }
    float v = ma.value();
    FL_CHECK_GT(v, 0.333f);
    FL_CHECK_LT(v, 0.334f);
}

FL_TEST_CASE("MovingAverage - negative values") {
    MovingAverage<float, 3> ma;
    ma.update(-5.0f);
    ma.update(-10.0f);
    ma.update(-15.0f);
    FL_CHECK_CLOSE(ma.value(), -10.0f, 0.01f);
}

FL_TEST_CASE("MovingAverage - alternating positive negative") {
    MovingAverage<float, 4> ma;
    ma.update(100.0f);
    ma.update(-100.0f);
    ma.update(100.0f);
    ma.update(-100.0f);
    FL_CHECK_CLOSE(ma.value(), 0.0f, 0.01f);
}

// --- MedianFilter corner cases ---

FL_TEST_CASE("MedianFilter - all duplicate values") {
    MedianFilter<float, 5> mf;
    for (int i = 0; i < 5; ++i) mf.update(42.0f);
    FL_CHECK_EQ(mf.value(), 42.0f);
    // Now push a different value
    mf.update(100.0f);
    // Window: {42, 42, 42, 42, 100}, median = 42
    FL_CHECK_EQ(mf.value(), 42.0f);
}

FL_TEST_CASE("MedianFilter - negative values") {
    MedianFilter<float, 3> mf;
    mf.update(-5.0f);
    mf.update(-1.0f);
    mf.update(-3.0f);
    // Sorted: {-5, -3, -1}, median = -3
    FL_CHECK_EQ(mf.value(), -3.0f);
}

FL_TEST_CASE("MedianFilter - before full returns partial median") {
    MedianFilter<float, 5> mf;
    mf.update(10.0f);
    // 1 element, median = element
    FL_CHECK_EQ(mf.value(), 10.0f);
    mf.update(20.0f);
    // 2 elements, sorted: {10, 20}, index 1 = 20
    FL_CHECK_EQ(mf.value(), 20.0f);
    mf.update(5.0f);
    // 3 elements, sorted: {5, 10, 20}, index 1 = 10
    FL_CHECK_EQ(mf.value(), 10.0f);
}

FL_TEST_CASE("MedianFilter - descending input") {
    MedianFilter<float, 5> mf;
    mf.update(50.0f);
    mf.update(40.0f);
    mf.update(30.0f);
    mf.update(20.0f);
    mf.update(10.0f);
    // Sorted: {10, 20, 30, 40, 50}, median = 30
    FL_CHECK_EQ(mf.value(), 30.0f);
}

FL_TEST_CASE("MedianFilter - many evictions stress test") {
    MedianFilter<float, 3> mf;
    // Push a long ascending sequence and verify median at each step
    for (int i = 0; i < 100; ++i) {
        mf.update(static_cast<float>(i));
    }
    // Window should be {97, 98, 99}, median = 98
    FL_CHECK_EQ(mf.value(), 98.0f);
}

// --- WeightedMovingAverage corner cases ---

FL_TEST_CASE("WeightedMovingAverage - single sample") {
    WeightedMovingAverage<float, 4> wma;
    wma.update(7.0f);
    // Only 1 sample with weight 1, result = 7
    FL_CHECK_CLOSE(wma.value(), 7.0f, 0.01f);
}

FL_TEST_CASE("WeightedMovingAverage - negative values") {
    WeightedMovingAverage<float, 3> wma;
    wma.update(-3.0f);
    wma.update(-6.0f);
    wma.update(-9.0f);
    // Weights: [1,2,3], sum = (-3*1 + -6*2 + -9*3) / 6 = (-3-12-27)/6 = -42/6 = -7
    FL_CHECK_CLOSE(wma.value(), -7.0f, 0.01f);
}

// --- TriangularFilter corner cases ---

FL_TEST_CASE("TriangularFilter - single sample") {
    TriangularFilter<float, 5> tf;
    tf.update(3.0f);
    FL_CHECK_CLOSE(tf.value(), 3.0f, 0.01f);
}

FL_TEST_CASE("TriangularFilter - constant signal") {
    TriangularFilter<float, 5> tf;
    for (int i = 0; i < 5; ++i) tf.update(8.0f);
    FL_CHECK_CLOSE(tf.value(), 8.0f, 0.01f);
}

// --- GaussianFilter corner cases ---

FL_TEST_CASE("GaussianFilter - single sample") {
    GaussianFilter<float, 5> gf;
    gf.update(3.0f);
    // 1 sample, binom coeff [1], result = 3
    FL_CHECK_CLOSE(gf.value(), 3.0f, 0.01f);
}

FL_TEST_CASE("GaussianFilter - two samples") {
    GaussianFilter<float, 5> gf;
    gf.update(0.0f);
    gf.update(10.0f);
    // Binomial for n=2: [1, 1], so average = 5.0
    FL_CHECK_CLOSE(gf.value(), 5.0f, 0.01f);
}

// --- AlphaTrimmedMean corner cases ---

FL_TEST_CASE("AlphaTrimmedMean - trim equals half (becomes median)") {
    AlphaTrimmedMean<float, 5> atm(2); // trim 2 from each end of 5 = 1 left = median
    atm.update(1.0f);
    atm.update(2.0f);
    atm.update(3.0f);
    atm.update(4.0f);
    atm.update(5.0f);
    // Sorted: [1,2,3,4,5], trim 2 each end → [3], avg = 3.0
    FL_CHECK_CLOSE(atm.value(), 3.0f, 0.01f);
}

FL_TEST_CASE("AlphaTrimmedMean - trim exceeds half (falls back to median)") {
    AlphaTrimmedMean<float, 5> atm(3); // trim 3 from each end of 5 → lo=3, hi=2 → lo >= hi
    atm.update(10.0f);
    atm.update(20.0f);
    atm.update(30.0f);
    atm.update(40.0f);
    atm.update(50.0f);
    // Falls back to median: sorted[2] = 30
    FL_CHECK_CLOSE(atm.value(), 30.0f, 0.01f);
}

FL_TEST_CASE("AlphaTrimmedMean - negative values") {
    AlphaTrimmedMean<float, 5> atm(1);
    atm.update(-10.0f);
    atm.update(-5.0f);
    atm.update(0.0f);
    atm.update(5.0f);
    atm.update(10.0f);
    // Sorted: [-10,-5,0,5,10], trim 1 each end → [-5,0,5], avg = 0.0
    FL_CHECK_CLOSE(atm.value(), 0.0f, 0.01f);
}

FL_TEST_CASE("AlphaTrimmedMean - before full") {
    AlphaTrimmedMean<float, 5> atm(1);
    atm.update(10.0f);
    // 1 sample, trim 1 each end: lo=1, hi=0 → falls back to median
    FL_CHECK_CLOSE(atm.value(), 10.0f, 0.01f);
}

// --- HampelFilter corner cases ---

FL_TEST_CASE("HampelFilter - all identical then slight variation") {
    HampelFilter<float, 5> hf(3.0f);
    hf.update(10.0f);
    hf.update(10.0f);
    hf.update(10.0f);
    hf.update(10.0f);
    // All identical: MAD = 0. Any different value is treated as outlier.
    float v = hf.update(10.1f);
    // With MAD=0, deviation != 0, so replaced with median = 10.0
    FL_CHECK_CLOSE(v, 10.0f, 0.01f);
}

FL_TEST_CASE("HampelFilter - first sample passes through") {
    HampelFilter<float, 5> hf(3.0f);
    // First sample: sortedCount=0, no median to compare against
    float v = hf.update(42.0f);
    FL_CHECK_CLOSE(v, 42.0f, 0.01f);
}

FL_TEST_CASE("HampelFilter - negative values") {
    HampelFilter<float, 5> hf(3.0f);
    hf.update(-10.0f);
    hf.update(-10.0f);
    hf.update(-10.0f);
    hf.update(-10.0f);
    float v = hf.update(-1000.0f);
    // Outlier should be replaced with median = -10
    FL_CHECK_CLOSE(v, -10.0f, 0.01f);
}

// --- LeakyIntegrator corner cases ---

FL_TEST_CASE("LeakyIntegrator - negative integer values") {
    LeakyIntegrator<int, 1> li; // alpha = 1/2
    li.update(-100);
    // y = 0 + (-100 - 0) >> 1
    // For negative right shift: implementation-defined, but typically arithmetic
    // On most platforms: (-100) >> 1 = -50
    int v = li.value();
    // Should be approximately -50 (exact value depends on platform shift behavior)
    FL_CHECK_LE(v, -49);
    FL_CHECK_GE(v, -51);
}

FL_TEST_CASE("LeakyIntegrator - converges from above") {
    LeakyIntegrator<float, 2> li(10.0f); // start at 10
    for (int i = 0; i < 100; ++i) {
        li.update(0.0f);
    }
    FL_CHECK_CLOSE(li.value(), 0.0f, 0.01f);
}

FL_TEST_CASE("LeakyIntegrator - K=0 means alpha=1 (snap to input)") {
    LeakyIntegrator<float, 0> li; // alpha = 1/1 = 1
    float v = li.update(5.0f);
    FL_CHECK_CLOSE(v, 5.0f, 0.01f);
}

// --- ExponentialSmoother corner cases ---

FL_TEST_CASE("ExponentialSmoother - very large dt snaps to input") {
    ExponentialSmoother<float> ema(0.1f, 0.0f);
    // dt = 1000 seconds >> tau, so exp(-10000) ≈ 0, snaps to input
    float v = ema.update(5.0f, 1000.0f);
    FL_CHECK_CLOSE(v, 5.0f, 0.01f);
}

FL_TEST_CASE("ExponentialSmoother - dt=0 holds value") {
    ExponentialSmoother<float> ema(0.1f, 3.0f);
    // dt=0: exp(0) = 1, so y = input + (y - input)*1 = y (no change)
    float v = ema.update(100.0f, 0.0f);
    FL_CHECK_CLOSE(v, 3.0f, 0.01f);
}

FL_TEST_CASE("ExponentialSmoother - negative input") {
    ExponentialSmoother<float> ema(0.1f, 0.0f);
    for (int i = 0; i < 100; ++i) {
        ema.update(-5.0f, 0.01f);
    }
    FL_CHECK_GT(ema.value(), -5.1f);
    FL_CHECK_LT(ema.value(), -4.9f);
}

// --- AttackDecayFilter corner cases ---

FL_TEST_CASE("AttackDecayFilter - zero tau snaps to input") {
    // tau=0: dt/tau = inf, exp(-inf) = 0, so y = input + (y-input)*0 = input
    AttackDecayFilter<float> adf(0.0f, 0.0f, 0.0f);
    float v = adf.update(5.0f, 0.01f);
    FL_CHECK_EQ(v, v); // not NaN
    FL_CHECK_CLOSE(v, 5.0f, 0.01f);
}

FL_TEST_CASE("AttackDecayFilter - equal signal (no attack or decay)") {
    AttackDecayFilter<float> adf(0.1f, 0.5f, 5.0f);
    // Input equals current value - should stay
    float v = adf.update(5.0f, 0.01f);
    FL_CHECK_CLOSE(v, 5.0f, 0.01f);
}

// --- CascadedEMA corner cases ---

FL_TEST_CASE("CascadedEMA - single stage same as EMA") {
    CascadedEMA<float, 1> cema(0.1f, 0.0f);
    ExponentialSmoother<float> ema(0.1f, 0.0f);
    for (int i = 0; i < 50; ++i) {
        float vc = cema.update(1.0f, 0.01f);
        float ve = ema.update(1.0f, 0.01f);
        FL_CHECK_CLOSE(vc, ve, 0.001f);
    }
}

FL_TEST_CASE("CascadedEMA - zero tau snaps to input") {
    CascadedEMA<float, 2> cema(0.0f, 0.0f);
    float v = cema.update(5.0f, 0.01f);
    FL_CHECK_EQ(v, v); // not NaN
}

// --- DCBlocker corner cases ---

FL_TEST_CASE("DCBlocker - R=0 is pure differentiator") {
    DCBlocker<float> dc(0.0f);
    // y[n] = x[n] - x[n-1] + 0*y[n-1] = x[n] - x[n-1]
    dc.update(5.0f);  // y = 5 - 0 = 5
    float v = dc.update(8.0f); // y = 8 - 5 = 3
    FL_CHECK_CLOSE(v, 3.0f, 0.01f);
    v = dc.update(8.0f); // y = 8 - 8 = 0
    FL_CHECK_CLOSE(v, 0.0f, 0.01f);
}

FL_TEST_CASE("DCBlocker - R=1 accumulates forever") {
    DCBlocker<float> dc(1.0f);
    // y[n] = x[n] - x[n-1] + 1*y[n-1]
    float v = dc.update(5.0f);  // 5 - 0 + 0 = 5
    FL_CHECK_CLOSE(v, 5.0f, 0.01f);
    v = dc.update(5.0f); // 5 - 5 + 5 = 5 (constant in = constant out with R=1)
    FL_CHECK_CLOSE(v, 5.0f, 0.01f);
}

FL_TEST_CASE("DCBlocker - negative values") {
    DCBlocker<float> dc(0.995f);
    for (int i = 0; i < 1000; ++i) {
        dc.update(-3.0f);
    }
    // Constant DC at -3 should converge to 0
    FL_CHECK_GT(dc.value(), -0.1f);
    FL_CHECK_LT(dc.value(), 0.1f);
}

// --- KalmanFilter corner cases ---

FL_TEST_CASE("KalmanFilter - high process noise tracks fast") {
    KalmanFilter<float> kf(1.0f, 0.01f, 0.0f); // high Q, low R
    float v = kf.update(100.0f);
    // Should track almost immediately with high Q and low R
    FL_CHECK_GT(v, 90.0f);
}

FL_TEST_CASE("KalmanFilter - low process noise is sluggish") {
    KalmanFilter<float> kf(0.001f, 10.0f, 0.0f); // low Q, high R
    float v = kf.update(100.0f);
    // Should barely move
    FL_CHECK_LT(v, 10.0f);
}

FL_TEST_CASE("KalmanFilter - negative values") {
    KalmanFilter<float> kf(0.01f, 0.1f, 0.0f);
    for (int i = 0; i < 100; ++i) {
        kf.update(-3.0f);
    }
    FL_CHECK_CLOSE(kf.value(), -3.0f, 0.1f);
}

// --- OneEuroFilter corner cases ---

FL_TEST_CASE("OneEuroFilter - first sample returns input") {
    OneEuroFilter<float> oef(1.0f, 0.5f);
    float v = oef.update(42.0f, 0.016f);
    FL_CHECK_EQ(v, 42.0f);
}

FL_TEST_CASE("OneEuroFilter - negative values") {
    OneEuroFilter<float> oef(1.0f, 0.0f);
    for (int i = 0; i < 100; ++i) {
        oef.update(-5.0f, 0.016f);
    }
    FL_CHECK_CLOSE(oef.value(), -5.0f, 0.1f);
}

FL_TEST_CASE("OneEuroFilter - high beta tracks fast moves") {
    OneEuroFilter<float> low_beta(1.0f, 0.0f);
    OneEuroFilter<float> high_beta(1.0f, 10.0f);
    // Settle both at 0
    for (int i = 0; i < 50; ++i) {
        low_beta.update(0.0f, 0.016f);
        high_beta.update(0.0f, 0.016f);
    }
    // Jump to 100
    for (int i = 0; i < 5; ++i) {
        low_beta.update(100.0f, 0.016f);
        high_beta.update(100.0f, 0.016f);
    }
    // High beta should track faster
    FL_CHECK_GT(high_beta.value(), low_beta.value());
}

// --- BiquadFilter corner cases ---

FL_TEST_CASE("BiquadFilter - cutoff near Nyquist") {
    // cutoff = 499 Hz at 1000 Hz sample rate (near Nyquist = 500 Hz)
    auto lpf = BiquadFilter<float>::butterworth(499.0f, 1000.0f);
    float v = 0.0f;
    for (int i = 0; i < 200; ++i) {
        v = lpf.update(1.0f);
    }
    // Should still converge to DC
    FL_CHECK_EQ(v, v); // not NaN
    FL_CHECK_GT(v, 0.5f);
}

FL_TEST_CASE("BiquadFilter - custom coefficients identity") {
    // b0=1, b1=0, b2=0, a1=0, a2=0 → y = x (pass-through)
    BiquadFilter<float> pass(1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    FL_CHECK_CLOSE(pass.update(7.0f), 7.0f, 0.001f);
    FL_CHECK_CLOSE(pass.update(-3.0f), -3.0f, 0.001f);
}

// --- SavitzkyGolayFilter corner cases ---

FL_TEST_CASE("SavitzkyGolayFilter - before 5 samples uses simple average") {
    SavitzkyGolayFilter<float, 7> sg;
    sg.update(10.0f);
    FL_CHECK_CLOSE(sg.value(), 10.0f, 0.01f);
    sg.update(20.0f);
    FL_CHECK_CLOSE(sg.value(), 15.0f, 0.01f);
    sg.update(30.0f);
    FL_CHECK_CLOSE(sg.value(), 20.0f, 0.01f);
}

FL_TEST_CASE("SavitzkyGolayFilter - quadratic signal") {
    // SG with quadratic fit should perfectly reproduce a quadratic at center
    SavitzkyGolayFilter<float, 5> sg;
    // y = x^2: -2, -1, 0, 1, 2 → 4, 1, 0, 1, 4
    sg.update(4.0f);
    sg.update(1.0f);
    sg.update(0.0f);
    sg.update(1.0f);
    sg.update(4.0f);
    // Center value of the quadratic is 0.0
    FL_CHECK_CLOSE(sg.value(), 0.0f, 0.5f);
}

// --- BilateralFilter corner cases ---

FL_TEST_CASE("BilateralFilter - single sample") {
    BilateralFilter<float, 5> bf(1.0f);
    bf.update(7.0f);
    FL_CHECK_CLOSE(bf.value(), 7.0f, 0.01f);
}

FL_TEST_CASE("BilateralFilter - negative values") {
    BilateralFilter<float, 3> bf(1000.0f); // large sigma = box filter
    bf.update(-1.0f);
    bf.update(-2.0f);
    bf.update(-3.0f);
    FL_CHECK_CLOSE(bf.value(), -2.0f, 0.1f);
}

FL_TEST_CASE("BilateralFilter - all same values with tiny sigma") {
    BilateralFilter<float, 5> bf(0.001f);
    for (int i = 0; i < 5; ++i) bf.update(5.0f);
    // All values identical, so all weights = exp(0) = 1
    FL_CHECK_CLOSE(bf.value(), 5.0f, 0.01f);
}

// --- Cross-filter consistency ---

FL_TEST_CASE("MedianFilter vs AlphaTrimmedMean with max trim") {
    // AlphaTrimmedMean with trim=2 on N=5 should equal median
    MedianFilter<float, 5> mf;
    AlphaTrimmedMean<float, 5> atm(2);
    float data[] = {3.0f, 7.0f, 1.0f, 9.0f, 5.0f};
    for (int i = 0; i < 5; ++i) {
        mf.update(data[i]);
        atm.update(data[i]);
    }
    FL_CHECK_CLOSE(mf.value(), atm.value(), 0.01f);
}

// --- Dynamic buffer corner cases ---

FL_TEST_CASE("WeightedMovingAverage - dynamic buffer") {
    WeightedMovingAverage<float, 0> wma(3);
    wma.update(1.0f);
    wma.update(2.0f);
    wma.update(3.0f);
    float v = wma.value();
    FL_CHECK_GT(v, 2.3f);
    FL_CHECK_LT(v, 2.4f);
}

FL_TEST_CASE("TriangularFilter - dynamic buffer") {
    TriangularFilter<float, 0> tf(5);
    for (int i = 0; i < 5; ++i) tf.update(8.0f);
    FL_CHECK_CLOSE(tf.value(), 8.0f, 0.01f);
}

FL_TEST_CASE("GaussianFilter - dynamic buffer") {
    GaussianFilter<float, 0> gf(5);
    for (int i = 0; i < 5; ++i) gf.update(7.0f);
    FL_CHECK_CLOSE(gf.value(), 7.0f, 0.01f);
}

FL_TEST_CASE("SavitzkyGolayFilter - dynamic buffer") {
    SavitzkyGolayFilter<float, 0> sg(5);
    for (int i = 0; i < 5; ++i) sg.update(3.0f);
    FL_CHECK_CLOSE(sg.value(), 3.0f, 0.01f);
}

FL_TEST_CASE("BilateralFilter - dynamic buffer") {
    BilateralFilter<float, 0> bf(5, 1.0f);
    for (int i = 0; i < 5; ++i) bf.update(4.0f);
    FL_CHECK_CLOSE(bf.value(), 4.0f, 0.01f);
}

FL_TEST_CASE("HampelFilter - dynamic buffer") {
    HampelFilter<float, 0> hf(5, 3.0f);
    hf.update(10.0f);
    hf.update(10.0f);
    hf.update(10.0f);
    hf.update(10.0f);
    float v = hf.update(1000.0f);
    FL_CHECK_CLOSE(v, 10.0f, 0.1f);
}

FL_TEST_CASE("AlphaTrimmedMean - dynamic buffer") {
    AlphaTrimmedMean<float, 0> atm(5, 1);
    atm.update(1.0f);
    atm.update(5.0f);
    atm.update(5.0f);
    atm.update(5.0f);
    atm.update(100.0f);
    FL_CHECK_CLOSE(atm.value(), 5.0f, 0.01f);
}

// ============================================================================
// Bulk update(span) equivalence tests
// ============================================================================

FL_TEST_CASE("WeightedMovingAverage - bulk update equivalent to sequential") {
    float data[] = {3.0f, 7.0f, 1.0f, 9.0f, 5.0f, 2.0f, 8.0f};

    WeightedMovingAverage<float, 5> seq;
    for (int i = 0; i < 7; ++i) seq.update(data[i]);

    WeightedMovingAverage<float, 5> bulk;
    bulk.update(fl::span<const float>(data, 7));

    FL_CHECK_CLOSE(seq.value(), bulk.value(), 0.001f);
}

FL_TEST_CASE("WeightedMovingAverage - bulk fewer than window") {
    float data[] = {3.0f, 7.0f};

    WeightedMovingAverage<float, 5> seq;
    for (int i = 0; i < 2; ++i) seq.update(data[i]);

    WeightedMovingAverage<float, 5> bulk;
    bulk.update(fl::span<const float>(data, 2));

    FL_CHECK_CLOSE(seq.value(), bulk.value(), 0.001f);
}

FL_TEST_CASE("WeightedMovingAverage - bulk single element") {
    float data[] = {42.0f};

    WeightedMovingAverage<float, 5> seq;
    seq.update(data[0]);

    WeightedMovingAverage<float, 5> bulk;
    bulk.update(fl::span<const float>(data, 1));

    FL_CHECK_CLOSE(seq.value(), bulk.value(), 0.001f);
}

FL_TEST_CASE("WeightedMovingAverage - bulk empty span") {
    WeightedMovingAverage<float, 5> f;
    f.update(5.0f);
    float before = f.value();
    f.update(fl::span<const float>(nullptr, 0));
    FL_CHECK_CLOSE(f.value(), before, 0.001f);
}

FL_TEST_CASE("TriangularFilter - bulk update equivalent to sequential") {
    float data[] = {3.0f, 7.0f, 1.0f, 9.0f, 5.0f, 2.0f, 8.0f};

    TriangularFilter<float, 5> seq;
    for (int i = 0; i < 7; ++i) seq.update(data[i]);

    TriangularFilter<float, 5> bulk;
    bulk.update(fl::span<const float>(data, 7));

    FL_CHECK_CLOSE(seq.value(), bulk.value(), 0.001f);
}

FL_TEST_CASE("GaussianFilter - bulk update equivalent to sequential") {
    float data[] = {3.0f, 7.0f, 1.0f, 9.0f, 5.0f, 2.0f, 8.0f};

    GaussianFilter<float, 5> seq;
    for (int i = 0; i < 7; ++i) seq.update(data[i]);

    GaussianFilter<float, 5> bulk;
    bulk.update(fl::span<const float>(data, 7));

    FL_CHECK_CLOSE(seq.value(), bulk.value(), 0.001f);
}

FL_TEST_CASE("SavitzkyGolayFilter - bulk update equivalent to sequential") {
    float data[] = {3.0f, 7.0f, 1.0f, 9.0f, 5.0f, 2.0f, 8.0f};

    SavitzkyGolayFilter<float, 5> seq;
    for (int i = 0; i < 7; ++i) seq.update(data[i]);

    SavitzkyGolayFilter<float, 5> bulk;
    bulk.update(fl::span<const float>(data, 7));

    FL_CHECK_CLOSE(seq.value(), bulk.value(), 0.001f);
}

FL_TEST_CASE("BilateralFilter - bulk update equivalent to sequential") {
    float data[] = {3.0f, 7.0f, 1.0f, 9.0f, 5.0f, 2.0f, 8.0f};

    BilateralFilter<float, 5> seq(1.0f);
    for (int i = 0; i < 7; ++i) seq.update(data[i]);

    BilateralFilter<float, 5> bulk(1.0f);
    bulk.update(fl::span<const float>(data, 7));

    FL_CHECK_CLOSE(seq.value(), bulk.value(), 0.001f);
}

FL_TEST_CASE("MedianFilter - bulk update equivalent to sequential") {
    float data[] = {3.0f, 7.0f, 1.0f, 9.0f, 5.0f, 2.0f, 8.0f};

    MedianFilter<float, 5> seq;
    for (int i = 0; i < 7; ++i) seq.update(data[i]);

    MedianFilter<float, 5> bulk;
    bulk.update(fl::span<const float>(data, 7));

    FL_CHECK_CLOSE(seq.value(), bulk.value(), 0.001f);
}

FL_TEST_CASE("MedianFilter - bulk fewer than window") {
    float data[] = {3.0f, 7.0f};

    MedianFilter<float, 5> seq;
    for (int i = 0; i < 2; ++i) seq.update(data[i]);

    MedianFilter<float, 5> bulk;
    bulk.update(fl::span<const float>(data, 2));

    FL_CHECK_CLOSE(seq.value(), bulk.value(), 0.001f);
}

FL_TEST_CASE("MedianFilter - bulk empty span") {
    MedianFilter<float, 5> f;
    f.update(5.0f);
    float before = f.value();
    f.update(fl::span<const float>(nullptr, 0));
    FL_CHECK_CLOSE(f.value(), before, 0.001f);
}

FL_TEST_CASE("AlphaTrimmedMean - bulk update equivalent to sequential") {
    float data[] = {3.0f, 7.0f, 1.0f, 9.0f, 5.0f, 2.0f, 8.0f};

    AlphaTrimmedMean<float, 5> seq(1);
    for (int i = 0; i < 7; ++i) seq.update(data[i]);

    AlphaTrimmedMean<float, 5> bulk(1);
    bulk.update(fl::span<const float>(data, 7));

    FL_CHECK_CLOSE(seq.value(), bulk.value(), 0.001f);
}

FL_TEST_CASE("HampelFilter - bulk update equivalent to sequential") {
    float data[] = {3.0f, 7.0f, 1.0f, 9.0f, 5.0f, 2.0f, 8.0f};

    HampelFilter<float, 5> seq(3.0f);
    for (int i = 0; i < 7; ++i) seq.update(data[i]);

    HampelFilter<float, 5> bulk(3.0f);
    bulk.update(fl::span<const float>(data, 7));

    FL_CHECK_CLOSE(seq.value(), bulk.value(), 0.001f);
}

FL_TEST_CASE("HampelFilter - bulk empty span") {
    HampelFilter<float, 5> f(3.0f);
    f.update(5.0f);
    float before = f.value();
    f.update(fl::span<const float>(nullptr, 0));
    FL_CHECK_CLOSE(f.value(), before, 0.001f);
}

} // anonymous namespace
