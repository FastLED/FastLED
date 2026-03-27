#pragma once

// FastLED Adapter for the animartrix fx library.
// Copyright Stefen Petrick 2023.
// Adapted to C++ by Netmindz 2023.
// Adapted to FastLED by Zach Vorhies 2024.
// Refactored to Context + free functors 2026.
//
// Licensed under Creative Commons Attribution License CC BY-NC 3.0
// https://creativecommons.org/licenses/by-nc/3.0/

#include "crgb.h"
#include "fl/system/log.h"
#include "fl/stl/memory.h"
#include "fl/stl/unique_ptr.h"
#include "fl/math/xymap.h"
#include "fl/stl/vector.h"
#include "fl/stl/pair.h"
#include "fl/stl/string.h"
#include "fl/fx/fx2d.h"
#include "eorder.h"
#include "pixel_controller.h"

#include "fl/fx/2d/animartrix_detail.h"
#include "fl/stl/noexcept.h"

namespace fl {

FASTLED_SHARED_PTR(Animartrix);

enum class AnimartrixAnim {
    RGB_BLOBS5 = 0,
    RGB_BLOBS4,
    RGB_BLOBS3,
    RGB_BLOBS2,
    RGB_BLOBS,
    POLAR_WAVES,
    SLOW_FADE,
    ZOOM2,
    ZOOM,
    HOT_BLOB,
    SPIRALUS2,
    SPIRALUS,
    YVES,
    SCALEDEMO1,
    LAVA1,
    CALEIDO3,
    CALEIDO2,
    CALEIDO1,
    DISTANCE_EXPERIMENT,
    CENTER_FIELD,
    WAVES,
    CHASING_SPIRALS,
    ROTATING_BLOB,
    RINGS,
    COMPLEX_KALEIDO,
    COMPLEX_KALEIDO_2,
    COMPLEX_KALEIDO_3,
    COMPLEX_KALEIDO_4,
    COMPLEX_KALEIDO_5,
    COMPLEX_KALEIDO_6,
    WATER,
    PARAMETRIC_WATER,
    MODULE_EXPERIMENT1,
    MODULE_EXPERIMENT2,
    MODULE_EXPERIMENT3,
    MODULE_EXPERIMENT4,
    MODULE_EXPERIMENT5,
    MODULE_EXPERIMENT6,
    MODULE_EXPERIMENT7,
    MODULE_EXPERIMENT8,
    MODULE_EXPERIMENT9,
    MODULE_EXPERIMENT10,
    MODULE_EXPERIMENT_SM1,
    MODULE_EXPERIMENT_SM2,
    MODULE_EXPERIMENT_SM3,
    MODULE_EXPERIMENT_SM4,
    MODULE_EXPERIMENT_SM5,
    MODULE_EXPERIMENT_SM6,
    MODULE_EXPERIMENT_SM8,
    MODULE_EXPERIMENT_SM9,
    MODULE_EXPERIMENT_SM10,
    FLUFFY_BLOBS,
    NUM_ANIMATIONS
};

// Animation entry: maps enum to name and a factory that creates the visualizer.
struct AnimartrixEntry {
    AnimartrixAnim anim;
    const char *name;
    IAnimartrix2Viz* (*factory)();
};

// Factory helper: creates a heap-allocated instance of any IAnimartrix2Viz subclass.
template<typename T>
static IAnimartrix2Viz* makeViz() { return new T(); }  // ok bare allocation

// The animation dispatch table
static const AnimartrixEntry ANIMATION_TABLE[] = {
    {AnimartrixAnim::RGB_BLOBS5, "RGB_BLOBS5", &makeViz<RGB_Blobs5>},
    {AnimartrixAnim::RGB_BLOBS4, "RGB_BLOBS4", &makeViz<RGB_Blobs4>},
    {AnimartrixAnim::RGB_BLOBS3, "RGB_BLOBS3", &makeViz<RGB_Blobs3>},
    {AnimartrixAnim::RGB_BLOBS2, "RGB_BLOBS2", &makeViz<RGB_Blobs2>},
    {AnimartrixAnim::RGB_BLOBS, "RGB_BLOBS", &makeViz<RGB_Blobs>},
    {AnimartrixAnim::POLAR_WAVES, "POLAR_WAVES", &makeViz<Polar_Waves>},
    {AnimartrixAnim::SLOW_FADE, "SLOW_FADE", &makeViz<Slow_Fade>},
    {AnimartrixAnim::ZOOM2, "ZOOM2", &makeViz<Zoom2>},
    {AnimartrixAnim::ZOOM, "ZOOM", &makeViz<Zoom>},
    {AnimartrixAnim::HOT_BLOB, "HOT_BLOB", &makeViz<Hot_Blob>},
    {AnimartrixAnim::SPIRALUS2, "SPIRALUS2", &makeViz<Spiralus2>},
    {AnimartrixAnim::SPIRALUS, "SPIRALUS", &makeViz<Spiralus>},
    {AnimartrixAnim::YVES, "YVES", &makeViz<Yves>},
    {AnimartrixAnim::SCALEDEMO1, "SCALEDEMO1", &makeViz<Scaledemo1>},
    {AnimartrixAnim::LAVA1, "LAVA1", &makeViz<Lava1>},
    {AnimartrixAnim::CALEIDO3, "CALEIDO3", &makeViz<Caleido3>},
    {AnimartrixAnim::CALEIDO2, "CALEIDO2", &makeViz<Caleido2>},
    {AnimartrixAnim::CALEIDO1, "CALEIDO1", &makeViz<Caleido3>},  // Note: same as original - Caleido1 maps to Caleido3
    {AnimartrixAnim::DISTANCE_EXPERIMENT, "DISTANCE_EXPERIMENT", &makeViz<Distance_Experiment>},
    {AnimartrixAnim::CENTER_FIELD, "CENTER_FIELD", &makeViz<Center_Field>},
    {AnimartrixAnim::WAVES, "WAVES", &makeViz<Waves>},
    {AnimartrixAnim::CHASING_SPIRALS, "CHASING_SPIRALS", &makeViz<Chasing_Spirals_Q31_SIMD>}, // Optimized: SIMD sincos32
    {AnimartrixAnim::ROTATING_BLOB, "ROTATING_BLOB", &makeViz<Rotating_Blob>},
    {AnimartrixAnim::RINGS, "RINGS", &makeViz<Rings>},
    {AnimartrixAnim::COMPLEX_KALEIDO, "COMPLEX_KALEIDO", &makeViz<Complex_Kaleido>},
    {AnimartrixAnim::COMPLEX_KALEIDO_2, "COMPLEX_KALEIDO_2", &makeViz<Complex_Kaleido_2>},
    {AnimartrixAnim::COMPLEX_KALEIDO_3, "COMPLEX_KALEIDO_3", &makeViz<Complex_Kaleido_3>},
    {AnimartrixAnim::COMPLEX_KALEIDO_4, "COMPLEX_KALEIDO_4", &makeViz<Complex_Kaleido_4>},
    {AnimartrixAnim::COMPLEX_KALEIDO_5, "COMPLEX_KALEIDO_5", &makeViz<Complex_Kaleido_5>},
    {AnimartrixAnim::COMPLEX_KALEIDO_6, "COMPLEX_KALEIDO_6", &makeViz<Complex_Kaleido_6>},
    {AnimartrixAnim::WATER, "WATER", &makeViz<Water>},
    {AnimartrixAnim::PARAMETRIC_WATER, "PARAMETRIC_WATER", &makeViz<Parametric_Water>},
    {AnimartrixAnim::MODULE_EXPERIMENT1, "MODULE_EXPERIMENT1", &makeViz<Module_Experiment1>},
    {AnimartrixAnim::MODULE_EXPERIMENT2, "MODULE_EXPERIMENT2", &makeViz<Module_Experiment2>},
    {AnimartrixAnim::MODULE_EXPERIMENT3, "MODULE_EXPERIMENT3", &makeViz<Module_Experiment3>},
    {AnimartrixAnim::MODULE_EXPERIMENT4, "MODULE_EXPERIMENT4", &makeViz<Module_Experiment4>},
    {AnimartrixAnim::MODULE_EXPERIMENT5, "MODULE_EXPERIMENT5", &makeViz<Module_Experiment5>},
    {AnimartrixAnim::MODULE_EXPERIMENT6, "MODULE_EXPERIMENT6", &makeViz<Module_Experiment6>},
    {AnimartrixAnim::MODULE_EXPERIMENT7, "MODULE_EXPERIMENT7", &makeViz<Module_Experiment7>},
    {AnimartrixAnim::MODULE_EXPERIMENT8, "MODULE_EXPERIMENT8", &makeViz<Module_Experiment8>},
    {AnimartrixAnim::MODULE_EXPERIMENT9, "MODULE_EXPERIMENT9", &makeViz<Module_Experiment9>},
    {AnimartrixAnim::MODULE_EXPERIMENT10, "MODULE_EXPERIMENT10", &makeViz<Module_Experiment10>},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM1, "MODULE_EXPERIMENT_SM1", &makeViz<SpiralMatrix1>},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM2, "MODULE_EXPERIMENT_SM2", &makeViz<SpiralMatrix2>},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM3, "MODULE_EXPERIMENT_SM3", &makeViz<SpiralMatrix3>},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM4, "MODULE_EXPERIMENT_SM4", &makeViz<SpiralMatrix4>},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM5, "MODULE_EXPERIMENT_SM5", &makeViz<SpiralMatrix5>},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM6, "MODULE_EXPERIMENT_SM6", &makeViz<SpiralMatrix6>},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM8, "MODULE_EXPERIMENT_SM8", &makeViz<SpiralMatrix8>},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM9, "MODULE_EXPERIMENT_SM9", &makeViz<SpiralMatrix9>},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM10, "MODULE_EXPERIMENT_SM10", &makeViz<SpiralMatrix10>},
    {AnimartrixAnim::FLUFFY_BLOBS, "FLUFFY_BLOBS", &makeViz<Fluffy_Blobs>},
};

