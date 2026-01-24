#include "fl/fx/1d/particles.h"
#include "fl/fastled.h"
#include "fl/blur.h"

namespace fl {

Particles1d::Particles1d(u16 num_leds, u8 max_particles, u8 fade_rate)
    : Fx1d(num_leds),
      mFadeRate(fade_rate),
      mOverdrawCount(20),
      mSpeedMultiplier(1.0f),
      mCyclical(true),
      mParticles(max_particles) {
}

Particles1d::~Particles1d() = default;

void Particles1d::draw(DrawContext context) {
    if (context.leds == nullptr || mNumLeds == 0) return;

    u32 now = context.now;

    // Overdraw loop: multiple updates per frame for smooth trails
    for (int overdraw = 0; overdraw < mOverdrawCount; overdraw++) {
        // Fade trails
        for (u16 i = 0; i < mNumLeds; i++) {
            context.leds[i].nscale8(255 - mFadeRate);
        }

        // Update and draw all particles
        for (size_t i = 0; i < mParticles.size(); i++) {
            mParticles[i].update(now, mNumLeds, mSpeedMultiplier, mCyclical);
            mParticles[i].draw(context.leds, now, mNumLeds);
        }
    }

    // Soften composite with blur
    blur1d(context.leds, mNumLeds, 64);
}

void Particles1d::spawnRandomParticle() {
    // First, try to find an inactive particle
    for (size_t i = 0; i < mParticles.size(); i++) {
        if (!mParticles[i].active) {
            mParticles[i].spawn(mNumLeds);
            return;
        }
    }

    // All slots full - find and reuse the oldest particle
    size_t oldestIdx = 0;
    u32 oldestTime = mParticles[0].birthTime;
    for (size_t i = 1; i < mParticles.size(); i++) {
        if (mParticles[i].birthTime < oldestTime) {
            oldestTime = mParticles[i].birthTime;
            oldestIdx = i;
        }
    }
    mParticles[oldestIdx].spawn(mNumLeds);
}

void Particles1d::setLifetime(u16 lifetime_ms) {
    mLifetimeMs = lifetime_ms;
}

void Particles1d::setOverdrawCount(u8 count) {
    mOverdrawCount = count;
}

void Particles1d::setSpeed(float speed) {
    mSpeedMultiplier = speed;
}

void Particles1d::setFadeRate(u8 fade_rate) {
    mFadeRate = fade_rate;
}

void Particles1d::setCyclical(bool cyclical) {
    mCyclical = cyclical;
}

fl::string Particles1d::fxName() const {
    return "Particles1d";
}

// Particle methods
Particles1d::Particle::Particle() : pos(0), baseVel(0), birthTime(0), lifetime(0), active(false) {}

float Particles1d::Particle::getPower(u32 now) const {
    if (!active || !lifetime) return 0.0f;
    float power = 1.0f - (float)(now - birthTime) / lifetime;
    return fl::clamp(power, 0.0f, 1.0f);
}

void Particles1d::Particle::spawn(u16 numLeds) {
    pos = random16(numLeds);
    float speed = 0.02f + (random16(1000) / 1000.0f) * 0.13f;
    baseVel = random8(2) ? speed : -speed;
    baseColor = CHSV(random8(), random8(), 120 + random8(81));
    lifetime = 4000 * (500 + random16(1000)) / 1000;  // 2-6 seconds
    birthTime = millis();
    active = true;
}

void Particles1d::Particle::spawn(float pos, float baseVel, CHSV baseColor, u32 lifetime) {
    this->pos = pos;
    this->baseVel = baseVel;
    this->baseColor = baseColor;
    this->lifetime = lifetime;
    this->birthTime = millis();
    this->active = true;
}

void Particles1d::Particle::update(u32 now, u16 numLeds, float speedMultiplier, bool cyclical) {
    if (!active) return;
    float power = getPower(now);
    if (power <= 0.0f) {
        active = false;
        return;
    }

    pos += baseVel * power * speedMultiplier;

    if (cyclical) {
        // Wrap around (cyclical)
        while (pos >= numLeds) pos -= numLeds;
        while (pos < 0) pos += numLeds;
    } else {
        // Stop at edges
        if (pos >= numLeds || pos < 0) {
            active = false;
        }
    }
}

void Particles1d::Particle::draw(CRGB* leds, u32 now, u16 numLeds) {
    if (!active) return;
    float power = getPower(now);
    if (power <= 0.0f) return;

    // Power effects: slow down, saturate, dim
    u8 sat = baseColor.sat + (1.0f - power) * (255 - baseColor.sat);
    u8 val = baseColor.val * power;
    CRGB color = CHSV(baseColor.hue, sat, val);

    // Sub-pixel rendering
    int i = (int)pos;
    float frac = pos - i;
    if (i >= 0 && i < (int)numLeds)
        leds[i] += color.nscale8(255 * (1.0f - frac));
    if (i + 1 >= 0 && i + 1 < (int)numLeds && frac > 0)
        leds[i + 1] += color.nscale8(255 * frac);
}

} // namespace fl
