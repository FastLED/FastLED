#include "fl/fx/1d/perlin_particle_punch.h"
#include "fl/math/math.h"
#include "noise.h"
#include "fl/stl/noexcept.h"

namespace fl {

namespace {
// Clamp a float to [0, 255] before casting to u8, avoiding UB from
// out-of-range float-to-integer conversion.
inline u8 clamp_u8(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return u8(v);
}
} // namespace

// ---------------------------------------------------------------------------
// Particle structs
// ---------------------------------------------------------------------------

struct PerlinParticlePunch::AmbientParticle {
    bool alive = false;
    float position = 0.0f;
    float velocity = 0.0f;
    float brightness = 0.0f;
    u8 paletteIndex = 0;
    u8 headWidth = 3;
};

struct PerlinParticlePunch::MeteorParticle {
    bool alive = false;
    float position = 0.0f;
    float velocity = 0.0f;
    float intensity = 0.0f;
    u8 debrisSpawned = 0;
    u8 maxDebris = 5;
    u8 frameCounter = 0;

    float tailLength() const {
        float len = velocity * 3.0f;
        if (len < 5.0f)
            len = 5.0f;
        if (len > 25.0f)
            len = 25.0f;
        return len;
    }

    bool shouldSpawnDebris() const {
        return alive && debrisSpawned < maxDebris && frameCounter > 2 &&
               (frameCounter % 5 == 0);
    }
};

struct PerlinParticlePunch::DebrisParticle {
    bool alive = false;
    float position = 0.0f;
    float velocity = 0.0f;
    float brightness = 0.0f;
    CRGB color = CRGB::Black;
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PerlinParticlePunch::PerlinParticlePunch(u16 num_leds) : Fx1d(num_leds) {
    mAmbientParticles.resize(50);
    mMeteorParticles.resize(5);
    mDebrisParticles.resize(50);
    mTrailBuffer.resize(num_leds);
    // Default blue-white palette
    CRGBPalette16 defaultPalette(CRGB(0, 0, 40), CRGB(0, 40, 120),
                                  CRGB(100, 160, 255),
                                  CRGB(255, 255, 255));
    mNoisePalette = defaultPalette;
    mAmbientPalette = defaultPalette;
}

PerlinParticlePunch::~PerlinParticlePunch() FL_NOEXCEPT = default;

fl::string PerlinParticlePunch::fxName() const {
    return "PerlinParticlePunch";
}

// ---------------------------------------------------------------------------
// Setters
// ---------------------------------------------------------------------------

void PerlinParticlePunch::setTimeMultiplier(float mult) {
    mTimeMultiplier = mult;
}

void PerlinParticlePunch::setNoisePalette(const CRGBPalette16 &palette) {
    mNoisePalette = palette;
}

void PerlinParticlePunch::setAmbientPalette(const CRGBPalette16 &palette) {
    mAmbientPalette = palette;
}

void PerlinParticlePunch::setMeteorGradient(CRGB headColor, CRGB midColor,
                                             CRGB tailColor) {
    mMeteorHeadColor = headColor;
    mMeteorMidColor = midColor;
    mMeteorTailColor = tailColor;
}

void PerlinParticlePunch::setDrag(float drag) {
    mDrag = drag;
    // Meteor drag is slightly heavier than ambient.
    // Scale the difference from 1.0, not the value itself.
    // drag=0.99 → meteorDrag=0.985, drag=0.80 → meteorDrag=0.74
    float diff = 1.0f - drag;
    mMeteorDrag = 1.0f - diff * 1.5f;
    if (mMeteorDrag < 0.0f)
        mMeteorDrag = 0.0f;
}

void PerlinParticlePunch::setSpeed(float speed) { mSpeed = speed; }

void PerlinParticlePunch::setAmbientTrailIntensity(u8 intensity) {
    mAmbientTrailIntensity = intensity;
}

void PerlinParticlePunch::setMeteorTrailIntensity(u8 intensity) {
    mMeteorTrailIntensity = intensity;
}

void PerlinParticlePunch::setAmbientBrightnessDecay(float decay) {
    mAmbientBrightnessDecay = decay;
}

void PerlinParticlePunch::setMinVelocity(float minVel) {
    mMinVelocity = minVel;
}

void PerlinParticlePunch::setDebrisBrightnessDecay(float decay) {
    mDebrisBrightnessDecay = decay;
}

void PerlinParticlePunch::setDebrisVelocityDecay(float decay) {
    mDebrisVelocityDecay = decay;
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

void PerlinParticlePunch::spawnAmbient(float intensity) {
    AmbientParticle *p = tryAllocateAmbient();
    if (!p)
        return;
    p->alive = true;
    p->position = 0.0f; // All particles punch out from position 0 (ground)
    float jitter = float(random8(80, 120)) / 100.0f;
    p->velocity = (0.5f + intensity * 1.5f) * mSpeed * jitter;
    p->brightness = 128.0f + intensity * 127.0f;
    p->paletteIndex = random8();
    p->headWidth = 3 + random8(3); // 3, 4, or 5
}

void PerlinParticlePunch::spawnMeteor(float intensity) {
    MeteorParticle *m = tryAllocateMeteor();
    if (!m)
        return;
    if (intensity < 0.0f)
        intensity = 0.0f;
    if (intensity > 1.0f)
        intensity = 1.0f;
    m->alive = true;
    m->position = 0.0f;
    m->velocity = (1.5f + intensity * 5.0f) * mSpeed;
    m->intensity = intensity;
    m->debrisSpawned = 0;
    m->maxDebris = u8(5 + intensity * 5);
    m->frameCounter = 0;
}

// ---------------------------------------------------------------------------
// Pool allocation
// ---------------------------------------------------------------------------

PerlinParticlePunch::AmbientParticle *
PerlinParticlePunch::tryAllocateAmbient() {
    u16 n = (u16)mAmbientParticles.size();
    for (u16 i = 0; i < n; ++i) {
        if (!mAmbientParticles[i].alive) {
            mAmbientParticles[i] = AmbientParticle();
            return &mAmbientParticles[i];
        }
    }
    return nullptr;
}

PerlinParticlePunch::MeteorParticle *
PerlinParticlePunch::tryAllocateMeteor() {
    u16 n = (u16)mMeteorParticles.size();
    for (u16 i = 0; i < n; ++i) {
        if (!mMeteorParticles[i].alive) {
            mMeteorParticles[i] = MeteorParticle();
            return &mMeteorParticles[i];
        }
    }
    return nullptr;
}

PerlinParticlePunch::DebrisParticle *
PerlinParticlePunch::tryAllocateDebris() {
    u16 n = (u16)mDebrisParticles.size();
    for (u16 i = 0; i < n; ++i) {
        if (!mDebrisParticles[i].alive) {
            mDebrisParticles[i] = DebrisParticle();
            return &mDebrisParticles[i];
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Perlin noise (kept from original, with time-warp and palette)
// ---------------------------------------------------------------------------

s16x16 PerlinParticlePunch::mapf(s16x16 x, s16x16 in_min, s16x16 in_max,
                                  s16x16 out_min, s16x16 out_max) {
    // Divide first to avoid s16x16 overflow on large intermediate products.
    // e.g. (223 * 255) exceeds s16x16 max (~32767), but 223/223 * 255 = 255 fits.
    return (x - in_min) / (in_max - in_min) * (out_max - out_min) + out_min;
}

s16x16 PerlinParticlePunch::circleNoiseGen(u32 now, s16x16 theta) const {
    s16x16 sin_val, cos_val;
    s16x16::sincos(theta, sin_val, cos_val);
    u32 x =
        u32((i64(cos_val.raw() + s16x16::SCALE) * 0xafff) >> s16x16::FRAC_BITS);
    u32 y =
        u32((i64(sin_val.raw() + s16x16::SCALE) * 0xafff) >> s16x16::FRAC_BITS);
    // Time dimension with time-warp multiplier
    u32 z = u32(float(now * 0x000fu) * mTimeMultiplier);
    u16 val = inoise16(x, y, z);
    s16x16 tmp = s16x16::from_raw(
        i32((u32(val) << s16x16::FRAC_BITS) / 0xcfffu));
    constexpr s16x16 one(1.0f);
    if (tmp > one)
        tmp = one;
    tmp = tmp * tmp;
    tmp = tmp * tmp;
    tmp = tmp * s16x16(255);
    return tmp;
}

void PerlinParticlePunch::noiseCircleDraw(u32 now, fl::span<CRGB> dst) {
    // Apply time-warp to rotation speed — this is the visible acceleration
    u32 warped_now = u32(float(now) * mTimeMultiplier);
    s16x16 time_factor = s16x16::from_raw(static_cast<i32>(warped_now * 32u));
    constexpr s16x16 two_pi(6.2831853f);
    s16x16 step = two_pi / s16x16(i32(mNumLeds));
    s16x16 theta = -time_factor;
    constexpr s16x16 threshold(32.0f);
    constexpr s16x16 zero(0.0f);
    constexpr s16x16 max_val(255.0f);
    for (u16 i = 0; i < mNumLeds; ++i) {
        s16x16 val = circleNoiseGen(now + 1000, theta);
        if (val < threshold) {
            val = zero;
        } else {
            val = mapf(val, threshold, max_val, zero, max_val);
        }
        u8 val_u8 = u8(val.to_int());
        // Palette lookup: val_u8 selects color AND brightness
        dst[i] = ColorFromPalette(mNoisePalette, val_u8, val_u8, LINEARBLEND);
        theta = theta + step;
    }
}

// ---------------------------------------------------------------------------
// Rendering helpers
// ---------------------------------------------------------------------------

void PerlinParticlePunch::writeMax(CRGB &dst, const CRGB &src) {
    if (src.r > dst.r)
        dst.r = src.r;
    if (src.g > dst.g)
        dst.g = src.g;
    if (src.b > dst.b)
        dst.b = src.b;
}

void PerlinParticlePunch::renderAmbient(const AmbientParticle &p) {
    int center = int(p.position);
    float frac = p.position - float(center);
    u8 bri = clamp_u8(p.brightness);
    CRGB baseColor =
        ColorFromPalette(mAmbientPalette, p.paletteIndex, bri, LINEARBLEND);

    for (int offset = 0; offset < p.headWidth; ++offset) {
        int idx = center - offset; // trail extends behind (toward pos 0)
        if (idx < 0 || idx >= mNumLeds)
            continue;

        // Linear falloff: 100% at head → 20% at tail of gradient
        float falloff = 1.0f - (float(offset) / float(p.headWidth)) * 0.8f;
        CRGB pixel = baseColor;
        pixel.nscale8(clamp_u8(falloff * 255.0f));

        // Sub-pixel blending for the leading edge
        if (offset == 0 && frac > 0.01f) {
            int nextIdx = center + 1;
            if (nextIdx < mNumLeds) {
                CRGB subPixel = pixel;
                subPixel.nscale8(clamp_u8(frac * 255.0f));
                writeMax(mTrailBuffer[nextIdx], subPixel);
            }
            pixel.nscale8(clamp_u8((1.0f - frac) * 255.0f));
        }

        writeMax(mTrailBuffer[idx], pixel);
    }
}

void PerlinParticlePunch::renderMeteor(const MeteorParticle &m) {
    int center = int(m.position);

    // --- Head: 5-pixel gaussian kernel ---
    static const float kGaussian[] = {0.15f, 0.60f, 1.0f, 0.60f, 0.15f};
    for (int offset = -2; offset <= 2; ++offset) {
        int idx = center + offset;
        if (idx < 0 || idx >= mNumLeds)
            continue;
        float weight = kGaussian[offset + 2];
        u8 bri = clamp_u8(255.0f * m.intensity * weight);
        // Re-entry sparkle: random brightness jitter per pixel per frame
        bri = scale8(bri, random8(153, 255));
        CRGB headColor = mMeteorHeadColor;
        headColor.nscale8(bri);
        writeMax(mTrailBuffer[idx], headColor);
    }

    // --- Tail: gradient behind the head ---
    float tailLen = m.tailLength();
    int tailPixels = int(tailLen);
    for (int i = 1; i <= tailPixels; ++i) {
        int idx = center - i;
        if (idx < 0)
            break;
        if (idx >= mNumLeds)
            continue;

        // t: 0.0 at head, 1.0 at tail tip
        float t = float(i) / float(tailPixels);
        fract8 blendAmt = clamp_u8(t * 255.0f);
        CRGB tailColor = blend(mMeteorMidColor, mMeteorTailColor, blendAmt);
        // Quadratic brightness falloff along tail
        float bri = (1.0f - t * t) * m.intensity;
        tailColor.nscale8(clamp_u8(bri * 255.0f));
        writeMax(mTrailBuffer[idx], tailColor);
    }
}

void PerlinParticlePunch::renderDebris(const DebrisParticle &d) {
    int idx = int(d.position);
    float frac = d.position - float(idx);
    u8 bri = clamp_u8(d.brightness);
    CRGB pixel = d.color;
    pixel.nscale8(bri);

    if (idx >= 0 && idx < mNumLeds) {
        CRGB main = pixel;
        main.nscale8(clamp_u8((1.0f - frac) * 255.0f));
        writeMax(mTrailBuffer[idx], main);
    }
    int nextIdx = idx + 1;
    if (nextIdx >= 0 && nextIdx < mNumLeds && frac > 0.01f) {
        CRGB sub = pixel;
        sub.nscale8(clamp_u8(frac * 255.0f));
        writeMax(mTrailBuffer[nextIdx], sub);
    }
}

void PerlinParticlePunch::spawnDebrisFromMeteor(MeteorParticle &m, u32) {
    DebrisParticle *d = tryAllocateDebris();
    if (!d)
        return;

    float tailLen = m.tailLength();
    u8 maxOffset = clamp_u8(tailLen);
    if (maxOffset < 3)
        maxOffset = 3;
    float spawnOffset = float(random8(2, maxOffset));
    float spawnPos = m.position - spawnOffset;
    if (spawnPos < 0.0f)
        spawnPos = 0.0f;

    // Interpolate meteor tail color at detach point
    float t = spawnOffset / tailLen;
    if (t > 1.0f)
        t = 1.0f;
    fract8 blendAmt = clamp_u8(t * 255.0f);
    CRGB debrisColor = blend(mMeteorMidColor, mMeteorTailColor, blendAmt);

    d->alive = true;
    d->position = spawnPos;
    d->velocity = float(random8(10, 40)) / 100.0f; // 0.1-0.4 LEDs/frame
    d->brightness = 180.0f;
    d->color = debrisColor;
    m.debrisSpawned++;
}

// ---------------------------------------------------------------------------
// Main draw
// ---------------------------------------------------------------------------

void PerlinParticlePunch::draw(DrawContext context) {
    fl::span<CRGB> leds = context.leds;
    if (leds.empty() || mNumLeds == 0) {
        return;
    }
    u32 now = context.now;

    // --- Layer 1: Perlin noise background (fresh each frame) ---
    noiseCircleDraw(now, leds);

    // --- Trail buffer decay ---
    // Use the larger of the two trail intensities for the shared buffer.
    // Higher intensity value = less fade per frame = longer trails.
    u8 trailIntensity = mAmbientTrailIntensity > mMeteorTrailIntensity
                            ? mAmbientTrailIntensity
                            : mMeteorTrailIntensity;
    u8 decayRate = 255 - trailIntensity;
    fadeToBlackBy(mTrailBuffer.data(), mNumLeds, decayRate);

    // --- Layer 2: Ambient particles ---
    for (u16 i = 0; i < (u16)mAmbientParticles.size(); ++i) {
        AmbientParticle &p = mAmbientParticles[i];
        if (!p.alive)
            continue;
        // Physics update
        p.velocity *= mDrag;
        p.position += p.velocity;
        p.brightness *= mAmbientBrightnessDecay;
        if (p.position >= float(mNumLeds) || p.position < 0.0f ||
            p.brightness < 8.0f || p.velocity < mMinVelocity) {
            p.alive = false;
            continue;
        }
        renderAmbient(p);
    }

    // --- Layer 3: Meteors ---
    for (u16 i = 0; i < (u16)mMeteorParticles.size(); ++i) {
        MeteorParticle &m = mMeteorParticles[i];
        if (!m.alive)
            continue;
        // Debris spawn check before physics update
        if (m.shouldSpawnDebris()) {
            spawnDebrisFromMeteor(m, now);
        }
        // Physics update
        m.velocity *= mMeteorDrag;
        m.position += m.velocity;
        m.frameCounter++;
        if (m.position >= float(mNumLeds) || m.velocity < mMinVelocity) {
            m.alive = false;
            continue;
        }
        renderMeteor(m);
    }

    // --- Debris ---
    for (u16 i = 0; i < (u16)mDebrisParticles.size(); ++i) {
        DebrisParticle &d = mDebrisParticles[i];
        if (!d.alive)
            continue;
        // Physics update
        d.position += d.velocity;
        d.velocity *= mDebrisVelocityDecay;
        d.brightness *= mDebrisBrightnessDecay;
        if (d.brightness < 5.0f || d.position >= float(mNumLeds)) {
            d.alive = false;
            continue;
        }
        renderDebris(d);
    }

    // --- Composite: max(noise, trail) per channel ---
    for (u16 i = 0; i < mNumLeds; ++i) {
        if (mTrailBuffer[i].r > leds[i].r)
            leds[i].r = mTrailBuffer[i].r;
        if (mTrailBuffer[i].g > leds[i].g)
            leds[i].g = mTrailBuffer[i].g;
        if (mTrailBuffer[i].b > leds[i].b)
            leds[i].b = mTrailBuffer[i].b;
    }
}

} // namespace fl
