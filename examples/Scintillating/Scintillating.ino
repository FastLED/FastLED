// originally from:
// https://github.com/sutaburosu/scintillating_heatshrink/

#include <Arduino.h>
#include <FastLED.h>
#include <stdint.h>
#include <avr/pgmspace.h>
#include "heatshrink_config.h"
#include "heatshrink_decoder.h"

enum XY_matrix_config { SERPENTINE = 1, ROWMAJOR = 2, FLIPMAJOR = 4, FLIPMINOR = 8 };

// MAX_MILLIWATTS can only be changed at compile-time. Use 0 to disable limit.
// Brightness can be changed at runtime via serial with 'b' and 'B'
#define MAX_MILLIWATTS 0
#define BRIGHTNESS     255
#define LED_PIN        2
#define COLOR_ORDER    GRB
#define CHIPSET        WS2812B
#define kMatrixWidth   16
#define kMatrixHeight  16
#define XY_MATRIX      (ROWMAJOR)
#define NUM_LEDS       ((kMatrixWidth) * (kMatrixHeight))

#define SERIAL_UI      1    // if 1, can be controlled via keypresses in PuTTY
#define FADEIN_FRAMES  32   // how long to reach full brightness when powered on
#define MS_GOAL        20   // to try maintain 1000 / 20ms == 50 frames per second

#define GIF2H_MAX_PALETTE_ENTRIES 140 // RAM is always tight on ATmega328...
#define GIF2H_NUM_PIXELS NUM_LEDS     // all images must be the same size as the matrix

CRGB leds[NUM_LEDS];
CRGBPalette16 currentPalette;

uint16_t XY(uint8_t x, uint8_t y) {
  uint8_t major, minor, sz_major, sz_minor;
  if (x >= kMatrixWidth || y >= kMatrixHeight)
    return NUM_LEDS;
  if (XY_MATRIX & ROWMAJOR)
    major = x, minor = y, sz_major = kMatrixWidth,  sz_minor = kMatrixHeight;
  else
    major = y, minor = x, sz_major = kMatrixHeight, sz_minor = kMatrixWidth;
  if (XY_MATRIX & FLIPMINOR)
    minor = sz_minor - 1 - minor;
  if ((XY_MATRIX & FLIPMAJOR) ^ (minor & 1 && (XY_MATRIX & SERPENTINE)))
    major = sz_major - 1 - major;
  return (uint16_t) minor * sz_major + major;
}


#if FASTLED_USE_PROGMEM == 1
#define FL_PGM_READ_PTR_NEAR(x) (pgm_read_ptr_near(x))
#else
#if !defined(FL_PGM_READ_PTR_NEAR)
#define FL_PGM_READ_PTR_NEAR(addr) ({typeof(addr) _addr = (addr); *(void * const *)(_addr); })
#endif
#if !defined(memcpy_P)
#define memcpy_P(dest, src, n) memcpy(dest, src, n)
#endif
#endif


// Stuff from FastLED pull requests and gists that will hopefully be merged one day

// Blend one CRGB color toward another CRGB color by a given amount.
// Blending is linear, and done in the RGB color space.
// These functions modify 'cur' in place.
// https://gist.github.com/kriegsman/d0a5ed3c8f38c64adcb4837dafb6e690
CRGB fadeTowardColour(CRGB& cur, const CRGB& target, uint8_t amount) {
  nblendU8TowardU8(cur.red,   target.red,   amount);
  nblendU8TowardU8(cur.green, target.green, amount);
  nblendU8TowardU8(cur.blue,  target.blue,  amount);
  return cur;
}
CRGB fadeTowardColour_video(CRGB& cur, const CRGB& target, uint8_t amount) {
  nblendU8TowardU8_video(cur.red,   target.red,   amount);
  nblendU8TowardU8_video(cur.green, target.green, amount);
  nblendU8TowardU8_video(cur.blue,  target.blue,  amount);
  return cur;
}

