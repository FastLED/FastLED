// visual_control_bus.cpp - derive the visual control bus from the audio stream.
#include "visual_control_bus.h"

#include "fl/math/math.h"

namespace mood_ring {

const char *toString(SoundState s) {
    switch (s) {
    case SoundState::Silence:      return "Silence";
    case SoundState::Disorganized: return "Disorganized";
    case SoundState::BpmLocked:    return "BpmLocked";
    }
    return "?";
}

namespace {

constexpr float kTwoPi = 6.2831853f;

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Ambient breathing rate for the Silence state, in cycles per second.
constexpr float kSilenceBreathHz = 0.12f;

// Sign-preserving bound on the value handed to FxEngine. 4.0 (the audio-derived
// ceiling) times 10.0 (the Time Speed slider's extreme) -- generous enough that
// legitimate settings pass through untouched, tight enough that a bad scalar
// cannot publish something wild.
constexpr float kMaxAbsTransportSpeed = 40.0f;

} // namespace

float clampTransportSpeed(float v) {
    if (v > kMaxAbsTransportSpeed) return kMaxAbsTransportSpeed;
    if (v < -kMaxAbsTransportSpeed) return -kMaxAbsTransportSpeed;
    return v;
}

float softKnee(float x) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) {
        // Above unity, compress hard rather than clip: keeps a very loud band
        // distinguishable from a merely loud one. Meets the lower branch at
        // exactly 0.75 when x == 1.
        return 1.0f - 0.25f / (1.0f + (x - 1.0f) * 4.0f);
    }
    // Below unity: near-linear at the bottom, easing toward the knee. Must
    // evaluate to 0.75 at x == 1 so the two branches join -- an earlier
    // version used (1.25 - 0.25x), which hit 1.0 here and made a band pinned
    // at full scale read DIMMER than one at 0.999.
    return x * (1.0f - 0.25f * x);
}

float EnvFollower::update(float target, float dtMs, float attackMs,
                          float releaseMs) {
    if (dtMs <= 0.0f) return mValue;
    const float tau = (target > mValue) ? attackMs : releaseMs;
    if (tau <= 0.0f) {
        mValue = target;
        return mValue;
    }
    // One-pole: alpha = 1 - exp(-dt/tau), approximated to avoid expf().
    float alpha = dtMs / (tau + dtMs);
    if (alpha > 1.0f) alpha = 1.0f;
    mValue += (target - mValue) * alpha;
    return mValue;
}

