#pragma once

// FastLED Adapter for the animartrix2 fx library.
// Refactored from animartrix.hpp to use free-function architecture.
// Original: Copyright Stefen Petrick 2023.
// Adapted to C++ by Netmindz 2023.
// Adapted to FastLED by Zach Vorhies 2024.
// Refactored to Animartrix2 (Context + free functors) 2026.

//
// ⚠️  WARNING: EXPERIMENTAL API - NOT FOR PUBLIC CONSUMPTION ⚠️
//
// Animartrix2 is a performance-focused rewrite of Animartrix that transitions
// from floating-point to fixed-point math for a 4-16x speedup in rendering.
//
// This API is currently UGLY by design - it's optimized for performance tuning
// and experimentation, not for user-facing elegance. The interface WILL change
// drastically as optimizations are completed and tested.
//
// Once optimization is complete and the API is made beautiful, Animartrix2 will
// replace the original Animartrix. Until then, use animartrix.hpp for stable,
// public-facing work.
//

#include "crgb.h"
#include "fl/dbg.h"
#include "fl/stl/memory.h"
#include "fl/stl/unique_ptr.h"
#include "fl/xymap.h"
#include "fl/stl/vector.h"
#include "fl/stl/pair.h"
#include "fl/str.h"
#include "fl/fx/fx2d.h"
#include "eorder.h"
#include "pixel_controller.h"

#include "fl/fx/2d/animartrix2_detail.h"