// Helper function that blends one uint8_t toward another by a given amount
void nblendU8TowardU8(uint8_t& cur, const uint8_t target, uint8_t amount) {
  if (cur == target) return;

  if (cur < target ) {
    uint8_t delta = target - cur;
    delta = scale8(delta, amount);
    cur += delta;
  } else {
    uint8_t delta = cur - target;
    delta = scale8(delta, amount);
    cur -= delta;
  }
}
void nblendU8TowardU8_video(uint8_t& cur, const uint8_t target, uint8_t amount) {
  if (cur == target) return;

  if (cur < target ) {
    uint8_t delta = target - cur;
    delta = scale8_video(delta, amount);
    cur += delta;
  } else {
    uint8_t delta = cur - target;
    delta = scale8_video(delta, amount);
    cur -= delta;
  }
}

// FastLED uses 4-bit interpolation.  8-bit looks far less janky.
// https://github.com/FastLED/FastLED/pull/202
CRGB ColorFromPaletteExtended(const CRGBPalette16& pal, uint16_t index, uint8_t brightness, TBlendType blendType) {
  // Extract the four most significant bits of the index as a palette index.
  uint8_t index_4bit = (index >> 12);
  // Calculate the 8-bit offset from the palette index.
  uint8_t offset = (uint8_t)(index >> 4);
  // Get the palette entry from the 4-bit index
  const CRGB* entry = &(pal[0]) + index_4bit;
  uint8_t red1   = entry->red;
  uint8_t green1 = entry->green;
  uint8_t blue1  = entry->blue;

  uint8_t blend = offset && (blendType != NOBLEND);
  if (blend) {
    if (index_4bit == 15) {
      entry = &(pal[0]);
    } else {
      entry++;
    }

    // Calculate the scaling factor and scaled values for the lower palette value.
    uint8_t f1 = 255 - offset;
    red1   = scale8_LEAVING_R1_DIRTY(red1,   f1);
    green1 = scale8_LEAVING_R1_DIRTY(green1, f1);
    blue1  = scale8_LEAVING_R1_DIRTY(blue1,  f1);

    // Calculate the scaled values for the neighbouring palette value.
    uint8_t red2   = entry->red;
    uint8_t green2 = entry->green;
    uint8_t blue2  = entry->blue;
    red2   = scale8_LEAVING_R1_DIRTY(red2,   offset);
    green2 = scale8_LEAVING_R1_DIRTY(green2, offset);
    blue2  = scale8_LEAVING_R1_DIRTY(blue2,  offset);
    cleanup_R1();

    // These sums can't overflow, so no qadd8 needed.
    red1   += red2;
    green1 += green2;
    blue1  += blue2;
  }
  if (brightness != 255) {
    // nscale8x3_video(red1, green1, blue1, brightness);
    nscale8x3(red1, green1, blue1, brightness);
  }
  return CRGB(red1, green1, blue1);
}
CRGB ColorFromPaletteExtended(const CRGBPalette32& pal, uint16_t index, uint8_t brightness, TBlendType blendType) {
  // Extract the five most significant bits of the index as a palette index.
  uint8_t index_5bit = (index >> 11);
  // Calculate the 8-bit offset from the palette index.
  uint8_t offset = (uint8_t)(index >> 3);
  // Get the palette entry from the 5-bit index
  const CRGB* entry = &(pal[0]) + index_5bit;
  uint8_t red1   = entry->red;
  uint8_t green1 = entry->green;
  uint8_t blue1  = entry->blue;

  uint8_t blend = offset && (blendType != NOBLEND);
  if (blend) {
    if (index_5bit == 31) {
      entry = &(pal[0]);
    } else {
      entry++;
    }

    // Calculate the scaling factor and scaled values for the lower palette value.
    uint8_t f1 = 255 - offset;
    red1   = scale8_LEAVING_R1_DIRTY(red1,   f1);
    green1 = scale8_LEAVING_R1_DIRTY(green1, f1);
    blue1  = scale8_LEAVING_R1_DIRTY(blue1,  f1);

    // Calculate the scaled values for the neighbouring palette value.
    uint8_t red2   = entry->red;
    uint8_t green2 = entry->green;
    uint8_t blue2  = entry->blue;
    red2   = scale8_LEAVING_R1_DIRTY(red2,   offset);
    green2 = scale8_LEAVING_R1_DIRTY(green2, offset);
    blue2  = scale8_LEAVING_R1_DIRTY(blue2,  offset);
    cleanup_R1();

    // These sums can't overflow, so no qadd8 needed.
    red1   += red2;
    green1 += green2;
    blue1  += blue2;
  }
  if (brightness != 255) {
    // nscale8x3_video(red1, green1, blue1, brightness);
    nscale8x3(red1, green1, blue1, brightness);
  }
  return CRGB(red1, green1, blue1);
}

