#pragma once

// FastLED Adapter for the animartrix fx library.
// Copyright Stefen Petrick 2023.
// Adapted to C++ by Netmindz 2023.
// Adapted to FastLED by Zach Vorhies 2024.
// For details on the animartrix library and licensing information, see
// fx/aninamtrix_detail.hpp

#include <iostream> // ok include

#include "crgb.h"
#include "fx/fx2d.h"
#include "namespace.h"
#include "ref.h"
#include "xymap.h"

#define ANIMARTRIX_INTERNAL
#include "animartrix_detail.hpp"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(Animartrix);

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
    NUM_ANIMATIONS
};

class FastLEDANIMartRIX;
class Animartrix : public FxGrid {
  public:
    Animartrix(XYMap xyMap, AnimartrixAnim which_animation) : FxGrid(xyMap) {
        // Note: Swapping out height and width.
        this->current_animation = which_animation;
    }

    Animartrix(const Animartrix &) = delete;
    void lazyInit() override { this->mXyMap.convertToLookUpTable(); }
    void draw(DrawContext context) override;
    int fxNum() const override { return NUM_ANIMATIONS; }
    void fxSet(int fx) override;
    int fxGet() const override { return static_cast<int>(current_animation); }
    const char *fxName(int which) const override {
        return getAnimationName(which != -1 ? static_cast<AnimartrixAnim>(which) : current_animation);
    }
    void fxNext(int fx = 1) override { fxSet(fxGet() + fx); }

  private:
    friend void AnimartrixLoop(Animartrix &self, uint32_t now);
    friend class FastLEDANIMartRIX;
    static const char *getAnimationName(AnimartrixAnim animation);
    AnimartrixAnim prev_animation = NUM_ANIMATIONS;
    scoped_ptr<FastLEDANIMartRIX> impl;
    CRGB *leds = nullptr; // Only set during draw, then unset back to nullptr.
    AnimartrixAnim current_animation = RGB_BLOBS5;
};

void AnimartrixLoop(Animartrix &self, uint32_t now);

/// ##################################################
/// Details with the implementation of Animartrix

