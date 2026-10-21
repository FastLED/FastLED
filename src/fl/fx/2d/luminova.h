#pragma once

#include "fl/stl/stdint.h"

#include "crgb.h"           // Needed for CRGB parameter types  // IWYU pragma: keep
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros  // IWYU pragma: keep
#include "fl/stl/shared_ptr.h"  // For shared_ptr  // IWYU pragma: keep
#include "fl/fx/fx2d.h"
#include "fl/stl/noexcept.h"

namespace fl {

FASTLED_SHARED_PTR(Luminova);

struct LuminovaParams {
    // Global fade amount applied each frame (higher = faster fade)
    u8 fade_amount = 18;
    // Blur amount applied each frame for trail softness
    u8 blur_amount = 24;
    // Per-dot gain applied to plotted pixels to prevent blowout on small grids
    u8 point_gain = 128; // 50%
    // Number of particles alive in the system (upper bound)
    int max_particles = 256;
};

// 2D particle field with soft white trails inspired by the Luminova example.
class Luminova : public Fx2d {
  public:
    using Params = LuminovaParams;

    explicit Luminova(const XYMap &xyMap, const Params &params = Params()) FL_NO_EXCEPT;

    void draw(DrawContext context) FL_NO_EXCEPT override;

    fl::string fxName() const FL_NO_EXCEPT override { return "Luminova"; }

    void setFadeAmount(u8 fade_amount) FL_NO_EXCEPT { mParams.fade_amount = fade_amount; }
    void setBlurAmount(u8 blur_amount) FL_NO_EXCEPT { mParams.blur_amount = blur_amount; }
    void setPointGain(u8 point_gain) FL_NO_EXCEPT { mParams.point_gain = point_gain; }

    // Adjust maximum particle slots (reinitializes pool if size changes)
    void setMaxParticles(int max_particles) FL_NO_EXCEPT;

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

    void resetParticle(Particle &p, fl::u32 tick) FL_NO_EXCEPT;
    void plotDot(fl::span<CRGB> leds, int x, int y, u8 v) const FL_NO_EXCEPT;
    void plotSoftDot(fl::span<CRGB> leds, float fx, float fy, float s) const FL_NO_EXCEPT;

    Params mParams;
    fl::u32 mTick = 0;
    fl::vector<Particle> mParticles;
};

} // namespace fl
