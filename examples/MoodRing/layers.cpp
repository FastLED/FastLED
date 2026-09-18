// layers.cpp - Breath, Flow, Groove.
#include "layers.h"

#include "fl/math/math.h"

namespace mood_ring {

namespace {

constexpr float kTwoPi = 6.2831853f;

// ---- Breath ----
constexpr float kBreathHz = 0.10f; // one breath every ten seconds
constexpr float kBreathDriftTurnsPerSec = 0.012f;
constexpr float kBreathSigma = 0.18f; // wide, soft blobs
constexpr float kBreathFloor = 0.12f; // never fully dark

// ---- Flow ----
constexpr float kFlowBaseTurnsPerSec = 0.05f;
constexpr float kFlowMidTurnsPerSec = 0.35f;
constexpr float kFlowSigmaMin = 0.04f;
constexpr float kFlowSigmaMax = 0.16f;
constexpr float kFlowHueWalkPerSec = 0.06f;
constexpr float kFlowBodyAttackMs = 20.0f;
constexpr float kFlowBodyReleaseMs = 90.0f;
constexpr float kFlowSigmaSlewMs = 120.0f;
constexpr float kGrainsPerSecAtFull = 60.0f; // per 100 LEDs at full shimmer
constexpr float kGrainAttackSec = 0.04f;
constexpr float kGrainLifeSec = 0.30f;

// ---- Groove ----
constexpr float kMarkerSigma = 0.03f;
constexpr float kSwellStart = 0.75f; // beat phase where the swell begins
constexpr float kSwellAttackMs = 25.0f;
constexpr float kSwellReleaseMs = 140.0f;
constexpr float kFlashAttackMs = 15.0f;
constexpr float kFlashReleaseMs = 220.0f;
constexpr float kSnareAttackMs = 10.0f;
constexpr float kSnareReleaseMs = 110.0f;
constexpr float kCorrectionBeats = 0.4f; // finish a phase correction in this
                                         // fraction of a beat

float advance(float pos, float turnsPerSec, float dtSec) {
    return wrapTurns(pos + turnsPerSec * dtSec);
}

/// Rise over attack, hold, fall to zero at life. Smooth at both ends.
float grainEnvelope(float ageSec) {
    if (ageSec < kGrainAttackSec) {
        return smoothstepf(0.0f, kGrainAttackSec, ageSec);
    }
    return 1.0f - smoothstepf(kGrainAttackSec, kGrainLifeSec, ageSec);
}

} // namespace

// ---------------------------------------------------------------------------

void BreathLayer::render(const RoomSense &s, const MoodPalette &pal,
                         float dtSec, RingCanvas &c) {
    (void)s;
    mBreath += kTwoPi * kBreathHz * dtSec;
    while (mBreath > kTwoPi)
        mBreath -= kTwoPi;
    mPosA = advance(mPosA, kBreathDriftTurnsPerSec, dtSec);
    mPosB = advance(mPosB, -kBreathDriftTurnsPerSec * 0.7f, dtSec);

    // Whole-ring breath between floor and a restrained ceiling.
    const float breath =
        kBreathFloor + 0.25f * (0.5f + 0.5f * fl::sin(mBreath));
    c.fill(paletteColor(pal, 0.0f), breath);

    // Two blobs on opposite ends of the family, each breathing out of phase.
    const float aGain = 0.35f + 0.25f * (0.5f + 0.5f * fl::sin(mBreath + 1.2f));
    const float bGain = 0.35f + 0.25f * (0.5f + 0.5f * fl::sin(mBreath + 3.6f));
    c.lobe(mPosA, kBreathSigma, paletteColor(pal, 0.35f), aGain);
    c.lobe(mPosB, kBreathSigma, paletteColor(pal, 1.0f), bGain);
}

// ---------------------------------------------------------------------------

void FlowLayer::spawnGrains(const RoomSense &s, float dtSec, int ringSize) {
    // Spawn rate is per second and scales with ring length, so the density
    // of sparkle is the same on a 60-LED ring and a 244-LED one.
    const float rate = kGrainsPerSecAtFull *
                       (static_cast<float>(ringSize) / 100.0f) * s.shimmer *
                       (0.5f + 0.5f * s.high);
    mSpawnBudget += rate * dtSec;
    while (mSpawnBudget >= 1.0f) {
        mSpawnBudget -= 1.0f;
        for (int g = 0; g < kMaxGrains; ++g) {
            if (mGrains[g].active)
                continue;
            mGrains[g].active = true;
            mGrains[g].ageSec = 0.0f;
            mGrains[g].index =
                static_cast<int>(mRng.unit() * static_cast<float>(ringSize));
            break;
        }
    }
    if (mSpawnBudget > 4.0f)
        mSpawnBudget = 4.0f; // no burst after a stall
}

void FlowLayer::drawGrains(const MoodPalette &pal, float dtSec, RingCanvas &c) {
    const RGBf tint = paletteColor(pal, 1.0f);
    for (int g = 0; g < kMaxGrains; ++g) {
        Grain &grain = mGrains[g];
        if (!grain.active)
            continue;
        grain.ageSec += dtSec;
        if (grain.ageSec >= kGrainLifeSec) {
            grain.active = false;
            continue;
        }
        const float e = grainEnvelope(grain.ageSec);
        // A bright core with dimmer shoulders so a grain is a glint, not a
        // single hot pixel.
        c.pixel(grain.index, rgbfWhite(), 0.45f * e);
        c.pixel(grain.index, tint, 0.35f * e);
        c.pixel(grain.index - 1, tint, 0.20f * e);
        c.pixel(grain.index + 1, tint, 0.20f * e);
    }
}

void FlowLayer::render(const RoomSense &s, const MoodPalette &pal, float dtSec,
                       RingCanvas &c) {
    const float dtMs = dtSec * 1000.0f;

    // Mids set travel speed; a rising room speeds up, a settling one slows.
    const float speed = kFlowBaseTurnsPerSec +
                        kFlowMidTurnsPerSec * s.mid * (1.0f + 0.5f * s.trend);
    mPosA = advance(mPosA, speed, dtSec);
    mPosB = advance(mPosB, -speed * 0.8f, dtSec);
    mHueWalk = wrapTurns(mHueWalk + kFlowHueWalkPerSec * s.mid * dtSec);

    // Bass sets lobe size; trend widens on the way up and tightens on the
    // way down so a build feels like it is opening. Slewed so a bass drop
    // grows the lobe instead of snapping it.
    float sigmaTarget = kFlowSigmaMin + (kFlowSigmaMax - kFlowSigmaMin) * s.low;
    sigmaTarget *= 1.0f + 0.35f * s.trend;
    sigmaTarget =
        clampf(sigmaTarget, kFlowSigmaMin * 0.7f, kFlowSigmaMax * 1.4f);
    const float sigma =
        mSigma.update(sigmaTarget, dtMs, kFlowSigmaSlewMs, kFlowSigmaSlewMs);

    // Punch adds a brief brightness kick so a hit is visible without a beat.
    const float bodyTarget = 0.45f + 0.45f * s.energy + 0.4f * s.punch;
    const float body =
        mBody.update(bodyTarget, dtMs, kFlowBodyAttackMs, kFlowBodyReleaseMs);

    c.lobe(mPosA, sigma, paletteColor(pal, mHueWalk), body);
    c.lobe(mPosB, sigma * 0.8f, paletteColor(pal, wrapTurns(mHueWalk + 0.5f)),
           body * 0.8f);

    // A dim bed so the ring is never black between lobes.
    c.fill(paletteColor(pal, 0.5f), 0.06f + 0.10f * s.energy);

    spawnGrains(s, dtSec, c.size());
    drawGrains(pal, dtSec, c);
}

// ---------------------------------------------------------------------------

void GrooveLayer::render(const RoomSense &s, const MoodPalette &pal,
                         float dtSec, RingCanvas &c) {
    const float dtMs = dtSec * 1000.0f;
    const float slot = 1.0f / static_cast<float>(kMarkers);
    const float beatsPerSec = (s.bpm > 20.0f) ? s.bpm / 60.0f : 0.0f;

    // Rotate continuously at tempo: one slot per beat. On each beat, measure
    // how far the nearest marker is from its slot boundary and slew that
    // error out over the next fraction of a beat, so the geometry locks to
    // the music without ever jumping.
    mRotation = advance(mRotation, slot * beatsPerSec, dtSec);
    if (s.events.beat) {
        const float inSlot =
            mRotation -
            slot * static_cast<float>(static_cast<int>(mRotation / slot));
        // Nearest boundary: either back to the start of this slot or forward
        // to the next one.
        const float err = (inSlot < slot * 0.5f) ? -inSlot : (slot - inSlot);
        mCorrection = err;
    }
    if (mCorrection != 0.0f && beatsPerSec > 0.0f) {
        const float maxStep = slot * beatsPerSec * dtSec / kCorrectionBeats;
        float step = mCorrection;
        if (step > maxStep)
            step = maxStep;
        if (step < -maxStep)
            step = -maxStep;
        mRotation = wrapTurns(mRotation + step);
        mCorrection -= step;
        if (mCorrection < 1e-5f && mCorrection > -1e-5f)
            mCorrection = 0.0f;
    }

    // Swell into the beat: brightness climbs over the last quarter of the
    // beat and releases right after it. Followed so a beat that lands early
    // (phase reset from 0.9 to 0) ramps over a frame instead of stepping.
    float swellTarget;
    if (s.beatPhase >= kSwellStart) {
        swellTarget = smoothstepf(kSwellStart, 1.0f, s.beatPhase);
    } else {
        swellTarget = 1.0f - smoothstepf(0.0f, 0.25f, s.beatPhase);
    }
    const float swell =
        mSwell.update(swellTarget, dtMs, kSwellAttackMs, kSwellReleaseMs);

    // Event envelopes: instant-feeling attack, smooth release.
    const float flash = mFlash.update(s.events.downbeat ? 1.0f : 0.0f, dtMs,
                                      kFlashAttackMs, kFlashReleaseMs);
    const float snare = mSnare.update(s.events.snare ? 1.0f : 0.0f, dtMs,
                                      kSnareAttackMs, kSnareReleaseMs);

    const float markerGain = 0.45f + 0.55f * swell + 0.5f * s.punch;

    // Bed: measure phase breathes the whole ring once per bar.
    const float bar = 0.5f + 0.5f * fl::sin(kTwoPi * s.measurePhase);
    c.fill(paletteColor(pal, 0.2f), 0.08f + 0.12f * bar);

    // Markers alternate ends of the palette so the geometry reads.
    for (int k = 0; k < kMarkers; ++k) {
        const float pos = wrapTurns(mRotation + slot * static_cast<float>(k));
        const float t = (k & 1) ? 1.0f : 0.4f;
        c.lobe(pos, kMarkerSigma + 0.02f * s.low, paletteColor(pal, t),
               markerGain);
    }

    // Snare: a thin counter-marker halfway between each pair.
    if (snare > 0.001f) {
        for (int k = 0; k < kMarkers; ++k) {
            const float pos =
                wrapTurns(mRotation + slot * (static_cast<float>(k) + 0.5f));
            c.lobe(pos, kMarkerSigma * 0.6f, rgbfWhite(), 0.6f * snare);
        }
    }

    // Downbeat: whole-ring flash in the warm end of the family.
    if (flash > 0.001f) {
        c.fill(paletteColor(pal, 1.0f), 0.5f * flash);
    }
}

} // namespace mood_ring
