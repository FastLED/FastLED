# FastLED FX: Internal Helpers (`detail/`)

This folder contains utilities used by the FX engine. Most apps won’t include these directly; they’re pulled in by higher‑level classes.

### Components
- **`draw_context.h`**: Defines `fl::_DrawContext` (commonly referred to via `Fx::DrawContext`), which carries per‑frame data into effects: `now` (ms), `CRGB* leds`, `frame_time`, and a `speed` hint.
- **`fx_layer.h`**: Wraps an `Fx` and a `Frame` surface. Manages start/pause/draw and exposes the surface for compositing.
- **`fx_compositor.h`**: Cross‑fades between two `FxLayer`s over time using `Transition`. Produces a final buffer by blending surfaces each frame; automatically completes when progress reaches 100%.
- **`transition.h`**: Tracks a time‑based 0–255 progress value between a start time and duration. Used by the compositor to animate transitions.

### When you’ll encounter these
- If you use an FX engine/controller that switches effects with fades.
- If you build your own multi‑effect player, you may interact with `FxLayer` and `FxCompositor` directly.

These pieces are small and dependency‑light, but they’re part of the internal plumbing for higher‑level modules.