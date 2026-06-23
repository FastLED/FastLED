#pragma once

/// @file    perlin_particle_punch.h
/// @brief   Audio-reactive perlin noise background with ambient particles
///          and beat meteor overlay. Sketch controls spawning via audio
///          detector callbacks.

#include "fl/fx/fx1d.h"
#include "fl/gfx/colorutils.h"
#include "fl/math/fixed_point/s16x16.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

namespace fl {

FASTLED_SHARED_PTR(PerlinParticlePunch);

class PerlinParticlePunch : public Fx1d {
  public:
    PerlinParticlePunch(u16 num_leds);
    ~PerlinParticlePunch() FL_NOEXCEPT;

    void draw(DrawContext context) override;
    fl::string fxName() const override;

    // --- Particle spawning (called from sketch via detector callbacks) ---

    /// Spawn an ambient particle at a random position.
    /// @param intensity 0.0-1.0, controls brightness and trail length.
    void spawnAmbient(float intensity = 0.5f);

    /// Spawn a BEAT meteor at position 0, traveling toward end of strip.
    /// @param intensity 0.0-1.0, controls speed, head brightness, debris.
    void spawnMeteor(float intensity = 1.0f);

    // --- Perlin noise layer control ---

    /// Set time multiplier for noise evolution (1.0 = normal, >1 = warp).
    void setTimeMultiplier(float mult);

    /// Set palette for Perlin noise background.
    void setNoisePalette(const CRGBPalette16 &palette);

    // --- Ambient control ---

    /// Set palette for ambient particles.
    void setAmbientPalette(const CRGBPalette16 &palette);

    // --- Meteor control ---

    /// Set the meteor color gradient: head → mid → tail.
    void setMeteorGradient(CRGB headColor, CRGB midColor, CRGB tailColor);

    // --- Physics tuning ---

    /// Set per-frame drag for ambient particles (0.0 = instant stop, 1.0 = no
    /// drag). Meteor drag is derived from this at a heavier ratio.
    void setDrag(float drag);

    /// Set velocity multiplier. Default 1.0.
    void setSpeed(float speed);

    // --- Trail control ---

    /// Ambient trail intensity: 0 = no trail, 255 = long persistent trail.
    void setAmbientTrailIntensity(u8 intensity);

    /// Meteor trail intensity: controls how long meteor/debris trails linger.
    void setMeteorTrailIntensity(u8 intensity);

    // --- Particle lifetime tuning ---

    /// Per-frame brightness decay for ambient particles. 1.0 = no decay,
    /// 0.90 = fast decay. Default 0.97.
    void setAmbientBrightnessDecay(float decay);

    /// Minimum velocity before a particle dies. Lower = longer life.
    /// Default 0.05.
    void setMinVelocity(float minVel);

    /// Per-frame brightness decay for debris particles. Default 0.90.
    void setDebrisBrightnessDecay(float decay);

    /// Per-frame velocity decay for debris particles. Default 0.95.
    void setDebrisVelocityDecay(float decay);

  private:
    struct AmbientParticle;
    struct MeteorParticle;
    struct DebrisParticle;

    // Perlin noise
    float mTimeMultiplier = 1.0f;
    CRGBPalette16 mNoisePalette;

    // Ambient
    CRGBPalette16 mAmbientPalette;
    float mDrag = 0.95f;
    float mSpeed = 1.0f;
    u8 mAmbientTrailIntensity = 200;

    // Meteor
    CRGB mMeteorHeadColor = CRGB(255, 255, 255);
    CRGB mMeteorMidColor = CRGB(140, 180, 255);
    CRGB mMeteorTailColor = CRGB(0, 40, 120);
    float mMeteorDrag = 0.92f;
    u8 mMeteorTrailIntensity = 220;

    // Particle lifetime
    float mAmbientBrightnessDecay = 0.97f;
    float mMinVelocity = 0.05f;
    float mDebrisBrightnessDecay = 0.90f;
    float mDebrisVelocityDecay = 0.95f;

    // Particle pools
    fl::vector<AmbientParticle> mAmbientParticles;
    fl::vector<MeteorParticle> mMeteorParticles;
    fl::vector<DebrisParticle> mDebrisParticles;

    // Trail buffer (persistent between frames)
    fl::vector<CRGB> mTrailBuffer;

    // Noise helpers (kept from original, using s16x16)
    static s16x16 mapf(s16x16 x, s16x16 in_min, s16x16 in_max, s16x16 out_min,
                        s16x16 out_max);
    s16x16 circleNoiseGen(u32 now, s16x16 theta) const;
    void noiseCircleDraw(u32 now, fl::span<CRGB> dst);

    // Particle helpers
    AmbientParticle *tryAllocateAmbient();
    MeteorParticle *tryAllocateMeteor();
    DebrisParticle *tryAllocateDebris();
    void renderAmbient(const AmbientParticle &p);
    void renderMeteor(const MeteorParticle &m);
    void renderDebris(const DebrisParticle &d);
    void spawnDebrisFromMeteor(MeteorParticle &m, u32 now);

    static void writeMax(CRGB &dst, const CRGB &src);
};

} // namespace fl
