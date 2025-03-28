#pragma once

// FastLED Adapter for the animartrix fx library.
// Copyright Stefen Petrick 2023.
// Adapted to C++ by Netmindz 2023.
// Adapted to FastLED by Zach Vorhies 2024.
// For details on the animartrix library and licensing information, see
// fx/aninamtrix_detail.hpp


#include "crgb.h"
#include "fx/fx2d.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/scoped_ptr.h"
#include "fl/dbg.h"
#include "fl/xymap.h"

#define ANIMARTRIX_INTERNAL
#include "animartrix_detail.hpp"

namespace fl {

FASTLED_SMART_PTR(Animartrix);

enum AnimartrixAnim {
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
    NUM_ANIMATIONS
};



class FastLEDANIMartRIX;
class Animartrix : public Fx2d {
  public:
    Animartrix(XYMap xyMap, AnimartrixAnim which_animation) : Fx2d(xyMap) {
        // Note: Swapping out height and width.
        this->current_animation = which_animation;
        mXyMap.convertToLookUpTable();
    }

    Animartrix(const Animartrix &) = delete;
    void draw(DrawContext context) override;
    int fxNum() const { return NUM_ANIMATIONS; }
    void fxSet(int fx);
    int fxGet() const { return static_cast<int>(current_animation); }
    Str fxName() const override {
        return "Animartrix:";
    }
    void fxNext(int fx = 1) { fxSet(fxGet() + fx); }

  private:
    friend void AnimartrixLoop(Animartrix &self, uint32_t now);
    friend class FastLEDANIMartRIX;
    static const char *getAnimationName(AnimartrixAnim animation);
    AnimartrixAnim prev_animation = NUM_ANIMATIONS;
    fl::scoped_ptr<FastLEDANIMartRIX> impl;
    CRGB *leds = nullptr; // Only set during draw, then unset back to nullptr.
    AnimartrixAnim current_animation = RGB_BLOBS5;
};

void AnimartrixLoop(Animartrix &self, uint32_t now);

/// ##################################################
/// Details with the implementation of Animartrix


struct AnimartrixEntry {
    AnimartrixAnim anim;
    const char* name;
    void (FastLEDANIMartRIX::*func)();
};



class FastLEDANIMartRIX : public animartrix_detail::ANIMartRIX {
    Animartrix *data = nullptr;

  public:
    FastLEDANIMartRIX(Animartrix *_data) {
        this->data = _data;
        this->init(data->getWidth(), data->getWidth());
    }

    void setPixelColor(int x, int y, CRGB pixel) {
        data->leds[xyMap(x, y)] = pixel;
    }
    void setPixelColorInternal(int x, int y, animartrix_detail::rgb pixel) override {
        setPixelColor(x, y, CRGB(pixel.red, pixel.green, pixel.blue));
    }

    uint16_t xyMap(uint16_t x, uint16_t y) override {
        return data->xyMap(x, y);
    }

