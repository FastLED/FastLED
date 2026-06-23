#pragma once

// FastLED Adapter for the animartrix fx library.
// Original concept by Stefan Petrick 2023.
// Adapted to C++ by Netmindz 2023.
// Adapted to FastLED by Zach Vorhies 2024.
// Refactored to Context + free functors 2026.
// Fixed-point optimization migration 2026.
//
// Licensed under the MIT License.
// See LICENSE file in the root of this repository.

#include "crgb.h"
#include "fl/log/log.h"
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

// Viz factory function signature: returns a heap-allocated IAnimartrix2Viz.
using AnimartrixVizFactory = IAnimartrix2Viz* (*)();

// Animation entry: maps enum to name and a factory that creates the visualizer.
struct AnimartrixEntry {
    AnimartrixAnim anim;
    const char *name;
    AnimartrixVizFactory factory;
};

// Factory helper: creates a heap-allocated instance of any IAnimartrix2Viz subclass.
template<typename T>
static IAnimartrix2Viz* makeViz() { return new T(); }  // ok bare allocation

// Select between floating-point and fixed-point animartrix visualizers.
// Define FL_ANIMARTRIX_USE_FIXED_POINT=0 to force floating-point, or =1 to force fixed-point.
// Default: fixed-point on all platforms except ESP32-P4 (which has a hardware FPU).
#ifndef FL_ANIMARTRIX_USE_FIXED_POINT
#if defined(FL_IS_ESP_32P4)
#define FL_ANIMARTRIX_USE_FIXED_POINT 0
#else
#define FL_ANIMARTRIX_USE_FIXED_POINT 1
#endif
#endif

// Helper macro: selects the correct viz class based on FL_ANIMARTRIX_USE_FIXED_POINT.
#if FL_ANIMARTRIX_USE_FIXED_POINT
#define ANIMARTRIX_VIZ(FloatClass, FPClass) &makeViz<FPClass>
#else
#define ANIMARTRIX_VIZ(FloatClass, FPClass) &makeViz<FloatClass>
#endif

