#pragma once

#include "FastLED.h"

// TwinkleFox effect parameters
#define TWINKLE_SPEED 4
#define TWINKLE_DENSITY 5
#define SECONDS_PER_PALETTE 30
#define AUTO_SELECT_BACKGROUND_COLOR 0
#define COOL_LIKE_INCANDESCENT 1

struct TwinkleFoxData {
    CRGB* leds;
    uint16_t num_leds;
    CRGBPalette16 currentPalette;
    CRGBPalette16 targetPalette;
    CRGB backgroundColor;
    uint8_t twinkleSpeed;
    uint8_t twinkleDensity;
    bool coolLikeIncandescent;
    bool autoSelectBackgroundColor;
    
    TwinkleFoxData(CRGB* leds, uint16_t num_leds)
        : leds(leds), num_leds(num_leds), backgroundColor(CRGB::Black),
          twinkleSpeed(TWINKLE_SPEED), twinkleDensity(TWINKLE_DENSITY),
          coolLikeIncandescent(COOL_LIKE_INCANDESCENT), autoSelectBackgroundColor(AUTO_SELECT_BACKGROUND_COLOR) {}
};

// Function declarations
void chooseNextColorPalette(CRGBPalette16& pal);
CRGB computeOneTwinkle(TwinkleFoxData& self, uint32_t ms, uint8_t salt);
uint8_t attackDecayWave8(uint8_t i);
void coolLikeIncandescent(CRGB& c, uint8_t phase);

