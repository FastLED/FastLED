// room_sense.cpp - derive RoomSense from the audio Processor.
#include "room_sense.h"

#include "fl/math/math.h"
#include "fl/stl/chrono.h"
#include "ring_canvas.h"

namespace mood_ring {

namespace {

// Energy followers. Fast minus slow is the trend: positive while the room is
// getting louder, negative while it is settling.
constexpr float kEnergyFastMs = 120.0f;
constexpr float kEnergySlowMs = 1800.0f;

// Transient followers: instant up, short tail.
constexpr float kTransientAttackMs = 5.0f;
constexpr float kTransientReleaseMs = 160.0f;

// Band followers: quick enough to feel live, slow enough not to strobe.
constexpr float kBandAttackMs = 30.0f;
constexpr float kBandReleaseMs = 250.0f;

// Longest frame the followers are allowed to see. A stall (or a pause in
// listening) must not turn into alpha == 1 and snap every follower to its
// target; it should just play out as one slow frame.
constexpr float kMaxFrameMs = 100.0f;

// Mood moves slowly on purpose. A palette that flips every second reads as
// noise, not feeling.
constexpr float kMoodMs = 4000.0f;

// Soft-knee clip into [0, 1]. Meets the two branches at exactly 0.75 when
// x == 1 so a band pinned at full scale never reads dimmer than one at 0.99.
float softKnee(float x) {
    if (x <= 0.0f)
        return 0.0f;
    if (x >= 1.0f)
        return 1.0f - 0.25f / (1.0f + (x - 1.0f) * 4.0f);
    return x * (1.0f - 0.25f * x);
}

bool edge(fl::u32 stamp, fl::u32 &seen) {
    if (stamp != 0 && stamp != seen) {
        seen = stamp;
        return true;
    }
    return false;
}

} // namespace

float Follower::update(float target, float dtMs, float attackMs,
                       float releaseMs) {
    if (dtMs <= 0.0f)
        return mValue;
    const float tau = (target > mValue) ? attackMs : releaseMs;
    if (tau <= 0.0f) {
        mValue = target;
        return mValue;
    }
    // One-pole with alpha = dt / (tau + dt): no expf, never overshoots.
    float alpha = dtMs / (tau + dtMs);
    if (alpha > 1.0f)
        alpha = 1.0f;
    mValue += (target - mValue) * alpha;
    return mValue;
}

RoomListener::RoomListener(fl::shared_ptr<fl::audio::Processor> processor)
    : mProcessor(processor) {
    mPresence.reset(0.0f);
    mWarmth.reset(0.5f);
}

void RoomListener::begin() {
    if (!mProcessor)
        return;
    // The Processor keeps one callback per event, so the last begin() wins.
    // Callbacks only stamp a time; update() turns stamps into edges.
    RoomListener *self = this;
    mProcessor->onBeat([self]() { self->mLastBeatMs = fl::millis(); });
    mProcessor->onDownbeat([self]() { self->mLastDownbeatMs = fl::millis(); });
    mProcessor->onKick([self]() { self->mLastKickMs = fl::millis(); });
    mProcessor->onSnare([self]() { self->mLastSnareMs = fl::millis(); });
    mProcessor->onHiHat([self]() { self->mLastHiHatMs = fl::millis(); });
}

float RoomListener::stepClock(fl::u32 nowMs) {
    float dtMs = (mLastMs == 0) ? 16.0f : static_cast<float>(nowMs - mLastMs);
    mLastMs = nowMs;
    return clampf(dtMs, 0.0f, kMaxFrameMs);
}

void RoomListener::update(fl::u32 nowMs) {
    const float dtMs = stepClock(nowMs);

    if (!mProcessor) {
        mSense = RoomSense{};
        return;
    }

    deriveEnergy(dtMs);
    deriveBlend(dtMs);
    deriveRhythm(nowMs);
    deriveMood(dtMs);
    deriveEvents();
}

void RoomListener::deriveEnergy(float dtMs) {
    fl::audio::Processor &p = *mProcessor;

    mSense.low = mLow.update(softKnee(p.getEqBass()), dtMs, kBandAttackMs,
                             kBandReleaseMs);
    mSense.mid = mMid.update(softKnee(p.getEqMid()), dtMs, kBandAttackMs,
                             kBandReleaseMs);
    mSense.high = mHigh.update(softKnee(p.getEqTreble()), dtMs, kBandAttackMs,
                               kBandReleaseMs);

    const float vol = softKnee(p.getEqVolume());
    const float fast =
        mEnergyFast.update(vol, dtMs, kEnergyFastMs, kEnergyFastMs);
    const float slow =
        mEnergySlow.update(vol, dtMs, kEnergySlowMs, kEnergySlowMs);
    mSense.energy = fast;
    // A 0.25 swing between fast and slow is a full-scale trend.
    mSense.trend = clampf((fast - slow) * 4.0f, -1.0f, 1.0f);

    // Punch is relative-minus-attenuated: during a sustained loud passage
    // the slow level catches the fast one and punch falls to zero, so only
    // a transient reads as a hit.
    const float bassHit =
        fl::max(0.0f, p.getVibeBass() - p.getVibeBassAtt()) * tuning.punchGain;
    const float trebHit = fl::max(0.0f, p.getVibeTreb() - p.getVibeTrebAtt());
    mSense.punch = clampf(
        mPunch.update(bassHit, dtMs, kTransientAttackMs, kTransientReleaseMs),
        0.0f, 1.0f);
    mSense.shimmer = clampf(
        mShimmer.update(trebHit, dtMs, kTransientAttackMs, kTransientReleaseMs),
        0.0f, 1.0f);
}

void RoomListener::deriveBlend(float dtMs) {
    fl::audio::Processor &p = *mProcessor;
    // Groove confidence: tempo AND beat have to agree.
    const float conf = p.getTempoConfidence() * p.getBeatConfidence();
    blendFrom(p.isSilent() ? 0.0f : 1.0f, conf, dtMs);
}

void RoomListener::blendFrom(float presenceTarget, float grooveConf,
                             float dtMs) {
    // Presence: fast attack so one clap wakes the ring, slow release so a
    // breath between phrases does not drop it into calm.
    mSense.presence =
        mPresence.update(presenceTarget, dtMs, tuning.presenceAttackMs,
                         tuning.presenceReleaseMs);

    // Slow both ways: a beat has to prove itself, and one dropped bar
    // should not lose it.
    mSense.grooveConfidence = mGroove.update(
        grooveConf, dtMs, tuning.grooveAttackMs, tuning.grooveReleaseMs);

    // Blend. smoothstep between exit and enter is the whole "hysteresis".
    const float g = smoothstepf(tuning.grooveExit, tuning.grooveEnter,
                                mSense.grooveConfidence);
    mSense.calm = 1.0f - mSense.presence;
    mSense.groove = mSense.presence * g;
    mSense.flow = mSense.presence * (1.0f - g);
}

void RoomListener::deriveRhythm(fl::u32 nowMs) {
    fl::audio::Processor &p = *mProcessor;

    mSense.bpm = p.getTempoBPM();
    mSense.measurePhase = clampf(p.getMeasurePhase(), 0.0f, 1.0f);

    // Beat phase from our own beat stamps and the reported tempo. Wraps so
    // a missed beat callback still leaves the phase running at tempo.
    if (mSense.bpm > 20.0f && mLastBeatMs != 0) {
        const float beatMs = 60000.0f / mSense.bpm;
        const float since = static_cast<float>(nowMs - mLastBeatMs);
        mSense.beatPhase = wrapTurns(since / beatMs);
    } else {
        mSense.beatPhase = 0.0f;
    }
}

void RoomListener::deriveMood(float dtMs) {
    fl::audio::Processor &p = *mProcessor;
    // Valence is [-1, 1]; warmth is [0, 1] with 0.5 neutral.
    const float warmthTarget =
        clampf(0.5f + 0.5f * p.getMoodValence(), 0.0f, 1.0f);
    const float arousalTarget = clampf(p.getMoodArousal(), 0.0f, 1.0f);
    mSense.warmth = mWarmth.update(warmthTarget, dtMs, kMoodMs, kMoodMs);
    mSense.arousal = mArousal.update(arousalTarget, dtMs, kMoodMs, kMoodMs);
}

void RoomListener::deriveEvents() {
    RoomEvents &e = mSense.events;
    // Edges are consumed regardless of regime so a beat that landed while
    // the ring was calm does not fire the moment groove takes over.
    e.beat = edge(mLastBeatMs, mSeenBeatMs);
    e.downbeat = edge(mLastDownbeatMs, mSeenDownbeatMs);
    e.kick = edge(mLastKickMs, mSeenKickMs);
    e.snare = edge(mLastSnareMs, mSeenSnareMs);
    e.hihat = edge(mLastHiHatMs, mSeenHiHatMs);
}

void RoomListener::rest(fl::u32 nowMs) {
    const float dtMs = stepClock(nowMs);

    // Everything audio-driven eases to zero at its own release rate, so
    // switching listening off is a fade, not a cut. Mood is left where it
    // was: the palette has no reason to move.
    mSense.low = mLow.update(0.0f, dtMs, kBandAttackMs, kBandReleaseMs);
    mSense.mid = mMid.update(0.0f, dtMs, kBandAttackMs, kBandReleaseMs);
    mSense.high = mHigh.update(0.0f, dtMs, kBandAttackMs, kBandReleaseMs);
    const float fast =
        mEnergyFast.update(0.0f, dtMs, kEnergyFastMs, kEnergyFastMs);
    const float slow =
        mEnergySlow.update(0.0f, dtMs, kEnergySlowMs, kEnergySlowMs);
    mSense.energy = fast;
    mSense.trend = clampf((fast - slow) * 4.0f, -1.0f, 1.0f);
    mSense.punch =
        mPunch.update(0.0f, dtMs, kTransientAttackMs, kTransientReleaseMs);
    mSense.shimmer =
        mShimmer.update(0.0f, dtMs, kTransientAttackMs, kTransientReleaseMs);
    mSense.beatPhase = 0.0f;

    blendFrom(0.0f, 0.0f, dtMs);

    // Consume edges that landed while resting so they cannot fire on resume,
    // and publish none.
    deriveEvents();
    mSense.events = RoomEvents{};
}

const char *RoomListener::regimeName() const {
    if (mSense.calm >= mSense.flow && mSense.calm >= mSense.groove)
        return "calm";
    if (mSense.groove >= mSense.flow)
        return "groove";
    return "flow";
}

} // namespace mood_ring
