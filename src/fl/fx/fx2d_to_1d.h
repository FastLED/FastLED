#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/unique_ptr.h"

#include "fl/gfx/sample.h"
#include "fl/math/screenmap.h"
#include "fl/fx/fx1d.h"
#include "fl/fx/fx2d.h"
#include "fl/stl/noexcept.h"

namespace fl {

FASTLED_SHARED_PTR(Fx2dTo1d);

/// @brief Fx2dTo1d samples a 2D effect into a 1D LED strip using a ScreenMap.
///
/// This class wraps any Fx2d effect and samples it into a 1D LED strip based on
/// a ScreenMap that defines the coordinate mapping. This is useful for creating
/// effects like sampling a circle from a rectangular grid, or any other arbitrary
/// path through 2D space.
///
/// Example usage:
/// @code
/// XYMap xymap = XYMap::constructRectangularGrid(32, 32);
/// auto animartrix = fl::make_shared<fl::Animartrix>(xymap, fl::RGB_BLOBS5);
///
/// // Create circular sampling pattern
/// fl::ScreenMap screenmap = fl::ScreenMap(60, 0.5f, [](int index, fl::vec2f& pt_out) {
///     float angle = (TWO_PI * index) / 60;
///     pt_out.x = 16.0f + cos(angle) * 15.0f;
///     pt_out.y = 16.0f + sin(angle) * 15.0f;
/// });
///
/// auto fx = fl::make_shared<fl::Fx2dTo1d>(60, animartrix, screenmap);
/// @endcode
class Fx2dTo1d : public Fx1d {
  public:
    /// Interpolation mode for sampling the 2D grid
    enum InterpolationMode {
        NEAREST = int(SampleMode::SAMPLE_NEAREST),   ///< Nearest neighbor (fast, pixelated)
        BILINEAR = int(SampleMode::SAMPLE_BILINEAR), ///< Bilinear interpolation (smooth)
    };

    /// @brief Construct a 2D-to-1D sampling effect
    /// @param numLeds Number of LEDs in the 1D strip
    /// @param fx2d The 2D effect to sample from
    /// @param screenMap Mapping from 1D index to 2D float coordinates
    /// @param mode Interpolation mode (default: BILINEAR)
    Fx2dTo1d(u16 numLeds, Fx2dPtr fx2d, const ScreenMap &screenMap,
             InterpolationMode mode = BILINEAR) FL_NO_EXCEPT;

    void draw(DrawContext context) FL_NO_EXCEPT override;
    fl::string fxName() const FL_NO_EXCEPT override;

    bool hasFixedFrameRate(float *fps) const FL_NO_EXCEPT override {
        return mFx2d->hasFixedFrameRate(fps);
    }

    void pause(u32 now) FL_NO_EXCEPT override { mFx2d->pause(now); }
    void resume(u32 now) FL_NO_EXCEPT override { mFx2d->resume(now); }

    /// @brief Set the screen map for coordinate mapping
    void setScreenMap(const ScreenMap &screenMap) FL_NO_EXCEPT { mScreenMap = screenMap; }

    /// @brief Set the interpolation mode
    void setInterpolationMode(InterpolationMode mode) FL_NO_EXCEPT {
        mInterpolationMode = mode;
    }

    /// @brief Replace the underlying 2D effect
    void setFx2d(Fx2dPtr fx2d) FL_NO_EXCEPT;

    /// @brief Get the underlying 2D effect
    Fx2dPtr getFx2d() const FL_NO_EXCEPT { return mFx2d; }

  private:
    Fx2dPtr mFx2d;
    ScreenMap mScreenMap;
    InterpolationMode mInterpolationMode;

    // Working buffer for 2D effect rendering
    fl::unique_ptr<CRGB[]> mGrid;
};

} // namespace fl