namespace fl {

FASTLED_SHARED_PTR(Animartrix2);

// Reuse the same animation enum from Animartrix
// (defined in animartrix.hpp, but we duplicate here to avoid dependency)
enum Animartrix2Anim {
    ANIM2_RGB_BLOBS5 = 0,
    ANIM2_RGB_BLOBS4,
    ANIM2_RGB_BLOBS3,
    ANIM2_RGB_BLOBS2,
    ANIM2_RGB_BLOBS,
    ANIM2_POLAR_WAVES,
    ANIM2_SLOW_FADE,
    ANIM2_ZOOM2,
    ANIM2_ZOOM,
    ANIM2_HOT_BLOB,
    ANIM2_SPIRALUS2,
    ANIM2_SPIRALUS,
    ANIM2_YVES,
    ANIM2_SCALEDEMO1,
    ANIM2_LAVA1,
    ANIM2_CALEIDO3,
    ANIM2_CALEIDO2,
    ANIM2_CALEIDO1,
    ANIM2_DISTANCE_EXPERIMENT,
    ANIM2_CENTER_FIELD,
    ANIM2_WAVES,
    ANIM2_CHASING_SPIRALS,
    ANIM2_ROTATING_BLOB,
    ANIM2_RINGS,
    ANIM2_COMPLEX_KALEIDO,
    ANIM2_COMPLEX_KALEIDO_2,
    ANIM2_COMPLEX_KALEIDO_3,
    ANIM2_COMPLEX_KALEIDO_4,
    ANIM2_COMPLEX_KALEIDO_5,
    ANIM2_COMPLEX_KALEIDO_6,
    ANIM2_WATER,
    ANIM2_PARAMETRIC_WATER,
    ANIM2_MODULE_EXPERIMENT1,
    ANIM2_MODULE_EXPERIMENT2,
    ANIM2_MODULE_EXPERIMENT3,
    ANIM2_MODULE_EXPERIMENT4,
    ANIM2_MODULE_EXPERIMENT5,
    ANIM2_MODULE_EXPERIMENT6,
    ANIM2_MODULE_EXPERIMENT7,
    ANIM2_MODULE_EXPERIMENT8,
    ANIM2_MODULE_EXPERIMENT9,
    ANIM2_MODULE_EXPERIMENT10,
    ANIM2_MODULE_EXPERIMENT_SM1,
    ANIM2_MODULE_EXPERIMENT_SM2,
    ANIM2_MODULE_EXPERIMENT_SM3,
    ANIM2_MODULE_EXPERIMENT_SM4,
    ANIM2_MODULE_EXPERIMENT_SM5,
    ANIM2_MODULE_EXPERIMENT_SM6,
    ANIM2_MODULE_EXPERIMENT_SM8,
    ANIM2_MODULE_EXPERIMENT_SM9,
    ANIM2_MODULE_EXPERIMENT_SM10,
    ANIM2_FLUFFY_BLOBS,
    ANIM2_NUM_ANIMATIONS
};

// Animation entry: maps enum to name and visualizer function
struct Animartrix2Entry {
    Animartrix2Anim anim;
    const char *name;
    Visualizer func;
};

// The animation dispatch table
static const Animartrix2Entry ANIMATION2_TABLE[] = {
    {ANIM2_RGB_BLOBS5, "RGB_BLOBS5", &RGB_Blobs5},
    {ANIM2_RGB_BLOBS4, "RGB_BLOBS4", &RGB_Blobs4},
    {ANIM2_RGB_BLOBS3, "RGB_BLOBS3", &RGB_Blobs3},
    {ANIM2_RGB_BLOBS2, "RGB_BLOBS2", &RGB_Blobs2},
    {ANIM2_RGB_BLOBS, "RGB_BLOBS", &RGB_Blobs},
    {ANIM2_POLAR_WAVES, "POLAR_WAVES", &Polar_Waves},
    {ANIM2_SLOW_FADE, "SLOW_FADE", &Slow_Fade},
    {ANIM2_ZOOM2, "ZOOM2", &Zoom2},
    {ANIM2_ZOOM, "ZOOM", &Zoom},
    {ANIM2_HOT_BLOB, "HOT_BLOB", &Hot_Blob},
    {ANIM2_SPIRALUS2, "SPIRALUS2", &Spiralus2},
    {ANIM2_SPIRALUS, "SPIRALUS", &Spiralus},
    {ANIM2_YVES, "YVES", &Yves},
    {ANIM2_SCALEDEMO1, "SCALEDEMO1", &Scaledemo1},
    {ANIM2_LAVA1, "LAVA1", &Lava1},
    {ANIM2_CALEIDO3, "CALEIDO3", &Caleido3},
    {ANIM2_CALEIDO2, "CALEIDO2", &Caleido2},
    {ANIM2_CALEIDO1, "CALEIDO1", &Caleido3},  // Note: same as original - Caleido1 maps to Caleido3
    {ANIM2_DISTANCE_EXPERIMENT, "DISTANCE_EXPERIMENT", &Distance_Experiment},
    {ANIM2_CENTER_FIELD, "CENTER_FIELD", &Center_Field},
    {ANIM2_WAVES, "WAVES", &Waves},
    {ANIM2_CHASING_SPIRALS, "CHASING_SPIRALS", &fl::Chasing_Spirals_Q31_SIMD}, // Optimized: SIMD sincos32
    {ANIM2_ROTATING_BLOB, "ROTATING_BLOB", &Rotating_Blob},
    {ANIM2_RINGS, "RINGS", &Rings},
    {ANIM2_COMPLEX_KALEIDO, "COMPLEX_KALEIDO", &Complex_Kaleido},
    {ANIM2_COMPLEX_KALEIDO_2, "COMPLEX_KALEIDO_2", &Complex_Kaleido_2},
    {ANIM2_COMPLEX_KALEIDO_3, "COMPLEX_KALEIDO_3", &Complex_Kaleido_3},
    {ANIM2_COMPLEX_KALEIDO_4, "COMPLEX_KALEIDO_4", &Complex_Kaleido_4},
    {ANIM2_COMPLEX_KALEIDO_5, "COMPLEX_KALEIDO_5", &Complex_Kaleido_5},
    {ANIM2_COMPLEX_KALEIDO_6, "COMPLEX_KALEIDO_6", &Complex_Kaleido_6},
    {ANIM2_WATER, "WATER", &Water},
    {ANIM2_PARAMETRIC_WATER, "PARAMETRIC_WATER", &Parametric_Water},
    {ANIM2_MODULE_EXPERIMENT1, "MODULE_EXPERIMENT1", &Module_Experiment1},
    {ANIM2_MODULE_EXPERIMENT2, "MODULE_EXPERIMENT2", &Module_Experiment2},
    {ANIM2_MODULE_EXPERIMENT3, "MODULE_EXPERIMENT3", &Module_Experiment3},
    {ANIM2_MODULE_EXPERIMENT4, "MODULE_EXPERIMENT4", &Module_Experiment4},
    {ANIM2_MODULE_EXPERIMENT5, "MODULE_EXPERIMENT5", &Module_Experiment5},
    {ANIM2_MODULE_EXPERIMENT6, "MODULE_EXPERIMENT6", &Module_Experiment6},
    {ANIM2_MODULE_EXPERIMENT7, "MODULE_EXPERIMENT7", &Module_Experiment7},
    {ANIM2_MODULE_EXPERIMENT8, "MODULE_EXPERIMENT8", &Module_Experiment8},
    {ANIM2_MODULE_EXPERIMENT9, "MODULE_EXPERIMENT9", &Module_Experiment9},
    {ANIM2_MODULE_EXPERIMENT10, "MODULE_EXPERIMENT10", &Module_Experiment10},
    {ANIM2_MODULE_EXPERIMENT_SM1, "MODULE_EXPERIMENT_SM1", &SpiralMatrix1},
    {ANIM2_MODULE_EXPERIMENT_SM2, "MODULE_EXPERIMENT_SM2", &SpiralMatrix2},
    {ANIM2_MODULE_EXPERIMENT_SM3, "MODULE_EXPERIMENT_SM3", &SpiralMatrix3},
    {ANIM2_MODULE_EXPERIMENT_SM4, "MODULE_EXPERIMENT_SM4", &SpiralMatrix4},
    {ANIM2_MODULE_EXPERIMENT_SM5, "MODULE_EXPERIMENT_SM5", &SpiralMatrix5},
    {ANIM2_MODULE_EXPERIMENT_SM6, "MODULE_EXPERIMENT_SM6", &SpiralMatrix6},
    {ANIM2_MODULE_EXPERIMENT_SM8, "MODULE_EXPERIMENT_SM8", &SpiralMatrix8},
    {ANIM2_MODULE_EXPERIMENT_SM9, "MODULE_EXPERIMENT_SM9", &SpiralMatrix9},
    {ANIM2_MODULE_EXPERIMENT_SM10, "MODULE_EXPERIMENT_SM10", &SpiralMatrix10},
    {ANIM2_FLUFFY_BLOBS, "FLUFFY_BLOBS", &Fluffy_Blobs},
};

inline fl::string getAnimartrix2Name(int animation) {
    if (animation < 0 || animation >= ANIM2_NUM_ANIMATIONS) {
        return "UNKNOWN";
    }
    return ANIMATION2_TABLE[animation].name;
}

inline int getAnimartrix2Count() {
    return ANIM2_NUM_ANIMATIONS;
}

struct Animartrix2AnimInfo {
    int id;
    const char *name;
};

inline Animartrix2AnimInfo getAnimartrix2Info(int index) {
    if (index < 0 || index >= ANIM2_NUM_ANIMATIONS) {
        return {-1, "UNKNOWN"};
    }
    return {static_cast<int>(ANIMATION2_TABLE[index].anim),
            ANIMATION2_TABLE[index].name};
}

// XYMap callback adapter
inline u16 xyMapCallbackAdapter(u16 x, u16 y, void *userData) {
    Fx2d *self = static_cast<Fx2d *>(userData);
    return self->xyMap(x, y);
}

class Animartrix2 : public Fx2d {
  public:
    Animartrix2(const XYMap &xyMap, Animartrix2Anim which_animation)
        : Fx2d(xyMap) {
        mCurrentAnimation = which_animation;
        mXyMap.convertToLookUpTable();
    }

