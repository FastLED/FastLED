#pragma once


/*
  ___        _            ___  ______ _____    _
 / _ \      (_)          / _ \ | ___ \_   _|  (_)
/ /_\ \_ __  _ _ __ ___ / /_\ \| |_/ / | |_ __ ___  __
|  _  | '_ \| | '_ ` _ \|  _  ||    /  | | '__| \ \/ /
| | | | | | | | | | | | | | | || |\ \  | | |  | |>  <
\_| |_/_| |_|_|_| |_| |_\_| |_/\_| \_| \_/_|  |_/_/\_\

by Stefan Petrick 2023.

High quality LED animations for your next project.

This is a Shader and 5D Coordinate Mapper made for realtime
rendering of generative animations & artistic dynamic visuals.

This is also a modular animation synthesizer with waveform
generators, oscillators, filters, modulators, noise generators,
compressors... and much more.

VO.42 beta version

This code is licenced under a Creative Commons Attribution
License CC BY-NC 3.0


*/
#include "crgb.h"
#include "namespace.h"

#define ANIMARTRIX_INTERNAL
#include "animartrix_detail.hpp"

FASTLED_NAMESPACE_BEGIN

class Fx2d {
  public:
    virtual ~Fx2d() {}
    virtual void lazyInit() {}
    virtual void draw() = 0;
    virtual uint16_t xy(uint16_t x, uint16_t y) = 0;

    virtual const char* fxName() const = 0;  // Get the name of the current fx. This is the class name if there is only one.

    // Optionally implement these for multi fx classes.
    virtual int fxNum() const { return 1; };  // Return 1 if you only have one fx managed by this class.
    virtual void fxSet(int fx) {};  // Get the current fx number.
    virtual void fxNext(int fx = 1) {};  // Negative numbers are allowed. -1 means previous fx.
    virtual int fxGet() const { return 0; };  // Set the current fx number.
};


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
class AnimartrixData : public Fx2d {
  public:
    AnimartrixData(int x, int y, CRGB *leds, AnimartrixAnim first_animation,
                   bool serpentine) {
        // Note: Swapping out height and width.
        this->x = y;
        this->y = x;
        this->leds = leds;
        this->serpentine = serpentine;
        this->current_animation = first_animation;
    }

    void lazyInit() override {}

    uint16_t xy(uint16_t x, uint16_t y) override {
        return xy_no_virtual(x, y);
    }

    uint16_t xy_no_virtual(uint16_t x, uint16_t y) {
        if (serpentine && y & 1) // check last bit
            return (y + 1) * this->x - 1 -
                   x; // reverse every second line for a serpentine lled layout
        else
            return y * this->x +
                   x; // use this equation only for a line by line layout
    }

    void draw() override;

    int fxNum() const override { return NUM_ANIMATIONS; }

    void fxSet(int fx) override {
        int curr = fxGet();
        if (fx < 0) {
            fx = curr + fx;
            if (fx < 0) {
                fx = NUM_ANIMATIONS - 1;
            } else {
                fx = fx % NUM_ANIMATIONS;
            }
        }
        current_animation = static_cast<AnimartrixAnim>(fx);
    }
    int fxGet() const override { return static_cast<int>(current_animation); }
    const char* fxName() const override { return getAnimationName(current_animation); }

    void fxNext(int fx = 1) override {
        fxSet(fxGet() + fx);
    }


  private:
    friend void AnimartrixLoop(AnimartrixData &self);
    friend class FastLEDANIMartRIX;
    static const char *getAnimationName(AnimartrixAnim animation);
    AnimartrixAnim prev_animation = NUM_ANIMATIONS;
    FastLEDANIMartRIX *impl = nullptr;
    int x = 0;
    int y = 0;
    bool serpentine = true;
    bool destroy = false;
    CRGB *leds = nullptr;
    AnimartrixAnim current_animation = RGB_BLOBS5;
};

void AnimartrixLoop(AnimartrixData &self);

/// ##################################################
/// Details with the implementation of Animartrix


const char *AnimartrixData::getAnimationName(AnimartrixAnim animation) {
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
    default:
        return "UNKNOWN";
    }
}

class FastLEDANIMartRIX : public animartrix::ANIMartRIX {
    AnimartrixData *data = nullptr;

  public:
    FastLEDANIMartRIX(AnimartrixData *_data) {
        this->data = _data;
        this->init(data->x, data->y, data->serpentine);
    }

    void setPixelColor(int x, int y, CRGB pixel) {
        data->leds[xy(x, y)] = pixel;
    }
    void setPixelColorInternal(int x, int y, animartrix::rgb pixel) {
        setPixelColor(x, y, CRGB(pixel.red, pixel.green, pixel.blue));
    }

    uint16_t xy(uint16_t x, uint16_t y) override {
        return data->xy_no_virtual(x, y);
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
        }
    }
};



void AnimartrixLoop(AnimartrixData &self) {
    if (self.destroy) {
        if (self.impl) {
            delete self.impl;
            self.impl = nullptr;
        }
        self.destroy = false;
        return;
    }

    if (self.prev_animation != self.current_animation) {
        if (self.impl) {
            // Re-initialize object.
            self.impl->init(self.x, self.y, self.serpentine);
        }
        self.prev_animation = self.current_animation;
    }
    if (self.impl == nullptr) {
        self.impl = new FastLEDANIMartRIX(&self);
    }
    self.impl->loop();
}

void AnimartrixData::draw() {
    AnimartrixLoop(*this); 
}

FASTLED_NAMESPACE_END
