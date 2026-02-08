#pragma once

// FastLED Adapter for the animartrix2 fx library.
// Refactored from animartrix.hpp to use free-function architecture.
// Original: Copyright Stefen Petrick 2023.
// Adapted to C++ by Netmindz 2023.
// Adapted to FastLED by Zach Vorhies 2024.
// Refactored to Animartrix2 (Context + free functors) 2026.

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

#define ANIMARTRIX2_INTERNAL
#include "fl/fx/2d/animartrix2_detail.hpp"

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
    animartrix2_detail::Visualizer func;
};

// The animation dispatch table
static const Animartrix2Entry ANIMATION2_TABLE[] = {
    {ANIM2_RGB_BLOBS5, "RGB_BLOBS5", &animartrix2_detail::RGB_Blobs5},
    {ANIM2_RGB_BLOBS4, "RGB_BLOBS4", &animartrix2_detail::RGB_Blobs4},
    {ANIM2_RGB_BLOBS3, "RGB_BLOBS3", &animartrix2_detail::RGB_Blobs3},
    {ANIM2_RGB_BLOBS2, "RGB_BLOBS2", &animartrix2_detail::RGB_Blobs2},
    {ANIM2_RGB_BLOBS, "RGB_BLOBS", &animartrix2_detail::RGB_Blobs},
    {ANIM2_POLAR_WAVES, "POLAR_WAVES", &animartrix2_detail::Polar_Waves},
    {ANIM2_SLOW_FADE, "SLOW_FADE", &animartrix2_detail::Slow_Fade},
    {ANIM2_ZOOM2, "ZOOM2", &animartrix2_detail::Zoom2},
    {ANIM2_ZOOM, "ZOOM", &animartrix2_detail::Zoom},
    {ANIM2_HOT_BLOB, "HOT_BLOB", &animartrix2_detail::Hot_Blob},
    {ANIM2_SPIRALUS2, "SPIRALUS2", &animartrix2_detail::Spiralus2},
    {ANIM2_SPIRALUS, "SPIRALUS", &animartrix2_detail::Spiralus},
    {ANIM2_YVES, "YVES", &animartrix2_detail::Yves},
    {ANIM2_SCALEDEMO1, "SCALEDEMO1", &animartrix2_detail::Scaledemo1},
    {ANIM2_LAVA1, "LAVA1", &animartrix2_detail::Lava1},
    {ANIM2_CALEIDO3, "CALEIDO3", &animartrix2_detail::Caleido3},
    {ANIM2_CALEIDO2, "CALEIDO2", &animartrix2_detail::Caleido2},
    {ANIM2_CALEIDO1, "CALEIDO1", &animartrix2_detail::Caleido3},  // Note: same as original - Caleido1 maps to Caleido3
    {ANIM2_DISTANCE_EXPERIMENT, "DISTANCE_EXPERIMENT", &animartrix2_detail::Distance_Experiment},
    {ANIM2_CENTER_FIELD, "CENTER_FIELD", &animartrix2_detail::Center_Field},
    {ANIM2_WAVES, "WAVES", &animartrix2_detail::Waves},
    {ANIM2_CHASING_SPIRALS, "CHASING_SPIRALS", &animartrix2_detail::q31::Chasing_Spirals_Q31},
    {ANIM2_ROTATING_BLOB, "ROTATING_BLOB", &animartrix2_detail::Rotating_Blob},
    {ANIM2_RINGS, "RINGS", &animartrix2_detail::Rings},
    {ANIM2_COMPLEX_KALEIDO, "COMPLEX_KALEIDO", &animartrix2_detail::Complex_Kaleido},
    {ANIM2_COMPLEX_KALEIDO_2, "COMPLEX_KALEIDO_2", &animartrix2_detail::Complex_Kaleido_2},
    {ANIM2_COMPLEX_KALEIDO_3, "COMPLEX_KALEIDO_3", &animartrix2_detail::Complex_Kaleido_3},
    {ANIM2_COMPLEX_KALEIDO_4, "COMPLEX_KALEIDO_4", &animartrix2_detail::Complex_Kaleido_4},
    {ANIM2_COMPLEX_KALEIDO_5, "COMPLEX_KALEIDO_5", &animartrix2_detail::Complex_Kaleido_5},
    {ANIM2_COMPLEX_KALEIDO_6, "COMPLEX_KALEIDO_6", &animartrix2_detail::Complex_Kaleido_6},
    {ANIM2_WATER, "WATER", &animartrix2_detail::Water},
    {ANIM2_PARAMETRIC_WATER, "PARAMETRIC_WATER", &animartrix2_detail::Parametric_Water},
    {ANIM2_MODULE_EXPERIMENT1, "MODULE_EXPERIMENT1", &animartrix2_detail::Module_Experiment1},
    {ANIM2_MODULE_EXPERIMENT2, "MODULE_EXPERIMENT2", &animartrix2_detail::Module_Experiment2},
    {ANIM2_MODULE_EXPERIMENT3, "MODULE_EXPERIMENT3", &animartrix2_detail::Module_Experiment3},
    {ANIM2_MODULE_EXPERIMENT4, "MODULE_EXPERIMENT4", &animartrix2_detail::Module_Experiment4},
    {ANIM2_MODULE_EXPERIMENT5, "MODULE_EXPERIMENT5", &animartrix2_detail::Module_Experiment5},
    {ANIM2_MODULE_EXPERIMENT6, "MODULE_EXPERIMENT6", &animartrix2_detail::Module_Experiment6},
    {ANIM2_MODULE_EXPERIMENT7, "MODULE_EXPERIMENT7", &animartrix2_detail::Module_Experiment7},
    {ANIM2_MODULE_EXPERIMENT8, "MODULE_EXPERIMENT8", &animartrix2_detail::Module_Experiment8},
    {ANIM2_MODULE_EXPERIMENT9, "MODULE_EXPERIMENT9", &animartrix2_detail::Module_Experiment9},
    {ANIM2_MODULE_EXPERIMENT10, "MODULE_EXPERIMENT10", &animartrix2_detail::Module_Experiment10},
    {ANIM2_MODULE_EXPERIMENT_SM1, "MODULE_EXPERIMENT_SM1", &animartrix2_detail::SpiralMatrix1},
    {ANIM2_MODULE_EXPERIMENT_SM2, "MODULE_EXPERIMENT_SM2", &animartrix2_detail::SpiralMatrix2},
    {ANIM2_MODULE_EXPERIMENT_SM3, "MODULE_EXPERIMENT_SM3", &animartrix2_detail::SpiralMatrix3},
    {ANIM2_MODULE_EXPERIMENT_SM4, "MODULE_EXPERIMENT_SM4", &animartrix2_detail::SpiralMatrix4},
    {ANIM2_MODULE_EXPERIMENT_SM5, "MODULE_EXPERIMENT_SM5", &animartrix2_detail::SpiralMatrix5},
    {ANIM2_MODULE_EXPERIMENT_SM6, "MODULE_EXPERIMENT_SM6", &animartrix2_detail::SpiralMatrix6},
    {ANIM2_MODULE_EXPERIMENT_SM8, "MODULE_EXPERIMENT_SM8", &animartrix2_detail::SpiralMatrix8},
    {ANIM2_MODULE_EXPERIMENT_SM9, "MODULE_EXPERIMENT_SM9", &animartrix2_detail::SpiralMatrix9},
    {ANIM2_MODULE_EXPERIMENT_SM10, "MODULE_EXPERIMENT_SM10", &animartrix2_detail::SpiralMatrix10},
    {ANIM2_FLUFFY_BLOBS, "FLUFFY_BLOBS", &animartrix2_detail::Fluffy_Blobs},
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
            animartrix2_detail::init(mCtx, getWidth(), getHeight());
            mPrevAnimation = mCurrentAnimation;
        }
        if (mCtx.mEngine == nullptr) {
            animartrix2_detail::init(mCtx, getWidth(), getHeight());
        }

        animartrix2_detail::setTime(mCtx, ctx.now);

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
    animartrix2_detail::Context mCtx;
};

} // namespace fl
