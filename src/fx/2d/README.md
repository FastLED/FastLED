# FastLED FX: 2D Effects

This folder contains two-dimensional (matrix/panel) effects that derive from `fl::Fx2d`. 2D effects require an `XYMap` that translates a logical `(x, y)` coordinate into a linear LED index in your `CRGB*` matrix. For examples, most matrices are laid out in serpentine, back and forth layout. `XYMap` allows you to pretend an effect is a plain rectangular grid, then transform it to a matrix layout, (commonly serpentine back and forth.)

If you’re new: construct the effect with your `XYMap` and call `draw(...)` each frame. The effect fills your LED buffer according to its internal animation.

### What’s here
- **RedSquare (`redsquare.h`)**: Simple centered red square demo. Good as a sanity check.
- **NoisePalette (`noisepalette.h`)**: 2D noise field mapped through color palettes. Includes helpers to randomize/select palettes and an optional fixed FPS hint.
- **WaveFx (`wave.h`)**: 2D wave simulation with configurable speed, dampening, supersampling, cylindrical wrapping, and color mapping via:
  - `WaveCrgbMapDefault`: grayscale by wave amplitude.
  - `WaveCrgbGradientMap`: map amplitude to a `CRGBPalette16` gradient.
- **ScaleUp (`scale_up.h`)**: Wrap another `Fx2d` and up-scale its output using bilinear interpolation. Useful when your MCU can’t render full resolution in real time.
- **Blend2d (`blend.h`)**: Compose multiple `Fx2d` layers. Bottom layer draws at full strength; upper layers blend with optional blur passes.
- **Animartrix (`animartrix.hpp`, `animartrix_detail.hpp`)**: Integration of Stefen Petrick’s ANIMartRIX library providing many parameterized 2D animations. See Licensing below.

### Common usage
- Provide an `fl::XYMap` that matches your panel/strip weaving, then construct effects like `fl::NoisePalette mapFx(xyMap);`, `fl::WaveFx waves(xyMap);`, etc.
- Call `fx.draw(fl::Fx::DrawContext(now, leds));` each frame. Some effects (e.g., `NoisePalette`) advertise a fixed FPS via `hasFixedFrameRate` to help you pace the main loop.
- For `Blend2d`, create it with your `XYMap`, add `Fx2d` layers, optionally set per-layer blur, then call `draw(...)` on the blender.
- For `ScaleUp`, pass a low-res delegate `Fx2d`; it will render internally and upscale to your target resolution.

### Parameters to try
- WaveFx: `setSpeed`, `setDampening`, `setHalfDuplex`, `setSuperSample`, `setXCylindrical`, and switch CRGB mapping with `setCrgbMap(...)`.
- NoisePalette: `setPalette`, `setSpeed`, `setScale`, or `setPalettePreset(...)` to quickly swap among curated looks.
- Blend2d: `setGlobalBlurAmount`, `setGlobalBlurPasses`, or per-layer `Params` on `add(...)` / `setParams(...)`.

### Licensing note (Animartrix)
Animartrix is free for non‑commercial use and requires a paid license otherwise. See the top-level `src/fx/readme` and `animartrix_detail.hpp` header comments.

### Key types referenced
- **`fl::Fx2d`**: Base for 2D effects, wraps an `XYMap` and provides `draw(DrawContext)`.
- **`fl::XYMap`**: Maps `(x, y)` to LED indices; can be rectangular or custom.
- **`CRGBPalette16`**: 16‑entry FastLED palette used by several mappers/effects.

These effects are designed for more capable MCUs, but many run well on modern 8‑bit boards at modest sizes. Start with `RedSquare` to validate mapping, then try `NoisePalette` and `WaveFx` for richer motion.

### Examples
- NoisePalette (from `examples/FxNoisePlusPalette/FxNoisePlusPalette.ino`):
  ```cpp
  #include <FastLED.h>
  #include "fx/2d/noisepalette.h"
  using namespace fl;
  #define W 16
  #define H 16
  #define NUM_LEDS (W*H)
  CRGB leds[NUM_LEDS];
  XYMap xyMap(W, H, /* serpentine? */ true);
  NoisePalette fx(xyMap);
  void setup(){ FastLED.addLeds<WS2811, 3, GRB>(leds, NUM_LEDS); FastLED.setBrightness(96); }
  void loop(){ fx.draw(Fx::DrawContext(millis(), leds)); FastLED.show(); }
  ```
- WaveFx layered with Blend2d (from `examples/FxWave2d/`):
  ```cpp
  #include <FastLED.h>
  #include "fx/2d/wave.h"
  #include "fx/2d/blend.h"
  using namespace fl;
  #define W 64
  #define H 64
  CRGB leds[W*H];
  XYMap xyMap(W, H, true);
  Blend2d blender(xyMap);
  auto lower = fl::make_shared<WaveFx>(xyMap, WaveFx::Args());
  auto upper = fl::make_shared<WaveFx>(xyMap, WaveFx::Args());
  void setup(){ FastLED.addLeds<WS2811, 3, GRB>(leds, W*H); blender.add(lower); blender.add(upper); }
  void loop(){ blender.draw(Fx::DrawContext(millis(), leds)); FastLED.show(); }
  ```
- RedSquare (from concept in `redsquare.h`):
  ```cpp
  #include <FastLED.h>
  #include "fx/2d/redsquare.h"
  using namespace fl;
  #define W 16
  #define H 16
  CRGB leds[W*H];
  XYMap xyMap(W, H, true);
  RedSquare fx(xyMap);
  void setup(){ FastLED.addLeds<WS2811,3,GRB>(leds, W*H); }
  void loop(){ fx.draw(Fx::DrawContext(millis(), leds)); FastLED.show(); }
  ```