    Animartrix2(const Animartrix2 &) = delete;

    void draw(DrawContext ctx) override {
        // Set up context for rendering
        mCtx.leds = ctx.leds;
        mCtx.xyMapFn = &xyMapCallbackAdapter;
        mCtx.xyMapUserData = this;

        // Initialize or reinitialize engine on animation change
        if (mPrevAnimation != mCurrentAnimation) {
            init(mCtx, getWidth(), getHeight());
            mPrevAnimation = mCurrentAnimation;
        }
        if (mCtx.mEngine == nullptr) {
            init(mCtx, getWidth(), getHeight());
        }

        setTime(mCtx, ctx.now);

        // Dispatch to the selected animation
        for (const auto &entry : ANIMATION2_TABLE) {
            if (entry.anim == mCurrentAnimation) {
                entry.func(mCtx);
                break;
            }
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

        mCtx.leds = nullptr;
    }

    int fxNum() const { return ANIM2_NUM_ANIMATIONS; }

    void fxSet(int fx) {
        int curr = fxGet();
        if (fx < 0) {
            fx = curr + fx;
            if (fx < 0) {
                fx = ANIM2_NUM_ANIMATIONS - 1;
            }
        }
        fx = fx % ANIM2_NUM_ANIMATIONS;
        mCurrentAnimation = static_cast<Animartrix2Anim>(fx);
    }

    int fxGet() const { return static_cast<int>(mCurrentAnimation); }

    fl::string fxName() const override { return "Animartrix2:"; }

    void fxNext(int fx = 1) { fxSet(fxGet() + fx); }

    void setColorOrder(EOrder order) { mColorOrder = order; }
    EOrder getColorOrder() const { return mColorOrder; }

    static fl::vector<fl::pair<int, fl::string>> getAnimationList() {
        fl::vector<fl::pair<int, fl::string>> list;
        for (int i = 0; i < ANIM2_NUM_ANIMATIONS; i++) {
            Animartrix2AnimInfo info = getAnimartrix2Info(i);
            list.push_back(fl::make_pair(info.id, fl::string(info.name)));
        }
        return list;
    }

  private:
    Animartrix2Anim mPrevAnimation = ANIM2_NUM_ANIMATIONS;
    Animartrix2Anim mCurrentAnimation = ANIM2_RGB_BLOBS5;
    EOrder mColorOrder = RGB;
    Context mCtx;
};

} // namespace fl