    void loop();
};

void Animartrix::fxSet(int fx) {
    int curr = fxGet();
    if (fx < 0) {
        fx = curr + fx;
        if (fx < 0) {
            fx = NUM_ANIMATIONS - 1;
        }
    }
    fx = fx % NUM_ANIMATIONS;
    current_animation = static_cast<AnimartrixAnim>(fx);
    FASTLED_DBG("Setting animation to " << getAnimationName(current_animation));
}

void AnimartrixLoop(Animartrix &self, uint32_t now) {
    if (self.prev_animation != self.current_animation) {
        if (self.impl) {
            // Re-initialize object.
            self.impl->init(self.getWidth(), self.getHeight());
        }
        self.prev_animation = self.current_animation;
    }
    if (!self.impl) {
        self.impl.reset(new FastLEDANIMartRIX(&self));
    }
    self.impl->setTime(now);
    self.impl->loop();
}

static const AnimartrixEntry ANIMATION_TABLE[] = {
    {RGB_BLOBS5, "RGB_BLOBS5", &FastLEDANIMartRIX::RGB_Blobs5},
    {RGB_BLOBS4, "RGB_BLOBS4", &FastLEDANIMartRIX::RGB_Blobs4},
    {RGB_BLOBS3, "RGB_BLOBS3", &FastLEDANIMartRIX::RGB_Blobs3},
    {RGB_BLOBS2, "RGB_BLOBS2", &FastLEDANIMartRIX::RGB_Blobs2},
    {RGB_BLOBS, "RGB_BLOBS", &FastLEDANIMartRIX::RGB_Blobs},
    {POLAR_WAVES, "POLAR_WAVES", &FastLEDANIMartRIX::Polar_Waves},
    {SLOW_FADE, "SLOW_FADE", &FastLEDANIMartRIX::Slow_Fade},
    {ZOOM2, "ZOOM2", &FastLEDANIMartRIX::Zoom2},
    {ZOOM, "ZOOM", &FastLEDANIMartRIX::Zoom},
    {HOT_BLOB, "HOT_BLOB", &FastLEDANIMartRIX::Hot_Blob},
    {SPIRALUS2, "SPIRALUS2", &FastLEDANIMartRIX::Spiralus2},
    {SPIRALUS, "SPIRALUS", &FastLEDANIMartRIX::Spiralus},
    {YVES, "YVES", &FastLEDANIMartRIX::Yves},
    {SCALEDEMO1, "SCALEDEMO1", &FastLEDANIMartRIX::Scaledemo1},
    {LAVA1, "LAVA1", &FastLEDANIMartRIX::Lava1},
    {CALEIDO3, "CALEIDO3", &FastLEDANIMartRIX::Caleido3},
    {CALEIDO2, "CALEIDO2", &FastLEDANIMartRIX::Caleido2},
    {CALEIDO1, "CALEIDO1", &FastLEDANIMartRIX::Caleido1},
    {DISTANCE_EXPERIMENT, "DISTANCE_EXPERIMENT", &FastLEDANIMartRIX::Distance_Experiment},
    {CENTER_FIELD, "CENTER_FIELD", &FastLEDANIMartRIX::Center_Field},
    {WAVES, "WAVES", &FastLEDANIMartRIX::Waves},
    {CHASING_SPIRALS, "CHASING_SPIRALS", &FastLEDANIMartRIX::Chasing_Spirals},
    {ROTATING_BLOB, "ROTATING_BLOB", &FastLEDANIMartRIX::Rotating_Blob},
    {RINGS, "RINGS", &FastLEDANIMartRIX::Rings},
    {COMPLEX_KALEIDO, "COMPLEX_KALEIDO", &FastLEDANIMartRIX::Complex_Kaleido},
    {COMPLEX_KALEIDO_2, "COMPLEX_KALEIDO_2", &FastLEDANIMartRIX::Complex_Kaleido_2},
    {COMPLEX_KALEIDO_3, "COMPLEX_KALEIDO_3", &FastLEDANIMartRIX::Complex_Kaleido_3},
    {COMPLEX_KALEIDO_4, "COMPLEX_KALEIDO_4", &FastLEDANIMartRIX::Complex_Kaleido_4},
    {COMPLEX_KALEIDO_5, "COMPLEX_KALEIDO_5", &FastLEDANIMartRIX::Complex_Kaleido_5},
    {COMPLEX_KALEIDO_6, "COMPLEX_KALEIDO_6", &FastLEDANIMartRIX::Complex_Kaleido_6},
    {WATER, "WATER", &FastLEDANIMartRIX::Water},
    {PARAMETRIC_WATER, "PARAMETRIC_WATER", &FastLEDANIMartRIX::Parametric_Water},
    {MODULE_EXPERIMENT1, "MODULE_EXPERIMENT1", &FastLEDANIMartRIX::Module_Experiment1},
    {MODULE_EXPERIMENT2, "MODULE_EXPERIMENT2", &FastLEDANIMartRIX::Module_Experiment2},
    {MODULE_EXPERIMENT3, "MODULE_EXPERIMENT3", &FastLEDANIMartRIX::Module_Experiment3},
    {MODULE_EXPERIMENT4, "MODULE_EXPERIMENT4", &FastLEDANIMartRIX::Module_Experiment4},
    {MODULE_EXPERIMENT5, "MODULE_EXPERIMENT5", &FastLEDANIMartRIX::Module_Experiment5},
    {MODULE_EXPERIMENT6, "MODULE_EXPERIMENT6", &FastLEDANIMartRIX::Module_Experiment6},
    {MODULE_EXPERIMENT7, "MODULE_EXPERIMENT7", &FastLEDANIMartRIX::Module_Experiment7},
    {MODULE_EXPERIMENT8, "MODULE_EXPERIMENT8", &FastLEDANIMartRIX::Module_Experiment8},
    {MODULE_EXPERIMENT9, "MODULE_EXPERIMENT9", &FastLEDANIMartRIX::Module_Experiment9},
    {MODULE_EXPERIMENT10, "MODULE_EXPERIMENT10", &FastLEDANIMartRIX::Module_Experiment10},
    {MODULE_EXPERIMENT_SM1, "MODULE_EXPERIMENT_SM1", &FastLEDANIMartRIX::SM1},
    {MODULE_EXPERIMENT_SM2, "MODULE_EXPERIMENT_SM2", &FastLEDANIMartRIX::SM2},
    {MODULE_EXPERIMENT_SM3, "MODULE_EXPERIMENT_SM3", &FastLEDANIMartRIX::SM3},
    {MODULE_EXPERIMENT_SM4, "MODULE_EXPERIMENT_SM4", &FastLEDANIMartRIX::SM4},
    {MODULE_EXPERIMENT_SM5, "MODULE_EXPERIMENT_SM5", &FastLEDANIMartRIX::SM5},
    {MODULE_EXPERIMENT_SM6, "MODULE_EXPERIMENT_SM6", &FastLEDANIMartRIX::SM6},
    {MODULE_EXPERIMENT_SM8, "MODULE_EXPERIMENT_SM8", &FastLEDANIMartRIX::SM8},
    {MODULE_EXPERIMENT_SM9, "MODULE_EXPERIMENT_SM9", &FastLEDANIMartRIX::SM9},
    {MODULE_EXPERIMENT_SM10, "MODULE_EXPERIMENT_SM10", &FastLEDANIMartRIX::SM10},
};

void FastLEDANIMartRIX::loop() {
    for (const auto& entry : ANIMATION_TABLE) {
        if (entry.anim == data->current_animation) {
            (this->*entry.func)();
            return;
        }
    }
    // (this->*ANIMATION_TABLE[index].func)();
    FASTLED_DBG("Animation not found for " << int(data->current_animation));
}

const char* Animartrix::getAnimationName(AnimartrixAnim animation) {
    for (const auto& entry : ANIMATION_TABLE) {
        if (entry.anim == animation) {
            return entry.name;
        }
    }
    FASTLED_DBG("Animation not found for " << int(animation));
    return "UNKNOWN";
}

void Animartrix::draw(DrawContext ctx) {
    this->leds = ctx.leds;
    AnimartrixLoop(*this, ctx.now);
    this->leds = nullptr;
}

}  // namespace fl
