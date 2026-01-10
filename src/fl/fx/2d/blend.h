/*
Fx2d class that allows to blend multiple Fx2d layers together.
The bottom layer is always drawn at full capacity. Upper layers
are blended by the the max luminance of the components.
*/

#pragma once

#include "fl/stl/stdint.h"

#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/stl/shared_ptr.h"  // For shared_ptr
#include "fl/fx/fx2d.h"

// Forward declarations to reduce header overhead
namespace fl {
class Frame;
class XYMap;
template<typename T, typename Allocator>
class vector;

struct Blend2dParams {
    uint8_t blur_amount = 0;
    uint8_t blur_passes = 1;
};

FASTLED_SHARED_PTR(Blend2d);

class Blend2d : public Fx2d {
  public:
    using Params = Blend2dParams;
    // Note that if this xymap is non rectangular then it's recommended that the
    // Fx2d layers that are added should be rectangular.
    Blend2d(const XYMap &xymap);
    fl::string fxName() const override;
    void add(Fx2dPtr layer, const Params &p = Params());
    void add(Fx2d &layer, const Params &p = Params());
    void draw(DrawContext context) override;
    void clear();
    void setGlobalBlurAmount(uint8_t blur_amount) {
        mGlobalBlurAmount = blur_amount;
    }
    void setGlobalBlurPasses(uint8_t blur_passes) {
        mGlobalBlurPasses = blur_passes;
    }
    bool setParams(Fx2dPtr fx, const Params &p);
    bool setParams(Fx2d &fx, const Params &p);

  protected:
    struct Entry {
        Fx2dPtr fx;
        uint8_t blur_amount = 0;
        uint8_t blur_passes = 1;
        Entry() = default;
        Entry(Fx2dPtr fx, uint8_t blur_amount, uint8_t blur_passes)
            : fx(fx), blur_amount(blur_amount), blur_passes(blur_passes) {}
    };
    vector<Entry> mLayers;
    fl::shared_ptr<Frame> mFrame;
    fl::shared_ptr<Frame> mFrameTransform;
    uint8_t mGlobalBlurAmount = 0;
    uint8_t mGlobalBlurPasses = 1;
};

} // namespace fl
