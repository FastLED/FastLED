# FastLED FX Library (`src/fx`)

The FX library adds optional, higher‑level visual effects and a small runtime around FastLED. It includes ready‑to‑use animations for strips (1D) and matrices (2D), utilities to layer/up‑scale/blend effects, and a simple video playback pipeline.

If you’re new to FastLED and C++: an effect is an object that you construct once and call `draw(...)` on every frame, passing time and your LED buffer. Start with the 1D/2D examples, then explore composition and video as needed.

### What’s included
- [`1d/`](./1d/README.md): Strip effects like Cylon, DemoReel100, Fire2012, NoiseWave, Pacifica, Pride2015, and TwinkleFox.
- [`2d/`](./2d/README.md): Matrix effects including NoisePalette, WaveFx, ScaleUp, Blend2d (compositing effects), and Animartrix integrations.
- [`video/`](./video/README.md): Read frames from files/streams, buffer, and interpolate for smooth playback on your LEDs.
- [`detail/`](./detail/README.md): Internal helpers for draw context, layering, transitions, and compositing.

### Core concepts and types
- **Base classes**:
  - `fl::Fx`: abstract base with a single method to implement: `void draw(DrawContext ctx)`.
  - `fl::Fx1d`: base for strip effects; holds LED count and (optionally) an `XMap`.
  - `fl::Fx2d`: base for matrix effects; holds an `XYMap` to convert `(x,y)` to LED indices.
- **DrawContext**: `Fx::DrawContext` carries per‑frame data: `now` (ms), `CRGB* leds`, optional `frame_time`, and a `speed` hint.
- **Palettes and helpers**: Many effects use FastLED’s `CRGB`, `CHSV`, `CRGBPalette16`, and timing helpers like `beatsin*`.

### Basic usage (1D example)
```cpp
#include "fx/1d/cylon.h"

fl::Cylon fx(num_leds);
...
fx.draw(fl::Fx::DrawContext(millis(), leds));
```

### Basic usage (2D example)
```cpp
#include "fx/2d/noisepalette.h"

fl::XYMap xy(width, height, /* your mapper */);
fl::NoisePalette fx(xy);
...
fx.draw(fl::Fx::DrawContext(millis(), leds));
```

### Composition and transitions
- Use `Blend2d` to stack multiple 2D effects and blur/blend them.
- The `detail/` components (`FxLayer`, `FxCompositor`, `Transition`) support cross‑fading between effects over time.

### Video playback
- The `video/` pipeline reads frames from a `FileHandle` or `ByteStream`, keeps a small buffer, and interpolates between frames to match your output rate.

### Performance and targets
- FX components may use more memory/CPU than the FastLED core. They are designed for more capable MCUs (e.g., ESP32/Teensy/RP2040), but many examples will still run on modest hardware at smaller sizes.

### Licensing
- Most code follows the standard FastLED license. Animartrix is free for non‑commercial use and paid otherwise. See [`src/fx/readme`](./readme) and headers for details.

Explore each subfolder’s README to find the effect you want, then copy the corresponding header into your project and call `draw(...)` every frame.

### Examples (from `examples/Fx*`)
- 1D strip effects:
  - Cylon: `examples/FxCylon/FxCylon.ino`
  - DemoReel100: `examples/FxDemoReel100/FxDemoReel100.ino`
  - Fire2012: `examples/FxFire2012/FxFire2012.ino`
  - Pacifica: `examples/FxPacifica/FxPacifica.ino`
  - Pride2015: `examples/FxPride2015/FxPride2015.ino`
  - TwinkleFox: `examples/FxTwinkleFox/FxTwinkleFox.ino`
- 2D matrix effects:
  - NoisePalette: `examples/FxNoisePlusPalette/FxNoisePlusPalette.ino`
  - WaveFx layered: `examples/FxWave2d/`
- Video pipeline:
  - Memory stream: `examples/FxGfx2Video/FxGfx2Video.ino`
  - SD card playback: `examples/FxSdCard/FxSdCard.ino`

Minimal 1D call pattern:
```cpp
fx.draw(fl::Fx::DrawContext(millis(), leds));
FastLED.show();
```
Minimal 2D call pattern (requires `XYMap`):
```cpp
fl::NoisePalette fx(xyMap);
fx.draw(fl::Fx::DrawContext(millis(), leds));
FastLED.show();
```