typedef struct HSprite {
  uint16_t datasize;
  uint16_t frames;
  uint16_t duration;
  uint8_t flags;
  uint8_t palette_entries;
  CRGB palette[];
  uint8_t hs_data[];
} HSprite;

#include "gifs.h"

FL_PROGMEM extern const HSprite *const hsprite_list[] = {
  (HSprite*) &HSpr_35disk,
  (HSprite*) &HSpr_load,
  (HSprite*) &HSpr_vn,
  (HSprite*) &HSpr_unionflag16,
  (HSprite*) &HSpr_eu,
  (HSprite*) &HSpr_ghost,
  (HSprite*) &HSpr_scheisse,
  (HSprite*) &HSpr_devil_nod,
  (HSprite*) &HSpr_kputrummis,
  (HSprite*) &HSpr_rjb,
  (HSprite*) &HSpr_owl,
  (HSprite*) &HSpr_twylogo,
  (HSprite*) &HSpr_gear,
  (HSprite*) &HSpr_16x16_oric,
  (HSprite*) &HSpr_zx_k_ref,
  (HSprite*) &HSpr_invader,
  (HSprite*) &HSpr_c64,
  (HSprite*) &HSpr_amigaroll_trs,
  (HSprite*) &HSpr_joystick,
  (HSprite*) &HSpr_slime,
  (HSprite*) &HSpr_DigDug16x16,
  (HSprite*) &HSpr_octorokblue,
  (HSprite*) &HSpr_ptititi,
  (HSprite*) &HSpr_burgertime,
  (HSprite*) &HSpr_pouet_avatar_poi_charly_walk2,
  (HSprite*) &HSpr_rockhell,
  (HSprite*) &HSpr_pouet_avatar_mario,
  (HSprite*) &HSpr_kirby_run,
  (HSprite*) &HSpr_bub,
  (HSprite*) &HSpr_sirlord,
  (HSprite*) &HSpr_plane,
  (HSprite*) &HSpr_fire,
  (HSprite*) &HSpr_bird16,
  (HSprite*) &HSpr_mspacman,
  (HSprite*) &HSpr_ghost3,
  (HSprite*) &HSpr_batman2,
  (HSprite*) &HSpr_lemming,
  (HSprite*) &HSpr_mwk,
  (HSprite*) &HSpr_gwain2,
  (HSprite*) &HSpr_scoopexrulez,
  (HSprite*) &HSpr_8bit_avatar,
};
#define HSPRITES_N (sizeof(hsprite_list) / sizeof(hsprite_list[0]))


typedef struct {
  uint8_t xw, yw, xd, yd;
  int16_t xdd, ydd, bcd;
} RainbowSmoothiePreset;
typedef struct {
  uint32_t frame = 0;
  uint32_t millis = 0;
  uint8_t effect = 2;
  uint8_t palette_n = 1;
  uint8_t alpha_fade = 64;
  uint8_t brightness = BRIGHTNESS;
  uint8_t rs_preset_n = 0;
  TBlendType currentBlending = LINEARBLEND;
  uint8_t extendedmixing = 1;
  RainbowSmoothiePreset rs;
} Config;
Config cfg;

// /*__attribute__ ((section(".noinit")))*/ uint8_t * nv_preset;
void setup() {
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setCorrection(UncorrectedColor);
  FastLED.setTemperature(UncorrectedTemperature);
  FastLED.setDither(DISABLE_DITHER);
  // FastLED.setMaxRefreshRate(400);
  if (MAX_MILLIWATTS > 0) FastLED.setMaxPowerInMilliWatts(MAX_MILLIWATTS);

  if (SERIAL_UI == 1) {
    Serial.begin(250000);
    halp();
  }

  pinMode(LED_BUILTIN, OUTPUT);
  inc_palette(0);
  inc_rs_preset(0);
}

