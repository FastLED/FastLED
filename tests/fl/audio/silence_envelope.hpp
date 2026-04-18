// Unit tests for fl::audio::SilenceEnvelope

#include "fl/audio/silence_envelope.h"
#include "fl/math/math.h"

using namespace fl;

FL_TEST_CASE("audio::SilenceEnvelope - pass-through during audio") {
    audio::SilenceEnvelope env;
    // Default config: tau=0.5s, target=0. During audio (isSilent=false) the
    // envelope must return currentValue unchanged regardless of tau.
    FL_CHECK_EQ(env.update(false, 0.7f, 0.016f), 0.7f);
    FL_CHECK_EQ(env.update(false, 1.2f, 0.016f), 1.2f);
    FL_CHECK_EQ(env.update(false, -0.3f, 0.016f), -0.3f);
    // Cached value tracks the last audio sample.
    FL_CHECK_EQ(env.value(), -0.3f);
}

FL_TEST_CASE("audio::SilenceEnvelope - exponential decay toward target") {
    audio::SilenceEnvelope::Config cfg;
    cfg.decayTauSeconds = 0.5f;
    cfg.targetValue = 0.0f;
    audio::SilenceEnvelope env(cfg);

    // Seed with a value from "audio" frame, then go silent.
    env.update(false, 1.0f, 0.016f);
    FL_CHECK_EQ(env.value(), 1.0f);

    // After one full tau of silence, envelope should be within ~37% of start.
    // We integrate in small steps (16ms ≈ 60fps) and check the curve.
    float dt = 0.016f;
    // Run for 0.5s (= 1 tau).
    for (int i = 0; i < 31; ++i) { env.update(true, 0.0f, dt); }
    // At 1*tau with target=0 the envelope should be near exp(-1) ≈ 0.368.
    FL_CHECK_GT(env.value(), 0.30f);
    FL_CHECK_LT(env.value(), 0.45f);

    // Run another 1s (total 3*tau). Should be within 5% of target (0).
    for (int i = 0; i < 63; ++i) { env.update(true, 0.0f, dt); }
    FL_CHECK_LT(env.value(), 0.05f);
}

FL_TEST_CASE("audio::SilenceEnvelope - snap-back on silence-to-audio transition") {
    // Tight tau so the envelope truly converges within epsilon in the test time.
    audio::SilenceEnvelope::Config cfg;
    cfg.decayTauSeconds = 0.1f;
    cfg.targetValue = 0.0f;
    audio::SilenceEnvelope env(cfg);

    // Drive to zero via silence.
    env.update(false, 1.0f, 0.016f);
    for (int i = 0; i < 200; ++i) { env.update(true, 0.0f, 0.016f); }
    FL_CHECK(env.isGated());

    // Audio returns — a beat hits with value 2.0. Envelope must NOT apply
    // any attack lag; it must pass through immediately.
    FL_CHECK_EQ(env.update(false, 2.0f, 0.016f), 2.0f);
    FL_CHECK_FALSE(env.isGated());
}

FL_TEST_CASE("audio::SilenceEnvelope - isGated threshold") {
    audio::SilenceEnvelope env;
    env.reset(1.0f);
    FL_CHECK_FALSE(env.isGated());

    env.reset(0.00005f);
    FL_CHECK(env.isGated());   // below default epsilon 1e-4

    env.reset(0.002f);
    FL_CHECK_FALSE(env.isGated(1e-4f));
    FL_CHECK(env.isGated(0.01f)); // larger epsilon accepts
}

FL_TEST_CASE("audio::SilenceEnvelope - non-zero target value") {
    audio::SilenceEnvelope::Config cfg;
    cfg.decayTauSeconds = 0.2f;
    cfg.targetValue = 0.5f;
    audio::SilenceEnvelope env(cfg);

    env.update(false, 1.0f, 0.016f);   // live value = 1.0
    for (int i = 0; i < 125; ++i) { env.update(true, 0.0f, 0.016f); }  // 2s = 10*tau
    // Well past 10*tau → converged to targetValue 0.5.
    FL_CHECK_GT(env.value(), 0.49f);
    FL_CHECK_LT(env.value(), 0.51f);
}

FL_TEST_CASE("audio::SilenceEnvelope - reset with initial value") {
    audio::SilenceEnvelope env;
    env.update(false, 42.0f, 0.016f);
    env.reset(7.0f);
    FL_CHECK_EQ(env.value(), 7.0f);
}

FL_TEST_CASE("audio::SilenceEnvelope - zero tau snaps to target immediately") {
    audio::SilenceEnvelope::Config cfg;
    cfg.decayTauSeconds = 0.0f;
    cfg.targetValue = 0.0f;
    audio::SilenceEnvelope env(cfg);

    env.update(false, 1.0f, 0.016f);
    env.update(true, 0.0f, 0.016f);
    FL_CHECK_EQ(env.value(), 0.0f);
}
