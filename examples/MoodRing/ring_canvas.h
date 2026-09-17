// ring_canvas.h - ring geometry and additive drawing on a float ring.
//
// Positions are in turns: 0 is the first LED, 1 wraps back to it. Widths
// are also in turns, so a lobe of width 0.05 covers 5% of the ring no
// matter how many LEDs there are.
//
// Everything accumulates in float and is quantised to CRGB exactly once, in
// MoodPainter. Scaling 8-bit values repeatedly is what produced the stair
// steps in the old overlay: a dim pixel at 3 -> 2 -> 1 -> 0 is a visible
// staircase, and a weight quantised to 1/255 steps between frames.
#pragma once

#include "FastLED.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"

namespace mood_ring {

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/// Linear-light float pixel, nominal range [0, 1], may exceed 1 before clip.
struct RGBf {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

RGBf toRGBf(CRGB c);
CRGB toCRGB(const RGBf &p);
inline RGBf rgbfWhite() { return RGBf{1.0f, 1.0f, 1.0f}; }

/// 0 below e0, 1 above e1, smooth in between.
float smoothstepf(float e0, float e1, float x);

/// Wrap into [0, 1).
float wrapTurns(float t);

/// Signed shortest offset from b to a on the ring, in (-0.5, 0.5].
float ringOffset(float a, float b);

/// Shortest distance between two ring positions, in turns, in [0, 0.5].
float ringDistance(float a, float b);

/// Smooth bump: 1 at distance 0, 0 at distance >= sigma.
float falloff(float distance, float sigma);

/// Small deterministic RNG for sparkle. No libc, no global state.
class Xorshift {
  public:
    explicit Xorshift(fl::u32 seed = 0x9E3779B9u) : mState(seed ? seed : 1u) {}
    fl::u32 next();
    /// Uniform in [0, 1).
    float unit();

  private:
    fl::u32 mState;
};

/// Additive drawing helpers over a span of float pixels.
class RingCanvas {
  public:
    explicit RingCanvas(fl::span<RGBf> px) : mPx(px) {}

    int size() const { return static_cast<int>(mPx.size()); }

    void clear();

    /// Add a smooth lobe centred at pos (turns) with half-width sigma (turns).
    void lobe(float pos, float sigma, const RGBf &color, float gain);

    /// Add a band at distance radius (turns, 0..0.5) from origin, on both
    /// sides of it at once: an expanding pulse ring.
    void ring(float origin, float radius, float sigma, const RGBf &color,
              float gain);

    /// Add the same colour to every pixel.
    void fill(const RGBf &color, float gain);

    /// Add to one pixel, index wrapped.
    void pixel(int i, const RGBf &color, float gain);

    /// Position of pixel i in turns.
    float posOf(int i) const;

  private:
    fl::span<RGBf> mPx;
};

} // namespace mood_ring