#define MAX_EFFECTS 3
void loop() {
  uint8_t effect = cfg.effect + 1;
  if (effect & 1) rainbow_smoothie();
  if (effect & 2) scintillating_heatshrink();
  if (SERIAL_UI == 1) poll_serial();
  if (cfg.frame <= FADEIN_FRAMES)
    FastLED.setBrightness(scale8(cfg.brightness, (cfg.frame * 255) / FADEIN_FRAMES));

  // cap the frame rate and indicate idle time via the built-in LED
  cfg.frame++;
  uint32_t frame_time = millis() - cfg.millis;  // wraparound safe
  int32_t pause = MS_GOAL - frame_time;
  // if (pause < 0 && SERIAL_UI == 1) { Serial.print(-pause); Serial.println("ms late"); }
  digitalWrite(LED_BUILTIN, HIGH);
  if (pause > 0) delay(pause);
  digitalWrite(LED_BUILTIN, LOW);
  cfg.millis = millis();
  FastLED.show();
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

void poll_serial() {
  if (Serial.available() <= 0)
    return;
  int input = Serial.read();
  switch (input) {
    case 'e':
      cfg.effect = addmod8(cfg.effect, 1, MAX_EFFECTS);
      break;
    case 'f':
      cfg.alpha_fade = qsub8(cfg.alpha_fade, 1);
      break;
    case 'F':
      cfg.alpha_fade = qadd8(cfg.alpha_fade, 1);
      break;
    case '{':
      cfg.currentBlending = LINEARBLEND;
      break;
    case '}':
      cfg.currentBlending = NOBLEND;
      break;
    case '[':
      cfg.extendedmixing = 1;
      break;
    case ']':
      cfg.extendedmixing = 0;
      break;
    case 'p':
      inc_palette(1);
      break;
    case '~':
      inc_rs_preset(1);
      break;
    case 'B':
      cfg.brightness = qadd8(cfg.brightness, 1);
      FastLED.setBrightness(cfg.brightness);
      break;
    case 'b':
      cfg.brightness = qsub8(cfg.brightness, 1);
      FastLED.setBrightness(cfg.brightness);
      break;
    case 'd':
      FastLED.setDither(DISABLE_DITHER);
      break;
    case 'D':
      FastLED.setDither(BINARY_DITHER);
      break;
    case ',':
      cfg.rs.bcd -= 64;
      break;
    case '<':
      cfg.rs.bcd -= 256;
      break;
    case '.':
      cfg.rs.bcd += 64;
      break;
    case '>':
      cfg.rs.bcd += 256;
      break;
    case '#':
      randomise_rainbowsmoothie();
      break;
    case 13:
      print_params();
      break;
    case 10:
      break;
    default:
      Serial.println(F("?"));
  }
}
void halp() {
  Serial.println(F(" #\trandomise rainbow smoothie\r\n"
                   " ~\tchoose next preset for rainbow smoothie\r\n"
                   " <enter>\tprint parameters\r\n"
                   " e\tswitch to next Effect\r\n"
                   " p\tswitch to next palette\r\n"
                   " {}\tturn extended palette blending on/off\r\n"
                   " []\tturn linear palette blending on/off\r\n"
                   " ,.<>\tchange colour cycling rate\r\n"
                   " bB\tbrightness down/up\r\n"
                   " dD\tbinary dither off/on\r\n"
                   " fF\tdecrease/increase fade out rate\r\n"));
}
void print_params() {
  Serial.println(F("bright\tfade\txw\tyw\txd\tyd\txdd\tydd\tbcd\tFPS"));
  Serial.print(cfg.brightness); Serial.print("\t");
  Serial.print(cfg.alpha_fade); Serial.print("\t");
  Serial.print(cfg.rs.xw); Serial.print("\t");
  Serial.print(cfg.rs.yw); Serial.print("\t");
  Serial.print(cfg.rs.xd); Serial.print("\t");
  Serial.print(cfg.rs.yd); Serial.print("\t");
  Serial.print(cfg.rs.xdd); Serial.print("\t");
  Serial.print(cfg.rs.ydd); Serial.print("\t");
  Serial.print(cfg.rs.bcd); Serial.print("\t");
  Serial.println(FastLED.getFPS());
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

// FL_PROGMEM extern const TProgmemPalette16 myRedWhiteBluePalette_p = {
//     CRGB::Red,    CRGB::Gray,    CRGB::Blue,    CRGB::Black,
//     CRGB::Red,    CRGB::Gray,    CRGB::Blue,    CRGB::Black,
//     CRGB::Red,    CRGB::Red,     CRGB::Gray,    CRGB::Gray,
//     CRGB::Blue,   CRGB::Blue,    CRGB::Black,   CRGB::Black
// };
// FL_PROGMEM extern const TProgmemPalette16 myBlackWhitePalette_p = {
//     CRGB::Black,    CRGB::Black,    CRGB::White,    CRGB::Grey,
//     CRGB::Black,    CRGB::Black,    CRGB::White,    CRGB::Grey,
//     CRGB::Black,    CRGB::Black,    CRGB::White,    CRGB::Grey,
//     CRGB::Black,    CRGB::Black,    CRGB::White,    CRGB::Grey,
// };
FL_PROGMEM extern const TProgmemRGBPalette16 RainbowStripeColors2_p = {
  0xFF0000, 0x7F0000, 0xAB5500, 0x552A00,
  0xABAB00, 0x555500, 0x00FF00, 0x007F00,
  0x00AB55, 0x00552A, 0x0000FF, 0x00007F,
  0x5500AB, 0x2A0055, 0xAB0055, 0x55002A
};

// DEFINE_GRADIENT_PALETTE(froth616_gp) {
// // http://soliton.vm.bytemark.co.uk/pub/cpt-city/fractint/base/tn/froth616.png.index.html
// // Size: 116 bytes of program space.
//     0, 247,  0,247,
//    17,  75,  0, 79,
//    33, 247,  0,  0,
//    51,  75,  0,  0,
//    68, 247,248,  0,
//    84,  75, 91,  0,
//   102,   0,248,  0,
//   119,   0, 91,  0,
//   135,   0,  0,247,
//   153,   0,  0, 79,
//   170,   0,248,247,
//   186,   0, 91, 79,
//   204,   0,  0,  0,
//   237,   0,  0,  0,
//   255, 247,248,247};

FL_PROGMEM extern const TProgmemRGBPalette16 *const palettes16[] = {
  &RainbowColors_p, &RainbowStripeColors_p, &RainbowStripeColors2_p,
  // &HeatColors_p, &CloudColors_p, &LavaColors_p, &OceanColors_p,
  // &ForestColors_p, &PartyColors_p,
  // &myRedWhiteBluePalette_p, &myBlackWhitePalette_p
};
#define RGB16PALETTES (sizeof(palettes16) / sizeof(palettes16[0]))
FL_PROGMEM extern const TProgmemRGBGradientPalettePtr palettesGrad[] = {
  // froth616_gp,
};
#define RGBGRADPALETTES (sizeof(palettesGrad) / sizeof(TProgmemRGBGradientPalettePtr))

void inc_palette(uint8_t n) {
  uint8_t total_palettes = RGB16PALETTES + RGBGRADPALETTES;
  cfg.palette_n = n = addmod8(cfg.palette_n, n, total_palettes);
  if (n < RGBGRADPALETTES) {
    currentPalette = (TProgmemRGBGradientPalettePtr) FL_PGM_READ_PTR_NEAR(&palettesGrad[n]);
    return;
  }
  n -= RGBGRADPALETTES;
  if (n < RGB16PALETTES) {
    currentPalette = *(TProgmemRGBPalette16 *) FL_PGM_READ_PTR_NEAR(&palettes16[n]);
    return;
  }
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

FL_PROGMEM const RainbowSmoothiePreset rs_presets[] = {
  //xw yw   xd   yd     xdd     ydd    bcd
  { 4,  3,  80, 208,  15327,  -3010,  27000},
  // { 4,  4,  14,  14,   6514, -16076,  16384},
  { 1,  1,  34,  86,  -7661,  30835,  9600},
  // { 7, 12, 105,  46,   5563,   5313,-10240},
  // {19, 19, 200, 237,  20142,  14656, -2857},
  // {20,  6, 116,  33,  -8683, -10095, -4633},
  // { 4,  3,  16,  48,  15347,   6636,  3331},
  // { 5, 10,   2,  44,  -7661,  30835, -2414},
  // { 3,  3,  64, 231,   8691,   7618,  3795},
  // { 7,  5,  36, 111,  25249,   4738, -1283},
  // { 4,  8, 255, 255, -32767, -32690, -6234},
  // { 2,  5, 173, 103, -18586,  14361,  3788},
};
#define MAX_RS_PRESETS (sizeof(rs_presets) / sizeof(RainbowSmoothiePreset))

void inc_rs_preset(uint8_t n) {
  cfg.rs_preset_n = addmod8(cfg.rs_preset_n, n, MAX_RS_PRESETS);
  uint8_t* src = (uint8_t *) &rs_presets[cfg.rs_preset_n];
  uint8_t* dst = (uint8_t *) &cfg.rs;
  memcpy_P(dst, src, sizeof(cfg.rs));
}

void rainbow_smoothie() {
  uint32_t startHue = cfg.frame * cfg.rs.bcd;
  uint32_t yHueDelta = ((uint32_t)sin16(cfg.frame * cfg.rs.xd) * cfg.rs.xw);
  uint32_t xHueDelta = ((uint32_t)cos16(cfg.frame * cfg.rs.yd) * cfg.rs.yw);
  uint32_t lineStartHue = startHue - (kMatrixHeight / 2) * yHueDelta;
  uint32_t yd2 = cfg.rs.ydd * 2048;
  uint32_t xd2 = cfg.rs.xdd * 2048;
  for (byte y = 0; y < kMatrixHeight; y++) {
    lineStartHue += yHueDelta;
    yHueDelta += yd2;
    uint32_t pixelHue = lineStartHue - (kMatrixWidth / 2) * xHueDelta;
    uint32_t xhd = xHueDelta;
    for (byte x = 0; x < kMatrixWidth; x++) {
      pixelHue += xhd;
      xhd += xd2;
      if (cfg.extendedmixing == 1) {
        leds[XY(x, y)] = ColorFromPaletteExtended(currentPalette, pixelHue >> 7, 255, cfg.currentBlending);
      } else {
        leds[XY(x, y)] = ColorFromPalette(currentPalette, pixelHue >> 15, 255, cfg.currentBlending);
      }
    }
  }
}

void randomise_rainbowsmoothie() {
  random16_add_entropy(random());
  cfg.rs.bcd = random16(16383) - 8192;
  cfg.rs.xd = random8();
  cfg.rs.yd = random8();
  cfg.rs.xdd = random16() - 32768;
  cfg.rs.ydd = random16() - 32768;
  cfg.rs.xw = random8(24);
  cfg.rs.yw = random8(24);
}

struct hstatus {
  uint32_t last_millis;
  heatshrink_decoder hsd;
  size_t heatsunk;
  uint16_t frame;
  uint8_t hs_spr_n = -1;
  uint8_t loops;
  CRGB palette[GIF2H_MAX_PALETTE_ENTRIES];
} hstatus;

// HStatus hstatus;

void heatshrunk_sprite_reset(struct hstatus * status, uint8_t hs_spr_n) {
  // decompress the palette and leave the decompressor ready to emit pixels
  status->hs_spr_n = hs_spr_n;
  status->heatsunk = 0;
  status->frame = 0;
  HSprite *spr_ptr = (HSprite *) FL_PGM_READ_PTR_NEAR(&hsprite_list[hs_spr_n]);
  // uint16_t datasize = FL_PGM_READ_WORD_NEAR(&spr_ptr->datasize);
  uint8_t palette_entries = FL_PGM_READ_BYTE_NEAR(&spr_ptr->palette_entries);

  // set heatshrink's window buffer position, so the pixels end up aligned with the start of the buffer
  heatshrink_decoder_reset(&status->hsd);
  status->hsd.head_index -= palette_entries * sizeof(CRGB);

  uint8_t *dst = (uint8_t *) &status->palette;
  uint16_t remaining = palette_entries * sizeof(CRGB);
  size_t count = 0;
  while (remaining) {
    heatshrink_decoder_sinkP(&status->hsd, &spr_ptr->hs_data[status->heatsunk],
                             HEATSHRINK_STATIC_INPUT_BUFFER_SIZE, &count);
    status->heatsunk += count;
    HSD_poll_res pres;
    do {
      pres = heatshrink_decoder_poll(&status->hsd, dst, remaining, &count);
      dst += count;
      remaining -= count;
    } while ((pres == HSDR_POLL_MORE) && remaining);
  }
}

void heatshrunk_sprite_prepare(struct hstatus * status, uint8_t hs_spr_n) {
  HSprite *spr_ptr = (HSprite *) FL_PGM_READ_PTR_NEAR(&hsprite_list[hs_spr_n]);
  uint16_t datasize = FL_PGM_READ_WORD_NEAR(&spr_ptr->datasize);
  uint16_t frames = FL_PGM_READ_WORD_NEAR(&spr_ptr->frames);
  uint16_t duration = FL_PGM_READ_WORD_NEAR(&spr_ptr->duration);
  // uint8_t flags = FL_PGM_READ_BYTE_NEAR(&spr_ptr->flags);

  if (hs_spr_n != status->hs_spr_n) {
    // a different sprite has been selected;  force a reset
    status->frame = frames;
    status->loops = 0;
    duration = 0;
  }

  // time to decompress the next frame?
  if ((cfg.millis - status->last_millis) < duration) return;
  if (frames == 1 && status->loops != 0) return;

  // need to loop/reset first?
  if (status->frame >= frames) {
    heatshrunk_sprite_reset(status, hs_spr_n);
    status->loops += 1;
  }

  status->frame++;
  status->last_millis = cfg.millis;
  size_t count = 0;
  uint16_t remaining = GIF2H_NUM_PIXELS;
  while (remaining) {
    if (status->heatsunk < datasize) {
      heatshrink_decoder_sinkP(&status->hsd, &spr_ptr->hs_data[status->heatsunk],
                               datasize - status->heatsunk, &count);
      status->heatsunk += count;
    }
    HSD_poll_res pres;
    do {
      pres = heatshrink_decoder_poll(&status->hsd, 0, remaining, &count);
      remaining -= count;
    } while ((pres == HSDR_POLL_MORE) && remaining);
    if (0 && (status->heatsunk >= datasize)) {
      heatshrink_decoder_finish(&status->hsd);  // not strictly needed when using static buffers
    }
  }
}

void heatshrunk_sprite_plot(int8_t xstart, int8_t ystart, struct hstatus * status, const uint8_t fade_speed) {
  CRGB *pal = (CRGB *) status->palette;
  uint8_t *pixels = status->hsd.buffers + HEATSHRINK_STATIC_INPUT_BUFFER_SIZE;
  // plot each sprite pixel modulo the size of the matrix, i.e. wrap the image
  for (uint8_t y = 0; y < kMatrixHeight; y++) {
    for (uint8_t x = 0; x < kMatrixWidth; x++) {
      uint16_t led_index = XY(addmod8(x, xstart, kMatrixWidth),
                              addmod8(y, ystart, kMatrixHeight));
      uint8_t palette_index = *pixels++;
      if (palette_index > 0 && cfg.effect != 1) {
        // non-0 palette entry;  copy the palette entry to the LED
        leds[led_index] = pal[palette_index];
      } else {
        // palette entry 0 is the mask;  fade this LED to the mask colour (black usually)
        fadeTowardColour(leds[led_index], pal[palette_index], fade_speed);
      }
    }
  }
}

void scintillating_heatshrink() {
  static uint8_t spr_n = 0;
  static uint32_t last_spr_change = 0;
  if ((hstatus.loops > 0) && (cfg.millis - last_spr_change > 2500)) {
    last_spr_change = cfg.millis;
    spr_n = addmod8(spr_n, 1, HSPRITES_N);
  }
  heatshrunk_sprite_prepare(&hstatus, spr_n);
  heatshrunk_sprite_plot(0 /*- (cos8(cfg.frame & 0x7f) >> 3)*/, 0, &hstatus, cfg.alpha_fade);
}