void BusDeriver::derive(fl::audio::Processor &p, SoundState state,
                        const BusConfig &cfg, const AudioEvents &events,
                        fl::u32 nowMs, float manualSpeedScalar,
                        VisualControlBus *out) {
    if (!out) return;

    const float dtMs =
        (mLastMs == 0) ? 16.0f : static_cast<float>(nowMs - mLastMs);
    mLastMs = nowMs;

    // --- punch: relative minus attenuated ---
    // This is the whole reason a hit reads differently from loudness. During a
    // sustained loud passage the attenuated (slow) level catches up to the
    // instantaneous level, so punch falls to ~0 and nothing pulses. Only a
    // transient -- fast level above slow level -- produces punch.
    const float bassPunch = fl::max(0.0f, p.getVibeBass() - p.getVibeBassAtt());
    const float trebPunch = fl::max(0.0f, p.getVibeTreb() - p.getVibeTrebAtt());

    // --- continuous bands ---
    out->lowBand = softKnee(p.getEqBass());
    out->midBand = softKnee(p.getEqMid());
    out->highBand = softKnee(p.getEqTreble());

    // --- transport speed, per state policy ---
    float speed;
    switch (state) {
    case SoundState::Silence:
        // Ambient: fixed and slow. Audio deliberately does not drive speed
        // here -- silence should look intentional, not idle-twitchy.
        speed = 0.25f;
        break;
    case SoundState::Disorganized:
        speed = cfg.baseSpeed + 0.8f * cfg.bassPunchGain * bassPunch;
        break;
    case SoundState::BpmLocked: {
        // Baseline tracks tempo so a fast song reads fast; the transient
        // contribution is halved because pulses now carry the accent visually
        // instead of by yanking the clock.
        const float bpm = p.getTempoBPM();
        float bpmScale = 1.0f;
        if (bpm > 1.0f) {
            bpmScale = clampf(bpm / 120.0f, 0.6f, 1.6f);
        }
        speed = cfg.baseSpeed * bpmScale + 0.4f * cfg.bassPunchGain * bassPunch;
        break;
    }
    default:
        speed = cfg.baseSpeed;
        break;
    }
    // The audio-derived component is clamped to its documented band, then
    // composed with the sketch's Time Speed scalar. The scalar is deliberately
    // NOT folded into that clamp: the slider spans -10..10 and negative values
    // run the animation backwards, so clamping the product to [0.1, 4.0] would
    // silently delete reverse and slow-motion. The product still gets a
    // sign-preserving magnitude bound so nothing unbounded reaches FxEngine.
    out->transportSpeed =
        clampTransportSpeed(clampf(speed, 0.1f, 4.0f) * manualSpeedScalar);

    // --- radial pressure ---
    if (state == SoundState::Silence) {
        // Slow breathing LFO rather than an audio follower.
        mSilenceLfoPhase += kTwoPi * kSilenceBreathHz * (dtMs / 1000.0f);
        while (mSilenceLfoPhase > kTwoPi) mSilenceLfoPhase -= kTwoPi;
        out->radialPressure = 0.5f + 0.5f * fl::sin(mSilenceLfoPhase);
        mRadial.update(out->radialPressure, dtMs, 200.0f, 200.0f);
    } else {
        const float target = 0.7f * out->lowBand + 0.3f * bassPunch;
        out->radialPressure = mRadial.update(target, dtMs, 15.0f, 220.0f);
    }

    // --- rotation bias ---
    out->rotationBias = (out->midBand - 0.5f) * 2.0f;
    if (state == SoundState::BpmLocked) {
        out->rotationBias += 0.3f * fl::sin(kTwoPi * p.getMeasurePhase());
    }
    out->rotationBias = clampf(out->rotationBias, -1.0f, 1.0f);

    // --- palette drift ---
    if (state == SoundState::Silence) {
        out->paletteDrift = 0.05f;
    } else {
        const float tempoTerm = clampf(p.getTempoBPM() / 180.0f, 0.0f, 1.0f);
        out->paletteDrift = clampf(0.5f * out->midBand + 0.35f * out->highBand +
                                       0.15f * tempoTerm,
                                   0.0f, 1.0f);
    }

    // --- sparkle ---
    if (state == SoundState::Silence) {
        out->sparkleDensity = mSparkle.update(0.0f, dtMs, 5.0f, 180.0f);
    } else {
        const float target =
            0.6f * trebPunch + 0.4f * (events.hiHat ? 1.0f : 0.0f);
        out->sparkleDensity =
            clampf(mSparkle.update(target, dtMs, 5.0f, 180.0f), 0.0f, 1.0f);
    }

    // --- decay: long trails when ambient, crisp when locked ---
    out->decayAmount = (state == SoundState::Silence)   ? 0.85f
                       : (state == SoundState::BpmLocked) ? 0.35f
                                                          : 0.60f;

    // --- event lane ---
    // Cleared every frame; only BpmLocked populates it. Disorganized expresses
    // itself through the continuous fields, which is what keeps non-rhythmic
    // audio from faking a pulse grid.
    out->pulseStrength = 0.0f;
    out->pulseIsDownbeat = false;
    out->snareAccent = false;
    out->hihatTick = false;

    const bool newKick = events.lastKickMs != 0 && events.lastKickMs != mSeenKickMs;
    const bool newSnare =
        events.lastSnareMs != 0 && events.lastSnareMs != mSeenSnareMs;
    const bool newDownbeat =
        events.lastDownbeatMs != 0 && events.lastDownbeatMs != mSeenDownbeatMs;

    if (state == SoundState::BpmLocked) {
        if (newDownbeat) {
            out->pulseStrength = 1.0f;
            out->pulseIsDownbeat = true;
        } else if (newKick) {
            out->pulseStrength = 0.75f;
        }
        out->snareAccent = newSnare;
        out->hihatTick = events.hiHat;
    }

    // Consume the edges regardless of state, so re-entering BpmLocked does not
    // immediately fire a stale event that happened while we were elsewhere.
    if (newKick) mSeenKickMs = events.lastKickMs;
    if (newSnare) mSeenSnareMs = events.lastSnareMs;
    if (newDownbeat) mSeenDownbeatMs = events.lastDownbeatMs;
}

} // namespace mood_ring
