// mood_painter.cpp - layer mix, trails, pulses.
#include "mood_painter.h"

#include "fl/math/math.h"

namespace mood_ring {

namespace {

constexpr fl::u32 kPulseLifetimeMs = 700;
constexpr float kPulseSigmaBeat = 0.035f;
constexpr float kPulseSigmaDownbeat = 0.060f;
constexpr float kPulseFadeIn = 0.08f; // fraction of life to reach full

// Trails: long when calm, medium when flowing, crisp when grooving.
constexpr float kKeepCalm = 0.85f;
constexpr float kKeepFlow = 0.60f;
constexpr float kKeepGroove = 0.35f;

// Below this a trail channel is treated as dark, so a decaying tail ends
// cleanly instead of lingering as denormals.
constexpr float kTrailFloor = 1.0f / 1024.0f;

// Hue drift: slow travel of the family so a long quiet stretch still moves.
constexpr float kHueDriftTurnsPerSecCalm = 0.004f;

} // namespace

void MoodPainter::ensureBuffers(fl::size n) {
    if (mBufBreath.size() != n)
        mBufBreath.assign(n, RGBf{});
    if (mBufFlow.size() != n)
        mBufFlow.assign(n, RGBf{});
    if (mBufGroove.size() != n)
        mBufGroove.assign(n, RGBf{});
    if (mMix.size() != n)
        mMix.assign(n, RGBf{});
    if (mTrail.size() != n)
        mTrail.assign(n, RGBf{});
}

void MoodPainter::draw(const RoomSense &s, fl::u32 nowMs, fl::span<CRGB> out) {
    if (out.empty())
        return;
    ensureBuffers(out.size());

    const float dtSec =
        (mLastMs == 0) ? 0.016f : static_cast<float>(nowMs - mLastMs) * 0.001f;
    mLastMs = nowMs;

    // Hue drift only while calm; a busy room gets its motion from the layers.
    mHueDrift =
        wrapTurns(mHueDrift + kHueDriftTurnsPerSecCalm * s.calm * dtSec);
    const MoodPalette pal = paletteFor(s, mHueDrift);

    // Every layer renders every frame, even at zero weight, so its
    // envelopes, grains and phase correction keep running. Skipping a layer
    // would freeze a half-finished flash and replay it when the layer
    // returned. The pixel loops are cheap at ring scale.
    RingCanvas breath(mBufBreath);
    RingCanvas flow(mBufFlow);
    RingCanvas groove(mBufGroove);
    breath.clear();
    flow.clear();
    groove.clear();
    mBreath.render(s, pal, dtSec, breath);
    mFlow.render(s, pal, dtSec, flow);
    mGroove.render(s, pal, dtSec, groove);

    mixLayers(s);

    if (tuning.trails) {
        float keep = tuning.trailOverride;
        if (keep < 0.0f) {
            keep = kKeepCalm * s.calm + kKeepFlow * s.flow +
                   kKeepGroove * s.groove;
        }
        applyTrails(keep);
    } else {
        // Drop history so re-enabling does not smear a stale frame.
        for (fl::size i = 0; i < mTrail.size(); ++i)
            mTrail[i] = RGBf{};
    }

    if (tuning.pulses) {
        // Pulses belong to groove. Weight them by it so a beat that fires
        // while groove is fading in arrives at the right strength.
        if (s.events.downbeat) {
            emitPulse(1.0f * s.groove, true, nowMs);
        } else if (s.events.kick || s.events.beat) {
            emitPulse(0.7f * s.groove, false, nowMs);
        }
        drawPulses(pal, nowMs);
    }

    // The one and only quantisation.
    for (fl::size i = 0; i < out.size(); ++i)
        out[i] = toCRGB(mMix[i]);
}

void MoodPainter::mixLayers(const RoomSense &s) {
    const float wb = clampf(s.calm * tuning.breathGain, 0.0f, 1.0f);
    const float wf = clampf(s.flow * tuning.flowGain, 0.0f, 1.0f);
    const float wg = clampf(s.groove * tuning.grooveGain, 0.0f, 1.0f);
    const fl::size n = mMix.size();
    for (fl::size i = 0; i < n; ++i) {
        const RGBf &b = mBufBreath[i];
        const RGBf &f = mBufFlow[i];
        const RGBf &g = mBufGroove[i];
        RGBf &m = mMix[i];
        m.r = b.r * wb + f.r * wf + g.r * wg;
        m.g = b.g * wb + f.g * wf + g.g * wg;
        m.b = b.b * wb + f.b * wf + g.b * wg;
    }
}

void MoodPainter::applyTrails(float keep) {
    // Persistence by per-channel max, not accumulation: unity gain by
    // construction, so a static frame yields exactly itself and a keep of
    // zero is a pure passthrough. In float, so a dim tail fades smoothly
    // to nothing instead of stepping 3 -> 2 -> 1 -> 0.
    keep = clampf(keep, 0.0f, 0.98f);
    const fl::size n = mMix.size();
    for (fl::size i = 0; i < n; ++i) {
        RGBf &t = mTrail[i];
        RGBf &live = mMix[i];
        t.r *= keep;
        t.g *= keep;
        t.b *= keep;
        if (t.r < kTrailFloor)
            t.r = 0.0f;
        if (t.g < kTrailFloor)
            t.g = 0.0f;
        if (t.b < kTrailFloor)
            t.b = 0.0f;
        if (live.r > t.r)
            t.r = live.r;
        if (live.g > t.g)
            t.g = live.g;
        if (live.b > t.b)
            t.b = live.b;
        live = t;
    }
}

void MoodPainter::emitPulse(float strength, bool downbeat, fl::u32 nowMs) {
    if (strength <= 0.02f)
        return;
    int slot = -1;
    for (int i = 0; i < kMaxPulses; ++i) {
        if (!mPulses[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        // All busy: recycle the oldest. The newest hits are the ones the
        // eye is tracking.
        slot = 0;
        for (int i = 1; i < kMaxPulses; ++i) {
            if (mPulses[i].startMs < mPulses[slot].startMs)
                slot = i;
        }
    }
    mPulses[slot].startMs = nowMs;
    mPulses[slot].strength = strength;
    mPulses[slot].downbeat = downbeat;
    mPulses[slot].active = true;
}

void MoodPainter::drawPulses(const MoodPalette &pal, fl::u32 nowMs) {
    RingCanvas c(mMix);
    const float origin = wrapTurns(tuning.pulseOrigin);
    for (int i = 0; i < kMaxPulses; ++i) {
        Pulse &p = mPulses[i];
        if (!p.active)
            continue;
        const fl::u32 elapsed = nowMs - p.startMs;
        if (elapsed >= kPulseLifetimeMs) {
            p.active = false;
            continue;
        }
        const float life =
            static_cast<float>(elapsed) / static_cast<float>(kPulseLifetimeMs);
        const float radius = life * 0.5f; // origin to the far side
        const float sigma = p.downbeat ? kPulseSigmaDownbeat : kPulseSigmaBeat;
        // Fade in over the first few percent so the ring grows out of the
        // origin rather than appearing there, then dissolve as it travels.
        const float amp =
            p.strength * smoothstepf(0.0f, kPulseFadeIn, life) * (1.0f - life);
        // Downbeats take the warm end of the family; beats stay white so the
        // bar boundary is distinguishable by colour as well as by width.
        const RGBf tint = p.downbeat ? paletteColor(pal, 1.0f) : rgbfWhite();
        c.ring(origin, radius, sigma, tint, amp);
    }
}

int MoodPainter::activePulseCount() const {
    int count = 0;
    for (int i = 0; i < kMaxPulses; ++i) {
        if (mPulses[i].active)
            ++count;
    }
    return count;
}

} // namespace mood_ring