struct AnimartrixAnimInfo {
    int id;
    const char* name;
};

inline fl::string getAnimartrixName(int animation) {
    if (animation < 0 || animation >= static_cast<int>(AnimartrixAnim::NUM_ANIMATIONS)) {
        return "UNKNOWN";
    }
    return ANIMATION_TABLE[animation].name;
}

inline int getAnimartrixCount() {
    return static_cast<int>(AnimartrixAnim::NUM_ANIMATIONS);
}

inline AnimartrixAnimInfo getAnimartrixInfo(int index) {
    if (index < 0 || index >= static_cast<int>(AnimartrixAnim::NUM_ANIMATIONS)) {
        return {-1, "UNKNOWN"};
    }
    return {static_cast<int>(ANIMATION_TABLE[index].anim),
            ANIMATION_TABLE[index].name};
}

// XYMap callback adapter
inline u16 xyMapCallbackAdapter(u16 x, u16 y, void *userData) {
    Fx2d *self = static_cast<Fx2d *>(userData);
    return self->xyMap(x, y);
}

class Animartrix : public Fx2d {
  public:
    Animartrix(const XYMap &xyMap, AnimartrixAnim which_animation)
        : Fx2d(xyMap) {
        mCurrentAnimation = which_animation;
        // convertToLookUpTable() is deferred to draw() to avoid
        // cross-DLL static initialization order issues on Windows.
    }