// The animation dispatch table.
// Uses fixed-point (_FP) versions by default for ~2-3x speedup on MCUs without FPU.
// On ESP32-P4 (hardware FPU), uses floating-point versions instead.
// To override at compile time: #define FL_ANIMARTRIX_USE_FIXED_POINT 0 or 1
// To override a specific entry at runtime, use Animartrix::setVizFactory().
static const AnimartrixEntry ANIMATION_TABLE[] = {
    {AnimartrixAnim::RGB_BLOBS5, "RGB_BLOBS5", ANIMARTRIX_VIZ(RGB_Blobs5, RGB_Blobs5_FP)},
    {AnimartrixAnim::RGB_BLOBS4, "RGB_BLOBS4", ANIMARTRIX_VIZ(RGB_Blobs4, RGB_Blobs4_FP)},
    {AnimartrixAnim::RGB_BLOBS3, "RGB_BLOBS3", ANIMARTRIX_VIZ(RGB_Blobs3, RGB_Blobs3_FP)},
    {AnimartrixAnim::RGB_BLOBS2, "RGB_BLOBS2", ANIMARTRIX_VIZ(RGB_Blobs2, RGB_Blobs2_FP)},
    {AnimartrixAnim::RGB_BLOBS, "RGB_BLOBS", ANIMARTRIX_VIZ(RGB_Blobs, RGB_Blobs_FP)},
    {AnimartrixAnim::POLAR_WAVES, "POLAR_WAVES", ANIMARTRIX_VIZ(Polar_Waves, Polar_Waves_FP)},
    {AnimartrixAnim::SLOW_FADE, "SLOW_FADE", ANIMARTRIX_VIZ(Slow_Fade, Slow_Fade_FP)},
    {AnimartrixAnim::ZOOM2, "ZOOM2", ANIMARTRIX_VIZ(Zoom2, Zoom2_FP)},
    {AnimartrixAnim::ZOOM, "ZOOM", ANIMARTRIX_VIZ(Zoom, Zoom_FP)},
    {AnimartrixAnim::HOT_BLOB, "HOT_BLOB", ANIMARTRIX_VIZ(Hot_Blob, Hot_Blob_FP)},
    {AnimartrixAnim::SPIRALUS2, "SPIRALUS2", ANIMARTRIX_VIZ(Spiralus2, Spiralus2_FP)},
    {AnimartrixAnim::SPIRALUS, "SPIRALUS", ANIMARTRIX_VIZ(Spiralus, Spiralus_FP)},
    {AnimartrixAnim::YVES, "YVES", ANIMARTRIX_VIZ(Yves, Yves_FP)},
    {AnimartrixAnim::SCALEDEMO1, "SCALEDEMO1", ANIMARTRIX_VIZ(Scaledemo1, Scaledemo1_FP)},
    {AnimartrixAnim::LAVA1, "LAVA1", ANIMARTRIX_VIZ(Lava1, Lava1_FP)},
    {AnimartrixAnim::CALEIDO3, "CALEIDO3", ANIMARTRIX_VIZ(Caleido3, Caleido3_FP)},
    {AnimartrixAnim::CALEIDO2, "CALEIDO2", ANIMARTRIX_VIZ(Caleido2, Caleido2_FP)},
    {AnimartrixAnim::CALEIDO1, "CALEIDO1", ANIMARTRIX_VIZ(Caleido3, Caleido3_FP)},  // Note: same as original - Caleido1 maps to Caleido3
    {AnimartrixAnim::DISTANCE_EXPERIMENT, "DISTANCE_EXPERIMENT", ANIMARTRIX_VIZ(Distance_Experiment, Distance_Experiment_FP)},
    {AnimartrixAnim::CENTER_FIELD, "CENTER_FIELD", ANIMARTRIX_VIZ(Center_Field, Center_Field_FP)},
    {AnimartrixAnim::WAVES, "WAVES", ANIMARTRIX_VIZ(Waves, Waves_FP)},
    {AnimartrixAnim::CHASING_SPIRALS, "CHASING_SPIRALS", ANIMARTRIX_VIZ(Chasing_Spirals_Float, Chasing_Spirals_Q31_SIMD)},
    {AnimartrixAnim::ROTATING_BLOB, "ROTATING_BLOB", ANIMARTRIX_VIZ(Rotating_Blob, Rotating_Blob_FP)},
    {AnimartrixAnim::RINGS, "RINGS", ANIMARTRIX_VIZ(Rings, Rings_FP)},
    {AnimartrixAnim::COMPLEX_KALEIDO, "COMPLEX_KALEIDO", ANIMARTRIX_VIZ(Complex_Kaleido, Complex_Kaleido_FP)},
    {AnimartrixAnim::COMPLEX_KALEIDO_2, "COMPLEX_KALEIDO_2", ANIMARTRIX_VIZ(Complex_Kaleido_2, Complex_Kaleido_2_FP)},
    {AnimartrixAnim::COMPLEX_KALEIDO_3, "COMPLEX_KALEIDO_3", ANIMARTRIX_VIZ(Complex_Kaleido_3, Complex_Kaleido_3_FP)},
    {AnimartrixAnim::COMPLEX_KALEIDO_4, "COMPLEX_KALEIDO_4", ANIMARTRIX_VIZ(Complex_Kaleido_4, Complex_Kaleido_4_FP)},
    {AnimartrixAnim::COMPLEX_KALEIDO_5, "COMPLEX_KALEIDO_5", ANIMARTRIX_VIZ(Complex_Kaleido_5, Complex_Kaleido_5_FP)},
    {AnimartrixAnim::COMPLEX_KALEIDO_6, "COMPLEX_KALEIDO_6", ANIMARTRIX_VIZ(Complex_Kaleido_6, Complex_Kaleido_6_FP)},
    {AnimartrixAnim::WATER, "WATER", ANIMARTRIX_VIZ(Water, Water_FP)},
    {AnimartrixAnim::PARAMETRIC_WATER, "PARAMETRIC_WATER", ANIMARTRIX_VIZ(Parametric_Water, Parametric_Water_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT1, "MODULE_EXPERIMENT1", ANIMARTRIX_VIZ(Module_Experiment1, Module_Experiment1_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT2, "MODULE_EXPERIMENT2", ANIMARTRIX_VIZ(Module_Experiment2, Module_Experiment2_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT3, "MODULE_EXPERIMENT3", ANIMARTRIX_VIZ(Module_Experiment3, Module_Experiment3_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT4, "MODULE_EXPERIMENT4", ANIMARTRIX_VIZ(Module_Experiment4, Module_Experiment4_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT5, "MODULE_EXPERIMENT5", ANIMARTRIX_VIZ(Module_Experiment5, Module_Experiment5_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT6, "MODULE_EXPERIMENT6", ANIMARTRIX_VIZ(Module_Experiment6, Module_Experiment6_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT7, "MODULE_EXPERIMENT7", ANIMARTRIX_VIZ(Module_Experiment7, Module_Experiment7_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT8, "MODULE_EXPERIMENT8", ANIMARTRIX_VIZ(Module_Experiment8, Module_Experiment8_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT9, "MODULE_EXPERIMENT9", ANIMARTRIX_VIZ(Module_Experiment9, Module_Experiment9_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT10, "MODULE_EXPERIMENT10", ANIMARTRIX_VIZ(Module_Experiment10, Module_Experiment10_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM1, "MODULE_EXPERIMENT_SM1", ANIMARTRIX_VIZ(SpiralMatrix1, SpiralMatrix1_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM2, "MODULE_EXPERIMENT_SM2", ANIMARTRIX_VIZ(SpiralMatrix2, SpiralMatrix2_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM3, "MODULE_EXPERIMENT_SM3", ANIMARTRIX_VIZ(SpiralMatrix3, SpiralMatrix3_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM4, "MODULE_EXPERIMENT_SM4", ANIMARTRIX_VIZ(SpiralMatrix4, SpiralMatrix4_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM5, "MODULE_EXPERIMENT_SM5", ANIMARTRIX_VIZ(SpiralMatrix5, SpiralMatrix5_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM6, "MODULE_EXPERIMENT_SM6", ANIMARTRIX_VIZ(SpiralMatrix6, SpiralMatrix6_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM8, "MODULE_EXPERIMENT_SM8", ANIMARTRIX_VIZ(SpiralMatrix8, SpiralMatrix8_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM9, "MODULE_EXPERIMENT_SM9", ANIMARTRIX_VIZ(SpiralMatrix9, SpiralMatrix9_FP)},
    {AnimartrixAnim::MODULE_EXPERIMENT_SM10, "MODULE_EXPERIMENT_SM10", ANIMARTRIX_VIZ(SpiralMatrix10, SpiralMatrix10_FP)},
    {AnimartrixAnim::FLUFFY_BLOBS, "FLUFFY_BLOBS", ANIMARTRIX_VIZ(Fluffy_Blobs, Fluffy_Blobs_FP)},
};

#undef ANIMARTRIX_VIZ

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
            mViz.reset(createViz(mCurrentAnimation));
        }

        setTime(mCtx, ctx.now);

        // Dispatch to the selected animation.
        // Custom viz takes precedence; otherwise use the table-created viz.
        if (mCustomViz) {
            mCustomViz->draw(mCtx);
        } else if (mViz) {
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
        FL_DBG("Setting animation to " << getAnimartrixName(static_cast<int>(mCurrentAnimation)));
    }

    int fxGet() const { return static_cast<int>(mCurrentAnimation); }

    fl::string fxName() const override { return "Animartrix:"; }

    void fxNext(int fx = 1) { fxSet(fxGet() + fx); }

    void setColorOrder(EOrder order) { mColorOrder = order; }
    EOrder getColorOrder() const { return mColorOrder; }

    /// Override the factory for a specific built-in animation enum.
    /// The factory will be used instead of the default ANIMATION_TABLE entry
    /// when the given animation is selected. Pass nullptr to revert to default.
    /// Example: replace RGB_BLOBS with a custom implementation:
    ///   animartrix.setVizFactory(AnimartrixAnim::RGB_BLOBS, &makeViz<MyCustomBlobs>);
    void setVizFactory(AnimartrixAnim anim, AnimartrixVizFactory factory) {
        // Search for existing override
        for (auto &ovr : mFactoryOverrides) {
            if (ovr.first == anim) {
                ovr.second = factory;
                // Force recreation on next draw
                mViz.reset();
                return;
            }
        }
        if (factory) {
            mFactoryOverrides.push_back(fl::make_pair(anim, factory));
            mViz.reset();
        }
    }

    /// Set a fully custom viz that bypasses the animation enum entirely.
    /// When set, this viz is used for all draw() calls regardless of which
    /// animation enum is selected. Pass nullptr to revert to enum-based dispatch.
    /// This is intended for external/user-provided animartrix implementations.
    void setCustomViz(IAnimartrix2Viz *viz) {
        mCustomViz.reset(viz);
        mViz.reset();
    }

    static fl::vector<fl::pair<int, fl::string>> getAnimationList() {
        fl::vector<fl::pair<int, fl::string>> list;
        for (int i = 0; i < static_cast<int>(AnimartrixAnim::NUM_ANIMATIONS); i++) {
            AnimartrixAnimInfo info = getAnimartrixInfo(i);
            list.push_back(fl::make_pair(info.id, fl::string(info.name)));
        }
        return list;
    }

  private:
    // Create a viz instance for the given animation, checking overrides first.
    IAnimartrix2Viz *createViz(AnimartrixAnim anim) {
        // Custom viz takes precedence over everything
        if (mCustomViz) {
            // Return a non-owning "alias" — mCustomViz owns the lifetime.
            // We return nullptr here; draw() will use mCustomViz directly.
            return nullptr;
        }
        // Check per-animation factory overrides
        for (const auto &ovr : mFactoryOverrides) {
            if (ovr.first == anim && ovr.second) {
                return ovr.second();
            }
        }
        // Default: look up in ANIMATION_TABLE
        for (const auto &entry : ANIMATION_TABLE) {
            if (entry.anim == anim) {
                return entry.factory();
            }
        }
        return nullptr;
    }

    AnimartrixAnim mPrevAnimation = AnimartrixAnim::NUM_ANIMATIONS;
    AnimartrixAnim mCurrentAnimation = AnimartrixAnim::RGB_BLOBS5;
    EOrder mColorOrder = EOrder::RGB;
    Context mCtx;
    fl::unique_ptr<IAnimartrix2Viz> mViz;
    fl::unique_ptr<IAnimartrix2Viz> mCustomViz; // External viz, owns lifetime
    fl::vector<fl::pair<AnimartrixAnim, AnimartrixVizFactory>> mFactoryOverrides;
};

} // namespace fl
