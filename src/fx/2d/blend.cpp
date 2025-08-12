/*
Fx2d class that allows to blend multiple Fx2d layers together.
The bottom layer is always drawn at full capacity. Upper layers
are blended by the the max luminance of the components.
*/

#include "blend.h"
#include "colorutils.h"
#include "pixelset.h"

#include "fl/stdint.h"

namespace fl {

Blend2d::Blend2d(const XYMap &xymap) : Fx2d(xymap) {
    // Warning, the xyMap will be the final transrformation applied to the
    // frame. If the delegate Fx2d layers have their own transformation then
    // both will be applied.
    mFrame = fl::make_shared<Frame>(mXyMap.getTotal());
    mFrameTransform = fl::make_shared<Frame>(mXyMap.getTotal());
}

Str Blend2d::fxName() const {
    fl::string out = "LayeredFx2d(";
    for (size_t i = 0; i < mLayers.size(); ++i) {
        out += mLayers[i].fx->fxName();
        if (i != mLayers.size() - 1) {
            out += ",";
        }
    }
    out += ")";
    return out;
}

void Blend2d::add(Fx2dPtr layer, const Params &p) {
    if (!layer->getXYMap().isRectangularGrid()) {
        if (!getXYMap().isRectangularGrid()) {
            FASTLED_WARN("Blend2d has a xymap, but so does the Sub layer " << layer->fxName() << ", the sub layer will have it's map replaced with a rectangular map, to avoid double transformation.");
            layer->setXYMap(XYMap::constructRectangularGrid(layer->getWidth(), layer->getHeight()));
        }
    }
    uint8_t blurAmount = p.blur_amount;
    uint8_t blurPasses = p.blur_passes;
    Entry entry(layer, blurAmount, blurPasses);
    mLayers.push_back(entry);
}

void Blend2d::add(Fx2d &layer, const Params &p) {
    Fx2dPtr fx = fl::make_shared_no_tracking(layer);
    this->add(fx, p);
}

void Blend2d::draw(DrawContext context) {
    mFrame->clear();
    mFrameTransform->clear();

    // Draw each layer in reverse order and applying the blending.
    bool first = true;
    for (auto it = mLayers.begin(); it != mLayers.end(); ++it) {
        DrawContext tmp_ctx = context;
        tmp_ctx.leds = mFrame->rgb();
        auto &fx = it->fx;
        fx->draw(tmp_ctx);
        DrawMode mode = first ? DrawMode::DRAW_MODE_OVERWRITE
                              : DrawMode::DRAW_MODE_BLEND_BY_MAX_BRIGHTNESS;
        first = false;
        // Apply the blur effect per effect.
        uint8_t blur_amount = it->blur_amount;
        if (blur_amount > 0) {
            const XYMap &xyMap = fx->getXYMap();
            uint8_t blur_passes = MAX(1, it->blur_passes);
            for (uint8_t i = 0; i < blur_passes; ++i) {
                // Apply the blur effect
                blur2d(mFrame->rgb(), mXyMap.getWidth(), mXyMap.getHeight(),
                       blur_amount, xyMap);
            }
        }
        mFrame->draw(mFrameTransform->rgb(), mode);
    }

    if (mGlobalBlurAmount > 0) {
        // Apply the blur effect
        uint16_t width = mXyMap.getWidth();
        uint16_t height = mXyMap.getHeight();
        XYMap rect = XYMap::constructRectangularGrid(width, height);
        CRGB *rgb = mFrameTransform->rgb();
        uint8_t blur_passes = MAX(1, mGlobalBlurPasses);
        for (uint8_t i = 0; i < blur_passes; ++i) {
            // Apply the blur effect
            blur2d(rgb, width, height, mGlobalBlurAmount, rect);
        }
    }

    // Copy the final result to the output
    // memcpy(mFrameTransform->rgb(), context.leds, sizeof(CRGB) *
    // mXyMap.getTotal());
    mFrameTransform->drawXY(context.leds, mXyMap,
                            DrawMode::DRAW_MODE_OVERWRITE);
}

void Blend2d::clear() { mLayers.clear(); }

bool Blend2d::setParams(Fx2dPtr fx, const Params &p) {
    uint8_t blur_amount = p.blur_amount;
    uint8_t blur_passes = p.blur_passes;
    for (auto &layer : mLayers) {
        if (layer.fx == fx) {
            layer.blur_amount = blur_amount;
            layer.blur_passes = blur_passes;
            return true;
        }
    }

    FASTLED_WARN("Fx2d not found in Blend2d::setBlurParams");
    return false;
}

bool Blend2d::setParams(Fx2d &fx, const Params &p) {

    Fx2dPtr fxPtr = fl::make_shared_no_tracking(fx);
    return setParams(fxPtr, p);
}

} // namespace fl