    Animartrix(const Animartrix &) FL_NOEXCEPT = delete;

    void draw(DrawContext ctx) override {
        if (!mXyMap.isLUT()) {
            mXyMap.convertToLookUpTable();
        }
        // Set up context for rendering
        mCtx.leds = ctx.leds;
        mCtx.xyMapFn = &xyMapCallbackAdapter;
        mCtx.xyMapUserData = this;

        // Initialize or reinitialize engine on animation change
        const bool anim_changed = (mPrevAnimation != mCurrentAnimation);
        if (anim_changed) {
            init(mCtx, getWidth(), getHeight());
            mPrevAnimation = mCurrentAnimation;
        }
        if (mCtx.mEngine == nullptr) {
            init(mCtx, getWidth(), getHeight());
        }

        // Create (or recreate after animation change) the visualizer instance.
        if (anim_changed || !mViz) {
            for (const auto &entry : ANIMATION_TABLE) {
                if (entry.anim == mCurrentAnimation) {
                    mViz.reset(entry.factory());
                    break;
                }
            }
        }

        setTime(mCtx, ctx.now);

        // Dispatch to the selected animation
        if (mViz) {
            mViz->draw(mCtx);
        }

        // Apply color order if not RGB
        if (mColorOrder != RGB) {
            for (int i = 0; i < mXyMap.getTotal(); ++i) {
                CRGB &pixel = ctx.leds[i];
                const u8 b0_index = RGB_BYTE0(mColorOrder);
                const u8 b1_index = RGB_BYTE1(mColorOrder);
                const u8 b2_index = RGB_BYTE2(mColorOrder);
                pixel = CRGB(pixel.raw[b0_index], pixel.raw[b1_index],
                             pixel.raw[b2_index]);
            }
        }

        mCtx.leds = fl::span<CRGB>();
    }

    int fxNum() const { return static_cast<int>(AnimartrixAnim::NUM_ANIMATIONS); }

    void fxSet(int fx) {
        int curr = fxGet();
        if (fx < 0) {
            fx = curr + fx;
            if (fx < 0) {
                fx = static_cast<int>(AnimartrixAnim::NUM_ANIMATIONS) - 1;
            }
        }
        fx = fx % static_cast<int>(AnimartrixAnim::NUM_ANIMATIONS);
        mCurrentAnimation = static_cast<AnimartrixAnim>(fx);
        FASTLED_DBG("Setting animation to " << getAnimartrixName(static_cast<int>(mCurrentAnimation)));
    }

    int fxGet() const { return static_cast<int>(mCurrentAnimation); }

    fl::string fxName() const override { return "Animartrix:"; }

    void fxNext(int fx = 1) { fxSet(fxGet() + fx); }

    void setColorOrder(EOrder order) { mColorOrder = order; }
    EOrder getColorOrder() const { return mColorOrder; }

    static fl::vector<fl::pair<int, fl::string>> getAnimationList() {
        fl::vector<fl::pair<int, fl::string>> list;
        for (int i = 0; i < static_cast<int>(AnimartrixAnim::NUM_ANIMATIONS); i++) {
            AnimartrixAnimInfo info = getAnimartrixInfo(i);
            list.push_back(fl::make_pair(info.id, fl::string(info.name)));
        }
        return list;
    }

  private:
    AnimartrixAnim mPrevAnimation = AnimartrixAnim::NUM_ANIMATIONS;
    AnimartrixAnim mCurrentAnimation = AnimartrixAnim::RGB_BLOBS5;
    EOrder mColorOrder = EOrder::RGB;
    Context mCtx;
    fl::unique_ptr<IAnimartrix2Viz> mViz;
};

} // namespace fl
