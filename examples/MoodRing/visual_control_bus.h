// visual_control_bus.h - the stable authoring surface between audio and visuals.
//
// The classifier decides WHAT KIND of sound the room contains; the bus decides
// WHAT THE VISUALS SHOULD DO about it. Consumers (the Animartrix engine, the
// ring overlay, and later the NoiseRing engine) read only the bus and never the
// raw detectors, so the audio pipeline can change without churning every visual.
// This is the MilkDrop q1..q32 analogue.
//
// See issue #3885 for the derivation rationale and #2256 for the wider design.
#pragma once

#include "FastLED.h"
#include "fl/audio/audio_processor.h"
#include "fl/stl/stdint.h"

namespace mood_ring {

/// What kind of sound the room contains. Lives here rather than in
/// sound_orchestrator.h because it is shared vocabulary: the bus derivation,
/// the overlay policy, and the classifier all speak it.
enum class SoundState : fl::u8 {
    Silence = 0,
    Disorganized = 1,
    BpmLocked = 2,
};

const char *toString(SoundState s);

/// Rising-edge audio events for the frame being derived. The orchestrator owns
/// the timestamps (it installs the Processor callbacks); the deriver turns them
/// into one-frame pulses by noticing when a timestamp changes.
struct AudioEvents {
    fl::u32 lastKickMs = 0;
    fl::u32 lastSnareMs = 0;
    fl::u32 lastDownbeatMs = 0;
    bool hiHat = false; ///< polled, not timestamped -- Processor::isHiHat()
    fl::u8 beatNumber = 0;
};

/// Everything a visual is allowed to react to. Continuous fields are smoothed
/// and valid every frame; the event lane is populated on the frame an event
/// fires and cleared on the next derive.
struct VisualControlBus {
    // --- continuous ---
    /// Engine speed handed to FxEngine::setSpeed. The audio-derived component
    /// is [0.1 .. 4.0]; the sketch's Time Speed scalar is then composed on top
    /// and MAY BE NEGATIVE (the slider spans -10..10 and reverse is a feature),
    /// so the published value is the product, bounded by clampTransportSpeed().
    float transportSpeed = 1.0f;
    float radialPressure = 0.0f;  ///< [0 .. 1] inward/outward bias
    float rotationBias = 0.0f;    ///< [-1 .. 1] clockwise vs counter
    float paletteDrift = 0.0f;    ///< [0 .. 1] rate of hue travel
    float sparkleDensity = 0.0f;  ///< [0 .. 1] treble-driven shimmer
    float decayAmount = 0.6f;     ///< [0 .. 1] post-process persistence
    float lowBand = 0.0f;         ///< [0 .. 1] smoothed band energy
    float midBand = 0.0f;
    float highBand = 0.0f;

    // --- event lane ---
    float pulseStrength = 0.0f;   ///< 0 = no pulse this frame, else 0.2 .. 1.0
    bool pulseIsDownbeat = false;
    bool snareAccent = false;
    bool hihatTick = false;
};

/// Tunables exposed to the UI.
struct BusConfig {
    float baseSpeed = 1.0f;      ///< engine speed before any audio contribution
    float bassPunchGain = 1.5f;  ///< how hard a transient pushes transportSpeed
};

/// Smoothed one-pole follower with independent attack and release. Rising
/// signals track fast (a hit should look instant); falling signals decay slowly
/// (so the visual has body instead of flickering off between samples).
class EnvFollower {
  public:
    float update(float target, float dtMs, float attackMs, float releaseMs);
    float value() const { return mValue; }

  private:
    float mValue = 0.0f;
};

/// Holds the cross-frame state the derivation needs (envelope followers, the
/// ambient LFO phase, and the event timestamps already consumed).
class BusDeriver {
  public:
    /// Fill *out for this frame. manualSpeedScalar is the sketch's "Time Speed"
    /// slider, composed on top of whatever the state policy chose.
    void derive(fl::audio::Processor &p, SoundState state, const BusConfig &cfg,
                const AudioEvents &events, fl::u32 nowMs,
                float manualSpeedScalar, VisualControlBus *out);

  private:
    EnvFollower mRadial;
    EnvFollower mSparkle;
    fl::u32 mLastMs = 0;
    float mSilenceLfoPhase = 0.0f;

    // Event edge detection: an event fires on the frame its timestamp changes.
    fl::u32 mSeenKickMs = 0;
    fl::u32 mSeenSnareMs = 0;
    fl::u32 mSeenDownbeatMs = 0;
};

/// Sign-preserving bound on the value handed to FxEngine. Shared so the
/// no-processor fallback cannot publish something the derived path never would.
float clampTransportSpeed(float v);

/// Soft-knee clip into [0,1]. Keeps the top of the range from pinning to 1.0
/// the moment a band gets loud, which is what makes band-driven visuals look
/// like an on/off switch instead of a level.
float softKnee(float x);

} // namespace mood_ring
