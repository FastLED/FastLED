# FastLED FX: 1D Effects

This folder contains one-dimensional (strip) visual effects. All effects derive from `fl::Fx1d` and render into a linear `CRGB* leds` buffer using a common `DrawContext`.

If you’re new to FastLED and C++: think of an effect as a small object you construct once and then call `draw(...)` every frame with the current time and your LED buffer.

### What’s here
- **Cylon (`cylon.h`)**: A single dot sweeps back and forth (Larson scanner). Tunable `fade_amount`, `delay_ms`.
- **DemoReel100 (`demoreel100.h`)**: Classic demo with rotating patterns (rainbow, glitter, confetti, sinelon, bpm, juggle). Auto-advances every ~10 seconds.
- **Fire2012 (`fire2012.h`)**: Procedural fire simulation with parameters `cooling` and `sparking`, optional reverse direction and palette.
- **NoiseWave (`noisewave.h`)**: Smooth noise-driven red/blue waves across the strip.
- **Pacifica (`pacifica.h`)**: Gentle blue-green ocean waves layered for depth.
- **Pride2015 (`pride2015.h`)**: Ever-changing rainbow animation.
- **TwinkleFox (`twinklefox.h`)**: Twinkling holiday-style lights using palettes; compile-time knobs for speed/density and incandescent-style cooling.

### Common usage
- Include the header for the effect you want and construct it with the LED count (e.g., `fl::Cylon fx(num_leds);`).
- Each frame (e.g., in your loop), call `fx.draw(fl::Fx::DrawContext(now, leds));`.
- Most effects expose small parameters via constructor (e.g., `Fire2012(num_leds, cooling, sparking)`), or have internal timing based on `now` from `DrawContext`.

### Notes and tips
- Effects guard against `nullptr` and zero length, but you should pass a valid `CRGB*` and correct `num_leds` to see output.
- Try different frame rates. Many patterns look good at ~50–100 FPS, but 30 FPS is fine for most.
- For palette-based effects (e.g., TwinkleFox, Pride2015, DemoReel’s BPM), experiment with `CRGBPalette16` to customize colors.
- These 1D effects are lightweight to medium-weight. They depend on FastLED helpers such as `CHSV`, `beatsin*`, `nblend`, palettes, and random noise.

### Key types referenced
- **`fl::Fx1d`**: Base class for 1D effects.
- **`fl::Fx::DrawContext`**: Carries `now` (time in ms), `CRGB* leds`, and optional per-frame hints.
- **`CRGB` / `CHSV`**: FastLED’s color types.

Explore the headers to see per-effect parameters and internal logic; everything here is self-contained and ready to drop into your render loop.

### Examples
- Cylon (from `examples/FxCylon/FxCylon.ino`):
  ```cpp
  #include <FastLED.h>
  #include "fx/1d/cylon.h"
  using namespace fl;
  #define NUM_LEDS 64
  CRGB leds[NUM_LEDS];
  Cylon cylon(NUM_LEDS);
  void setup() {
    FastLED.addLeds<WS2812, 2, RGB>(leds, NUM_LEDS);
  }
  void loop() {
    cylon.draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
    delay(cylon.delay_ms);
  }
  ```
- DemoReel100 (from `examples/FxDemoReel100/FxDemoReel100.ino`):
  ```cpp
  #include <FastLED.h>
  #include "fx/1d/demoreel100.h"
  using namespace fl;
  #define NUM_LEDS 64
  CRGB leds[NUM_LEDS];
  DemoReel100Ptr fx = fl::make_shared<DemoReel100>(NUM_LEDS);
  void setup() { FastLED.addLeds<WS2811, 3, GRB>(leds, NUM_LEDS); }
  void loop() {
    fx->draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
    FastLED.delay(1000/120);
  }
  ```
- Fire2012 (from `examples/FxFire2012/FxFire2012.ino`):
  ```cpp
  #include <FastLED.h>
  #include "fx/1d/fire2012.h"
  using namespace fl;
  #define NUM_LEDS 92
  CRGB leds[NUM_LEDS];
  Fire2012Ptr fire = fl::make_shared<Fire2012>(NUM_LEDS, 55, 120, false);
  void setup() { FastLED.addLeds<WS2811, 5, GRB>(leds, NUM_LEDS); }
  void loop() {
    fire->draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
    FastLED.delay(1000/30);
  }
  ```
- Pacifica (from `examples/FxPacifica/FxPacifica.ino`):
  ```cpp
  #include <FastLED.h>
  #include "fx/1d/pacifica.h"
  using namespace fl;
  #define NUM_LEDS 60
  CRGB leds[NUM_LEDS];
  Pacifica pacifica(NUM_LEDS);
  void setup() { FastLED.addLeds<WS2812B, 3, GRB>(leds, NUM_LEDS); }
  void loop() {
    pacifica.draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
  }
  ```
- Pride2015 (from `examples/FxPride2015/FxPride2015.ino`):
  ```cpp
  #include <FastLED.h>
  #include "fx/1d/pride2015.h"
  using namespace fl;
  #define NUM_LEDS 200
  CRGB leds[NUM_LEDS];
  Pride2015 pride(NUM_LEDS);
  void setup() { FastLED.addLeds<WS2811, 3, GRB>(leds, NUM_LEDS); }
  void loop() {
    pride.draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
  }
  ```
- TwinkleFox (from `examples/FxTwinkleFox/FxTwinkleFox.ino`):
  ```cpp
  #include <FastLED.h>
  #include "fx/1d/twinklefox.h"
  using namespace fl;
  #define NUM_LEDS 100
  CRGB leds[NUM_LEDS];
  TwinkleFox fx(NUM_LEDS);
  void setup() { FastLED.addLeds<WS2811, 3, GRB>(leds, NUM_LEDS).setRgbw(); }
  void loop() {
    EVERY_N_SECONDS(SECONDS_PER_PALETTE) { fx.chooseNextColorPalette(fx.targetPalette); }
    fx.draw(Fx::DrawContext(millis(), leds));
    FastLED.show();
  }
  ```
