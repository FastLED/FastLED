#pragma once

#include "fl/stdint.h"

#include "FastLED.h"
#include "fl/blur.h"
#include "fl/clamp.h"
#include "fl/math.h"
#include "fl/memory.h"
#include "fl/vector.h"
#include "fl/xymap.h"
#include "fx/fx2d.h"
#include "noise.h"

namespace fl {

FASTLED_SMART_PTR(Luminova);

struct LuminovaParams {
    // Global fade amount applied each frame (higher = faster fade)
    uint8_t fade_amount = 18;
    // Blur amount applied each frame for trail softness
    uint8_t blur_amount = 24;
    // Per-dot gain applied to plotted pixels to prevent blowout on small grids
    uint8_t point_gain = 128; // 50%
    // Number of particles alive in the system (upper bound)
    int max_particles = 256;
};

// 2D particle field with soft white trails inspired by the Luminova example.
class Luminova : public Fx2d {
  public:
    using Params = LuminovaParams;

    explicit Luminova(const XYMap &xyMap, const Params &params = Params());

    void draw(DrawContext context) override;

    fl::string fxName() const override { return "Luminova"; }

    void setFadeAmount(uint8_t fade_amount) { mParams.fade_amount = fade_amount; }
    void setBlurAmount(uint8_t blur_amount) { mParams.blur_amount = blur_amount; }
    void setPointGain(uint8_t point_gain) { mParams.point_gain = point_gain; }

    // Adjust maximum particle slots (reinitializes pool if size changes)
    void setMaxParticles(int max_particles);

  private:
    struct Particle {
        float x = 0.0f;
        float y = 0.0f;
        float a = 0.0f; // angle
        int f = 0;   // direction (+1 or -1)
        int g = 0;   // group id (derived from time)
        float s = 0.0f; // stroke weight / intensity
        bool alive = false;
    };

    void resetParticle(Particle &p, fl::u32 tick);
    void plotDot(CRGB *leds, int x, int y, uint8_t v) const;
    void plotSoftDot(CRGB *leds, float fx, float fy, float s) const;

    Params mParams;
    fl::u32 mTick = 0;
    fl::vector<Particle> mParticles;
};

} // namespace fl
