#ifndef FASTLED_INTERNAL
#define FASTLED_INTERNAL
#endif

#include "luminova.h"

namespace fl {

Luminova::Luminova(const XYMap &xyMap, const Params &params)
    : Fx2d(xyMap), mParams(params) {
    int cap = params.max_particles;
    if (cap <= 0) {
        cap = 1;
    }
    mParticles.resize(static_cast<size_t>(cap));
    // Initialize as dead
    for (size_t i = 0; i < mParticles.size(); ++i) {
        mParticles[i].alive = false;
    }
}

void Luminova::setMaxParticles(int max_particles) {
    if (max_particles <= 0) {
        max_particles = 1;
    }
    if (static_cast<int>(mParticles.size()) == max_particles) {
        return;
    }
    mParticles.clear();
    mParticles.resize(static_cast<size_t>(max_particles));
    for (size_t i = 0; i < mParticles.size(); ++i) {
        mParticles[i].alive = false;
    }
}

void Luminova::resetParticle(Particle &p, fl::u32 tt) {
    // Position at center
    const float cx = static_cast<float>(getWidth() - 1) * 0.5f;
    const float cy = static_cast<float>(getHeight() - 1) * 0.5f;
    p.x = cx;
    p.y = cy;

    // Original used noise(I)*W, we approximate with 1D noise scaled to width
    int I = static_cast<int>(tt / 50);
    uint8_t n1 = inoise8(static_cast<uint16_t>(I * 19));
    float noiseW = (static_cast<float>(n1) / 255.0f) * static_cast<float>(getWidth());

    p.a = static_cast<float>(tt) * 1.25f + noiseW;
    p.f = (tt & 1u) ? +1 : -1;
    p.g = I;
    p.s = 3.0f;
    p.alive = true;
}

void Luminova::plotDot(CRGB *leds, int x, int y, uint8_t v) const {
    if (!mXyMap.has(x, y)) {
        return;
    }
    const uint16_t idx = mXyMap.mapToIndex(static_cast<uint16_t>(x), static_cast<uint16_t>(y));
    leds[idx] += CHSV(0, 0, scale8(v, mParams.point_gain));
}

void Luminova::plotSoftDot(CRGB *leds, float fx, float fy, float s) const {
    // Map s (decays from ~3) to a pixel radius 1..3
    float r = fl::clamp<float>(s * 0.5f, 1.0f, 3.0f);
    int R = static_cast<int>(fl::ceil(r));
    // Avoid roundf() to prevent AVR macro conflicts; do manual symmetric rounding
    int cx = static_cast<int>(fx + (fx >= 0.0f ? 0.5f : -0.5f));
    int cy = static_cast<int>(fy + (fy >= 0.0f ? 0.5f : -0.5f));
    float r2 = r * r;
    for (int dy = -R; dy <= R; ++dy) {
        for (int dx = -R; dx <= R; ++dx) {
            float d2 = static_cast<float>(dx * dx + dy * dy);
            if (d2 <= r2) {
                float fall = 1.0f - (d2 / (r2 + 0.0001f));
                float vf = 255.0f * fall;
                if (vf < 0.0f) vf = 0.0f;
                if (vf > 255.0f) vf = 255.0f;
                uint8_t v = static_cast<uint8_t>(vf);
                plotDot(leds, cx + dx, cy + dy, v);
            }
        }
    }
}

void Luminova::draw(DrawContext context) {
    // Fade + blur trails each frame
    fadeToBlackBy(context.leds, getNumLeds(), mParams.fade_amount);
    blur2d(context.leds, static_cast<fl::u8>(getWidth()), static_cast<fl::u8>(getHeight()),
           mParams.blur_amount, mXyMap);

    // Spawn/overwrite one particle per frame, round-robin across pool
    if (!mParticles.empty()) {
        size_t idx = static_cast<size_t>(mTick % static_cast<fl::u32>(mParticles.size()));
        resetParticle(mParticles[idx], mTick);
    }

    // Update and draw all particles
    for (size_t i = 0; i < mParticles.size(); ++i) {
        Particle &p = mParticles[i];
        if (!p.alive) {
            continue;
        }

        // s *= 0.997
        p.s *= 0.997f;
        if (p.s < 0.5f) {
            p.alive = false;
            continue;
        }

        // angle jitter using 2D noise: (t/99, g)
        float tOver99 = static_cast<float>(mTick) / 99.0f;
        uint8_t n2 = inoise8(static_cast<uint16_t>(tOver99 * 4096.0f), static_cast<uint16_t>(p.g * 37));
        float n2c = (static_cast<int>(n2) - 128) / 255.0f; // ~ -0.5 .. +0.5
        p.a += (n2c) / 9.0f;

        float aa = p.a * static_cast<float>(p.f);
        p.x += ::cosf(aa);
        p.y += ::sinf(aa);

        plotSoftDot(context.leds, p.x, p.y, p.s);
    }

    ++mTick;
}

} // namespace fl