void TwinkleFoxLoop(TwinkleFoxData& self) {
    uint16_t PRNG16 = 11337;
    uint32_t clock32 = millis();

    CRGB bg = self.backgroundColor;
    if (self.autoSelectBackgroundColor && self.currentPalette[0] == self.currentPalette[1]) {
        bg = self.currentPalette[0];
        uint8_t bglight = bg.getAverageLight();
        if (bglight > 64) {
            bg.nscale8_video(16);
        } else if (bglight > 16) {
            bg.nscale8_video(64);
        } else {
            bg.nscale8_video(86);
        }
    }

    uint8_t backgroundBrightness = bg.getAverageLight();

    for (uint16_t i = 0; i < self.num_leds; i++) {
        PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384;
        uint16_t myclockoffset16 = PRNG16;
        PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384;
        uint8_t myspeedmultiplierQ5_3 = ((((PRNG16 & 0xFF) >> 4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
        uint32_t myclock30 = (uint32_t)((clock32 * myspeedmultiplierQ5_3) >> 3) + myclockoffset16;
        uint8_t myunique8 = PRNG16 >> 8;

        CRGB c = computeOneTwinkle(self, myclock30, myunique8);

        uint8_t cbright = c.getAverageLight();
        int16_t deltabright = cbright - backgroundBrightness;
        if (deltabright >= 32 || (!bg)) {
            self.leds[i] = c;
        } else if (deltabright > 0) {
            self.leds[i] = blend(bg, c, deltabright * 8);
        } else {
            self.leds[i] = bg;
        }
    }
}

CRGB computeOneTwinkle(TwinkleFoxData& self, uint32_t ms, uint8_t salt) {
    uint16_t ticks = ms >> (8 - self.twinkleSpeed);
    uint8_t fastcycle8 = ticks;
    uint16_t slowcycle16 = (ticks >> 8) + salt;
    slowcycle16 += sin8(slowcycle16);
    slowcycle16 = (slowcycle16 * 2053) + 1384;
    uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);
    
    uint8_t bright = 0;
    if (((slowcycle8 & 0x0E) / 2) < self.twinkleDensity) {
        bright = attackDecayWave8(fastcycle8);
    }

    uint8_t hue = slowcycle8 - salt;
    CRGB c;
    if (bright > 0) {
        c = ColorFromPalette(self.currentPalette, hue, bright, NOBLEND);
        if (self.coolLikeIncandescent) {
            coolLikeIncandescent(c, fastcycle8);
        }
    } else {
        c = CRGB::Black;
    }
    return c;
}

uint8_t attackDecayWave8(uint8_t i) {
    if (i < 86) {
        return i * 3;
    } else {
        i -= 86;
        return 255 - (i + (i / 2));
    }
}

void coolLikeIncandescent(CRGB& c, uint8_t phase) {
    if (phase < 128) return;

    uint8_t cooling = (phase - 128) >> 4;
    c.g = qsub8(c.g, cooling);
    c.b = qsub8(c.b, cooling * 2);
}

// Color palettes
// Color palette definitions
const TProgmemRGBPalette16 RedGreenWhite_p FL_PROGMEM = {
    CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, 
    CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, 
    CRGB::Red, CRGB::Red, CRGB::Gray, CRGB::Gray, 
    CRGB::Green, CRGB::Green, CRGB::Green, CRGB::Green
};

const TProgmemRGBPalette16 Holly_p FL_PROGMEM = {
    0x00580c, 0x00580c, 0x00580c, 0x00580c, 
    0x00580c, 0x00580c, 0x00580c, 0x00580c, 
    0x00580c, 0x00580c, 0x00580c, 0x00580c, 
    0x00580c, 0x00580c, 0x00580c, 0xB00402
};

const TProgmemRGBPalette16 RedWhite_p FL_PROGMEM = {
    CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, 
    CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray,
    CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, 
    CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray
};

const TProgmemRGBPalette16 BlueWhite_p FL_PROGMEM = {
    CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
    CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
    CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
    CRGB::Blue, CRGB::Gray, CRGB::Gray, CRGB::Gray
};

const TProgmemRGBPalette16 FairyLight_p FL_PROGMEM = {
    CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, 
    CRGB(CRGB::FairyLight).nscale8(uint8_t(128)).as_uint32_t(), CRGB(CRGB::FairyLight).nscale8(uint8_t(128)).as_uint32_t(), CRGB::FairyLight, CRGB::FairyLight, 
    CRGB(CRGB::FairyLight).nscale8(64).as_uint32_t(), CRGB(CRGB::FairyLight).nscale8(64).as_uint32_t(), CRGB::FairyLight, CRGB::FairyLight, 
    CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight
};

const TProgmemRGBPalette16 Snow_p FL_PROGMEM = {
    0x304048, 0x304048, 0x304048, 0x304048,
    0x304048, 0x304048, 0x304048, 0x304048,
    0x304048, 0x304048, 0x304048, 0x304048,
    0x304048, 0x304048, 0x304048, 0xE0F0FF
};

const TProgmemRGBPalette16 RetroC9_p FL_PROGMEM = {
    0xB80400, 0x902C02, 0xB80400, 0x902C02,
    0x902C02, 0xB80400, 0x902C02, 0xB80400,
    0x046002, 0x046002, 0x046002, 0x046002,
    0x070758, 0x070758, 0x070758, 0x606820
};

const TProgmemRGBPalette16 Ice_p FL_PROGMEM = {
    0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040,
    0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040,
    0x0C1040, 0x0C1040, 0x0C1040, 0x0C1040,
    0x182080, 0x182080, 0x182080, 0x5080C0
};

// Add or remove palette names from this list to control which color
// palettes are used, and in what order.
const TProgmemRGBPalette16* ActivePaletteList[] = {
    &RetroC9_p,
    &BlueWhite_p,
    &RainbowColors_p,
    &FairyLight_p,
    &RedGreenWhite_p,
    &PartyColors_p,
    &RedWhite_p,
    &Snow_p,
    &Holly_p,
    &Ice_p  
};

void chooseNextColorPalette(CRGBPalette16& pal) {
    const uint8_t numberOfPalettes = sizeof(ActivePaletteList) / sizeof(ActivePaletteList[0]);
    static uint8_t whichPalette = -1; 
    whichPalette = addmod8(whichPalette, 1, numberOfPalettes);
    pal = *(ActivePaletteList[whichPalette]);
}
