// layers.h - the three ring-native looks that MoodPainter mixes.
//
// Each layer renders the whole ring into its own float canvas every frame
// and is blind to the others. The painter weights them by RoomSense::calm /
// flow / groove, so a layer only has to look right at full strength.
//
// Nothing in a layer steps. Hits go through a Follower with a short attack
// so even an instant event ramps over a frame or two, and every position is
// advanced by rate * dt rather than snapped.
#pragma once

#include "FastLED.h"
#include "fl/stl/stdint.h"

#include "mood_palette.h"
#include "ring_canvas.h"
#include "room_sense.h"

namespace mood_ring {

/// Calm room. Two slow soft blobs drifting in opposite directions, and a
/// whole-ring breath. Audio does not drive this layer: silence should look
/// intentional, not twitchy.
class BreathLayer {
  public:
    void render(const RoomSense &s, const MoodPalette &pal, float dtSec,
                RingCanvas &c);

  private:
    float mBreath = 0.0f; ///< radians
    float mPosA = 0.0f;   ///< turns
    float mPosB = 0.5f;
};

/// Busy room without a beat. Bass sets the size of two counter-rotating
/// lobes, mids set how fast they travel, treble spawns sparkle grains that
/// fade in and out, and rising energy widens everything while falling
/// energy tightens it.
class FlowLayer {
  public:
    void render(const RoomSense &s, const MoodPalette &pal, float dtSec,
                RingCanvas &c);

  private:
    struct Grain {
        int index = 0;
        float ageSec = 0.0f;
        bool active = false;
    };
    static const int kMaxGrains = 24;

    void spawnGrains(const RoomSense &s, float dtSec, int ringSize);
    void drawGrains(const MoodPalette &pal, float dtSec, RingCanvas &c);

    float mPosA = 0.0f;
    float mPosB = 0.5f;
    float mHueWalk = 0.0f;
    float mSpawnBudget = 0.0f;
    Follower mBody;
    Follower mSigma;
    Grain mGrains[kMaxGrains];
    Xorshift mRng{0xC0FFEE11u};
};

/// Music with a beat. K evenly spaced markers rotate continuously at tempo
/// (one slot per beat) with a slewed phase correction on each beat, the
/// ring swells into each beat and releases after it, and downbeats flash
/// the whole ring.
class GrooveLayer {
  public:
    void render(const RoomSense &s, const MoodPalette &pal, float dtSec,
                RingCanvas &c);

  private:
    static const int kMarkers = 4;

    float mRotation = 0.0f;   ///< turns, advanced continuously
    float mCorrection = 0.0f; ///< remaining phase correction, turns
    Follower mSwell;
    Follower mFlash;
    Follower mSnare;
};

} // namespace mood_ring