const char *Animartrix::getAnimationName(AnimartrixAnim animation) {
    switch (animation) {
    case RGB_BLOBS5:
        return "RGB_BLOBS5";
    case RGB_BLOBS4:
        return "RGB_BLOBS4";
    case RGB_BLOBS3:
        return "RGB_BLOBS3";
    case RGB_BLOBS2:
        return "RGB_BLOBS2";
    case RGB_BLOBS:
        return "RGB_BLOBS";
    case POLAR_WAVES:
        return "POLAR_WAVES";
    case SLOW_FADE:
        return "SLOW_FADE";
    case ZOOM2:
        return "ZOOM2";
    case ZOOM:
        return "ZOOM";
    case HOT_BLOB:
        return "HOT_BLOB";
    case SPIRALUS2:
        return "SPIRALUS2";
    case SPIRALUS:
        return "SPIRALUS";
    case YVES:
        return "YVES";
    case SCALEDEMO1:
        return "SCALEDEMO1";
    case LAVA1:
        return "LAVA1";
    case CALEIDO3:
        return "CALEIDO3";
    case CALEIDO2:
        return "CALEIDO2";
    case CALEIDO1:
        return "CALEIDO1";
    case DISTANCE_EXPERIMENT:
        return "DISTANCE_EXPERIMENT";
    case CENTER_FIELD:
        return "CENTER_FIELD";
    case WAVES:
        return "WAVES";
    case CHASING_SPIRALS:
        return "CHASING_SPIRALS";
    case ROTATING_BLOB:
        return "ROTATING_BLOB";
    case RINGS:
        return "RINGS";
    case COMPLEX_KALEIDO:
        return "COMPLEX_KALEIDO";
    case COMPLEX_KALEIDO_2:
        return "COMPLEX_KALEIDO_2";
    case COMPLEX_KALEIDO_3:
        return "COMPLEX_KALEIDO_3";
    case COMPLEX_KALEIDO_4:
        return "COMPLEX_KALEIDO_4";
    case COMPLEX_KALEIDO_5:
        return "COMPLEX_KALEIDO_5";
    case COMPLEX_KALEIDO_6:
        return "COMPLEX_KALEIDO_6";
    case WATER:
        return "WATER";
    case PARAMETRIC_WATER:
        return "PARAMETRIC_WATER";
    case MODULE_EXPERIMENT1:
        return "MODULE_EXPERIMENT1";
    case MODULE_EXPERIMENT2:
        return "MODULE_EXPERIMENT2";
    case MODULE_EXPERIMENT3:
        return "MODULE_EXPERIMENT3";
    case MODULE_EXPERIMENT4:
        return "MODULE_EXPERIMENT4";
    case MODULE_EXPERIMENT5:
        return "MODULE_EXPERIMENT5";
    case MODULE_EXPERIMENT6:
        return "MODULE_EXPERIMENT6";
    case MODULE_EXPERIMENT7:
        return "MODULE_EXPERIMENT7";
    case MODULE_EXPERIMENT8:
        return "MODULE_EXPERIMENT8";
    case MODULE_EXPERIMENT9:
        return "MODULE_EXPERIMENT9";
    case MODULE_EXPERIMENT10:
        return "MODULE_EXPERIMENT10";
    case NUM_ANIMATIONS:
        return "NUM_ANIMATIONS";
    default:
        return "UNKNOWN";
    }
}

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

    void loop() {
        switch (data->current_animation) {
        case RGB_BLOBS5:
            RGB_Blobs5();
            break;
        case RGB_BLOBS4:
            RGB_Blobs4();
            break;
        case RGB_BLOBS3:
            RGB_Blobs3();
            break;
        case RGB_BLOBS2:
            RGB_Blobs2();
            break;
        case RGB_BLOBS:
            RGB_Blobs();
            break;
        case POLAR_WAVES:
            Polar_Waves();
            break;
        case SLOW_FADE:
            Slow_Fade();
            break;
        case ZOOM2:
            Zoom2();
            break;
        case ZOOM:
            Zoom();
            break;
        case HOT_BLOB:
            Hot_Blob();
            break;
        case SPIRALUS2:
            Spiralus2();
            break;
        case SPIRALUS:
            Spiralus();
            break;
        case YVES:
            Yves();
            break;
        case SCALEDEMO1:
            Scaledemo1();
            break;
        case LAVA1:
            Lava1();
            break;
        case CALEIDO3:
            Caleido3();
            break;
        case CALEIDO2:
            Caleido2();
            break;
        case CALEIDO1:
            Caleido1();
            break;
        case DISTANCE_EXPERIMENT:
            Distance_Experiment();
            break;
        case CENTER_FIELD:
            Center_Field();
            break;
        case WAVES:
            Waves();
            break;
        case CHASING_SPIRALS:
            Chasing_Spirals();
            break;
        case ROTATING_BLOB:
            Rotating_Blob();
            break;
        case RINGS:
            Rings();
            break;
        case COMPLEX_KALEIDO:
            Complex_Kaleido();
            break;
        case COMPLEX_KALEIDO_2:
            Complex_Kaleido_2();
            break;
        case COMPLEX_KALEIDO_3:
            Complex_Kaleido_3();
            break;
        case COMPLEX_KALEIDO_4:
            Complex_Kaleido_4();
            break;
        case COMPLEX_KALEIDO_5:
            Complex_Kaleido_5();
            break;
        case COMPLEX_KALEIDO_6:
            Complex_Kaleido_6();
            break;
        case WATER:
            Water();
            break;
        case PARAMETRIC_WATER:
            Parametric_Water();
            break;
        case MODULE_EXPERIMENT1:
            Module_Experiment1();
            break;
        case MODULE_EXPERIMENT2:
            Module_Experiment2();
            break;
        case MODULE_EXPERIMENT3:
            Module_Experiment3();
            break;
        case MODULE_EXPERIMENT4:
            Module_Experiment4();
            break;
        case MODULE_EXPERIMENT5:
            Module_Experiment5();
            break;
        case MODULE_EXPERIMENT6:
            Module_Experiment6();
            break;
        case MODULE_EXPERIMENT7:
            Module_Experiment7();
            break;
        case MODULE_EXPERIMENT8:
            Module_Experiment8();
            break;
        case MODULE_EXPERIMENT9:
            Module_Experiment9();
            break;
        case MODULE_EXPERIMENT10:
            Module_Experiment10();
            break;
        case NUM_ANIMATIONS:
            break;
        }
    }
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

void Animartrix::draw(DrawContext ctx) {
    this->leds = ctx.leds;
    AnimartrixLoop(*this, ctx.now);
    this->leds = nullptr;
}

FASTLED_NAMESPACE_END
