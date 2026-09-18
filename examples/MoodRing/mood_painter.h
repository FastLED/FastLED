// mood_painter.h - mix the three layers, then trails and pulses.
//
// The painter owns the float scratch buffers and the post-process state.
// It is the only thing that writes to the real LED array, and it does so
// exactly once per frame, after every contribution has been summed in
// float. That single quantisation is what keeps the output free of stair
// steps.
#pragma once

#include "FastLED.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"

#include "layers.h"
#include "mood_palette.h"
#include "ring_canvas.h"
#include "room_sense.h"

namespace mood_ring {

struct PainterTuning {
    bool trails = true;
    bool pulses = true;
    float trailOverride = -1.0f; ///< < 0 follows the regime blend
    float pulseOrigin = 0.0f;    ///< turns
    float breathGain = 1.0f;
    float flowGain = 1.0f;
    float grooveGain = 1.0f;
};

class MoodPainter {
  public:
    /// Render one frame into out. Buffers resize on first use and whenever
    /// the length changes.
    void draw(const RoomSense &s, fl::u32 nowMs, fl::span<CRGB> out);

    int activePulseCount() const;

    PainterTuning tuning;

  private:
    void ensureBuffers(fl::size n);
    void mixLayers(const RoomSense &s);
    void applyTrails(float keep);
    void emitPulse(float strength, bool downbeat, fl::u32 nowMs);
    void drawPulses(const MoodPalette &pal, fl::u32 nowMs);

    BreathLayer mBreath;
    FlowLayer mFlow;
    GrooveLayer mGroove;

    fl::vector<RGBf> mBufBreath;
    fl::vector<RGBf> mBufFlow;
    fl::vector<RGBf> mBufGroove;
    fl::vector<RGBf> mMix;   ///< this frame, summed
    fl::vector<RGBf> mTrail; ///< persistence shadow

    float mHueDrift = 0.0f; ///< slow hue travel, turns of the wheel
    fl::u32 mLastMs = 0;

    struct Pulse {
        fl::u32 startMs = 0;
        float strength = 0.0f;
        bool downbeat = false;
        bool active = false;
    };
    static const int kMaxPulses = 8;
    Pulse mPulses[kMaxPulses];
};

} // namespace mood_ring
