// room_sense.h - what the room sounds like, as a handful of smooth signals.
//
// RoomListener polls the audio Processor once per frame and fills RoomSense.
// Everything downstream reads RoomSense and nothing else, so the audio
// pipeline can change without touching a single visual.
//
// There is no state enum. The three regimes (calm room, busy room, music
// with a beat) are three weights that always sum to one. They move with
// asymmetric attack and release, which gives the same "slow to enter
// silence, fast to leave it" feel the old classifier needed timers for.
#pragma once

#include "FastLED.h"
#include "fl/audio/audio_processor.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/stdint.h"

namespace mood_ring {

/// One-frame rising edges. Set on the frame the event lands, cleared next.
struct RoomEvents {
    bool beat = false;
    bool downbeat = false;
    bool kick = false;
    bool snare = false;
    bool hihat = false;
};

/// Everything a visual is allowed to react to. All floats are smoothed and
/// valid every frame. Ranges are [0, 1] unless noted.
struct RoomSense {
    // Regime blend. calm + flow + groove == 1.
    float presence = 0.0f; ///< sound is present (silence -> 0)
    float calm = 1.0f;
    float flow = 0.0f;
    float groove = 0.0f;

    // Energy.
    float energy = 0.0f;  ///< smoothed loudness
    float trend = 0.0f;   ///< [-1, 1] falling .. rising
    float punch = 0.0f;   ///< bass transient (a hit, not loudness)
    float shimmer = 0.0f; ///< treble transient
    float low = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;

    // Rhythm.
    float bpm = 0.0f;
    float beatPhase = 0.0f;    ///< 0 on the beat, ramps to 1 before the next
    float measurePhase = 0.0f; ///< 0 on the downbeat
    float grooveConfidence = 0.0f;

    // Mood.
    float warmth = 0.5f;  ///< 0 cool .. 1 warm, from valence
    float arousal = 0.0f; ///< 0 calm .. 1 energetic

    RoomEvents events;
};

/// Everything the UI is allowed to tune on the listening side.
struct ListenerTuning {
    float presenceAttackMs = 150.0f;  ///< how fast sound wakes the ring
    float presenceReleaseMs = 900.0f; ///< how slowly silence settles in
    float grooveAttackMs = 800.0f;    ///< how slowly we start trusting a beat
    float grooveReleaseMs = 1500.0f;  ///< how slowly we stop trusting it
    float grooveEnter = 0.60f;        ///< confidence where groove is fully on
    float grooveExit = 0.35f;         ///< confidence where groove is fully off
    float punchGain = 1.5f;           ///< scales the bass transient
};

/// Smoothed one-pole follower with independent attack and release.
class Follower {
  public:
    float update(float target, float dtMs, float attackMs, float releaseMs);
    float value() const { return mValue; }
    void reset(float v) { mValue = v; }

  private:
    float mValue = 0.0f;
};

class RoomListener {
  public:
    explicit RoomListener(fl::shared_ptr<fl::audio::Processor> processor);

    /// Install the event callbacks. Call once after the Processor exists.
    void begin();

    /// Poll the Processor and refresh sense(). Call once per frame.
    void update(fl::u32 nowMs);

    /// Not listening this frame. Eases every signal toward calm at its
    /// normal release rate, consumes any event edges so they cannot fire on
    /// resume, and keeps the clock running so resuming does not snap.
    void rest(fl::u32 nowMs);

    const RoomSense &sense() const { return mSense; }

    /// Which regime currently dominates, for logs only.
    const char *regimeName() const;

    ListenerTuning tuning;

  private:
    /// Advance the frame clock; returns a bounded dt in ms.
    float stepClock(fl::u32 nowMs);
    void deriveBlend(float dtMs);
    void blendFrom(float presenceTarget, float grooveConf, float dtMs);
    void deriveEnergy(float dtMs);
    void deriveRhythm(fl::u32 nowMs);
    void deriveMood(float dtMs);
    void deriveEvents();

    fl::shared_ptr<fl::audio::Processor> mProcessor;
    RoomSense mSense;
    fl::u32 mLastMs = 0;

    Follower mPresence;
    Follower mGroove;
    Follower mEnergyFast;
    Follower mEnergySlow;
    Follower mPunch;
    Follower mShimmer;
    Follower mLow;
    Follower mMid;
    Follower mHigh;
    Follower mWarmth;
    Follower mArousal;

    // Event timestamps written by Processor callbacks, consumed by update().
    fl::u32 mLastBeatMs = 0;
    fl::u32 mLastDownbeatMs = 0;
    fl::u32 mLastKickMs = 0;
    fl::u32 mLastSnareMs = 0;
    fl::u32 mLastHiHatMs = 0;
    fl::u32 mSeenBeatMs = 0;
    fl::u32 mSeenDownbeatMs = 0;
    fl::u32 mSeenKickMs = 0;
    fl::u32 mSeenSnareMs = 0;
    fl::u32 mSeenHiHatMs = 0;
};

} // namespace mood_ring
