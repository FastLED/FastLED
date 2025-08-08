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
- **`fl::Fx1d`**: Base class for 1D effects. Holds `mNumLeds` and provides the `draw(DrawContext)` interface.
- **`fl::Fx::DrawContext`**: Carries `now` (time in ms), `CRGB* leds`, and optional per-frame hints.
- **`CRGB` / `CHSV`**: FastLED’s color types.

Explore the headers to see per-effect parameters and internal logic; everything here is self-contained and ready to drop into your render loop.