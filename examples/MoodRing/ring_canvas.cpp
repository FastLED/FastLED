// ring_canvas.cpp - ring geometry and additive drawing.
#include "ring_canvas.h"

namespace mood_ring {

RGBf toRGBf(CRGB c) {
    RGBf p;
    p.r = static_cast<float>(c.r) * (1.0f / 255.0f);
    p.g = static_cast<float>(c.g) * (1.0f / 255.0f);
    p.b = static_cast<float>(c.b) * (1.0f / 255.0f);
    return p;
}

CRGB toCRGB(const RGBf &p) {
    return CRGB(static_cast<fl::u8>(clampf(p.r, 0.0f, 1.0f) * 255.0f + 0.5f),
                static_cast<fl::u8>(clampf(p.g, 0.0f, 1.0f) * 255.0f + 0.5f),
                static_cast<fl::u8>(clampf(p.b, 0.0f, 1.0f) * 255.0f + 0.5f));
}

float smoothstepf(float e0, float e1, float x) {
    if (e1 <= e0)
        return x >= e1 ? 1.0f : 0.0f;
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float wrapTurns(float t) {
    while (t >= 1.0f)
        t -= 1.0f;
    while (t < 0.0f)
        t += 1.0f;
    return t;
}

float ringOffset(float a, float b) {
    float d = wrapTurns(a) - wrapTurns(b);
    if (d > 0.5f)
        d -= 1.0f;
    if (d <= -0.5f)
        d += 1.0f;
    return d;
}

float ringDistance(float a, float b) {
    float d = ringOffset(a, b);
    return d < 0.0f ? -d : d;
}

float falloff(float distance, float sigma) {
    if (sigma <= 0.0f)
        return 0.0f;
    float t = 1.0f - distance / sigma;
    if (t <= 0.0f)
        return 0.0f;
    if (t >= 1.0f)
        return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

fl::u32 Xorshift::next() {
    fl::u32 x = mState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    mState = x;
    return x;
}

float Xorshift::unit() {
    return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f);
}

void RingCanvas::clear() {
    for (fl::size i = 0; i < mPx.size(); ++i)
        mPx[i] = RGBf{};
}

float RingCanvas::posOf(int i) const {
    const int n = size();
    if (n <= 0)
        return 0.0f;
    return static_cast<float>(i) / static_cast<float>(n);
}

void RingCanvas::pixel(int i, const RGBf &color, float gain) {
    const int n = size();
    if (n <= 0 || gain <= 0.0f)
        return;
    i %= n;
    if (i < 0)
        i += n;
    RGBf &p = mPx[static_cast<fl::size>(i)];
    p.r += color.r * gain;
    p.g += color.g * gain;
    p.b += color.b * gain;
}

void RingCanvas::fill(const RGBf &color, float gain) {
    const int n = size();
    if (n <= 0 || gain <= 0.0f)
        return;
    for (int i = 0; i < n; ++i) {
        RGBf &p = mPx[static_cast<fl::size>(i)];
        p.r += color.r * gain;
        p.g += color.g * gain;
        p.b += color.b * gain;
    }
}

void RingCanvas::lobe(float pos, float sigma, const RGBf &color, float gain) {
    const int n = size();
    if (n <= 0 || gain <= 0.0f || sigma <= 0.0f)
        return;
    for (int i = 0; i < n; ++i) {
        const float k = falloff(ringDistance(posOf(i), pos), sigma);
        if (k <= 0.0f)
            continue;
        pixel(i, color, gain * k);
    }
}

void RingCanvas::ring(float origin, float radius, float sigma,
                      const RGBf &color, float gain) {
    const int n = size();
    if (n <= 0 || gain <= 0.0f || sigma <= 0.0f)
        return;
    for (int i = 0; i < n; ++i) {
        float d = ringDistance(posOf(i), origin) - radius;
        if (d < 0.0f)
            d = -d;
        const float k = falloff(d, sigma);
        if (k <= 0.0f)
            continue;
        pixel(i, color, gain * k);
    }
}

} // namespace mood_ring
